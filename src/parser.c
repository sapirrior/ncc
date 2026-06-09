#include "parser.h"

// Type helpers
Type *new_type_int(void) {
    Type *t = xmalloc(sizeof(Type));
    t->kind = KIND_INT;
    return t;
}

Type *new_type_bool(void) {
    Type *t = xmalloc(sizeof(Type));
    t->kind = KIND_BOOL;
    return t;
}

Type *new_type_array(Type *elem_type, int size) {
    Type *t = xmalloc(sizeof(Type));
    t->kind = KIND_ARRAY;
    t->array.elem_type = elem_type;
    t->array.size = size;
    return t;
}

Type *new_type_struct(const char *name, Field *fields, int field_count) {
    Type *t = xmalloc(sizeof(Type));
    t->kind = KIND_STRUCT;
    t->struct_def.name = name ? xstrdup(name) : NULL;
    t->struct_def.fields = fields;
    t->struct_def.field_count = field_count;
    return t;
}

Type *new_type_ptr(Type *ptr_to) {
    Type *t = xmalloc(sizeof(Type));
    t->kind = KIND_PTR;
    t->ptr_to = ptr_to;
    return t;
}

int type_size(Type *type) {
    if (!type) return 0;
    switch (type->kind) {
        case KIND_INT: return 4;
        case KIND_BOOL: return 4;
        case KIND_PTR: return 8;
        case KIND_ARRAY: return type_size(type->array.elem_type) * type->array.size;
        case KIND_STRUCT: {
            int offset = 0;
            for (int i = 0; i < type->struct_def.field_count; i++) {
                Field *f = &type->struct_def.fields[i];
                int align = type_align(f->type);
                offset = (offset + align - 1) / align * align;
                f->offset = offset;
                offset += type_size(f->type);
            }
            int max_align = type_align(type);
            offset = (offset + max_align - 1) / max_align * max_align;
            return offset;
        }
    }
    return 0;
}

int type_align(Type *type) {
    if (!type) return 1;
    switch (type->kind) {
        case KIND_INT: return 4;
        case KIND_BOOL: return 4;
        case KIND_PTR: return 8;
        case KIND_ARRAY: return type_align(type->array.elem_type);
        case KIND_STRUCT: {
            int max_align = 1;
            for (int i = 0; i < type->struct_def.field_count; i++) {
                int align = type_align(type->struct_def.fields[i].type);
                if (align > max_align) {
                    max_align = align;
                }
            }
            return max_align;
        }
    }
    return 1;
}

void free_type(Type *type) {
    if (!type) return;
    switch (type->kind) {
        case KIND_INT:
        case KIND_BOOL:
            break;
        case KIND_ARRAY:
            free_type(type->array.elem_type);
            break;
        case KIND_STRUCT:
            free(type->struct_def.name);
            for (int i = 0; i < type->struct_def.field_count; i++) {
                free(type->struct_def.fields[i].name);
                free_type(type->struct_def.fields[i].type);
            }
            free(type->struct_def.fields);
            break;
        case KIND_PTR:
            free_type(type->ptr_to);
            break;
    }
    free(type);
}

Type *copy_type(Type *type) {
    if (!type) return NULL;
    Type *t = xmalloc(sizeof(Type));
    t->kind = type->kind;
    switch (type->kind) {
        case KIND_INT:
        case KIND_BOOL:
            break;
        case KIND_ARRAY:
            t->array.elem_type = copy_type(type->array.elem_type);
            t->array.size = type->array.size;
            break;
        case KIND_STRUCT:
            t->struct_def.name = type->struct_def.name ? xstrdup(type->struct_def.name) : NULL;
            t->struct_def.field_count = type->struct_def.field_count;
            t->struct_def.fields = xmalloc(sizeof(Field) * t->struct_def.field_count);
            for (int i = 0; i < t->struct_def.field_count; i++) {
                t->struct_def.fields[i].name = xstrdup(type->struct_def.fields[i].name);
                t->struct_def.fields[i].type = copy_type(type->struct_def.fields[i].type);
                t->struct_def.fields[i].offset = type->struct_def.fields[i].offset;
            }
            break;
        case KIND_PTR:
            t->ptr_to = copy_type(type->ptr_to);
            break;
    }
    return t;
}

// Struct Tag Registry
typedef struct {
    char *name;
    Type *type;
} StructTag;

