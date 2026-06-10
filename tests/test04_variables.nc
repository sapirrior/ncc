// Expected: 27
fn main() -> i32 {
    mut x: i32 = 10;
    mut y: i32 = 5 + x;
    x = x + 2;
    return x + y;
}

