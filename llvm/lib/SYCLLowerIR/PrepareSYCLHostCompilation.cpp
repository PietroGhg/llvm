//===------ PrepareSYCLHostCompilation.cpp - SYCL Host Compilation Preparation
// Pass ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Prepares the kernel for SYCL Host Compilation:
// * Handles kernel calling convention and attributes.
// * Materializes spirv buitlins.
//===----------------------------------------------------------------------===//

#include "llvm/SYCLLowerIR/PrepareSYCLHostCompilation.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include <functional>
#include <numeric>

using namespace llvm;

namespace {
// Wrapper for the pass to make it working with the old pass manager
class PrepareSYCLHostCompilationLegacyPass : public ModulePass {
public:
  static char ID;
  PrepareSYCLHostCompilationLegacyPass() : ModulePass(ID) {
    initializePrepareSYCLHostCompilationLegacyPassPass(
        *PassRegistry::getPassRegistry());
  }

  // run the SYCLMutatePrintfAddrspace pass on the specified module
  bool runOnModule(Module &M) override {
    ModuleAnalysisManager MAM;
    auto PA = Impl.run(M, MAM);
    return !PA.areAllPreserved();
  }

private:
  PrepareSYCLHostCompilationPass Impl;
};

} // namespace

char PrepareSYCLHostCompilationLegacyPass::ID = 0;
INITIALIZE_PASS(PrepareSYCLHostCompilationLegacyPass, "prepare-sycl-hc",
                "Prepare SYCL Kernels for SYCL Host Compilation", false, false)

// Public interface to the SYCLMutatePrintfAddrspacePass.
ModulePass *llvm::createPrepareSYCLHostCompilationLegacyPass() {
  return new PrepareSYCLHostCompilationLegacyPass();
}

namespace {


void fixCallingConv(Function* F) {
  F->setCallingConv(llvm::CallingConv::C);
  // TODO: the frame-pointer=all attribute apparently makes the kernel crash at runtime
  F->setAttributes({});
}
Function *addArg(Function *oldF, Type *T) {
  auto oldT = oldF->getFunctionType();
  auto retT = oldT->getReturnType();

  std::vector<Type *> args;
  for (auto arg : oldT->params()) {
    args.push_back(arg);
  }
  args.push_back(T);
  auto newT = FunctionType::get(retT, args, oldF->isVarArg());
  auto newF = Function::Create(newT, oldF->getLinkage(), oldF->getName(),
                               oldF->getParent());
  // Copy the old function's attributes
  newF->setAttributes(oldF->getAttributes());

  // Map old arguments to new arguments
  ValueToValueMapTy VMap;
  for (auto pair : llvm::zip(oldF->args(), newF->args())) {
    auto &oldA = std::get<0>(pair);
    auto &newA = std::get<1>(pair);
    VMap[&oldA] = &newA;
  }

  SmallVector<ReturnInst *, 1> ReturnInst;
  if (!oldF->isDeclaration())
    CloneFunctionInto(newF, oldF, VMap,
                      CloneFunctionChangeType::LocalChangesOnly, ReturnInst);
  return newF;
}

static std::map<std::string, std::string> BuiltinNamesMap{
    {"__spirv_BuiltInGlobalInvocationId", "_Z16hc_get_global_idmP9_hc_state"}};

Function *getReplaceFunc(Module &M, Type *T, StringRef Name) {
  Function *F = M.getFunction(Name);
  assert(F && "Error retrieving replace function");
  return F;
}

Value *getStateArg(Function *F) {
  auto F_t = F->getFunctionType();
  return F->getArg(F_t->getNumParams() - 1);
}

} // namespace