static StructTag struct_tags[100];
static int struct_tag_count = 0;

static Type *find_struct_tag(const char *name) {
    for (int i = 0; i < struct_tag_count; i++) {
        if (strcmp(struct_tags[i].name, name) == 0) {
            return struct_tags[i].type;
        }
    }
    return NULL;
}

static void add_struct_tag(const char *name, Type *type) {
    if (find_struct_tag(name)) {
        error("Redeclaration of struct: %s", name);
    }
    struct_tags[struct_tag_count].name = xstrdup(name);
    struct_tags[struct_tag_count].type = copy_type(type);
    struct_tag_count++;
}

// AST helpers
ASTNode *new_ast_program(ASTNode **decls, int decl_count) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_PROGRAM;
    node->program.decls = decls;
    node->program.decl_count = decl_count;
    return node;
}

ASTNode *new_ast_func(const char *name, char **params, Type **param_types, int param_count, ASTNode **stmts, int stmt_count) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_FUNC_DECL;
    node->func.name = xstrdup(name);
    node->func.params = params;
    node->func.param_types = param_types;
    node->func.param_count = param_count;
    node->func.stmts = stmts;
    node->func.stmt_count = stmt_count;
    return node;
}

ASTNode *new_ast_struct_def(const char *name, Field *fields, int field_count) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_STMT_STRUCT_DEF;
    node->struct_def.name = xstrdup(name);
    node->struct_def.fields = fields;
    node->struct_def.field_count = field_count;
    return node;
}

ASTNode *new_ast_return(ASTNode *expr) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_STMT_RETURN;
    node->ret_stmt.expr = expr;
    return node;
}

ASTNode *new_ast_expr_stmt(ASTNode *expr) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_STMT_EXPR;
    node->expr_stmt.expr = expr;
    return node;
}

ASTNode *new_ast_decl(Type *var_type, const char *name, ASTNode *init) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_STMT_DECL;
    node->decl_stmt.var_type = var_type;
    node->decl_stmt.name = xstrdup(name);
    node->decl_stmt.init = init;
    return node;
}

ASTNode *new_ast_if(ASTNode *cond, ASTNode *then_branch, ASTNode *else_branch) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_STMT_IF;
    node->if_stmt.cond = cond;
    node->if_stmt.then_branch = then_branch;
    node->if_stmt.else_branch = else_branch;
    return node;
}

ASTNode *new_ast_while(ASTNode *cond, ASTNode *body) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_STMT_WHILE;
    node->while_stmt.cond = cond;
    node->while_stmt.body = body;
    return node;
}

ASTNode *new_ast_do_while(ASTNode *cond, ASTNode *body) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_STMT_DO_WHILE;
    node->while_stmt.cond = cond;
    node->while_stmt.body = body;
    return node;
}

ASTNode *new_ast_for(ASTNode *init, ASTNode *cond, ASTNode *post, ASTNode *body) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_STMT_FOR;
    node->for_stmt.init = init;
    node->for_stmt.cond = cond;
    node->for_stmt.post = post;
    node->for_stmt.body = body;
    return node;
}

ASTNode *new_ast_block(ASTNode **stmts, int stmt_count) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_STMT_BLOCK;
    node->block.stmts = stmts;
    node->block.stmt_count = stmt_count;
    return node;
}

ASTNode *new_ast_break(void) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_STMT_BREAK;
    return node;
}

ASTNode *new_ast_continue(void) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_STMT_CONTINUE;
    return node;
}

ASTNode *new_ast_call(const char *name, ASTNode **args, int arg_count) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_EXPR_CALL;
    node->call.name = xstrdup(name);
    node->call.args = args;
    node->call.arg_count = arg_count;
    return node;
}

ASTNode *new_ast_binary(BinaryOp op, ASTNode *left, ASTNode *right) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_EXPR_BINARY;
    node->binary.op = op;
    node->binary.left = left;
    node->binary.right = right;
    return node;
}

ASTNode *new_ast_unary(UnaryOp op, ASTNode *expr) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_EXPR_UNARY;
    node->unary.op = op;
    node->unary.expr = expr;
    return node;
}

ASTNode *new_ast_assign(ASTNode *left, ASTNode *right) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_EXPR_ASSIGN;
    node->assign.left = left;
    node->assign.right = right;
    return node;
}

