// Expected: 8
struct Box {
    int width;
    int height;
};

int main() {
    struct Box b1;
    b1.width = 5;
    b1.height = 8;

    struct Box b2;
    b2 = b1;

    return b2.height;
}
