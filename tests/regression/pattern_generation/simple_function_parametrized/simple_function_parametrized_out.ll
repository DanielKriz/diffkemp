; ModuleID = 'simple_function_parametrized_pattern'
source_filename = "simple_function_parametrized_pattern"

declare void @diffkemp.output_mapping(...)

define dso_local i32 @diffkemp.old.side(i32 %0, i32 %1, i32 %2) {
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  store i32 %0, ptr %4, align 4
  store i32 %1, ptr %5, align 4
  %6 = load i32, ptr %4, align 4
  %7 = load i32, ptr %5, align 4
  %8 = add nsw i32 %6, %7
  %9 = add nsw i32 %8, %2
  ret i32 %9
}

define dso_local i32 @diffkemp.new.side(i32 %0, i32 %1, i32 %2) {
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  store i32 %0, ptr %4, align 4
  store i32 %1, ptr %5, align 4
  %6 = load i32, ptr %4, align 4
  %7 = load i32, ptr %5, align 4
  %8 = add nsw i32 %6, %7
  %9 = add nsw i32 %8, %2
  ret i32 %9
}