ASTNode *new_ast_member(ASTNode *expr, const char *member_name) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_EXPR_MEMBER;
    node->member.expr = expr;
    node->member.member_name = xstrdup(member_name);
    return node;
}

ASTNode *new_ast_index(ASTNode *expr, ASTNode *index) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_EXPR_INDEX;
    node->index.expr = expr;
    node->index.index = index;
    return node;
}

ASTNode *new_ast_addr(ASTNode *expr) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_EXPR_ADDR;
    node->addr.expr = expr;
    return node;
}

ASTNode *new_ast_deref(ASTNode *expr) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_EXPR_DEREF;
    node->deref.expr = expr;
    return node;
}

ASTNode *new_ast_var(const char *name) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_EXPR_VAR;
    node->var.name = xstrdup(name);
    return node;
}

ASTNode *new_ast_num(int value) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_EXPR_NUM;
    node->num.value = value;
    return node;
}

void free_ast(ASTNode *node) {
    if (!node) return;
    switch (node->type) {
        case AST_PROGRAM:
            for (int i = 0; i < node->program.decl_count; i++) {
                free_ast(node->program.decls[i]);
            }
            free(node->program.decls);
            break;
        case AST_FUNC_DECL:
            free(node->func.name);
            for (int i = 0; i < node->func.param_count; i++) {
                free(node->func.params[i]);
                free_type(node->func.param_types[i]);
            }
            free(node->func.params);
            free(node->func.param_types);
            for (int i = 0; i < node->func.stmt_count; i++) {
                free_ast(node->func.stmts[i]);
            }
            free(node->func.stmts);
            break;
        case AST_STMT_STRUCT_DEF:
            free(node->struct_def.name);
            for (int i = 0; i < node->struct_def.field_count; i++) {
                free(node->struct_def.fields[i].name);
                free_type(node->struct_def.fields[i].type);
            }
            free(node->struct_def.fields);
            break;
        case AST_STMT_RETURN:
            free_ast(node->ret_stmt.expr);
            break;
        case AST_STMT_EXPR:
            free_ast(node->expr_stmt.expr);
            break;
        case AST_STMT_DECL:
            free(node->decl_stmt.name);
            free_type(node->decl_stmt.var_type);
            if (node->decl_stmt.init) {
                free_ast(node->decl_stmt.init);
            }
            break;
        case AST_STMT_IF:
            free_ast(node->if_stmt.cond);
            free_ast(node->if_stmt.then_branch);
            if (node->if_stmt.else_branch) {
                free_ast(node->if_stmt.else_branch);
            }
            break;
        case AST_STMT_WHILE:
        case AST_STMT_DO_WHILE:
            free_ast(node->while_stmt.cond);
            free_ast(node->while_stmt.body);
            break;
        case AST_STMT_FOR:
            if (node->for_stmt.init) free_ast(node->for_stmt.init);
            if (node->for_stmt.cond) free_ast(node->for_stmt.cond);
            if (node->for_stmt.post) free_ast(node->for_stmt.post);
            free_ast(node->for_stmt.body);
            break;
        case AST_STMT_BLOCK:
            for (int i = 0; i < node->block.stmt_count; i++) {
                free_ast(node->block.stmts[i]);
            }
            free(node->block.stmts);
            break;
        case AST_STMT_BREAK:
        case AST_STMT_CONTINUE:
            break;
        case AST_EXPR_CALL:
            free(node->call.name);
            for (int i = 0; i < node->call.arg_count; i++) {
                free_ast(node->call.args[i]);
            }
            free(node->call.args);
            break;
        case AST_EXPR_BINARY:
            free_ast(node->binary.left);
            free_ast(node->binary.right);
            break;
        case AST_EXPR_UNARY:
            free_ast(node->unary.expr);
            break;
        case AST_EXPR_ASSIGN:
            free_ast(node->assign.left);
            free_ast(node->assign.right);
            break;
        case AST_EXPR_MEMBER:
            free_ast(node->member.expr);
            free(node->member.member_name);
            break;
        case AST_EXPR_INDEX:
            free_ast(node->index.expr);
            free_ast(node->index.index);
            break;
        case AST_EXPR_ADDR:
            free_ast(node->addr.expr);
            break;
        case AST_EXPR_DEREF:
            free_ast(node->deref.expr);
            break;
        case AST_EXPR_VAR:
            free(node->var.name);
            break;
        case AST_EXPR_NUM:
            break;
    }
    free(node);
}

