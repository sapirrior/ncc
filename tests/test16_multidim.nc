// Expected: 15
fn main() -> i32 {
    mut grid: [[i32; 4]; 3];
    grid[1][2] = 15;
    return grid[1][2];
}

