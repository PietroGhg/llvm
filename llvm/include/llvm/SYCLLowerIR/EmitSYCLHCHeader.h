//===------- EmitSYCLHCHeader.h - Emits the SYCL Host Compilation helper header
//-------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass emits the SYCL Host Compilation helper header.
// The header mainly contatins the definition for the handler function which
// allows to call the kernel extracted by the device compiler from the host
// runtime.
//===----------------------------------------------------------------------===//

#pragma once

#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

class ModulePass;

class EmitSYCLHCHeaderPass : public PassInfoMixin<EmitSYCLHCHeaderPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
  EmitSYCLHCHeaderPass(const std::string &FileName) : HCHeaderName(FileName) {}
  EmitSYCLHCHeaderPass() = default;

private:
  std::string HCHeaderName;
};

ModulePass *createEmitSYCLHCHeaderLegacyPass();

} // namespace llvm