void print_ast(ASTNode *node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; i++) printf("  ");
    switch (node->type) {
        case AST_PROGRAM:
            printf("Program:\n");
            for (int i = 0; i < node->program.decl_count; i++) {
                print_ast(node->program.decls[i], indent + 1);
            }
            break;
        case AST_FUNC_DECL:
            printf("FuncDecl: %s (%d params)\n", node->func.name, node->func.param_count);
            for (int i = 0; i < node->func.stmt_count; i++) {
                print_ast(node->func.stmts[i], indent + 1);
            }
            break;
        case AST_STMT_STRUCT_DEF:
            printf("StructDef: struct %s\n", node->struct_def.name);
            break;
        case AST_STMT_RETURN:
            printf("ReturnStmt:\n");
            print_ast(node->ret_stmt.expr, indent + 1);
            break;
        case AST_STMT_EXPR:
            printf("ExprStmt:\n");
            print_ast(node->expr_stmt.expr, indent + 1);
            break;
        case AST_STMT_DECL:
            printf("DeclStmt: (type kind %d) %s\n",
                node->decl_stmt.var_type->kind,
                node->decl_stmt.name);
            if (node->decl_stmt.init) {
                print_ast(node->decl_stmt.init, indent + 1);
            }
            break;
        case AST_STMT_IF:
            printf("IfStmt:\n");
            print_ast(node->if_stmt.cond, indent + 1);
            print_ast(node->if_stmt.then_branch, indent + 1);
            if (node->if_stmt.else_branch) {
                print_ast(node->if_stmt.else_branch, indent + 1);
            }
            break;
        case AST_STMT_WHILE:
            printf("WhileStmt:\n");
            print_ast(node->while_stmt.cond, indent + 1);
            print_ast(node->while_stmt.body, indent + 1);
            break;
        case AST_STMT_DO_WHILE:
            printf("DoWhileStmt:\n");
            print_ast(node->while_stmt.cond, indent + 1);
            print_ast(node->while_stmt.body, indent + 1);
            break;
        case AST_STMT_FOR:
            printf("ForStmt:\n");
            if (node->for_stmt.init) print_ast(node->for_stmt.init, indent + 1);
            if (node->for_stmt.cond) print_ast(node->for_stmt.cond, indent + 1);
            if (node->for_stmt.post) print_ast(node->for_stmt.post, indent + 1);
            print_ast(node->for_stmt.body, indent + 1);
            break;
        case AST_STMT_BLOCK:
            printf("BlockStmt (%d stmts):\n", node->block.stmt_count);
            for (int i = 0; i < node->block.stmt_count; i++) {
                print_ast(node->block.stmts[i], indent + 1);
            }
            break;
        case AST_STMT_BREAK:
            printf("BreakStmt\n");
            break;
        case AST_STMT_CONTINUE:
            printf("ContinueStmt\n");
            break;
        case AST_EXPR_CALL:
            printf("CallExpr: %s (%d args)\n", node->call.name, node->call.arg_count);
            for (int i = 0; i < node->call.arg_count; i++) {
                print_ast(node->call.args[i], indent + 1);
            }
            break;
        case AST_EXPR_BINARY:
            printf("BinaryExpr: (op %d)\n", node->binary.op);
            print_ast(node->binary.left, indent + 1);
            print_ast(node->binary.right, indent + 1);
            break;
        case AST_EXPR_UNARY:
            printf("UnaryExpr: (op %d)\n", node->unary.op);
            print_ast(node->unary.expr, indent + 1);
            break;
        case AST_EXPR_ASSIGN:
            printf("AssignExpr:\n");
            print_ast(node->assign.left, indent + 1);
            print_ast(node->assign.right, indent + 1);
            break;
        case AST_EXPR_MEMBER:
            printf("MemberExpr: .%s\n", node->member.member_name);
            print_ast(node->member.expr, indent + 1);
            break;
        case AST_EXPR_INDEX:
            printf("IndexExpr:\n");
            print_ast(node->index.expr, indent + 1);
            print_ast(node->index.index, indent + 1);
            break;
        case AST_EXPR_ADDR:
            printf("AddrExpr:\n");
            print_ast(node->addr.expr, indent + 1);
            break;
        case AST_EXPR_DEREF:
            printf("DerefExpr:\n");
            print_ast(node->deref.expr, indent + 1);
            break;
        case AST_EXPR_VAR:
            printf("VarExpr: %s\n", node->var.name);
            break;
        case AST_EXPR_NUM:
            printf("NumExpr: %d\n", node->num.value);
            break;
    }
}

