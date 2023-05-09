; ModuleID = 'structure_usage_pattern'
source_filename = "structure_usage_pattern"

%struct.b = type { i32, i32 }
%struct.a = type { i32, i32 }

declare void @diffkemp.output_mapping(...)

define dso_local i32 @diffkemp.old.side(i64 %0) {
  %2 = alloca %struct.b, align 4
  store i64 %0, ptr %2, align 4
  %3 = getelementptr inbounds %struct.b, ptr %2, i32 0, i32 0
  %4 = load i32, ptr %3, align 4
  %5 = getelementptr inbounds %struct.b, ptr %2, i32 0, i32 1
  %6 = load i32, ptr %5, align 4
  %7 = add nsw i32 %4, %6
  %8 = add nsw i32 %7, 42
  ret i32 %8
}

define dso_local i32 @diffkemp.new.side(i64 %0) {
  %2 = alloca %struct.a, align 4
  store i64 %0, ptr %2, align 4
  %3 = getelementptr inbounds %struct.a, ptr %2, i32 0, i32 0
  %4 = load i32, ptr %3, align 4
  %5 = getelementptr inbounds %struct.a, ptr %2, i32 0, i32 1
  %6 = load i32, ptr %5, align 4
  %7 = add nsw i32 %4, %6
  %8 = add nsw i32 %7, 42
  ret i32 %8
}

!llvm.dbg.cu = !{}
