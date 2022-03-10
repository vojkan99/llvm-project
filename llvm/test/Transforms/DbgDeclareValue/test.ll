; RUN: opt -dbg-declare-value -S < %s | FileCheck %s
; RUN: opt -passes=dbg-declare-value -S < %s | FileCheck %s
 
; ModuleID = 'test_extended.c'
source_filename = "test_extended.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nounwind uwtable
define dso_local void @foo(i32 noundef %y) local_unnamed_addr #0 !dbg !7 {
  ; CHECK: entry
  ; CHECK-NOT: call void @llvm.dbg.value
  ; CHECK-NOT: call void @llvm.dbg.value
  ; CHECK-NEXT: ret void
  
entry:
  call void @llvm.dbg.value(metadata i32 %y, metadata !12, metadata !DIExpression()), !dbg !18
  call void @llvm.dbg.value(metadata i32 %y, metadata !13, metadata !DIExpression()), !dbg !18
  ret void, !dbg !19
}

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.value(metadata, metadata, metadata) #1

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #2

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #2

; Function Attrs: nounwind uwtable
define dso_local i32 @main() #0 !dbg !30 {
entry:
  %retval = alloca i32, align 4
  %x = alloca i32, align 4
  %y = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  %0 = bitcast i32* %x to i8*, !dbg !36
  
  ; CHECK: call void @llvm.lifetime.start.p0i8(i64 4, i8* %0) #3
  ; CHECK-NOT: call void @llvm.dbg.declare
  ; CHECK-NEXT: store i32 0, i32* %x, align 4
  ; CHECK: call void @llvm.lifetime.start.p0i8(i64 4, i8* %1) #3
  ; CHECK-NOT: call void @llvm.dbg.declare
  ; CHECK-NEXT: store i32 1, i32* %y, align 4
  
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %0) #3, !dbg !36
  call void @llvm.dbg.declare(metadata i32* %x, metadata !34, metadata !DIExpression()), !dbg !37
  store i32 0, i32* %x, align 4, !dbg !37, !tbaa !14
  %1 = bitcast i32* %y to i8*, !dbg !38
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %1) #3, !dbg !38
  call void @llvm.dbg.declare(metadata i32* %y, metadata !35, metadata !DIExpression()), !dbg !39
  store i32 1, i32* %y, align 4, !dbg !39, !tbaa !14
  %2 = load i32, i32* %x, align 4, !dbg !40, !tbaa !14
  %3 = load i32, i32* %y, align 4, !dbg !42, !tbaa !14
  %cmp = icmp slt i32 %2, %3, !dbg !43
  br i1 %cmp, label %if.then, label %if.else, !dbg !44

if.then:                                          ; preds = %entry
  %4 = load i32, i32* %x, align 4, !dbg !45, !tbaa !14
  store i32 %4, i32* %y, align 4, !dbg !47, !tbaa !14
  store i32 10, i32* %x, align 4, !dbg !48, !tbaa !14
  br label %if.end, !dbg !49

if.else:                                          ; preds = %entry
  %5 = load i32, i32* %y, align 4, !dbg !50, !tbaa !14
  store i32 %5, i32* %x, align 4, !dbg !52, !tbaa !14
  store i32 10, i32* %y, align 4, !dbg !53, !tbaa !14
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %6 = load i32, i32* %x, align 4, !dbg !54, !tbaa !14
  call void @foo(i32 noundef %6), !dbg !55
  %7 = load i32, i32* %x, align 4, !dbg !56, !tbaa !14
  %8 = load i32, i32* %y, align 4, !dbg !57, !tbaa !14
  %add = add nsw i32 %7, %8, !dbg !58
  %9 = bitcast i32* %y to i8*, !dbg !59
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %9) #3, !dbg !59
  %10 = bitcast i32* %x to i8*, !dbg !59
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %10) #3, !dbg !59
  ret i32 %add, !dbg !60
}