// Parsing logic
static Token *tok;

static void consume(TokenType type, const char *err_msg) {
    if (tok->type != type) {
        error_at("input", tok->line, tok->col, "%s (got type %d)", err_msg, tok->type);
    }
    tok = tok->next;
}

static bool match(TokenType type) {
    if (tok->type == type) {
        tok = tok->next;
        return true;
    }
    return false;
}

// Precedence levels for binary operators
typedef struct {
    TokenType type;
    int prec;
    BinaryOp op;
} BinOpInfo;

static BinOpInfo binops[] = {
    {TOKEN_EQ, 1, OP_EQ},
    {TOKEN_NE, 1, OP_NE},
    {TOKEN_LT, 2, OP_LT},
    {TOKEN_LE, 2, OP_LE},
    {TOKEN_GT, 2, OP_GT},
    {TOKEN_GE, 2, OP_GE},
    {TOKEN_PLUS, 3, OP_ADD},
    {TOKEN_MINUS, 3, OP_SUB},
    {TOKEN_STAR, 4, OP_MUL},
    {TOKEN_SLASH, 4, OP_DIV},
};

static int get_tok_precedence(TokenType type) {
    for (size_t i = 0; i < sizeof(binops)/sizeof(binops[0]); i++) {
        if (binops[i].type == type) {
            return binops[i].prec;
        }
    }
    return -1;
}

static BinaryOp get_bin_op(TokenType type) {
    for (size_t i = 0; i < sizeof(binops)/sizeof(binops[0]); i++) {
        if (binops[i].type == type) {
            return binops[i].op;
        }
    }
    error("Invalid binary token type");
    return OP_ADD;
}

static ASTNode *parse_expression(int min_prec);
static Type *parse_type_specifier(void);
static Type *parse_declarator_suffixes(Type *base_type);

static ASTNode *parse_primary_basic(void) {
    if (tok->type == TOKEN_NUM) {
        int val = atoi(tok->value);
        consume(TOKEN_NUM, "Expected number");
        return new_ast_num(val);
    }

    if (tok->type == TOKEN_IDENT) {
        char *name = tok->value;
        consume(TOKEN_IDENT, "Expected identifier");

        // Function call?
        if (match(TOKEN_LPAREN)) {
            ASTNode **args = NULL;
            int arg_count = 0;
            if (tok->type != TOKEN_RPAREN) {
                while (1) {
                    ASTNode *arg = parse_expression(0);
                    args = realloc(args, sizeof(ASTNode*) * (arg_count + 1));
                    args[arg_count++] = arg;
                    if (!match(TOKEN_COMMA)) {
                        break;
                    }
                }
            }
            consume(TOKEN_RPAREN, "Expected ')' after arguments");
            return new_ast_call(name, args, arg_count);
        }

        // Variable reference
        return new_ast_var(name);
    }

    if (match(TOKEN_LPAREN)) {
        ASTNode *expr = parse_expression(0);
        consume(TOKEN_RPAREN, "Expected ')'");
        return expr;
    }

    error_at("input", tok->line, tok->col, "Expected expression (primary)");
    return NULL;
}

static ASTNode *parse_postfix(void) {
    ASTNode *expr = parse_primary_basic();

    while (1) {
        if (match(TOKEN_LBRACK)) {
            ASTNode *index = parse_expression(0);
            consume(TOKEN_RBRACK, "Expected ']'");
            expr = new_ast_index(expr, index);
            continue;
        }

        if (match(TOKEN_DOT)) {
            if (tok->type != TOKEN_IDENT) {
                error_at("input", tok->line, tok->col, "Expected member name identifier");
            }
            char *member_name = tok->value;
            consume(TOKEN_IDENT, "Expected member name");
            expr = new_ast_member(expr, member_name);
            continue;
        }

        if (match(TOKEN_ARROW)) {
            if (tok->type != TOKEN_IDENT) {
                error_at("input", tok->line, tok->col, "Expected member name identifier");
            }
            char *member_name = tok->value;
            consume(TOKEN_IDENT, "Expected member name");
            expr = new_ast_member(new_ast_deref(expr), member_name);
            continue;
        }

        break;
    }

    return expr;
}

