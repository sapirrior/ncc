import os;
import console;

fn main() -> i32 {
    console::printf("--- Testing console standard library ---\n");

    // 1. Get dimensions
    let w: i32 = console::width();
    let h: i32 = console::height();
    console::printf("Terminal dimensions: %d columns x %d rows\n", w, h);

    // 2. Testing printf with floats and strings
    let val: f32 = 3.14159;
    console::printf("Math constant: pi = %f\n", val);
    console::printf("Hello from %s namespace!\n", "console");

    // 3. Test printchar
    console::printchar(65); // 'A'
    console::printchar(66); // 'B'
    console::printchar(67); // 'C'
    console::printchar(10); // '\n'

    // 4. Test coloring (if terminal supports it)
    console::printf("Normal Text\n");
    console::set_color(31, 40); // Red foreground, Black background
    console::printf("RED TEXT on black background!\n");
    console::reset_color();
    console::printf("Back to Normal Text\n");

    console::printf("--- Console library tests complete ---\n");
    return 0;
}
