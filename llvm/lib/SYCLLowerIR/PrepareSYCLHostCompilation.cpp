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

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
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
    {"__spirv_BuiltInGlobalInvocationId", "_hc_get_global_id"}};

Function *getReplaceFunc(Module &M, Type *T, StringRef Name) {
  Type *Int64_t = llvm::Type::getInt64Ty(M.getContext());
  FunctionType *F_t = FunctionType::get(Int64_t, {Int64_t, T}, false);
  Function *F =
      dyn_cast<Function>(M.getOrInsertFunction(Name, F_t, {}).getCallee());
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
  Type *StateType = StructType::create(M.getContext(), "struct._hc_state");
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
      auto load = dyn_cast<llvm::LoadInst>(Use.getUser());
      assert(load && "Builtin use that is not a load.");
      for (auto &LoadUse : load->uses()) {
        auto extract = dyn_cast<llvm::ExtractElementInst>(LoadUse.getUser());
        assert(extract && "Use of loaded builtin is not an extract");
        // replace "extract <builtin> <index>" with
        // "call builtin_func(<index>, state)". the state is the last argument
        // of the kernel function
        auto index = extract->getOperand(1);
        auto stateArg = getStateArg(extract->getFunction());
        assert(stateArg->getType() == StatePtrType);
        auto newCall =
            llvm::CallInst::Create(replaceFunc->getFunctionType(), replaceFunc,
                                   {index, stateArg}, "hc_builtin", extract);
        extract->replaceAllUsesWith(newCall);
        toDelete.push_back(extract);
        ModuleChanged = true;
      }
      toDelete.push_back(load);
    }
    for (auto &I : toDelete)
      I->eraseFromParent();
    Glob->eraseFromParent();
  }

  for (auto F : NewKernels) {
    fixCallingConv(F);
  }

  return ModuleChanged ? PreservedAnalyses::all() : PreservedAnalyses::none();
}
