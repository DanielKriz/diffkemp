struct a {
    int x;
    int y;
};

int structure_usage(struct a A) { return A.x + A.y + 42; }

int main(void) { return 0; }