static ASTNode *parse_unary(void) {
    if (match(TOKEN_MINUS)) {
        return new_ast_unary(OP_NEG, parse_unary());
    }
    if (match(TOKEN_TILDE)) {
        return new_ast_unary(OP_NOT, parse_unary());
    }
    if (match(TOKEN_BANG)) {
        return new_ast_unary(OP_LNOT, parse_unary());
    }
    if (match(TOKEN_AMP)) {
        return new_ast_addr(parse_unary());
    }
    if (match(TOKEN_STAR)) {
        return new_ast_deref(parse_unary());
    }
    return parse_postfix();
}

static ASTNode *parse_expression(int min_prec) {
    ASTNode *left = parse_unary();

    while (1) {
        int prec = get_tok_precedence(tok->type);
        if (prec < min_prec) {
            break;
        }

        TokenType op_type = tok->type;
        BinaryOp op = get_bin_op(op_type);
        tok = tok->next; // consume operator

        ASTNode *right = parse_expression(prec + 1);
        left = new_ast_binary(op, left, right);
    }

    if (min_prec == 0 && match(TOKEN_ASSIGN)) {
        ASTNode *right = parse_expression(0);
        left = new_ast_assign(left, right);
    }

    return left;
}

static Type *parse_type_specifier(void) {
    Type *t = NULL;
    if (match(TOKEN_INT)) {
        t = new_type_int();
    } else if (match(TOKEN_BOOL)) {
        t = new_type_bool();
    } else if (match(TOKEN_STRUCT)) {
        if (tok->type != TOKEN_IDENT) {
            error_at("input", tok->line, tok->col, "Expected struct tag identifier");
        }
        char *tag = tok->value;
        consume(TOKEN_IDENT, "Expected struct tag");

        t = find_struct_tag(tag);
        if (!t) {
            error("Undefined struct tag: %s", tag);
        }
        t = copy_type(t);
    } else {
        error_at("input", tok->line, tok->col, "Expected type specifier");
    }

    while (match(TOKEN_STAR)) {
        t = new_type_ptr(t);
    }

    return t;
}

static Type *parse_declarator_suffixes(Type *base_type) {
    if (match(TOKEN_LBRACK)) {
        if (tok->type != TOKEN_NUM) {
            error_at("input", tok->line, tok->col, "Expected constant array size");
        }
        int size = atoi(tok->value);
        consume(TOKEN_NUM, "Expected constant number");
        consume(TOKEN_RBRACK, "Expected ']'");

        Type *inner = parse_declarator_suffixes(base_type);
        return new_type_array(inner, size);
    }
    return base_type;
}

