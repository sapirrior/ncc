// Expected: 42
fn main() -> i32 {
    let a: f32 = 1.5;
    let b: f32 = 2.0;
    let c: f32 = a + b * 20.25;
    if c == 42.0 {
        return 42;
    }
    return 0;
}

