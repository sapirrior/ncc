// Expected: 77

fn main() -> i32 {
    let x: f32 = 2.5;
    let y: f32 = 4.0;
    let z: f32 = x * y; // 10.0
    printf("JIT run-in-memory print check: z = %f\n", z);
    return 77;
}

