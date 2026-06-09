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

static void register_locals(ASTNode *node) {
    if (!node) return;
    switch (node->type) {
        case AST_STMT_DECL:
            add_local(node->decl_stmt.name);
            break;
        case AST_STMT_BLOCK:
            for (int i = 0; i < node->block.stmt_count; i++) {
                register_locals(node->block.stmts[i]);
            }
            break;
        case AST_STMT_IF:
            register_locals(node->if_stmt.then_branch);
            if (node->if_stmt.else_branch) {
                register_locals(node->if_stmt.else_branch);
            }
            break;
        case AST_STMT_WHILE:
        case AST_STMT_DO_WHILE:
            register_locals(node->while_stmt.body);
            break;
        case AST_STMT_FOR:
            if (node->for_stmt.init && node->for_stmt.init->type == AST_STMT_DECL) {
                register_locals(node->for_stmt.init);
            }
            register_locals(node->for_stmt.body);
            break;
        default:
            break;
    }
}

typedef enum {
    LOOP_WHILE,
    LOOP_DO_WHILE,
    LOOP_FOR
} LoopType;

typedef struct {
    int label_num;
    LoopType type;
} LoopInfo;

static LoopInfo loop_stack[100];
static int loop_depth = 0;

static int label_counter = 0;

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

            // Register parameter offsets
            for (int i = 0; i < node->func.param_count; i++) {
                add_local(node->func.params[i]);
            }

            // Register local variable offsets recursively
            for (int i = 0; i < node->func.stmt_count; i++) {
                register_locals(node->func.stmts[i]);
            }

            // Aligned stack allocation
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

            // Copy incoming parameters into stack slots
            for (int i = 0; i < node->func.param_count; i++) {
                int offset = find_local(node->func.params[i]);
                fprintf(out, "    str w%d, [x29, #-%d]\n", i, offset);
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
                gen_node(node->decl_stmt.init, out); // Evaluate -> w0
            } else {
                fprintf(out, "    mov w0, #0\n"); // Default init to 0
            }
            fprintf(out, "    str w0, [x29, #-%d]\n", offset);
            break;
        }

        case AST_STMT_IF: {
            int lbl = label_counter++;
            gen_node(node->if_stmt.cond, out);
            fprintf(out, "    cmp w0, #0\n");
            if (node->if_stmt.else_branch) {
                fprintf(out, "    beq .L.else.%d\n", lbl);
                gen_node(node->if_stmt.then_branch, out);
                fprintf(out, "    b .L.end.%d\n", lbl);
                fprintf(out, ".L.else.%d:\n", lbl);
                gen_node(node->if_stmt.else_branch, out);
            } else {
                fprintf(out, "    beq .L.end.%d\n", lbl);
                gen_node(node->if_stmt.then_branch, out);
            }
            fprintf(out, ".L.end.%d:\n", lbl);
            break;
        }

        case AST_STMT_WHILE: {
            int lbl = label_counter++;
            loop_stack[loop_depth++] = (LoopInfo){lbl, LOOP_WHILE};

            fprintf(out, ".L.begin.%d:\n", lbl);
            gen_node(node->while_stmt.cond, out);
            fprintf(out, "    cmp w0, #0\n");
            fprintf(out, "    beq .L.end.%d\n", lbl);
            gen_node(node->while_stmt.body, out);
            fprintf(out, "    b .L.begin.%d\n", lbl);
            fprintf(out, ".L.end.%d:\n", lbl);

            loop_depth--;
            break;
        }

        case AST_STMT_DO_WHILE: {
            int lbl = label_counter++;
            loop_stack[loop_depth++] = (LoopInfo){lbl, LOOP_DO_WHILE};

            fprintf(out, ".L.begin.%d:\n", lbl);
            gen_node(node->while_stmt.body, out);
            fprintf(out, ".L.cond.%d:\n", lbl);
            gen_node(node->while_stmt.cond, out);
            fprintf(out, "    cmp w0, #0\n");
            fprintf(out, "    bne .L.begin.%d\n", lbl);
            fprintf(out, ".L.end.%d:\n", lbl);

            loop_depth--;
            break;
        }

        case AST_STMT_FOR: {
            int lbl = label_counter++;
            loop_stack[loop_depth++] = (LoopInfo){lbl, LOOP_FOR};

            if (node->for_stmt.init) {
                gen_node(node->for_stmt.init, out);
            }
            fprintf(out, ".L.begin.%d:\n", lbl);
            if (node->for_stmt.cond) {
                gen_node(node->for_stmt.cond, out);
                fprintf(out, "    cmp w0, #0\n");
                fprintf(out, "    beq .L.end.%d\n", lbl);
            }
            gen_node(node->for_stmt.body, out);
            fprintf(out, ".L.post.%d:\n", lbl);
            if (node->for_stmt.post) {
                gen_node(node->for_stmt.post, out);
            }
            fprintf(out, "    b .L.begin.%d\n", lbl);
            fprintf(out, ".L.end.%d:\n", lbl);

            loop_depth--;
            break;
        }

        case AST_STMT_BLOCK:
            for (int i = 0; i < node->block.stmt_count; i++) {
                gen_node(node->block.stmts[i], out);
            }
            break;

        case AST_STMT_BREAK: {
            if (loop_depth == 0) {
                error("break statement outside of loop");
            }
            int lbl = loop_stack[loop_depth - 1].label_num;
            fprintf(out, "    b .L.end.%d\n", lbl);
            break;
        }

        case AST_STMT_CONTINUE: {
            if (loop_depth == 0) {
                error("continue statement outside of loop");
            }
            LoopInfo info = loop_stack[loop_depth - 1];
            if (info.type == LOOP_WHILE) {
                fprintf(out, "    b .L.begin.%d\n", info.label_num);
            } else if (info.type == LOOP_DO_WHILE) {
                fprintf(out, "    b .L.cond.%d\n", info.label_num);
            } else if (info.type == LOOP_FOR) {
                fprintf(out, "    b .L.post.%d\n", info.label_num);
            }
            break;
        }

        case AST_EXPR_CALL: {
            // Evaluate arguments and push them to stack
            for (int i = 0; i < node->call.arg_count; i++) {
                gen_node(node->call.args[i], out);
                fprintf(out, "    str w0, [sp, #-16]!\n");
            }
            // Pop arguments into registers w0-w7
            for (int i = node->call.arg_count - 1; i >= 0; i--) {
                if (i < 8) {
                    fprintf(out, "    ldr w%d, [sp], #16\n", i);
                } else {
                    error("ncc currently supports up to 8 arguments in function calls");
                }
            }
            // Perform branch call
            fprintf(out, "    bl %s\n", node->call.name);
            break;
        }

        case AST_EXPR_BINARY:
            gen_node(node->binary.left, out); // w0 = left
            fprintf(out, "    str w0, [sp, #-16]!\n");
            gen_node(node->binary.right, out); // w0 = right
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
