#include "parser.h"

// AST helpers
ASTNode *new_ast_program(ASTNode **decls, int decl_count) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_PROGRAM;
    node->program.decls = decls;
    node->program.decl_count = decl_count;
    return node;
}

ASTNode *new_ast_func(const char *name, ASTNode **stmts, int stmt_count) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_FUNC_DECL;
    node->func.name = xstrdup(name);
    node->func.stmts = stmts;
    node->func.stmt_count = stmt_count;
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

ASTNode *new_ast_decl(VarType var_type, const char *name, ASTNode *init) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_STMT_DECL;
    node->decl_stmt.var_type = var_type;
    node->decl_stmt.name = xstrdup(name);
    node->decl_stmt.init = init;
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

ASTNode *new_ast_assign(const char *name, ASTNode *expr) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_EXPR_ASSIGN;
    node->assign.name = xstrdup(name);
    node->assign.expr = expr;
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
            for (int i = 0; i < node->func.stmt_count; i++) {
                free_ast(node->func.stmts[i]);
            }
            free(node->func.stmts);
            break;
        case AST_STMT_RETURN:
            free_ast(node->ret_stmt.expr);
            break;
        case AST_STMT_EXPR:
            free_ast(node->expr_stmt.expr);
            break;
        case AST_STMT_DECL:
            free(node->decl_stmt.name);
            if (node->decl_stmt.init) {
                free_ast(node->decl_stmt.init);
            }
            break;
        case AST_EXPR_BINARY:
            free_ast(node->binary.left);
            free_ast(node->binary.right);
            break;
        case AST_EXPR_UNARY:
            free_ast(node->unary.expr);
            break;
        case AST_EXPR_ASSIGN:
            free(node->assign.name);
            free_ast(node->assign.expr);
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
            printf("FuncDecl: %s\n", node->func.name);
            for (int i = 0; i < node->func.stmt_count; i++) {
                print_ast(node->func.stmts[i], indent + 1);
            }
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
            printf("DeclStmt: %s %s\n",
                node->decl_stmt.var_type == TYPE_INT ? "int" : "_Bool",
                node->decl_stmt.name);
            if (node->decl_stmt.init) {
                print_ast(node->decl_stmt.init, indent + 1);
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
            printf("AssignExpr: %s =\n", node->assign.name);
            print_ast(node->assign.expr, indent + 1);
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

static ASTNode *parse_primary(void) {
    if (tok->type == TOKEN_NUM) {
        int val = atoi(tok->value);
        consume(TOKEN_NUM, "Expected number");
        return new_ast_num(val);
    }

    if (tok->type == TOKEN_IDENT) {
        char *name = tok->value;
        consume(TOKEN_IDENT, "Expected identifier");

        // Assignment expression?
        if (match(TOKEN_ASSIGN)) {
            ASTNode *expr = parse_expression(0);
            return new_ast_assign(name, expr);
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
    return parse_primary();
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

    return left;
}

static ASTNode *parse_statement(void) {
    if (match(TOKEN_RETURN)) {
        ASTNode *expr = parse_expression(0);
        consume(TOKEN_SEMI, "Expected ';' after return statement");
        return new_ast_return(expr);
    }

    if (tok->type == TOKEN_INT || tok->type == TOKEN_BOOL) {
        VarType var_type = (tok->type == TOKEN_INT) ? TYPE_INT : TYPE_BOOL;
        tok = tok->next; // consume keyword

        if (tok->type != TOKEN_IDENT) {
            error_at("input", tok->line, tok->col, "Expected variable name identifier");
        }
        char *name = tok->value;
        consume(TOKEN_IDENT, "Expected identifier");

        ASTNode *init = NULL;
        if (match(TOKEN_ASSIGN)) {
            init = parse_expression(0);
        }
        consume(TOKEN_SEMI, "Expected ';' after variable declaration");
        return new_ast_decl(var_type, name, init);
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

    return new_ast_func(func_name, stmts, stmt_count);
}

ASTNode *parse(Token *tokens) {
    tok = tokens;

    ASTNode **decls = NULL;
    int decl_count = 0;

    while (tok && tok->type != TOKEN_EOF) {
        ASTNode *func = parse_function();
        decls = realloc(decls, sizeof(ASTNode*) * (decl_count + 1));
        if (!decls) {
            error("Out of memory during parsing");
        }
        decls[decl_count++] = func;
    }

    return new_ast_program(decls, decl_count);
}
