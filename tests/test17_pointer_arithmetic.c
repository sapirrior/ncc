// Expected: 20
int main() {
    int arr[5];
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 30;

    int *ptr = arr;
    int *ptr2 = ptr + 1;
    return *ptr2;
}
