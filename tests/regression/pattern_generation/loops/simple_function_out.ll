; ModuleID = 'simple_function_pattern'
source_filename = "simple_function_pattern"

declare void @diffkemp.output_mapping(...)

define dso_local i32 @diffkemp.old.side(i32 %0, i32 %1) {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 %0, ptr %3, align 4
  store i32 %1, ptr %4, align 4
  %5 = load i32, ptr %3, align 4
  %6 = load i32, ptr %4, align 4, !diffkemp.pattern !0
  %7 = add nsw i32 %5, %6
  ret i32 %7, !diffkemp.pattern !1
}

define dso_local i32 @diffkemp.new.side(i32 %0, i32 %1) {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 %0, ptr %3, align 4
  store i32 %1, ptr %4, align 4
  %5 = load i32, ptr %3, align 4
  %6 = add nsw i32 %5, 42, !diffkemp.pattern !0
  ret i32 %6, !diffkemp.pattern !1
}

!llvm.dbg.cu = !{}

!0 = !{!"pattern-start"}
!1 = !{!"pattern-end"}
