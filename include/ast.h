#ifndef AST_H
#define AST_H

#include "common.h"

typedef enum {
    AST_PROGRAM,
    AST_FUNC_DECL,
    AST_STMT_RETURN,
    AST_STMT_EXPR,
    AST_STMT_DECL,
    AST_EXPR_BINARY,
    AST_EXPR_UNARY,
    AST_EXPR_ASSIGN,
    AST_EXPR_VAR,
    AST_EXPR_NUM,
} ASTNodeType;

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE
} BinaryOp;

typedef enum {
    OP_NEG, OP_NOT, OP_LNOT // negation (-), bitwise not (~), logical not (!)
} UnaryOp;

typedef enum {
    TYPE_INT,
    TYPE_BOOL
} VarType;

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
            struct ASTNode **stmts;
            int stmt_count;
        } func;

        // Return statement
        struct {
            struct ASTNode *expr;
        } ret_stmt;

        // Expression statement
        struct {
            struct ASTNode *expr;
        } expr_stmt;

        // Variable declaration
        struct {
            VarType var_type;
            char *name;
            struct ASTNode *init; // Initializer expression (optional)
        } decl_stmt;

        // Binary expression
        struct {
            BinaryOp op;
            struct ASTNode *left;
            struct ASTNode *right;
        } binary;

        // Unary expression
        struct {
            UnaryOp op;
            struct ASTNode *expr;
        } unary;

        // Assignment expression
        struct {
            char *name;
            struct ASTNode *expr;
        } assign;

        // Variable reference
        struct {
            char *name;
        } var;

        // Number literal
        struct {
            int value;
        } num;
    };
} ASTNode;

// Helper function prototypes for AST creation and printing
ASTNode *new_ast_program(ASTNode **decls, int decl_count);
ASTNode *new_ast_func(const char *name, ASTNode **stmts, int stmt_count);
ASTNode *new_ast_return(ASTNode *expr);
ASTNode *new_ast_expr_stmt(ASTNode *expr);
ASTNode *new_ast_decl(VarType var_type, const char *name, ASTNode *init);
ASTNode *new_ast_binary(BinaryOp op, ASTNode *left, ASTNode *right);
ASTNode *new_ast_unary(UnaryOp op, ASTNode *expr);
ASTNode *new_ast_assign(const char *name, ASTNode *expr);
ASTNode *new_ast_var(const char *name);
ASTNode *new_ast_num(int value);

void free_ast(ASTNode *node);
void print_ast(ASTNode *node, int indent);

#endif // AST_H
