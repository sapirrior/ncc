#include "parser.h"

// AST helpers
ASTNode *new_ast_program(ASTNode **decls, int decl_count) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_PROGRAM;
    node->program.decls = decls;
    node->program.decl_count = decl_count;
    return node;
}

ASTNode *new_ast_func(const char *name, ASTNode *body) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_FUNC_DECL;
    node->func.name = xstrdup(name);
    node->func.body = body;
    return node;
}

ASTNode *new_ast_return(ASTNode *expr) {
    ASTNode *node = xmalloc(sizeof(ASTNode));
    node->type = AST_STMT_RETURN;
    node->ret_stmt.expr = expr;
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
            free_ast(node->func.body);
            break;
        case AST_STMT_RETURN:
            free_ast(node->ret_stmt.expr);
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
            print_ast(node->func.body, indent + 1);
            break;
        case AST_STMT_RETURN:
            printf("ReturnStmt:\n");
            print_ast(node->ret_stmt.expr, indent + 1);
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
        error_at("input", tok->line, tok->col, "%s", err_msg);
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

static ASTNode *parse_expression(void) {
    if (tok->type == TOKEN_NUM) {
        int val = atoi(tok->value);
        consume(TOKEN_NUM, "Expected number literal");
        return new_ast_num(val);
    }
    error_at("input", tok->line, tok->col, "Expected expression");
    return NULL;
}

static ASTNode *parse_statement(void) {
    if (match(TOKEN_RETURN)) {
        ASTNode *expr = parse_expression();
        consume(TOKEN_SEMI, "Expected ';' after return statement");
        return new_ast_return(expr);
    }
    error_at("input", tok->line, tok->col, "Expected statement");
    return NULL;
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

    ASTNode *body = parse_statement();

    consume(TOKEN_RBRACE, "Expected '}' to end function body");

    return new_ast_func(func_name, body);
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
