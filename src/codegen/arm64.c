#include "codegen.h"
#include "common.h"

typedef struct {
    char *name;
    int offset;
    Type *type;
} Symbol;

static Symbol locals[100];
static int local_count = 0;
static int current_stack_offset = 0;

static void clear_locals(void) {
    for (int i = 0; i < local_count; i++) {
        free(locals[i].name);
        free_type(locals[i].type);
    }
    local_count = 0;
    current_stack_offset = 0;
}

static Symbol *find_local(const char *name) {
    for (int i = 0; i < local_count; i++) {
        if (strcmp(locals[i].name, name) == 0) {
            return &locals[i];
        }
    }
    return NULL;
}

static int add_local(const char *name, Type *type) {
    if (find_local(name)) {
        error("Redeclaration of variable: %s", name);
    }
    int size = type_size(type);
    int align = type_align(type);

    current_stack_offset = (current_stack_offset + align - 1) / align * align;
    current_stack_offset += size;

    locals[local_count].name = xstrdup(name);
    locals[local_count].offset = current_stack_offset;
    locals[local_count].type = copy_type(type);
    local_count++;

    return current_stack_offset;
}

static void register_locals(ASTNode *node) {
    if (!node) return;
    switch (node->type) {
        case AST_STMT_DECL:
            add_local(node->decl_stmt.name, node->decl_stmt.var_type);
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

static Type *get_node_type(ASTNode *node) {
    if (!node) return NULL;
    switch (node->type) {
        case AST_EXPR_VAR: {
            Symbol *sym = find_local(node->var.name);
            if (!sym) error("Undefined variable in type lookup: %s", node->var.name);
            return sym->type;
        }
        case AST_EXPR_MEMBER: {
            Type *t = get_node_type(node->member.expr);
            if (t->kind == KIND_PTR) {
                t = t->ptr_to;
            }
            for (int i = 0; i < t->struct_def.field_count; i++) {
                if (strcmp(t->struct_def.fields[i].name, node->member.member_name) == 0) {
                    return t->struct_def.fields[i].type;
                }
            }
            error("Struct member not found in type lookup");
            return NULL;
        }
        case AST_EXPR_INDEX: {
            Type *t = get_node_type(node->index.expr);
            if (t->kind == KIND_PTR) {
                return t->ptr_to;
            }
            return t->array.elem_type;
        }
        case AST_EXPR_ADDR:
            return new_type_ptr(get_node_type(node->addr.expr));
        case AST_EXPR_DEREF:
            return get_node_type(node->deref.expr)->ptr_to;
        case AST_EXPR_NUM:
            return new_type_int();
        case AST_EXPR_CALL:
            return new_type_int();
        case AST_EXPR_BINARY:
            if (node->binary.op >= OP_EQ && node->binary.op <= OP_GE) {
                return new_type_bool();
            }
            return new_type_int();
        case AST_EXPR_UNARY:
            if (node->unary.op == OP_LNOT) {
                return new_type_bool();
            }
            return get_node_type(node->unary.expr);
        case AST_EXPR_ASSIGN:
            return get_node_type(node->assign.left);
        default:
            return new_type_int();
    }
}

static void gen_node(ASTNode *node, FILE *out);

static void gen_addr(ASTNode *node, FILE *out) {
    switch (node->type) {
        case AST_EXPR_VAR: {
            Symbol *sym = find_local(node->var.name);
            if (!sym) {
                error("Undefined variable: %s", node->var.name);
            }
            fprintf(out, "    sub x0, x29, #%d\n", sym->offset);
            break;
        }

        case AST_EXPR_MEMBER: {
            gen_addr(node->member.expr, out); // x0 = struct address

            Type *struct_type = get_node_type(node->member.expr);
            if (struct_type->kind == KIND_PTR) {
                fprintf(out, "    ldr x0, [x0]\n");
                struct_type = struct_type->ptr_to;
            }
            if (struct_type->kind != KIND_STRUCT) {
                error("Member access on non-struct type");
            }

            int offset = -1;
            for (int i = 0; i < struct_type->struct_def.field_count; i++) {
                if (strcmp(struct_type->struct_def.fields[i].name, node->member.member_name) == 0) {
                    offset = struct_type->struct_def.fields[i].offset;
                    break;
                }
            }
            if (offset == -1) {
                error("Struct member not found: %s", node->member.member_name);
            }

            if (offset > 0) {
                fprintf(out, "    add x0, x0, #%d\n", offset);
            }
            break;
        }

        case AST_EXPR_INDEX: {
            gen_addr(node->index.expr, out); // x0 = array base address
            fprintf(out, "    str x0, [sp, #-16]!\n"); // push base

            gen_node(node->index.index, out); // evaluate index -> w0
            fprintf(out, "    sxtw x0, w0\n"); // sign-extend index -> x0
            fprintf(out, "    ldr x1, [sp], #16\n"); // pop base -> x1

            Type *arr_type = get_node_type(node->index.expr);
            if (arr_type->kind == KIND_PTR) {
                fprintf(out, "    ldr x1, [x1]\n");
                arr_type = arr_type->ptr_to;
            } else if (arr_type->kind == KIND_ARRAY) {
                arr_type = arr_type->array.elem_type;
            } else {
                error("Index operator on non-array type");
            }

            int elem_size = type_size(arr_type);
            fprintf(out, "    mov x2, #%d\n", elem_size);
            fprintf(out, "    mul x0, x0, x2\n");
            fprintf(out, "    add x0, x1, x0\n");
            break;
        }

        case AST_EXPR_DEREF:
            gen_node(node->deref.expr, out); // Pointer value -> x0
            break;

        default:
            error("Cannot take address of non-Lvalue expression");
            break;
    }
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

            // Register parameter offsets
            for (int i = 0; i < node->func.param_count; i++) {
                add_local(node->func.params[i], node->func.param_types[i]);
            }

            // Register local variable offsets recursively
            for (int i = 0; i < node->func.stmt_count; i++) {
                register_locals(node->func.stmts[i]);
            }

            // Aligned stack allocation
            int total_size = current_stack_offset;
            int stack_alloc = ((total_size + 15) / 16) * 16;

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
                Symbol *sym = find_local(node->func.params[i]);
                int size = type_size(sym->type);
                if (size == 4) {
                    fprintf(out, "    str w%d, [x29, #-%d]\n", i, sym->offset);
                } else if (size == 8) {
                    fprintf(out, "    str x%d, [x29, #-%d]\n", i, sym->offset);
                }
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

        case AST_STMT_STRUCT_DEF:
            break;

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
            Symbol *sym = find_local(node->decl_stmt.name);
            int size = type_size(sym->type);
            if (node->decl_stmt.init) {
                gen_node(node->decl_stmt.init, out); // Evaluates to w0/x0
                fprintf(out, "    sub x1, x29, #%d\n", sym->offset);
                if (size == 4) {
                    fprintf(out, "    str w0, [x1]\n");
                } else if (size == 8) {
                    fprintf(out, "    str x0, [x1]\n");
                } else {
                    // Struct/array copy initializer
                    fprintf(out, "    mov x1, x0\n");
                    fprintf(out, "    sub x0, x29, #%d\n", sym->offset);
                    for (int i = 0; i < size; i += 4) {
                        fprintf(out, "    ldr w2, [x1, #%d]\n", i);
                        fprintf(out, "    str w2, [x0, #%d]\n", i);
                    }
                }
            } else {
                // Initialize allocation to 0
                fprintf(out, "    sub x0, x29, #%d\n", sym->offset);
                for (int i = 0; i < size; i += 4) {
                    fprintf(out, "    str wzr, [x0, #%d]\n", i);
                }
            }
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
                error("break statement outside loop");
            }
            int lbl = loop_stack[loop_depth - 1].label_num;
            fprintf(out, "    b .L.end.%d\n", lbl);
            break;
        }

        case AST_STMT_CONTINUE: {
            if (loop_depth == 0) {
                error("continue statement outside loop");
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
            for (int i = 0; i < node->call.arg_count; i++) {
                gen_node(node->call.args[i], out);
                fprintf(out, "    str x0, [sp, #-16]!\n"); // push as 64-bit to be safe
            }
            for (int i = node->call.arg_count - 1; i >= 0; i--) {
                if (i < 8) {
                    fprintf(out, "    ldr x%d, [sp], #16\n", i);
                } else {
                    error("ncc supports up to 8 arguments");
                }
            }
            fprintf(out, "    bl %s\n", node->call.name);
            break;
        }

        case AST_EXPR_BINARY: {
            Type *left_type = get_node_type(node->binary.left);
            Type *right_type = get_node_type(node->binary.right);

            bool left_is_ptr = left_type && (left_type->kind == KIND_PTR || left_type->kind == KIND_ARRAY);
            bool right_is_ptr = right_type && (right_type->kind == KIND_PTR || right_type->kind == KIND_ARRAY);

            gen_node(node->binary.left, out);
            fprintf(out, "    str x0, [sp, #-16]!\n");
            gen_node(node->binary.right, out);
            fprintf(out, "    ldr x1, [sp], #16\n");

            if (left_is_ptr && !right_is_ptr && (node->binary.op == OP_ADD || node->binary.op == OP_SUB)) {
                Type *elem_type = (left_type->kind == KIND_PTR) ? left_type->ptr_to : left_type->array.elem_type;
                int scale = type_size(elem_type);
                fprintf(out, "    sxtw x0, w0\n");
                if (scale > 1) {
                    fprintf(out, "    mov x2, #%d\n", scale);
                    fprintf(out, "    mul x0, x0, x2\n");
                }
                if (node->binary.op == OP_ADD) {
                    fprintf(out, "    add x0, x1, x0\n");
                } else {
                    fprintf(out, "    sub x0, x1, x0\n");
                }
            } else if (right_is_ptr && !left_is_ptr && node->binary.op == OP_ADD) {
                Type *elem_type = (right_type->kind == KIND_PTR) ? right_type->ptr_to : right_type->array.elem_type;
                int scale = type_size(elem_type);
                fprintf(out, "    sxtw x1, w1\n");
                if (scale > 1) {
                    fprintf(out, "    mov x2, #%d\n", scale);
                    fprintf(out, "    mul x1, x1, x2\n");
                }
                fprintf(out, "    add x0, x0, x1\n");
            } else {
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
            }
            break;
        }

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
            Type *type = get_node_type(node->assign.left);
            int size = type_size(type);

            gen_node(node->assign.right, out); // RHS -> w0/x0
            if (size <= 8) {
                fprintf(out, "    str %s, [sp, #-16]!\n", (size == 8) ? "x0" : "w0");
                gen_addr(node->assign.left, out); // LHS address -> x0
                fprintf(out, "    ldr %s, [sp], #16\n", (size == 8) ? "x1" : "w1");
                fprintf(out, "    str %s, [x0]\n", (size == 8) ? "x1" : "w1");
            } else {
                // Struct assignment copy
                fprintf(out, "    str x0, [sp, #-16]!\n"); // RHS address
                gen_addr(node->assign.left, out); // LHS address -> x0
                fprintf(out, "    ldr x1, [sp], #16\n"); // RHS address -> x1
                for (int i = 0; i < size; i += 4) {
                    fprintf(out, "    ldr w2, [x1, #%d]\n", i);
                    fprintf(out, "    str w2, [x0, #%d]\n", i);
                }
            }
            break;
        }

        case AST_EXPR_VAR:
        case AST_EXPR_MEMBER:
        case AST_EXPR_INDEX: {
            Type *type = get_node_type(node);
            int size = type_size(type);
            gen_addr(node, out); // address -> x0
            if (size == 4) {
                fprintf(out, "    ldr w0, [x0]\n");
            } else if (size == 8) {
                fprintf(out, "    ldr x0, [x0]\n");
            }
            break;
        }

        case AST_EXPR_ADDR:
            gen_addr(node->addr.expr, out); // x0 = address
            break;

        case AST_EXPR_DEREF: {
            Type *type = get_node_type(node);
            int size = type_size(type);
            gen_node(node->deref.expr, out); // pointer value -> x0
            if (size == 4) {
                fprintf(out, "    ldr w0, [x0]\n");
            } else if (size == 8) {
                fprintf(out, "    ldr x0, [x0]\n");
            }
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
