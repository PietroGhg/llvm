//===---- PipelineSYCLNativeCPU.cpp - Pass pipeline for SYCL Native CPU ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Defines the pass pipeline used when lowering device code for SYCL Native
// CPU.
// When NATIVECPU_USE_OCK is set, adds passes from the oneAPI Construction Kit.
//
//===----------------------------------------------------------------------===//
#include "llvm/SYCLLowerIR/UtilsSYCLNativeCPU.h"
#include "llvm/SYCLLowerIR/ConvertToMuxBuiltinsSYCLNativeCPU.h"
#include "llvm/SYCLLowerIR/PrepareSYCLNativeCPU.h"
#include "llvm/SYCLLowerIR/RenameKernelSYCLNativeCPU.h"
#include "llvm/Passes/PassBuilder.h"

#ifdef NATIVECPU_USE_OCK
#include "compiler/utils/builtin_info.h"
#include "compiler/utils/device_info.h"
#include "compiler/utils/sub_group_analysis.h"
#include "compiler/utils/work_item_loops_pass.h"
#include "vecz/pass.h"
#include "vecz/vecz_target_info.h"
#include "llvm/Transforms/IPO/AlwaysInliner.h"
#endif

namespace llvm {
cl::opt<unsigned> NativeCPUVeczWidth("ncpu-vecz-width", cl::init(8), cl::desc("Vector width for SYCL Native CPU vectorizer, defaults to 8"));
void addSYCLNativeCPUBackendPasses(llvm::ModulePassManager &MPM,
                                   ModuleAnalysisManager &MAM, unsigned OptLevel, bool DisableVecz) {
  MPM.addPass(ConvertToMuxBuiltinsSYCLNativeCPUPass());
#ifdef NATIVECPU_USE_OCK
  // Always enable vectorizer, unless explictly disabled or -O0 is set.
  if(OptLevel != 0 && !DisableVecz) {
    MAM.registerPass([&] { return vecz::TargetInfoAnalysis(); });
    MAM.registerPass([&] { return compiler::utils::DeviceInfoAnalysis(); });
    auto queryFunc =
        [](llvm::Function &F, llvm::ModuleAnalysisManager &,
           llvm::SmallVectorImpl<vecz::VeczPassOptions> &Opts) -> bool {
      if (F.getCallingConv() != llvm::CallingConv::SPIR_KERNEL) {
        return false;
      }
      compiler::utils::VectorizationFactor VF(NativeCPUVeczWidth, false);
      vecz::VeczPassOptions VPO;
      VPO.factor = VF;
      Opts.emplace_back(VPO);
      return true;
    };
    MAM.registerPass([&] { return vecz::VeczPassOptionsAnalysis(queryFunc); });
    MPM.addPass(vecz::RunVeczPass());
  }
  // Todo set options properly
  compiler::utils::WorkItemLoopsPassOptions Opts;
  Opts.IsDebug = false;
  Opts.ForceNoTail = false;
  MAM.registerPass([&] { return compiler::utils::BuiltinInfoAnalysis(); });
  MAM.registerPass([&] { return compiler::utils::SubgroupAnalysis(); });
  MPM.addPass(compiler::utils::WorkItemLoopsPass(Opts));
  MPM.addPass(AlwaysInlinerPass());
#endif
  MPM.addPass(PrepareSYCLNativeCPUPass());
  MPM.addPass(RenameKernelSYCLNativeCPUPass());

  // Run optimization passes after all the changes we made to the kernels.
  // Todo: check optimization level from clang
  // Todo: maybe we could find a set of relevant passes instead of re-running the full 
  // optimization pipeline.
  PassBuilder PB;
  OptimizationLevel Level;
  switch(OptLevel) {
  case 0:
    Level = OptimizationLevel::O0;
    break;
  case 1:
    Level = OptimizationLevel::O1;
    break;
  case 2:
    Level = OptimizationLevel::O2;
    break;
  case 3:
    Level = OptimizationLevel::O3;
    break;
  default:
    llvm_unreachable("Unsupported opt level");
  }
  MPM.addPass(PB.buildPerModuleDefaultPipeline(Level));
}
} // namespace llvm