attributes #0 = { nounwind uwtable }
attributes #1 = { nounwind readnone }
attributes #2 = { nounwind }
attributes #3 = { nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 15.0.0 (https://github.com/vojkan99/llvm-project.git b8804557686f28fdcb3b9777552bb01be818035a)", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "test_extended.c", directory: "/dir", checksumkind: CSK_MD5, checksum: "881811b63e3a0c6e51619183b9216ced")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 1, !"wchar_size", i32 4}
!5 = !{i32 7, !"uwtable", i32 1}
!6 = !{!"clang version 15.0.0 (https://github.com/vojkan99/llvm-project.git b8804557686f28fdcb3b9777552bb01be818035a)"}
!7 = distinct !DISubprogram(name: "foo", scope: !1, file: !1, line: 3, type: !8, scopeLine: 3, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !11)
!8 = !DISubroutineType(types: !9)
!9 = !{null, !10}
!10 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!11 = !{!12, !13}
!12 = !DILocalVariable(name: "y", arg: 1, scope: !7, file: !1, line: 3, type: !10)
!13 = !DILocalVariable(name: "x", scope: !7, file: !1, line: 4, type: !10)
!14 = !{!15, !15, i64 0}
!15 = !{!"int", !16, i64 0}
!16 = !{!"omnipotent char", !17, i64 0}
!17 = !{!"Simple C/C++ TBAA"}
!18 = !DILocation(line: 3, column: 14, scope: !7)
!19 = !DILocation(line: 4, column: 5, scope: !7)
!20 = !DILocation(line: 4, column: 9, scope: !7)
!21 = !DILocation(line: 4, column: 13, scope: !7)
!22 = !DILocation(line: 5, column: 9, scope: !23)
!23 = distinct !DILexicalBlock(scope: !7, file: !1, line: 5, column: 9)
!24 = !DILocation(line: 5, column: 11, scope: !23)
!25 = !DILocation(line: 5, column: 9, scope: !7)
!26 = !DILocation(line: 6, column: 8, scope: !23)
!27 = !DILocation(line: 6, column: 6, scope: !23)
!28 = !DILocation(line: 8, column: 8, scope: !23)
!29 = !DILocation(line: 9, column: 1, scope: !7)
!30 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 11, type: !31, scopeLine: 11, flags: DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !33)
!31 = !DISubroutineType(types: !32)
!32 = !{!10}
!33 = !{!34, !35}
!34 = !DILocalVariable(name: "x", scope: !30, file: !1, line: 12, type: !10)
!35 = !DILocalVariable(name: "y", scope: !30, file: !1, line: 13, type: !10)
!36 = !DILocation(line: 12, column: 5, scope: !30)
!37 = !DILocation(line: 12, column: 9, scope: !30)
!38 = !DILocation(line: 13, column: 5, scope: !30)
!39 = !DILocation(line: 13, column: 9, scope: !30)
!40 = !DILocation(line: 14, column: 9, scope: !41)
!41 = distinct !DILexicalBlock(scope: !30, file: !1, line: 14, column: 9)
!42 = !DILocation(line: 14, column: 13, scope: !41)
!43 = !DILocation(line: 14, column: 11, scope: !41)
!44 = !DILocation(line: 14, column: 9, scope: !30)
!45 = !DILocation(line: 15, column: 10, scope: !46)
!46 = distinct !DILexicalBlock(scope: !41, file: !1, line: 14, column: 16)
!47 = !DILocation(line: 15, column: 8, scope: !46)
!48 = !DILocation(line: 16, column: 8, scope: !46)
!49 = !DILocation(line: 17, column: 5, scope: !46)
!50 = !DILocation(line: 19, column: 10, scope: !51)
!51 = distinct !DILexicalBlock(scope: !41, file: !1, line: 18, column: 10)
!52 = !DILocation(line: 19, column: 8, scope: !51)
!53 = !DILocation(line: 20, column: 8, scope: !51)
!54 = !DILocation(line: 22, column: 9, scope: !30)
!55 = !DILocation(line: 22, column: 5, scope: !30)
!56 = !DILocation(line: 23, column: 12, scope: !30)
!57 = !DILocation(line: 23, column: 16, scope: !30)
!58 = !DILocation(line: 23, column: 14, scope: !30)
!59 = !DILocation(line: 24, column: 1, scope: !30)
!60 = !DILocation(line: 23, column: 5, scope: !30)