static ASTNode *parse_statement(void) {
    if (match(TOKEN_RETURN)) {
        ASTNode *expr = parse_expression(0);
        consume(TOKEN_SEMI, "Expected ';' after return statement");
        return new_ast_return(expr);
    }

    if (match(TOKEN_IF)) {
        consume(TOKEN_LPAREN, "Expected '(' after if");
        ASTNode *cond = parse_expression(0);
        consume(TOKEN_RPAREN, "Expected ')' after condition");

        ASTNode *then_branch = parse_statement();
        ASTNode *else_branch = NULL;
        if (match(TOKEN_ELSE)) {
            else_branch = parse_statement();
        }
        return new_ast_if(cond, then_branch, else_branch);
    }

    if (match(TOKEN_WHILE)) {
        consume(TOKEN_LPAREN, "Expected '(' after while");
        ASTNode *cond = parse_expression(0);
        consume(TOKEN_RPAREN, "Expected ')' after condition");

        ASTNode *body = parse_statement();
        return new_ast_while(cond, body);
    }

    if (match(TOKEN_DO)) {
        ASTNode *body = parse_statement();
        consume(TOKEN_WHILE, "Expected 'while' after do body");
        consume(TOKEN_LPAREN, "Expected '(' after while");
        ASTNode *cond = parse_expression(0);
        consume(TOKEN_RPAREN, "Expected ')' after condition");
        consume(TOKEN_SEMI, "Expected ';' after do-while statement");
        return new_ast_do_while(cond, body);
    }

    if (match(TOKEN_FOR)) {
        consume(TOKEN_LPAREN, "Expected '(' after for");

        ASTNode *init = NULL;
        if (tok->type == TOKEN_INT || tok->type == TOKEN_BOOL || tok->type == TOKEN_STRUCT) {
            Type *base_type = parse_type_specifier();
            if (tok->type != TOKEN_IDENT) {
                error_at("input", tok->line, tok->col, "Expected variable identifier");
            }
            char *name = tok->value;
            consume(TOKEN_IDENT, "Expected identifier");
            Type *full_type = parse_declarator_suffixes(base_type);
            ASTNode *init_val = NULL;
            if (match(TOKEN_ASSIGN)) {
                init_val = parse_expression(0);
            }
            init = new_ast_decl(full_type, name, init_val);
            consume(TOKEN_SEMI, "Expected ';' after for init declaration");
        } else {
            if (tok->type != TOKEN_SEMI) {
                init = parse_expression(0);
            }
            consume(TOKEN_SEMI, "Expected ';' after for init expression");
        }

        ASTNode *cond = NULL;
        if (tok->type != TOKEN_SEMI) {
            cond = parse_expression(0);
        }
        consume(TOKEN_SEMI, "Expected ';' after for condition");

        ASTNode *post = NULL;
        if (tok->type != TOKEN_RPAREN) {
            post = parse_expression(0);
        }
        consume(TOKEN_RPAREN, "Expected ')' after for header");

        ASTNode *body = parse_statement();
        return new_ast_for(init, cond, post, body);
    }

    if (match(TOKEN_LBRACE)) {
        ASTNode **stmts = NULL;
        int stmt_count = 0;
        while (tok && tok->type != TOKEN_RBRACE && tok->type != TOKEN_EOF) {
            ASTNode *stmt = parse_statement();
            stmts = realloc(stmts, sizeof(ASTNode*) * (stmt_count + 1));
            stmts[stmt_count++] = stmt;
        }
        consume(TOKEN_RBRACE, "Expected '}' to end block");
        return new_ast_block(stmts, stmt_count);
    }

    if (match(TOKEN_BREAK)) {
        consume(TOKEN_SEMI, "Expected ';' after break");
        return new_ast_break();
    }

    if (match(TOKEN_CONTINUE)) {
        consume(TOKEN_SEMI, "Expected ';' after continue");
        return new_ast_continue();
    }

    // Struct definition at statement level: struct Tag { members... };
    if (tok->type == TOKEN_STRUCT && tok->next->type == TOKEN_IDENT && tok->next->next->type == TOKEN_LBRACE) {
        consume(TOKEN_STRUCT, "Expected struct");
        char *tag = tok->value;
        consume(TOKEN_IDENT, "Expected tag");
        consume(TOKEN_LBRACE, "Expected '{'");

        Field *fields = NULL;
        int field_count = 0;
        while (tok->type != TOKEN_RBRACE) {
            Type *base_type = parse_type_specifier();
            if (tok->type != TOKEN_IDENT) {
                error_at("input", tok->line, tok->col, "Expected field name identifier");
            }
            char *name = tok->value;
            consume(TOKEN_IDENT, "Expected field name");

            Type *full_type = parse_declarator_suffixes(base_type);
            consume(TOKEN_SEMI, "Expected ';' after field declaration");

            fields = realloc(fields, sizeof(Field) * (field_count + 1));
            fields[field_count].name = xstrdup(name);
            fields[field_count].type = full_type;
            fields[field_count].offset = 0;
            field_count++;
        }
        consume(TOKEN_RBRACE, "Expected '}'");
        consume(TOKEN_SEMI, "Expected ';' after struct definition");

        Type *struct_type = new_type_struct(tag, fields, field_count);
        type_size(struct_type); // Calculate offsets
        add_struct_tag(tag, struct_type);

        return new_ast_struct_def(tag, fields, field_count);
    }

    // Variable declaration? e.g. int x; struct Point p; int arr[10];
    if (tok->type == TOKEN_INT || tok->type == TOKEN_BOOL || tok->type == TOKEN_STRUCT) {
        Type *base_type = parse_type_specifier();
        if (tok->type != TOKEN_IDENT) {
            error_at("input", tok->line, tok->col, "Expected variable name identifier");
        }
        char *name = tok->value;
        consume(TOKEN_IDENT, "Expected identifier");

        Type *full_type = parse_declarator_suffixes(base_type);

        ASTNode *init = NULL;
        if (match(TOKEN_ASSIGN)) {
            init = parse_expression(0);
        }
        consume(TOKEN_SEMI, "Expected ';' after variable declaration");
        return new_ast_decl(full_type, name, init);
    }

    // Expression statement
    ASTNode *expr = parse_expression(0);
    consume(TOKEN_SEMI, "Expected ';' after expression statement");
    return new_ast_expr_stmt(expr);
}

