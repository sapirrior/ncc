// Expected: 15
fn main() -> i32 {
    mut x: i32 = 10;
    if x > 5 {
        x = x + 5;
    } else {
        x = x - 5;
    }
    return x;
}

