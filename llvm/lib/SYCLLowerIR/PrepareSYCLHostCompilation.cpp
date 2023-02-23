//===------ PrepareSYCLHostCompilation.cpp - SYCL Host Compilation Preparation
//Pass ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Prepares the kernel for SYCL Host Compilation:
// * Emits Host Compilation header.
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
    initializeSYCLMutatePrintfAddrspaceLegacyPassPass(
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

cl::opt<std::string>
    HCHeaderName("hc-header", cl::init(""),
                 cl::desc("Host Compilation extra header file"));

char PrepareSYCLHostCompilationLegacyPass::ID = 0;
INITIALIZE_PASS(PrepareSYCLHostCompilationLegacyPass,
                "PrepareSYCLHostCompilation",
                "Prepare SYCL Kernels for SYCL Host Compilation", false, false)

// Public interface to the SYCLMutatePrintfAddrspacePass.
ModulePass *llvm::createPrepareSYCLHostCompilationLegacyPass() {
  return new PrepareSYCLHostCompilationLegacyPass();
}

namespace {
SmallVector<bool> getArgMask(Function *F) {
  SmallVector<bool> res;
  auto UsedNode = F->getMetadata("sycl_kernel_omit_args");
  if(!UsedNode) {
    // the metadata node is not available if -fenable-sycl-dae
    // was not set; set everything to true in the mask.
    for(unsigned I = 0; I < F->getFunctionType()->getNumParams(); I++) {
      res.push_back(true);
    }
    return res;
  }
  auto NumOperands = UsedNode->getNumOperands();
  for (unsigned I = 0; I < NumOperands; I++) {
    auto &Op = UsedNode->getOperand(I);
    auto CAM = dyn_cast<ConstantAsMetadata>(Op.get());
    auto Const = dyn_cast<ConstantInt>(CAM->getValue());
    auto Val = Const->getValue();
    res.push_back(!Val.getBoolValue());
  }
  return res;
}

void emitKernelDecl(const Function *F, const SmallVector<bool> &argMask,
                    raw_ostream &O) {
  unsigned numUsedArgs =
      std::accumulate(argMask.begin(), argMask.end(), 0, std::plus());
  // assert(F->getFunctionType()->getNumParams() == numUsedArgs);
  O << "extern \"C\" void " << F->getName() << "(";
  for (unsigned I = 0; I < numUsedArgs - 1; I++)
    O << "void *, ";
  O << "void *, _hc_state*);\n";
}

void emitSubKernelHandler(const Function *F, const SmallVector<bool> &argMask,
                          raw_ostream &O) {
  SmallVector<unsigned> usedArgIdx;
  O << "\nextern \"C\" void " << F->getName() << "subhandler(";
  O << "const std::vector<sycl::detail::HostCompilationArgDesc>& MArgs, "
       "_hc_state *state) {\n";
  for (unsigned I = 0; I < argMask.size(); I++) {
    if (argMask[I]) {
      O << "  void* ptr" << I << " = ";
      O << "MArgs[" << I << "].getPtr();\n";
      usedArgIdx.push_back(I);
    }
  }
  O << "  " << F->getName() << "(";
  for (unsigned I = 0; I < usedArgIdx.size() - 1; I++) {
    O << "ptr" << usedArgIdx[I] << ", ";
  }
  if (usedArgIdx.size() >= 1)
    O << "ptr" << usedArgIdx.back();
  O << ", state);\n";
  O << "};\n\n";
}

void fixCallingConv(Function* F) {
  F->setCallingConv(llvm::CallingConv::C);
  // TODO: the frame-pointer=all attribute apparently makes the kernel crash at runtime
  F->setAttributes({});
}
Function *addArg(Function *oldF, Type *T) {
#ifdef REPLACE_DBG
  errs() << "Adding arg: " << oldF->getName() << "\n";
#endif
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
  // TODO: this just replaces the uses of __spirv_BuiltInGlobalInvocationId
  // with a constant 0. Implement proper materialization.
  for (auto &entry : BuiltinNamesMap) {
    SmallVector<Instruction *> toDelete;
    // spirv builtins are global constants, find it in the module
    auto Glob = M.getNamedGlobal(entry.first);
    if (!Glob)
      continue;
    auto replaceFunc = getReplaceFunc(M, StatePtrType, entry.second);
    llvm::errs() << *replaceFunc << "\n";
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

  // Emit host compilation helper header
  if (HCHeaderName == "") {
    llvm::errs() << "Please provide a valid file name for the host compilation "
                    "header.\nExiting\n";
    // TODO(Pietro) terminate more nicely or (better) find a better way to
    // handle the file name
    exit(1);
  }
  int HCHeaderFD = 0;
  std::error_code EC =
      llvm::sys::fs::openFileForWrite(HCHeaderName, HCHeaderFD);
  if (EC) {
    llvm::errs() << "Error: " << EC.message() << "\n";
    // TODO(Pietro) terminate more nicely or (better) find a better way to
    // handle the file name
    exit(1);
  }
  llvm::raw_fd_ostream O(HCHeaderFD, true);
  O << "#pragma once\n";
  O << "#include <sycl/detail/host_compilation.hpp>\n";

  for (auto F : NewKernels) {
    auto argMask = getArgMask(F);
    emitKernelDecl(F, argMask, O);
    emitSubKernelHandler(F, argMask, O);
    fixCallingConv(F);
  }

  return ModuleChanged ? PreservedAnalyses::all() : PreservedAnalyses::none();
}
