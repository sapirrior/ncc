// Expected: 13
int main() {
    float f = 3.14;
    // printf returns the number of characters printed.
    // "f = 3.140000\n" has 13 characters.
    int res = printf("f = %f\n", f);
    return res;
}
