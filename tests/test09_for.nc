// Expected: 55
fn main() -> i32 {
    mut sum: i32 = 0;
    mut i: i32 = 1;
    for i <= 10 {
        sum = sum + i;
        i = i + 1;
    }
    return sum;
}

