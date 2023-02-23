;RUN: opt -passes=prepare-sycl-hc %s -S -o - | FileCheck %s
; ModuleID = 'out.ll'
source_filename = "../simple-vector-add.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%"class.sycl::_V1::id" = type { %"class.sycl::_V1::detail::array" }
%"class.sycl::_V1::detail::array" = type { [1 x i64] }

$_Z10SimpleVaddIiE = comdat any

; check that the spirv builtin has been removed
;CHECK-NOT: @__spirv_BuiltInGlobalInvocationId = external dso_local local_unnamed_addr constant <3 x i64>, align 32
@__spirv_BuiltInGlobalInvocationId = external dso_local local_unnamed_addr constant <3 x i64>, align 32

; check that we added the state struct
;CHECK: %struct._hc_state = type opaque

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite)
declare void @llvm.assume(i1 noundef) #0

; check that we added the state function arg
;CHECK: _Z10SimpleVaddIiE(i32*{{.*}}, %"class.sycl::_V1::id"*{{.*}}, i32*{{.*}}, %"class.sycl::_V1::id"*{{.*}}, i32*{{.*}}, %"class.sycl::_V1::id"*{{.*}}, %struct._hc_state{{.*}})
; Function Attrs: norecurse
define weak_odr dso_local spir_kernel void @_Z10SimpleVaddIiE(i32* noundef align 4 %_arg_accessorC, %"class.sycl::_V1::id"* noundef byval(%"class.sycl::_V1::id") align 8 %_arg_accessorC3, i32* noundef readonly align 4 %_arg_accessorA, %"class.sycl::_V1::id"* noundef byval(%"class.sycl::_V1::id") align 8 %_arg_accessorA6, i32* noundef readonly align 4 %_arg_accessorB, %"class.sycl::_V1::id"* noundef byval(%"class.sycl::_V1::id") align 8 %_arg_accessorB9) local_unnamed_addr #1 comdat !srcloc !46 !kernel_arg_buffer_location !47 !kernel_arg_runtime_aligned !48 !kernel_arg_exclusive_ptr !48 !sycl_fixed_targets !49 !sycl_kernel_omit_args !50 {
entry:
  %agg.tmp12.sroa.0.0..sroa_idx = getelementptr inbounds %"class.sycl::_V1::id", %"class.sycl::_V1::id"* %_arg_accessorC3, i64 0, i32 0, i32 0, i64 0
  %agg.tmp12.sroa.0.0.copyload = load i64, i64* %agg.tmp12.sroa.0.0..sroa_idx, align 8
  %add.ptr.i = getelementptr inbounds i32, i32* %_arg_accessorC, i64 %agg.tmp12.sroa.0.0.copyload
  %agg.tmp21.sroa.0.0..sroa_idx = getelementptr inbounds %"class.sycl::_V1::id", %"class.sycl::_V1::id"* %_arg_accessorA6, i64 0, i32 0, i32 0, i64 0
  %agg.tmp21.sroa.0.0.copyload = load i64, i64* %agg.tmp21.sroa.0.0..sroa_idx, align 8
  %add.ptr.i45 = getelementptr inbounds i32, i32* %_arg_accessorA, i64 %agg.tmp21.sroa.0.0.copyload
  %agg.tmp31.sroa.0.0..sroa_idx = getelementptr inbounds %"class.sycl::_V1::id", %"class.sycl::_V1::id"* %_arg_accessorB9, i64 0, i32 0, i32 0, i64 0
  %agg.tmp31.sroa.0.0.copyload = load i64, i64* %agg.tmp31.sroa.0.0..sroa_idx, align 8
  %add.ptr.i50 = getelementptr inbounds i32, i32* %_arg_accessorB, i64 %agg.tmp31.sroa.0.0.copyload
  %0 = load <3 x i64>, <3 x i64>* @__spirv_BuiltInGlobalInvocationId, align 32, !noalias !51
  %1 = extractelement <3 x i64> %0, i64 0
  ;CHECK-NOT:{{.*}} = load <3 x i64>, <3 x i64>* @__spirv_BuiltInGlobalInvocationId, align 32, !noalias !51
  ;CHECK-NOT:{{.*}} = extractelement <3 x i64> {{.*}}, i64 0
  ;CHECK: %hc_builtin = call i64 @_hc_get_global_id(i64 0, %struct._hc_state* %6)
  %cmp.i.i = icmp ult i64 %1, 2147483648
  tail call void @llvm.assume(i1 %cmp.i.i)
  %arrayidx.i.i = getelementptr inbounds i32, i32* %add.ptr.i45, i64 %1
  %2 = load i32, i32* %arrayidx.i.i, align 4, !tbaa !56
  %arrayidx.i16.i = getelementptr inbounds i32, i32* %add.ptr.i50, i64 %1
  %3 = load i32, i32* %arrayidx.i16.i, align 4, !tbaa !56
  %add.i = add nsw i32 %2, %3
  %arrayidx.i18.i = getelementptr inbounds i32, i32* %add.ptr.i, i64 %1
  store i32 %add.i, i32* %arrayidx.i18.i, align 4, !tbaa !56
  ret void
}

declare dso_local spir_func i32 @_Z18__spirv_ocl_printfPU3AS2Kcz(i8 addrspace(2)*, ...)

