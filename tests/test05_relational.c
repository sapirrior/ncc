// Expected: 1
int main() {
    _Bool a = 5 > 3;
    _Bool b = 2 == 3;
    _Bool c = 4 <= 4;
    return (a != b) == c;
}
