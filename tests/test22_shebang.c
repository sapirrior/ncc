#!/data/data/com.termux/files/usr/bin/env ncc
// Expected: 99
#include <stdio.h>

int main(int argc, char **argv) {
    printf("Shebang execution test: arg1 = %s\n", argv[1]);
    return 99;
}
