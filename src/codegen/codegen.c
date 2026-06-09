#include "codegen.h"
#include "common.h"
#include <string.h>

static enum {
    TARGET_ARM64,
    TARGET_X86_64
} current_target = TARGET_ARM64;

bool set_codegen_target(const char *target) {
    if (strcmp(target, "arm64") == 0) {
        current_target = TARGET_ARM64;
        return true;
    } else if (strcmp(target, "x86_64") == 0) {
        current_target = TARGET_X86_64;
        return true;
    }
    return false;
}

void generate_code(ASTNode *node, FILE *out) {
    switch (current_target) {
        case TARGET_ARM64:
            generate_arm64(node, out);
            break;
        case TARGET_X86_64:
            generate_x86_64(node, out);
            break;
    }
}

// Temporary stub for x86_64 backend
void generate_x86_64(ASTNode *node, FILE *out) {
    (void)node;
    (void)out;
    error("x86-64 target backend is not implemented yet!");
}
