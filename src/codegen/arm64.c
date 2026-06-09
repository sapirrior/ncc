#include "codegen.h"
#include "common.h"
#include "emitter.h"

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
    int end_label;
    int cont_label;
} LoopInfo;

static LoopInfo loop_stack[100];
static int loop_depth = 0;

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
        case AST_EXPR_FLOAT_LIT:
            return new_type_float();
        case AST_EXPR_STRING_LIT:
            return new_type_ptr(new_type_int());
        case AST_EXPR_CALL:
            return new_type_int();
        case AST_EXPR_BINARY:
            if (node->binary.op >= OP_EQ && node->binary.op <= OP_GE) {
                return new_type_bool();
            }
            return get_node_type(node->binary.left); // type propagates
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

static void gen_node(ASTNode *node, Emitter *e);

static void gen_addr(ASTNode *node, Emitter *e) {
    switch (node->type) {
        case AST_EXPR_VAR: {
            Symbol *sym = find_local(node->var.name);
            if (!sym) {
                error("Undefined variable: %s", node->var.name);
            }
            emit_sub_imm(e, true, REG_X0, REG_X29, sym->offset);
            break;
        }

        case AST_EXPR_MEMBER: {
            gen_addr(node->member.expr, e);

            Type *struct_type = get_node_type(node->member.expr);
            if (struct_type->kind == KIND_PTR) {
                emit_ldr_imm(e, 8, REG_X0, REG_X0, 0);
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
                emit_add_imm(e, true, REG_X0, REG_X0, offset);
            }
            break;
        }

        case AST_EXPR_INDEX: {
            gen_addr(node->index.expr, e);
            emit_str_preindex(e, 8, REG_X0, REG_SP, -16);

            gen_node(node->index.index, e);
            emit_sxtw(e, REG_X0, REG_X0);
            emit_ldr_postindex(e, 8, REG_X1, REG_SP, 16);

            Type *arr_type = get_node_type(node->index.expr);
            if (arr_type->kind == KIND_PTR) {
                emit_ldr_imm(e, 8, REG_X1, REG_X1, 0);
                arr_type = arr_type->ptr_to;
            } else if (arr_type->kind == KIND_ARRAY) {
                arr_type = arr_type->array.elem_type;
            } else {
                error("Index operator on non-array type");
            }

            int elem_size = type_size(arr_type);
            emit_mov_imm(e, true, REG_X2, elem_size);
            emit_mul_reg(e, true, REG_X0, REG_X0, REG_X2);
            emit_add_reg(e, true, REG_X0, REG_X1, REG_X0);
            break;
        }

        case AST_EXPR_DEREF:
            gen_node(node->deref.expr, e);
            break;

        default:
            error("Cannot take address of non-Lvalue expression");
            break;
    }
}

