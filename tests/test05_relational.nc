// Expected: 1
fn main() -> i32 {
    let a: bool = 5 > 3;
    let b: bool = 2 == 3;
    let c: bool = 4 <= 4;
    return (a != b) == c;
}

