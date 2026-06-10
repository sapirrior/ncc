// Expected: 20
fn main() -> i32 {
    mut arr: [i32; 5];
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 30;

    mut aptr: ptr = arr;
    mut ptr2: ptr = arr + 1;
    return *ptr2;
}

