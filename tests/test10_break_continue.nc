// Expected: 12
fn main() -> i32 {
    mut sum: i32 = 0;
    mut i: i32 = 1;
    for i <= 10 {
        if i == 3 {
            i = i + 1;
            continue;
        }
        if i == 6 {
            break;
        }
        sum = sum + i;
        i = i + 1;
    }
    return sum;
}

