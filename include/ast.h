#ifndef AST_H
#define AST_H

#include "common.h"

typedef enum {
    AST_PROGRAM,
    AST_FUNC_DECL,
    AST_STMT_RETURN,
    AST_EXPR_NUM,
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    union {
        // Program: list of top-level declarations
        struct {
            struct ASTNode **decls;
            int decl_count;
        } program;

        // Function declaration
        struct {
            char *name;
            struct ASTNode *body;
        } func;

        // Return statement
        struct {
            struct ASTNode *expr;
        } ret_stmt;

        // Number literal
        struct {
            int value;
        } num;
    };
} ASTNode;

// Helper function prototypes for AST creation and printing
ASTNode *new_ast_program(ASTNode **decls, int decl_count);
ASTNode *new_ast_func(const char *name, ASTNode *body);
ASTNode *new_ast_return(ASTNode *expr);
ASTNode *new_ast_num(int value);

void free_ast(ASTNode *node);
void print_ast(ASTNode *node, int indent);

#endif // AST_H
