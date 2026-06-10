// Expected: 25
struct Point {
    x: i32;
    y: i32;
};

fn main() -> i32 {
    mut p: struct Point;
    p.x = 10;
    p.y = 15;
    return p.x + p.y;
}

