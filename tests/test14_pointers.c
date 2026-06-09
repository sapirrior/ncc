// Expected: 99
int main() {
    int val = 42;
    int *ptr = &val;
    *ptr = 99;
    return val;
}
