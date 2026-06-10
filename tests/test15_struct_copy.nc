// Expected: 8
struct Box {
    width: i32;
    height: i32;
};

fn main() -> i32 {
    mut b1: struct Box;
    b1.width = 5;
    b1.height = 8;

    mut b2: struct Box;
    b2 = b1;

    return b2.height;
}

