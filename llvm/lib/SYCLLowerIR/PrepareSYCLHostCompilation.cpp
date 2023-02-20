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
  assert(UsedNode && "No sycl_kernel_omit_args found");
  auto NumOperands = UsedNode->getNumOperands();
  for (unsigned I = 0; I < NumOperands; I++) {
    auto &Op = UsedNode->getOperand(I);
    auto CAM = dyn_cast<ConstantAsMetadata>(Op.get());
    auto Const = dyn_cast<ConstantInt>(CAM->getValue());
    auto Val = Const->getValue();
    res.push_back(Val.getBoolValue());
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
  O << "void *);\n";
}

void emitSubKernelHandler(const Function *F, const SmallVector<bool> &argMask,
                          raw_ostream &O) {
  SmallVector<unsigned> usedArgIdx;
  O << "\nvoid " << F->getName() << "subhandler(";
  O << "const std::vector<sycl::detail::HostCompilationArgDesc>& MArgs) {\n";
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
  O << ");\n";
  O << "};\n\n";
}
} // namespace

PreservedAnalyses
PrepareSYCLHostCompilationPass::run(Module &M, ModuleAnalysisManager &MAM) {
  bool ModuleChanged = false;
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

  SmallVector<Function *> Kernels;
  for (auto &F : M) {
    if (F.getCallingConv() == llvm::CallingConv::SPIR_KERNEL)
      Kernels.push_back(&F);
  }

  for (auto F : Kernels) {
    auto argMask = getArgMask(F);
    emitKernelDecl(F, argMask, O);
    emitSubKernelHandler(F, argMask, O);
  }

  return ModuleChanged ? PreservedAnalyses::all() : PreservedAnalyses::none();
}
