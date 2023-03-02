;RUN: opt -passes=prepare-sycl-hc %s -S -o - | FileCheck %s
; ModuleID = 'out.ll'
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


; check that we added the state function arg
;CHECK: _Z10SimpleVaddIiE(i32*{{.*}}, i32*{{.*}}, i32*{{.*}}, %struct._hc_state{{.*}})
; Function Attrs: norecurse
define weak_odr dso_local spir_kernel void @_Z10SimpleVaddIiE(i32* noundef align 4 %_arg_accessorC, i32* noundef readonly align 4 %_arg_accessorA,  i32* noundef readonly align 4 %_arg_accessorB) local_unnamed_addr #1 !sycl_kernel_omit_args !50 {
entry:
  
  %0 = load <3 x i64>, <3 x i64>* @__spirv_BuiltInGlobalInvocationId, align 32, !noalias !51
  %1 = extractelement <3 x i64> %0, i64 0
  ;CHECK-NOT:{{.*}} = load <3 x i64>, <3 x i64>* @__spirv_BuiltInGlobalInvocationId, align 32, !noalias !51
  ;CHECK-NOT:{{.*}} = extractelement <3 x i64> {{.*}}, i64 0
  ;CHECK: %hc_builtin = call i64 @_hc_get_global_id(i64 0, %struct._hc_state* %3)
  %arrayidx.i.i = getelementptr inbounds i32, i32* %_arg_accessorA, i64 %1
  %2 = load i32, i32* %arrayidx.i.i, align 4, !tbaa !56
  %arrayidx.i16.i = getelementptr inbounds i32, i32* %_arg_accessorB, i64 %1
  %3 = load i32, i32* %arrayidx.i16.i, align 4, !tbaa !56
  %add.i = add nsw i32 %2, %3
  %arrayidx.i18.i = getelementptr inbounds i32, i32* %_arg_accessorC, i64 %1
  store i32 %add.i, i32* %arrayidx.i18.i, align 4, !tbaa !56
  ret void
}


attributes #1 = { norecurse "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "sycl-module-id"="../simple-vector-add.cpp" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" "uniform-work-group-size"="true" }


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
