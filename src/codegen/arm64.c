#include "codegen.h"
#include "common.h"

typedef struct {
    char *name;
    int offset;
} Symbol;

static Symbol locals[100];
static int local_count = 0;

static void clear_locals(void) {
    for (int i = 0; i < local_count; i++) {
        free(locals[i].name);
    }
    local_count = 0;
}

static int find_local(const char *name) {
    for (int i = 0; i < local_count; i++) {
        if (strcmp(locals[i].name, name) == 0) {
            return locals[i].offset;
        }
    }
    return -1;
}

static int add_local(const char *name) {
    int offset = find_local(name);
    if (offset != -1) {
        error("Redeclaration of variable: %s", name);
    }
    int new_offset = (local_count + 1) * 4;
    locals[local_count].name = xstrdup(name);
    locals[local_count].offset = new_offset;
    local_count++;
    return new_offset;
}

static void gen_node(ASTNode *node, FILE *out) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM:
            for (int i = 0; i < node->program.decl_count; i++) {
                gen_node(node->program.decls[i], out);
            }
            break;

        case AST_FUNC_DECL: {
            clear_locals();

            // First pass: scan function statements to register all variable declarations
            for (int i = 0; i < node->func.stmt_count; i++) {
                ASTNode *stmt = node->func.stmts[i];
                if (stmt->type == AST_STMT_DECL) {
                    add_local(stmt->decl_stmt.name);
                }
            }

            // Calculate 16-byte aligned stack allocation
            int total_vars = local_count;
            int stack_alloc = ((total_vars * 4) + 15) / 16 * 16;

            fprintf(out, "    .global %s\n", node->func.name);
            fprintf(out, "    .p2align 2\n");
            fprintf(out, "%s:\n", node->func.name);

            // Prologue
            fprintf(out, "    stp x29, x30, [sp, #-16]!\n");
            fprintf(out, "    mov x29, sp\n");
            if (stack_alloc > 0) {
                fprintf(out, "    sub sp, sp, #%d\n", stack_alloc);
            }
            fprintf(out, "\n");

            // Compile statements
            for (int i = 0; i < node->func.stmt_count; i++) {
                gen_node(node->func.stmts[i], out);
            }

            // Epilogue
            fprintf(out, "\n    mov sp, x29\n");
            fprintf(out, "    ldp x29, x30, [sp], #16\n");
            fprintf(out, "    ret\n");

            clear_locals();
            break;
        }

        case AST_STMT_RETURN:
            gen_node(node->ret_stmt.expr, out);
            fprintf(out, "    mov sp, x29\n");
            fprintf(out, "    ldp x29, x30, [sp], #16\n");
            fprintf(out, "    ret\n");
            break;

        case AST_STMT_EXPR:
            gen_node(node->expr_stmt.expr, out);
            break;

        case AST_STMT_DECL: {
            int offset = find_local(node->decl_stmt.name);
            if (node->decl_stmt.init) {
                gen_node(node->decl_stmt.init, out); // Evaluate initializer -> w0
            } else {
                fprintf(out, "    mov w0, #0\n"); // Default init to 0
            }
            fprintf(out, "    str w0, [x29, #-%d]\n", offset);
            break;
        }

        case AST_EXPR_BINARY:
            gen_node(node->binary.left, out); // w0 = left
            // Push w0 to stack
            fprintf(out, "    str w0, [sp, #-16]!\n");
            gen_node(node->binary.right, out); // w0 = right
            // Pop left into w1
            fprintf(out, "    ldr w1, [sp], #16\n");

            switch (node->binary.op) {
                case OP_ADD:
                    fprintf(out, "    add w0, w1, w0\n");
                    break;
                case OP_SUB:
                    fprintf(out, "    sub w0, w1, w0\n");
                    break;
                case OP_MUL:
                    fprintf(out, "    mul w0, w1, w0\n");
                    break;
                case OP_DIV:
                    fprintf(out, "    sdiv w0, w1, w0\n");
                    break;
                case OP_EQ:
                    fprintf(out, "    cmp w1, w0\n");
                    fprintf(out, "    cset w0, eq\n");
                    break;
                case OP_NE:
                    fprintf(out, "    cmp w1, w0\n");
                    fprintf(out, "    cset w0, ne\n");
                    break;
                case OP_LT:
                    fprintf(out, "    cmp w1, w0\n");
                    fprintf(out, "    cset w0, lt\n");
                    break;
                case OP_LE:
                    fprintf(out, "    cmp w1, w0\n");
                    fprintf(out, "    cset w0, le\n");
                    break;
                case OP_GT:
                    fprintf(out, "    cmp w1, w0\n");
                    fprintf(out, "    cset w0, gt\n");
                    break;
                case OP_GE:
                    fprintf(out, "    cmp w1, w0\n");
                    fprintf(out, "    cset w0, ge\n");
                    break;
            }
            break;

        case AST_EXPR_UNARY:
            gen_node(node->unary.expr, out);
            switch (node->unary.op) {
                case OP_NEG:
                    fprintf(out, "    neg w0, w0\n");
                    break;
                case OP_NOT:
                    fprintf(out, "    mvn w0, w0\n");
                    break;
                case OP_LNOT:
                    fprintf(out, "    cmp w0, #0\n");
                    fprintf(out, "    cset w0, eq\n");
                    break;
            }
            break;

        case AST_EXPR_ASSIGN: {
            int offset = find_local(node->assign.name);
            if (offset == -1) {
                error("Assignment to undeclared variable: %s", node->assign.name);
            }
            gen_node(node->assign.expr, out); // w0 = value
            fprintf(out, "    str w0, [x29, #-%d]\n", offset);
            break;
        }

        case AST_EXPR_VAR: {
            int offset = find_local(node->var.name);
            if (offset == -1) {
                error("Use of undeclared variable: %s", node->var.name);
            }
            fprintf(out, "    ldr w0, [x29, #-%d]\n", offset);
            break;
        }

        case AST_EXPR_NUM:
            fprintf(out, "    mov w0, #%d\n", node->num.value);
            break;
    }
}

void generate_arm64(ASTNode *node, FILE *out) {
    fprintf(out, "// Generated by ncc (Nano C Compiler)\n");
    fprintf(out, "    .text\n");

    gen_node(node, out);
}
