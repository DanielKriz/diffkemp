; ModuleID = 'simple_with_same_function_different_value_pattern'
source_filename = "simple_with_same_function_different_value_pattern"

declare void @diffkemp.output_mapping(...)

define dso_local i32 @diffkemp.old.side(i32 %0, i32 %1) {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 %0, ptr %3, align 4
  store i32 %1, ptr %4, align 4
  %5 = load i32, ptr %3, align 4
  %6 = load i32, ptr %4, align 4
  %7 = add nsw i32 %5, %6
  %8 = call i32 @f()
  %9 = add nsw i32 %7, %8
  %10 = add nsw i32 %9, 42, !diffkemp.pattern !0
  ret i32 %10, !diffkemp.pattern !1
}

define dso_local i32 @diffkemp.new.side(i32 %0, i32 %1) {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 %0, ptr %3, align 4
  store i32 %1, ptr %4, align 4
  %5 = load i32, ptr %3, align 4
  %6 = load i32, ptr %4, align 4
  %7 = add nsw i32 %5, %6
  %8 = call i32 @f()
  %9 = add nsw i32 %7, %8
  %10 = add nsw i32 %9, 10, !diffkemp.pattern !0
  ret i32 %10, !diffkemp.pattern !1
}

declare i32 @f()

!llvm.dbg.cu = !{}

!0 = !{!"pattern-start"}
!1 = !{!"pattern-end"}
