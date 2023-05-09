struct b {
    int x;
    int y;
};

int structure_usage(struct b B) { return B.x + B.y + 42; }

int main(void) { return 0; }
