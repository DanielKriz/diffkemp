int branching(int x, int y) {
    if (x < 42) {
        x += 2;
        return x;
    } else if (y == 2) {
        y += 7;
    }
    return x + y + 42;
}

int main(void) { return 0; }
