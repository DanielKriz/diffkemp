; ModuleID = 'structure_usage_variant_pattern-var-0'
source_filename = "structure_usage_variant_pattern-var-0"

%struct.b.0 = type { i32, i32 }
%struct.c.1 = type { i32, i32 }

define dso_local i32 @diffkemp.old.side(i64 %0) {
  %2 = alloca %struct.b.0, align 4
  store i64 %0, ptr %2, align 4
  %3 = getelementptr %struct.b.0, ptr %2, i32 0, i32 0
  %4 = load i32, ptr %3, align 4
  %5 = getelementptr %struct.b.0, ptr %2, i32 0, i32 1
  %6 = load i32, ptr %5, align 4
  %7 = add nsw i32 %4, %6
  %8 = add nsw i32 %7, 42
  ret i32 %8
}

define dso_local i32 @diffkemp.new.side(i64 %0) {
  %2 = alloca %struct.c.1, align 4
  store i64 %0, ptr %2, align 4
  %3 = getelementptr %struct.c.1, ptr %2, i32 0, i32 0
  %4 = load i32, ptr %3, align 4
  %5 = getelementptr %struct.c.1, ptr %2, i32 0, i32 1
  %6 = load i32, ptr %5, align 4
  %7 = add nsw i32 %4, %6
  %8 = add nsw i32 %7, 42
  ret i32 %8
}

!llvm.dbg.cu = !{}
