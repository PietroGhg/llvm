//===------ EmitSYCLHCHeader.cpp - Emit SYCL Host Compilation Header
// Pass ------===//
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

#include "llvm/SYCLLowerIR/EmitSYCLHCHeader.h"

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include <functional>
#include <numeric>

using namespace llvm;

namespace {
// Wrapper for the pass to make it working with the old pass manager
class EmitSYCLHCHeaderLegacyPass : public ModulePass {
public:
  static char ID;
  EmitSYCLHCHeaderLegacyPass() : ModulePass(ID) {
    initializeEmitSYCLHCHeaderLegacyPassPass(*PassRegistry::getPassRegistry());
  }
  EmitSYCLHCHeaderLegacyPass(const std::string &FileName)
      : ModulePass(ID), Impl(FileName) {
    initializeEmitSYCLHCHeaderLegacyPassPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override {
    ModuleAnalysisManager MAM;
    auto PA = Impl.run(M, MAM);
    return !PA.areAllPreserved();
  }

private:
  EmitSYCLHCHeaderPass Impl;
  std::string HCHeaderName;
};

} // namespace

char EmitSYCLHCHeaderLegacyPass::ID = 0;
INITIALIZE_PASS(EmitSYCLHCHeaderLegacyPass, "emit-sycl-hc-header",
                "Emit SYCL Host Compilation Header", false, false)

// Public interface to the SYCLMutatePrintfAddrspacePass.
ModulePass *llvm::createEmitSYCLHCHeaderLegacyPass() {
  return new EmitSYCLHCHeaderLegacyPass();
}

namespace {
SmallVector<bool> getArgMask(const Function *F) {
  SmallVector<bool> res;
  auto UsedNode = F->getMetadata("sycl_kernel_omit_args");
  if (!UsedNode) {
    // the metadata node is not available if -fenable-sycl-dae
    // was not set; set everything to true in the mask.
    for (unsigned I = 0; I < F->getFunctionType()->getNumParams(); I++) {
      res.push_back(true);
    }
    return res;
  }
  auto NumOperands = UsedNode->getNumOperands();
  for (unsigned I = 0; I < NumOperands; I++) {
    auto &Op = UsedNode->getOperand(I);
    auto CAM = dyn_cast<ConstantAsMetadata>(Op.get());
    assert(CAM && "Operand to sycl_kernel_omit_args is not a constant");
    auto Const = dyn_cast<ConstantInt>(CAM->getValue());
    assert(Const && "Operand to sycl_kernel_omit_args is not a constant int");
    auto Val = Const->getValue();
    res.push_back(!Val.getBoolValue());
  }
  return res;
}

SmallVector<StringRef> getArgTypeNames(const Function *F) {
  SmallVector<StringRef> Res;
  auto *TNNode = F->getMetadata("kernel_arg_type");
  assert(TNNode &&
         "kernel_arg_type metadata node is required for sycl host compilation");
  auto NumOperands = TNNode->getNumOperands();
  for (unsigned I = 0; I < NumOperands; I++) {
    auto &Op = TNNode->getOperand(I);
    auto *MDS = dyn_cast<MDString>(Op.get());
    assert(MDS && "kernel_arg_types operand not a metadata string");
    Res.push_back(MDS->getString());
  }
  return Res;
}

void emitKernelDecl(const Function *F, const SmallVector<bool> &argMask,
                    const SmallVector<StringRef> &ArgTypeNames,
                    raw_ostream &O) {
  auto EmitArgDecl = [&](const Argument *Arg, unsigned Index) {
    Type *ArgTy = Arg->getType();
    if (isa<PointerType>(ArgTy))
      return "void *";
    return ArgTypeNames[Index].data();
  };

  auto NumParams = F->getFunctionType()->getNumParams();
  O << "extern \"C\" void " << F->getName() << "(";

  unsigned I = 0, UsedI = 0;
  for (; I < argMask.size() - 1 && UsedI < NumParams - 1; I++) {
    if (!argMask[I])
      continue;
    O << EmitArgDecl(F->getArg(UsedI), I) << ", ";
    UsedI++;
  }

  // parameters may have been removed.
  if (UsedI == 0) {
    O << ");\n";
    return;
  }
  O << EmitArgDecl(F->getArg(UsedI), I) << ", _hc_state *);\n";
}

void emitSubKernelHandler(const Function *F, const SmallVector<bool> &argMask,
                          const SmallVector<StringRef> &ArgTypeNames,
                          raw_ostream &O) {
  SmallVector<unsigned> usedArgIdx;
  auto EmitParamCast = [&](Argument *Arg, unsigned Index) {
    std::string Res;
    llvm::raw_string_ostream OS(Res);
    usedArgIdx.push_back(Index);
    if (isa<PointerType>(Arg->getType())) {
      OS << "  void* arg" << Index << " = ";
      OS << "MArgs[" << Index << "].getPtr();\n";
      return OS.str();
    }
    auto TN = ArgTypeNames[Index].str();
    OS << "  " << TN << " arg" << Index << " = ";
    OS << "*(" << TN << "*)"
       << "MArgs[" << Index << "].getPtr();\n";
    return OS.str();
  };

  O << "\ninline static void " << F->getName() << "subhandler(";
  O << "const std::vector<sycl::detail::HostCompilationArgDesc>& MArgs, "
       "_hc_state *state) {\n";
  // Retrieve only the args that are used
  for (unsigned I = 0, UsedI = 0;
       I < argMask.size() && UsedI < F->getFunctionType()->getNumParams();
       I++) {
    if (argMask[I]) {
      O << EmitParamCast(F->getArg(UsedI), I);
      UsedI++;
    }
  }
  // Emit the actual kernel call
  O << "  " << F->getName() << "(";
  if (usedArgIdx.size() == 0) {
    O << ");\n";
  } else {
    for (unsigned I = 0; I < usedArgIdx.size() - 1; I++) {
      O << "arg" << usedArgIdx[I] << ", ";
    }
    if (usedArgIdx.size() >= 1)
      O << "arg" << usedArgIdx.back();
    O << ", state);\n";
  }
  O << "};\n\n";
}

} // namespace

PreservedAnalyses EmitSYCLHCHeaderPass::run(Module &M,
                                            ModuleAnalysisManager &MAM) {
  bool ModuleChanged = false;
  SmallVector<Function *> Kernels;
  for (auto &F : M) {
    if (F.getCallingConv() == llvm::CallingConv::SPIR_KERNEL)
      Kernels.push_back(&F);
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

  for (auto F : Kernels) {
    auto argMask = getArgMask(F);
    auto ArgTypeNames = getArgTypeNames(F);
    emitKernelDecl(F, argMask, ArgTypeNames, O);
    emitSubKernelHandler(F, argMask, ArgTypeNames, O);
  }

  return ModuleChanged ? PreservedAnalyses::all() : PreservedAnalyses::none();
}
