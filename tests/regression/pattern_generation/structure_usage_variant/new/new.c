struct a {
    int x;
    int y;
};

struct c {
    int x;
    int y;
};

int structure_usage(struct a A) { return A.x + A.y + 42; }

int structure_usage_other(struct c C) { return C.x + C.y + 42; }

int main(void) { return 0; }
