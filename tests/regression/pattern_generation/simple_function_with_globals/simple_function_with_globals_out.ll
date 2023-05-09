; ModuleID = 'simple_function_with_globals_pattern'
source_filename = "simple_function_with_globals_pattern"

@old_global = external global ptr, align 4
@new_global = external global ptr, align 4

declare void @diffkemp.output_mapping(...)

define dso_local i32 @diffkemp.old.side(i32 %0, i32 %1) {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 %0, ptr %3, align 4
  store i32 %1, ptr %4, align 4
  %5 = load i32, ptr %3, align 4
  %6 = load i32, ptr %4, align 4
  %7 = add nsw i32 %5, %6
  %8 = load i32, ptr @old_global, align 4, !diffkemp.pattern !0
  %9 = add nsw i32 %7, %8
  ret i32 %9, !diffkemp.pattern !1
}

define dso_local i32 @diffkemp.new.side(i32 %0, i32 %1) {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 %0, ptr %3, align 4
  store i32 %1, ptr %4, align 4
  %5 = load i32, ptr %3, align 4
  %6 = load i32, ptr %4, align 4
  %7 = add nsw i32 %5, %6
  %8 = load i32, ptr @new_global, align 4, !diffkemp.pattern !0
  %9 = add nsw i32 %7, %8
  ret i32 %9, !diffkemp.pattern !1
}

!llvm.dbg.cu = !{}

!0 = !{!"pattern-start"}
!1 = !{!"pattern-end"}
