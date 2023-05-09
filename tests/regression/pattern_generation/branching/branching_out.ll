; ModuleID = 'branching_pattern'
source_filename = "branching_pattern"

declare void @diffkemp.output_mapping(...)

define dso_local i32 @diffkemp.old.side(i32 %0, i32 %1) {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  store i32 %0, ptr %4, align 4
  store i32 %1, ptr %5, align 4
  %6 = load i32, ptr %4, align 4
  %7 = icmp slt i32 %6, 42
  br i1 %7, label %8, label %12

8:                                                ; preds = %2
  %9 = load i32, ptr %4, align 4
  %10 = add nsw i32 %9, 2
  store i32 %10, ptr %4, align 4
  %11 = load i32, ptr %4, align 4
  store i32 %11, ptr %3, align 4
  br label %24

12:                                               ; preds = %2
  %13 = load i32, ptr %5, align 4
  %14 = icmp eq i32 %13, 2
  br i1 %14, label %15, label %18

15:                                               ; preds = %12
  %16 = load i32, ptr %5, align 4
  %17 = add nsw i32 %16, 7, !diffkemp.pattern !0
  store i32 %17, ptr %5, align 4
  br label %18

18:                                               ; preds = %15, %12
  br label %19

19:                                               ; preds = %18
  %20 = load i32, ptr %4, align 4
  %21 = load i32, ptr %5, align 4
  %22 = add nsw i32 %20, %21
  %23 = add nsw i32 %22, 42
  store i32 %23, ptr %3, align 4
  br label %24

24:                                               ; preds = %19, %8
  %25 = load i32, ptr %3, align 4
  ret i32 %25, !diffkemp.pattern !1
}

define dso_local i32 @diffkemp.new.side(i32 %0, i32 %1) {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  store i32 %0, ptr %4, align 4
  store i32 %1, ptr %5, align 4
  %6 = load i32, ptr %4, align 4
  %7 = icmp slt i32 %6, 42
  br i1 %7, label %8, label %12

8:                                                ; preds = %2
  %9 = load i32, ptr %4, align 4
  %10 = add nsw i32 %9, 2
  store i32 %10, ptr %4, align 4
  %11 = load i32, ptr %4, align 4
  store i32 %11, ptr %3, align 4
  br label %24

12:                                               ; preds = %2
  %13 = load i32, ptr %5, align 4
  %14 = icmp eq i32 %13, 2
  br i1 %14, label %15, label %18

15:                                               ; preds = %12
  %16 = load i32, ptr %5, align 4
  %17 = add nsw i32 %16, 4, !diffkemp.pattern !0
  store i32 %17, ptr %5, align 4
  br label %18

18:                                               ; preds = %15, %12
  br label %19

19:                                               ; preds = %18
  %20 = load i32, ptr %4, align 4
  %21 = load i32, ptr %5, align 4
  %22 = add nsw i32 %20, %21
  %23 = add nsw i32 %22, 42
  store i32 %23, ptr %3, align 4
  br label %24

24:                                               ; preds = %19, %8
  %25 = load i32, ptr %3, align 4
  ret i32 %25, !diffkemp.pattern !1
}

!llvm.dbg.cu = !{}

!0 = !{!"pattern-start"}
!1 = !{!"pattern-end"}
