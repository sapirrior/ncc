#ifndef AST_H
#define AST_H

#include "common.h"

typedef enum {
    AST_PROGRAM,
    AST_FUNC_DECL,
    AST_STMT_RETURN,
    AST_STMT_EXPR,
    AST_STMT_DECL,
    AST_STMT_IF,
    AST_STMT_WHILE,
    AST_STMT_DO_WHILE,
    AST_STMT_FOR,
    AST_STMT_BLOCK,
    AST_STMT_BREAK,
    AST_STMT_CONTINUE,
    AST_EXPR_BINARY,
    AST_EXPR_UNARY,
    AST_EXPR_ASSIGN,
    AST_EXPR_VAR,
    AST_EXPR_NUM,
    AST_EXPR_CALL,
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
            char **params;
            VarType *param_types;
            int param_count;
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

        // If statement
        struct {
            struct ASTNode *cond;
            struct ASTNode *then_branch;
            struct ASTNode *else_branch; // optional
        } if_stmt;

        // While / Do-While statement
        struct {
            struct ASTNode *cond;
            struct ASTNode *body;
        } while_stmt;

        // For statement
        struct {
            struct ASTNode *init; // optional statement or decl
            struct ASTNode *cond; // optional expr
            struct ASTNode *post; // optional expr
            struct ASTNode *body;
        } for_stmt;

        // Block statement
        struct {
            struct ASTNode **stmts;
            int stmt_count;
        } block;

        // Function call expression
        struct {
            char *name;
            struct ASTNode **args;
            int arg_count;
        } call;

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
ASTNode *new_ast_func(const char *name, char **params, VarType *param_types, int param_count, ASTNode **stmts, int stmt_count);
ASTNode *new_ast_return(ASTNode *expr);
ASTNode *new_ast_expr_stmt(ASTNode *expr);
ASTNode *new_ast_decl(VarType var_type, const char *name, ASTNode *init);
ASTNode *new_ast_if(ASTNode *cond, ASTNode *then_branch, ASTNode *else_branch);
ASTNode *new_ast_while(ASTNode *cond, ASTNode *body);
ASTNode *new_ast_do_while(ASTNode *cond, ASTNode *body);
ASTNode *new_ast_for(ASTNode *init, ASTNode *cond, ASTNode *post, ASTNode *body);
ASTNode *new_ast_block(ASTNode **stmts, int stmt_count);
ASTNode *new_ast_break(void);
ASTNode *new_ast_continue(void);
ASTNode *new_ast_call(const char *name, ASTNode **args, int arg_count);
ASTNode *new_ast_binary(BinaryOp op, ASTNode *left, ASTNode *right);
ASTNode *new_ast_unary(UnaryOp op, ASTNode *expr);
ASTNode *new_ast_assign(const char *name, ASTNode *expr);
ASTNode *new_ast_var(const char *name);
ASTNode *new_ast_num(int value);

void free_ast(ASTNode *node);
void print_ast(ASTNode *node, int indent);

#endif // AST_H
