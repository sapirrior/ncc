#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"

// Set target architecture: "arm64" or "x86_64"
bool set_codegen_target(const char *target);

// Generate assembly to the output stream
void generate_code(ASTNode *node, FILE *out);

// Architecture-specific generator prototypes
void generate_arm64(ASTNode *node, FILE *out);
void generate_x86_64(ASTNode *node, FILE *out);

#endif // CODEGEN_H