static void gen_node(ASTNode *node, Emitter *e) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM:
            for (int i = 0; i < node->program.decl_count; i++) {
                gen_node(node->program.decls[i], e);
            }
            break;

        case AST_FUNC_DECL: {
            clear_locals();

            for (int i = 0; i < node->func.param_count; i++) {
                add_local(node->func.params[i], node->func.param_types[i]);
            }

            for (int i = 0; i < node->func.stmt_count; i++) {
                register_locals(node->func.stmts[i]);
            }

            int total_size = current_stack_offset;
            int stack_alloc = ((total_size + 15) / 16) * 16;

            if (!e->binary_mode) {
                fprintf(e->out_file, "    .global %s\n", node->func.name);
                fprintf(e->out_file, "    .p2align 2\n");
                fprintf(e->out_file, "%s:\n", node->func.name);
            } else {
                emitter_add_symbol(e, node->func.name, e->size);
            }

            // Prologue
            emit_stp_preindex(e, REG_X29, REG_X30, REG_SP, -16);
            emit_mov_reg(e, true, REG_X29, REG_SP);
            if (stack_alloc > 0) {
                emit_sub_imm(e, true, REG_SP, REG_SP, stack_alloc);
            }

            // Copy incoming parameters
            for (int i = 0; i < node->func.param_count; i++) {
                Symbol *sym = find_local(node->func.params[i]);
                int size = type_size(sym->type);
                if (sym->type->kind == KIND_FLOAT) {
                    emit_fstr_imm(e, (FloatReg)i, REG_X29, -sym->offset);
                } else if (size == 4) {
                    emit_str_imm(e, 4, (Reg)i, REG_X29, -sym->offset);
                } else if (size == 8) {
                    emit_str_imm(e, 8, (Reg)i, REG_X29, -sym->offset);
                }
            }

            // Compile statements
            for (int i = 0; i < node->func.stmt_count; i++) {
                gen_node(node->func.stmts[i], e);
            }

            // Epilogue
            emit_mov_reg(e, true, REG_SP, REG_X29);
            emit_ldp_postindex(e, REG_X29, REG_X30, REG_SP, 16);
            emit_ret(e);

            clear_locals();
            break;
        }

        case AST_STMT_STRUCT_DEF:
            break;

        case AST_STMT_RETURN:
            gen_node(node->ret_stmt.expr, e);
            emit_mov_reg(e, true, REG_SP, REG_X29);
            emit_ldp_postindex(e, REG_X29, REG_X30, REG_SP, 16);
            emit_ret(e);
            break;

        case AST_STMT_EXPR:
            gen_node(node->expr_stmt.expr, e);
            break;

        case AST_STMT_DECL: {
            Symbol *sym = find_local(node->decl_stmt.name);
            int size = type_size(sym->type);
            if (node->decl_stmt.init) {
                gen_node(node->decl_stmt.init, e); // Evaluates -> w0/x0/s0
                emit_sub_imm(e, true, REG_X1, REG_X29, sym->offset);
                if (sym->type->kind == KIND_FLOAT) {
                    emit_fstr_imm(e, REG_S0, REG_X1, 0);
                } else if (size == 4) {
                    emit_str_imm(e, 4, REG_X0, REG_X1, 0);
                } else if (size == 8) {
                    emit_str_imm(e, 8, REG_X0, REG_X1, 0);
                } else {
                    // Struct/array copy
                    emit_mov_reg(e, true, REG_X1, REG_X0);
                    emit_sub_imm(e, true, REG_X0, REG_X29, sym->offset);
                    for (int i = 0; i < size; i += 4) {
                        emit_ldr_imm(e, 4, REG_X2, REG_X1, i);
                        emit_str_imm(e, 4, REG_X2, REG_X0, i);
                    }
                }
            } else {
                emit_sub_imm(e, true, REG_X0, REG_X29, sym->offset);
                for (int i = 0; i < size; i += 4) {
                    emit_str_imm(e, 4, REG_XZR, REG_X0, i);
                }
            }
            break;
        }

        case AST_STMT_IF: {
            int else_lbl = emitter_new_label(e);
            int end_lbl = emitter_new_label(e);

            gen_node(node->if_stmt.cond, e);
            emit_cmp_imm(e, false, REG_X0, 0);

            if (node->if_stmt.else_branch) {
                emit_b_cond_label(e, COND_EQ, else_lbl);
                gen_node(node->if_stmt.then_branch, e);
                emit_b_label(e, end_lbl);
                emitter_define_label(e, else_lbl);
                gen_node(node->if_stmt.else_branch, e);
            } else {
                emit_b_cond_label(e, COND_EQ, end_lbl);
                gen_node(node->if_stmt.then_branch, e);
            }
            emitter_define_label(e, end_lbl);
            break;
        }

        case AST_STMT_WHILE: {
            int begin_lbl = emitter_new_label(e);
            int end_lbl = emitter_new_label(e);
            loop_stack[loop_depth++] = (LoopInfo){end_lbl, begin_lbl};

            emitter_define_label(e, begin_lbl);
            gen_node(node->while_stmt.cond, e);
            emit_cmp_imm(e, false, REG_X0, 0);
            emit_b_cond_label(e, COND_EQ, end_lbl);

            gen_node(node->while_stmt.body, e);
            emit_b_label(e, begin_lbl);
            emitter_define_label(e, end_lbl);

            loop_depth--;
            break;
        }

        case AST_STMT_DO_WHILE: {
            int begin_lbl = emitter_new_label(e);
            int cond_lbl = emitter_new_label(e);
            int end_lbl = emitter_new_label(e);
            loop_stack[loop_depth++] = (LoopInfo){end_lbl, cond_lbl};

            emitter_define_label(e, begin_lbl);
            gen_node(node->while_stmt.body, e);

            emitter_define_label(e, cond_lbl);
            gen_node(node->while_stmt.cond, e);
            emit_cmp_imm(e, false, REG_X0, 0);
            emit_b_cond_label(e, COND_NE, begin_lbl);
            emitter_define_label(e, end_lbl);

            loop_depth--;
            break;
        }

        case AST_STMT_FOR: {
            int begin_lbl = emitter_new_label(e);
            int post_lbl = emitter_new_label(e);
            int end_lbl = emitter_new_label(e);
            loop_stack[loop_depth++] = (LoopInfo){end_lbl, post_lbl};

            if (node->for_stmt.init) {
                gen_node(node->for_stmt.init, e);
            }
            emitter_define_label(e, begin_lbl);
            if (node->for_stmt.cond) {
                gen_node(node->for_stmt.cond, e);
                emit_cmp_imm(e, false, REG_X0, 0);
                emit_b_cond_label(e, COND_EQ, end_lbl);
            }
            gen_node(node->for_stmt.body, e);

            emitter_define_label(e, post_lbl);
            if (node->for_stmt.post) {
                gen_node(node->for_stmt.post, e);
            }
            emit_b_label(e, begin_lbl);
            emitter_define_label(e, end_lbl);

            loop_depth--;
            break;
        }

        case AST_STMT_BLOCK:
            for (int i = 0; i < node->block.stmt_count; i++) {
                gen_node(node->block.stmts[i], e);
            }
            break;

        case AST_STMT_BREAK: {
            if (loop_depth == 0) {
                error("break statement outside loop");
            }
            int lbl = loop_stack[loop_depth - 1].end_label;
            emit_b_label(e, lbl);
            break;
        }

        case AST_STMT_CONTINUE: {
            if (loop_depth == 0) {
                error("continue statement outside loop");
            }
            int lbl = loop_stack[loop_depth - 1].cont_label;
            emit_b_label(e, lbl);
            break;
        }

        case AST_EXPR_CALL: {
            bool is_printf = (strcmp(node->call.name, "printf") == 0);

            // Evaluate arguments and push them to stack
            for (int i = 0; i < node->call.arg_count; i++) {
                Type *arg_type = get_node_type(node->call.args[i]);
                gen_node(node->call.args[i], e);
                if (arg_type && arg_type->kind == KIND_FLOAT) {
                    emit_fstr_preindex(e, REG_S0, REG_SP, -16);
                } else {
                    emit_str_preindex(e, 8, REG_X0, REG_SP, -16);
                }
            }

            int int_arg_idx = 0;
            int float_arg_idx = 0;

            // Pop arguments in reverse order and assign to calling convention registers
            for (int i = node->call.arg_count - 1; i >= 0; i--) {
                Type *arg_type = get_node_type(node->call.args[i]);
                if (arg_type && arg_type->kind == KIND_FLOAT) {
                    emit_fldr_postindex(e, REG_S16, REG_SP, 16);
                    if (is_printf) {
                        emit_fcvt_d_s(e, float_arg_idx++, REG_S16);
                    } else {
                        emit_fmov(e, (FloatReg)float_arg_idx++, REG_S16);
                    }
                } else {
                    emit_ldr_postindex(e, 8, (Reg)int_arg_idx++, REG_SP, 16);
                }
            }

            emit_bl(e, node->call.name);
            break;
        }

        case AST_EXPR_BINARY: {
            Type *left_type = get_node_type(node->binary.left);
            Type *right_type = get_node_type(node->binary.right);

            bool left_is_ptr = left_type && (left_type->kind == KIND_PTR || left_type->kind == KIND_ARRAY);
            bool right_is_ptr = right_type && (right_type->kind == KIND_PTR || right_type->kind == KIND_ARRAY);
            bool is_float = (left_type && left_type->kind == KIND_FLOAT) || (right_type && right_type->kind == KIND_FLOAT);

            gen_node(node->binary.left, e);
            if (is_float) {
                emit_fstr_preindex(e, REG_S0, REG_SP, -16);
            } else {
                emit_str_preindex(e, 8, REG_X0, REG_SP, -16);
            }

            gen_node(node->binary.right, e);
            if (is_float) {
                emit_fldr_postindex(e, REG_S1, REG_SP, 16);
            } else {
                emit_ldr_postindex(e, 8, REG_X1, REG_SP, 16);
            }

            if (is_float) {
                switch (node->binary.op) {
                    case OP_ADD:
                        emit_fadd(e, REG_S0, REG_S1, REG_S0);
                        break;
                    case OP_SUB:
                        emit_fsub(e, REG_S0, REG_S1, REG_S0);
                        break;
                    case OP_MUL:
                        emit_fmul(e, REG_S0, REG_S1, REG_S0);
                        break;
                    case OP_DIV:
                        emit_fdiv(e, REG_S0, REG_S1, REG_S0);
                        break;
                    case OP_EQ:
                        emit_fcmp(e, REG_S1, REG_S0);
                        emit_cset(e, REG_X0, COND_EQ);
                        break;
                    case OP_NE:
                        emit_fcmp(e, REG_S1, REG_S0);
                        emit_cset(e, REG_X0, COND_NE);
                        break;
                    case OP_LT:
                        emit_fcmp(e, REG_S1, REG_S0);
                        emit_cset(e, REG_X0, COND_LT);
                        break;
                    case OP_LE:
                        emit_fcmp(e, REG_S1, REG_S0);
                        emit_cset(e, REG_X0, COND_LE);
                        break;
                    case OP_GT:
                        emit_fcmp(e, REG_S1, REG_S0);
                        emit_cset(e, REG_X0, COND_GT);
                        break;
                    case OP_GE:
                        emit_fcmp(e, REG_S1, REG_S0);
                        emit_cset(e, REG_X0, COND_GE);
                        break;
                }
            } else if (left_is_ptr && !right_is_ptr && (node->binary.op == OP_ADD || node->binary.op == OP_SUB)) {
                Type *elem_type = (left_type->kind == KIND_PTR) ? left_type->ptr_to : left_type->array.elem_type;
                int scale = type_size(elem_type);
                emit_sxtw(e, REG_X0, REG_X0);
                if (scale > 1) {
                    emit_mov_imm(e, true, REG_X2, scale);
                    emit_mul_reg(e, true, REG_X0, REG_X0, REG_X2);
                }
                if (node->binary.op == OP_ADD) {
                    emit_add_reg(e, true, REG_X0, REG_X1, REG_X0);
                } else {
                    emit_sub_reg(e, true, REG_X0, REG_X1, REG_X0);
                }
            } else if (right_is_ptr && !left_is_ptr && node->binary.op == OP_ADD) {
                Type *elem_type = (right_type->kind == KIND_PTR) ? right_type->ptr_to : right_type->array.elem_type;
                int scale = type_size(elem_type);
                emit_sxtw(e, REG_X1, REG_X1);
                if (scale > 1) {
                    emit_mov_imm(e, true, REG_X2, scale);
                    emit_mul_reg(e, true, REG_X1, REG_X1, REG_X2);
                }
                emit_add_reg(e, true, REG_X0, REG_X0, REG_X1);
            } else {
                switch (node->binary.op) {
                    case OP_ADD:
                        emit_add_reg(e, false, REG_X0, REG_X1, REG_X0);
                        break;
                    case OP_SUB:
                        emit_sub_reg(e, false, REG_X0, REG_X1, REG_X0);
                        break;
                    case OP_MUL:
                        emit_mul_reg(e, false, REG_X0, REG_X1, REG_X0);
                        break;
                    case OP_DIV:
                        emit_sdiv_reg(e, false, REG_X0, REG_X1, REG_X0);
                        break;
                    case OP_EQ:
                        emit_cmp_reg(e, false, REG_X1, REG_X0);
                        emit_cset(e, REG_X0, COND_EQ);
                        break;
                    case OP_NE:
                        emit_cmp_reg(e, false, REG_X1, REG_X0);
                        emit_cset(e, REG_X0, COND_NE);
                        break;
                    case OP_LT:
                        emit_cmp_reg(e, false, REG_X1, REG_X0);
                        emit_cset(e, REG_X0, COND_LT);
                        break;
                    case OP_LE:
                        emit_cmp_reg(e, false, REG_X1, REG_X0);
                        emit_cset(e, REG_X0, COND_LE);
                        break;
                    case OP_GT:
                        emit_cmp_reg(e, false, REG_X1, REG_X0);
                        emit_cset(e, REG_X0, COND_GT);
                        break;
                    case OP_GE:
                        emit_cmp_reg(e, false, REG_X1, REG_X0);
                        emit_cset(e, REG_X0, COND_GE);
                        break;
                }
            }
            break;
        }

        case AST_EXPR_UNARY:
            gen_node(node->unary.expr, e);
            switch (node->unary.op) {
                case OP_NEG: {
                    Type *type = get_node_type(node->unary.expr);
                    if (type && type->kind == KIND_FLOAT) {
                        emit_fneg(e, REG_S0, REG_S0);
                    } else {
                        emit_neg_reg(e, false, REG_X0, REG_X0);
                    }
                    break;
                }
                case OP_NOT:
                    emit_mvn_reg(e, false, REG_X0, REG_X0);
                    break;
                case OP_LNOT:
                    emit_cmp_imm(e, false, REG_X0, 0);
                    emit_cset(e, REG_X0, COND_EQ);
                    break;
            }
            break;

        case AST_EXPR_ASSIGN: {
            Type *type = get_node_type(node->assign.left);
            int size = type_size(type);

            gen_node(node->assign.right, e); // RHS -> s0/w0/x0
            if (type && type->kind == KIND_FLOAT) {
                emit_fstr_preindex(e, REG_S0, REG_SP, -16);
                gen_addr(node->assign.left, e); // LHS address -> x0
                emit_fldr_postindex(e, REG_S1, REG_SP, 16);
                emit_fstr_imm(e, REG_S1, REG_X0, 0);
            } else if (size <= 8 && type && type->kind != KIND_STRUCT && type->kind != KIND_ARRAY) {
                emit_str_preindex(e, size, REG_X0, REG_SP, -16);
                gen_addr(node->assign.left, e); // LHS address -> x0
                emit_ldr_postindex(e, size, REG_X1, REG_SP, 16);
                emit_str_imm(e, size, REG_X1, REG_X0, 0);
            } else {
                // Struct assignment copy
                emit_str_preindex(e, 8, REG_X0, REG_SP, -16);
                gen_addr(node->assign.left, e); // LHS address -> x0
                emit_ldr_postindex(e, 8, REG_X1, REG_SP, 16); // RHS address -> x1
                for (int i = 0; i < size; i += 4) {
                    emit_ldr_imm(e, 4, REG_X2, REG_X1, i);
                    emit_str_imm(e, 4, REG_X2, REG_X0, i);
                }
            }
            break;
        }

        case AST_EXPR_VAR:
        case AST_EXPR_MEMBER:
        case AST_EXPR_INDEX: {
            Type *type = get_node_type(node);
            int size = type_size(type);
            gen_addr(node, e); // address -> x0
            if (type && type->kind == KIND_FLOAT) {
                emit_fldr_imm(e, REG_S0, REG_X0, 0);
            } else if (type && type->kind != KIND_STRUCT && type->kind != KIND_ARRAY) {
                emit_ldr_imm(e, size, REG_X0, REG_X0, 0);
            }
            break;
        }

        case AST_EXPR_ADDR:
            gen_addr(node->addr.expr, e); // x0 = address
            break;

        case AST_EXPR_DEREF: {
            Type *type = get_node_type(node);
            int size = type_size(type);
            gen_node(node->deref.expr, e); // pointer value -> x0
            if (type && type->kind == KIND_FLOAT) {
                emit_fldr_imm(e, REG_S0, REG_X0, 0);
            } else if (type && type->kind != KIND_STRUCT && type->kind != KIND_ARRAY) {
                emit_ldr_imm(e, size, REG_X0, REG_X0, 0);
            }
            break;
        }

        case AST_EXPR_NUM:
            emit_mov_imm(e, false, REG_X0, node->num.value);
            break;

        case AST_EXPR_FLOAT_LIT: {
            int data_lbl = emitter_new_label(e);
            int skip_lbl = emitter_new_label(e);

            emit_b_label(e, skip_lbl);
            emitter_define_label(e, data_lbl);
            emit_float_data(e, node->float_lit.value);
            emitter_define_label(e, skip_lbl);

            emit_ldr_pc_relative_float(e, REG_S0, data_lbl);
            break;
        }

        case AST_EXPR_STRING_LIT: {
            int data_lbl = emitter_new_label(e);
            int skip_lbl = emitter_new_label(e);

            emit_b_label(e, skip_lbl);
            emitter_define_label(e, data_lbl);
            emit_string_data(e, node->string_lit.value);
            emitter_define_label(e, skip_lbl);

            emit_adr(e, REG_X0, data_lbl);
            break;
        }
    }
}

void generate_arm64(ASTNode *node, FILE *out) {
    Emitter e;
    emitter_init(&e, out, false); // Text Assembly mode

    fprintf(out, "// Generated by ncc (Nano C Compiler)\n");
    fprintf(out, "    .text\n");

    gen_node(node, &e);

    emitter_free(&e);
}

void generate_arm64_binary(ASTNode *node, Emitter *e) {
    gen_node(node, e);
    emitter_resolve_relocs(e);
}
