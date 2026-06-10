// Expected: 13
fn main() -> i32 {
    let f: f32 = 3.14;
    // printf returns the number of characters printed.
    // "f = 3.140000\n" has 13 characters.
    let res: i32 = printf("f = %f\n", f);
    return res;
}