PreservedAnalyses
PrepareSYCLHostCompilationPass::run(Module &M, ModuleAnalysisManager &MAM) {
  bool ModuleChanged = false;
  SmallVector<Function *> OldKernels;
  for (auto &F : M) {
    if (F.getCallingConv() == llvm::CallingConv::SPIR_KERNEL)
      OldKernels.push_back(&F);
  }

  // Materialize builtins
  // First we add a pointer to the host compilation state as arg to all the
  // kernels.
  Type *StateType = StructType::getTypeByName(M.getContext(), "struct._hc_state");
  assert(StateType && "Couldn't find the host compilation state in the module, make sure that -D __SYCL_HOST_COMPILATION__ is set");
  Type *StatePtrType = PointerType::getUnqual(StateType);
  SmallVector<Function *> NewKernels;
  for (auto &oldF : OldKernels) {
    auto newF = addArg(oldF, StatePtrType);
    newF->takeName(oldF);
    oldF->eraseFromParent();
    NewKernels.push_back(newF);
    ModuleChanged |= true;
  }

  for (auto &entry : BuiltinNamesMap) {
    SmallVector<Instruction *> toDelete;
    // spirv builtins are global constants, find it in the module
    auto Glob = M.getNamedGlobal(entry.first);
    if (!Glob)
      continue;
    auto replaceFunc = getReplaceFunc(M, StatePtrType, entry.second);
    for (auto &Use : Glob->uses()) {
      if (isa<LoadInst>(Use.getUser())) {
        auto load = dyn_cast<llvm::LoadInst>(Use.getUser());
        auto StateArg = getStateArg(load->getFunction());
        // the builtin is used in a load -> extractelement pattern
        bool IsExtractUse = llvm::all_of(load->uses(), [](class Use &U) {
          return isa<ExtractElementInst>(U.getUser());
        });
        if (IsExtractUse) {
          for (auto &LoadUse : load->uses()) {
            auto extract =
                dyn_cast<llvm::ExtractElementInst>(LoadUse.getUser());
            if (!extract)
              llvm::errs() << *load->getFunction()->getParent() << "\n";
            // replace "extract <builtin> <index>" with
            // "call builtin_func(<index>, state)". the state is the last
            // argument of the kernel function
            Value *index = extract->getOperand(1);
            assert(StateArg->getType() == StatePtrType);
            auto newCall = llvm::CallInst::Create(
                replaceFunc->getFunctionType(), replaceFunc, {index, StateArg},
                "hc_builtin", extract);
            extract->replaceAllUsesWith(newCall);
            toDelete.push_back(extract);
            ModuleChanged = true;
          }
          toDelete.push_back(load);
        } else {
          // this is a load straight from the builtin, replace with a call with
          // index 0
          auto &Ctx = load->getContext();
          Value *NewIndex = ConstantInt::get(IntegerType::get(Ctx, 64), 0);
          auto *NewCall = llvm::CallInst::Create(
              replaceFunc->getFunctionType(), replaceFunc, {NewIndex, StateArg},
              "hc_builtin", load);
          load->replaceAllUsesWith(NewCall);
          toDelete.push_back(load);
          ModuleChanged = true;
        }
      } else if (isa<GEPOperator>(Use.getUser())) {
        // the builting is used as val = load (gep builtin 0 <index>)
        // we replace it with val = builtin(<index>)
        auto GEPOp = dyn_cast<GEPOperator>(Use.getUser());
        assert(GEPOp->getNumIndices() == 2 &&
               "Unsupported GEPOperator in builtin use");
        auto Index = (GEPOp->op_begin() + 2)->get();
        for (auto &GEPOpUse : GEPOp->uses()) {
          auto *Load = dyn_cast<LoadInst>(GEPOpUse.getUser());
          if (!Load) {
            // Todo: the gepoperator here seems to have dead uses that are still
            // linked
            assert(GEPOpUse.getUser()->getNumUses() == 0);
            continue;
          }
          assert(Load && "GEPOperator use is not a load");
          auto stateArg = getStateArg(Load->getFunction());
          assert(stateArg->getType() == StatePtrType);
          auto newCall = llvm::CallInst::Create(replaceFunc->getFunctionType(),
                                                replaceFunc, {Index, stateArg},
                                                "hc_builtin", Load);
          Load->replaceAllUsesWith(newCall);
          toDelete.push_back(Load);
          ModuleChanged = true;
        }

      } else {
        llvm_unreachable("Unsupported builtin use");
      }
    }
    for (auto &I : toDelete) {
      llvm::errs() << "[PTRDBG] deleting: " << *I << "\n";
      llvm::errs() << "[PTRDBG] from: " << *I->getFunction() << "\n";
      I->eraseFromParent();
    }
    Glob->eraseFromParent();
  }

  for (auto F : NewKernels) {
    fixCallingConv(F);
  }

  return ModuleChanged ? PreservedAnalyses::all() : PreservedAnalyses::none();
}
