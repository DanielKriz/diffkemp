int old_global = 1;
int simple_function_with_globals(int x, int y) { return x + y + old_global; }

int main(void) { return 0; }
