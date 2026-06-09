// Expected: 12
int main() {
    int sum = 0;
    for (int i = 1; i <= 10; i = i + 1) {
        if (i == 3) {
            continue;
        }
        if (i == 6) {
            break;
        }
        sum = sum + i;
    }
    return sum;
}