attributes #0 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: readwrite) }
attributes #1 = { norecurse "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "sycl-module-id"="../simple-vector-add.cpp" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" "uniform-work-group-size"="true" }

!llvm.module.flags = !{!0, !1}
!opencl.spir.version = !{!2}
!spirv.Source = !{!3}
!sycl_aspects = !{!4, !5, !6, !7, !8, !9, !10, !11, !12, !13, !14, !15, !16, !17, !18, !19, !20, !21, !22, !23, !24, !25, !26, !27, !28, !29, !30, !31, !32, !33, !34, !35, !36, !37, !38, !39, !40, !41, !42, !43, !44}
!llvm.ident = !{!45}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"frame-pointer", i32 2}
!2 = !{i32 1, i32 2}
!3 = !{i32 4, i32 100000}
!4 = !{!"host", i32 0}
!5 = !{!"cpu", i32 1}
!6 = !{!"gpu", i32 2}
!7 = !{!"accelerator", i32 3}
!8 = !{!"custom", i32 4}
!9 = !{!"fp16", i32 5}
!10 = !{!"fp64", i32 6}
!11 = !{!"image", i32 9}
!12 = !{!"online_compiler", i32 10}
!13 = !{!"online_linker", i32 11}
!14 = !{!"queue_profiling", i32 12}
!15 = !{!"usm_device_allocations", i32 13}
!16 = !{!"usm_host_allocations", i32 14}
!17 = !{!"usm_shared_allocations", i32 15}
!18 = !{!"usm_restricted_shared_allocations", i32 16}
!19 = !{!"usm_system_allocations", i32 17}
!20 = !{!"ext_intel_pci_address", i32 18}
!21 = !{!"ext_intel_gpu_eu_count", i32 19}
!22 = !{!"ext_intel_gpu_eu_simd_width", i32 20}
!23 = !{!"ext_intel_gpu_slices", i32 21}
!24 = !{!"ext_intel_gpu_subslices_per_slice", i32 22}
!25 = !{!"ext_intel_gpu_eu_count_per_subslice", i32 23}
!26 = !{!"ext_intel_max_mem_bandwidth", i32 24}
!27 = !{!"ext_intel_mem_channel", i32 25}
!28 = !{!"usm_atomic_host_allocations", i32 26}
!29 = !{!"usm_atomic_shared_allocations", i32 27}
!30 = !{!"atomic64", i32 28}
!31 = !{!"ext_intel_device_info_uuid", i32 29}
!32 = !{!"ext_oneapi_srgb", i32 30}
!33 = !{!"ext_oneapi_native_assert", i32 31}
!34 = !{!"host_debuggable", i32 32}
!35 = !{!"ext_intel_gpu_hw_threads_per_eu", i32 33}
!36 = !{!"ext_oneapi_cuda_async_barrier", i32 34}
!37 = !{!"ext_oneapi_bfloat16_math_functions", i32 35}
!38 = !{!"ext_intel_free_memory", i32 36}
!39 = !{!"ext_intel_device_id", i32 37}
!40 = !{!"ext_intel_memory_clock_rate", i32 38}
!41 = !{!"ext_intel_memory_bus_width", i32 39}
!42 = !{!"int64_base_atomics", i32 7}
!43 = !{!"int64_extended_atomics", i32 8}
!44 = !{!"usm_system_allocator", i32 17}
!45 = !{!"clang version 16.0.0 (git@git.office.codeplay.com:oneapi-core/intel-llvm-mirror.git 3c08618c51a3c17c8db2f6ecb9eee12f75595531)"}
!46 = !{i32 2289}
!47 = !{i32 -1, i32 -1, i32 -1, i32 -1, i32 -1, i32 -1, i32 -1, i32 -1, i32 -1, i32 -1, i32 -1, i32 -1}
!48 = !{i1 true, i1 false, i1 false, i1 false, i1 true, i1 false, i1 false, i1 false, i1 true, i1 false, i1 false, i1 false}
!49 = !{}
!50 = !{i1 false, i1 true, i1 true, i1 false, i1 false, i1 true, i1 true, i1 false, i1 false, i1 true, i1 true, i1 false}
!51 = !{!52, !54}
!52 = distinct !{!52, !53, !"_ZN4sycl3_V16detail7Builder7getItemILi1ELb1EEENSt9enable_ifIXT0_EKNS0_4itemIXT_EXT0_EEEE4typeEv: %agg.result"}
!53 = distinct !{!53, !"_ZN4sycl3_V16detail7Builder7getItemILi1ELb1EEENSt9enable_ifIXT0_EKNS0_4itemIXT_EXT0_EEEE4typeEv"}
!54 = distinct !{!54, !55, !"_ZN4sycl3_V16detail7Builder10getElementILi1ELb1EEEDTcl7getItemIXT_EXT0_EEEEPNS0_4itemIXT_EXT0_EEE: %agg.result"}
!55 = distinct !{!55, !"_ZN4sycl3_V16detail7Builder10getElementILi1ELb1EEEDTcl7getItemIXT_EXT0_EEEEPNS0_4itemIXT_EXT0_EEE"}
!56 = !{!57, !57, i64 0}
!57 = !{!"int", !58, i64 0}
!58 = !{!"omnipotent char", !59, i64 0}
!59 = !{!"Simple C++ TBAA"}