static ASTNode *parse_function(void) {
    consume(TOKEN_INT, "Expected 'int' return type");

    if (tok->type != TOKEN_IDENT) {
        error_at("input", tok->line, tok->col, "Expected function name identifier");
    }
    char *func_name = tok->value;
    consume(TOKEN_IDENT, "Expected function name");

    consume(TOKEN_LPAREN, "Expected '(' after function name");

    char **params = NULL;
    Type **param_types = NULL;
    int param_count = 0;

    if (tok->type != TOKEN_RPAREN) {
        while (1) {
            Type *base_type = parse_type_specifier();
            if (tok->type != TOKEN_IDENT) {
                error_at("input", tok->line, tok->col, "Expected parameter name identifier");
            }
            char *p_name = tok->value;
            consume(TOKEN_IDENT, "Expected identifier");

            Type *full_type = parse_declarator_suffixes(base_type);

            params = realloc(params, sizeof(char*) * (param_count + 1));
            param_types = realloc(param_types, sizeof(Type*) * (param_count + 1));
            params[param_count] = xstrdup(p_name);
            param_types[param_count] = full_type;
            param_count++;

            if (!match(TOKEN_COMMA)) {
                break;
            }
        }
    }
    consume(TOKEN_RPAREN, "Expected ')' after parameters");
    consume(TOKEN_LBRACE, "Expected '{' to start function body");

    ASTNode **stmts = NULL;
    int stmt_count = 0;

    while (tok && tok->type != TOKEN_RBRACE && tok->type != TOKEN_EOF) {
        ASTNode *stmt = parse_statement();
        stmts = realloc(stmts, sizeof(ASTNode*) * (stmt_count + 1));
        if (!stmts) {
            error("Out of memory during statement parsing");
        }
        stmts[stmt_count++] = stmt;
    }

    consume(TOKEN_RBRACE, "Expected '}' to end function body");

    return new_ast_func(func_name, params, param_types, param_count, stmts, stmt_count);
}

ASTNode *parse(Token *tokens) {
    tok = tokens;

    ASTNode **decls = NULL;
    int decl_count = 0;

    while (tok && tok->type != TOKEN_EOF) {
        if (tok->type == TOKEN_STRUCT && tok->next && tok->next->type == TOKEN_IDENT && tok->next->next && tok->next->next->type == TOKEN_LBRACE) {
            consume(TOKEN_STRUCT, "Expected struct");
            char *tag = tok->value;
            consume(TOKEN_IDENT, "Expected tag");
            consume(TOKEN_LBRACE, "Expected '{'");

            Field *fields = NULL;
            int field_count = 0;
            while (tok->type != TOKEN_RBRACE) {
                Type *base_type = parse_type_specifier();
                if (tok->type != TOKEN_IDENT) {
                    error_at("input", tok->line, tok->col, "Expected field name");
                }
                char *name = tok->value;
                consume(TOKEN_IDENT, "Expected field name");

                Type *full_type = parse_declarator_suffixes(base_type);
                consume(TOKEN_SEMI, "Expected ';' after field declaration");

                fields = realloc(fields, sizeof(Field) * (field_count + 1));
                fields[field_count].name = xstrdup(name);
                fields[field_count].type = full_type;
                fields[field_count].offset = 0;
                field_count++;
            }
            consume(TOKEN_RBRACE, "Expected '}'");
            consume(TOKEN_SEMI, "Expected ';' after struct definition");

            Type *struct_type = new_type_struct(tag, fields, field_count);
            type_size(struct_type);
            add_struct_tag(tag, struct_type);

            ASTNode *def = new_ast_struct_def(tag, fields, field_count);
            decls = realloc(decls, sizeof(ASTNode*) * (decl_count + 1));
            decls[decl_count++] = def;
        } else {
            ASTNode *func = parse_function();
            decls = realloc(decls, sizeof(ASTNode*) * (decl_count + 1));
            if (!decls) {
                error("Out of memory during parsing");
            }
            decls[decl_count++] = func;
        }
    }

    return new_ast_program(decls, decl_count);
}
