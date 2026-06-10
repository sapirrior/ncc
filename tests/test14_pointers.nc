// Expected: 99
fn main() -> i32 {
    mut val: i32 = 42;
    mut p: ptr = &val;
    *p = 99;
    return val;
}

