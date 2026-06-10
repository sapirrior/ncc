// Expected: 60
fn main() -> i32 {
    mut arr: [i32; 5];
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 30;
    arr[3] = 40;
    arr[4] = 50;
    return arr[1] + arr[3];
}

