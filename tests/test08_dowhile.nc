// Expected: 10
fn main() -> i32 {
    mut x: i32 = 0;
    while x < 10 {
        x = x + 2;
    }
    return x;
}

