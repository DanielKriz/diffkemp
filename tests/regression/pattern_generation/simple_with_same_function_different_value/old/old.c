int f() {
    return 42;
}

int simple_with_same_function_different_value(int x, int y) { return x + y + f() + 42; }

int main(void) { return 0; }
