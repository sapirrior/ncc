// Expected: 25
struct Point {
    int x;
    int y;
};

int main() {
    struct Point p;
    p.x = 10;
    p.y = 15;
    return p.x + p.y;
}
