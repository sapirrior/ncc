#ifndef AST_H
#define AST_H

#include "common.h"

typedef enum {
    KIND_VOID,
    KIND_I32,
    KIND_I64,
    KIND_F32,
    KIND_F64,
    KIND_BOOL,
    KIND_ARRAY,
    KIND_STRUCT,
    KIND_PTR
} TypeKind;

typedef struct Type Type;

typedef struct Field {
    char *name;
    Type *type;
    int offset;
} Field;

struct Type {
    TypeKind kind;
    union {
        // Array
        struct {
            Type *elem_type;
            int size;
        } array;

        // Struct
        struct {
            char *name; // tag name
            Field *fields;
            int field_count;
        } struct_def;

        // Pointer
        Type *ptr_to;
    };
};

typedef enum {
    AST_PROGRAM,
    AST_FUNC_DECL,
    AST_STMT_STRUCT_DEF,
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
    AST_STMT_DEFER,
    AST_STMT_IMPORT,
    AST_EXPR_BINARY,
    AST_EXPR_UNARY,
    AST_EXPR_ASSIGN,
    AST_EXPR_VAR,
    AST_EXPR_NUM,
    AST_EXPR_FLOAT_LIT,
    AST_EXPR_STRING_LIT,
    AST_EXPR_CALL,
    AST_EXPR_MEMBER,
    AST_EXPR_INDEX,
    AST_EXPR_ADDR,
    AST_EXPR_DEREF,
    AST_EXPR_ALLOC,
    AST_EXPR_NULLPTR,
} ASTNodeType;

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE
} BinaryOp;

typedef enum {
    OP_NEG, OP_NOT, OP_LNOT // negation (-), bitwise not (~), logical not (!)
} UnaryOp;

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
            Type **param_types;
            int param_count;
            struct ASTNode **stmts;
            int stmt_count;
            Type *return_type;
        } func;

        // Struct definition statement
        struct {
            char *name; // struct name
            Field *fields;
            int field_count;
        } struct_def;

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
            Type *var_type;
            char *name;
            struct ASTNode *init; // Initializer expression (optional)
            bool is_mut;
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

        // Defer statement
        struct {
            struct ASTNode *stmt;
        } defer_stmt;

        // Import statement
        struct {
            char *path;
        } import_stmt;

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
            struct ASTNode *left;
            struct ASTNode *right;
        } assign;

        // Member access: expr.member
        struct {
            struct ASTNode *expr;
            char *member_name;
        } member;

        // Array indexing: expr[expr]
        struct {
            struct ASTNode *expr;
            struct ASTNode *index;
        } index;

        // Address-of: &expr
        struct {
            struct ASTNode *expr;
        } addr;

        // Dereference: *expr
        struct {
            struct ASTNode *expr;
        } deref;

        // Alloc: alloc(size)
        struct {
            struct ASTNode *size_expr;
        } alloc_expr;

        // Variable reference
        struct {
            char *name;
        } var;

        // Number literal
        struct {
            int value;
        } num;

        // Float literal
        struct {
            double value;
        } float_lit;

        // String literal
        struct {
            char *value;
        } string_lit;
    };
} ASTNode;

// Type helper prototypes
Type *new_type_void(void);
Type *new_type_i32(void);
Type *new_type_i64(void);
Type *new_type_f32(void);
Type *new_type_f64(void);
Type *new_type_bool(void);
Type *new_type_array(Type *elem_type, int size);
Type *new_type_struct(const char *name, Field *fields, int field_count);
Type *new_type_ptr(Type *ptr_to);
int type_size(Type *type);
int type_align(Type *type);
void free_type(Type *type);
Type *copy_type(Type *type);

// Helper function prototypes for AST creation and printing
ASTNode *new_ast_program(ASTNode **decls, int decl_count);
ASTNode *new_ast_func(const char *name, char **params, Type **param_types, int param_count, ASTNode **stmts, int stmt_count, Type *return_type);
ASTNode *new_ast_struct_def(const char *name, Field *fields, int field_count);
ASTNode *new_ast_return(ASTNode *expr);
ASTNode *new_ast_expr_stmt(ASTNode *expr);
ASTNode *new_ast_decl(Type *var_type, const char *name, ASTNode *init, bool is_mut);
ASTNode *new_ast_if(ASTNode *cond, ASTNode *then_branch, ASTNode *else_branch);
ASTNode *new_ast_while(ASTNode *cond, ASTNode *body);
ASTNode *new_ast_do_while(ASTNode *cond, ASTNode *body);
ASTNode *new_ast_for(ASTNode *init, ASTNode *cond, ASTNode *post, ASTNode *body);
ASTNode *new_ast_block(ASTNode **stmts, int stmt_count);
ASTNode *new_ast_defer(ASTNode *stmt);
ASTNode *new_ast_import(const char *path);
ASTNode *new_ast_break(void);
ASTNode *new_ast_continue(void);
ASTNode *new_ast_call(const char *name, ASTNode **args, int arg_count);
ASTNode *new_ast_binary(BinaryOp op, ASTNode *left, ASTNode *right);
ASTNode *new_ast_unary(UnaryOp op, ASTNode *expr);
ASTNode *new_ast_assign(ASTNode *left, ASTNode *right);
ASTNode *new_ast_member(ASTNode *expr, const char *member_name);
ASTNode *new_ast_index(ASTNode *expr, ASTNode *index);
ASTNode *new_ast_addr(ASTNode *expr);
ASTNode *new_ast_deref(ASTNode *expr);
ASTNode *new_ast_alloc(ASTNode *size_expr);
ASTNode *new_ast_nullptr(void);
ASTNode *new_ast_var(const char *name);
ASTNode *new_ast_num(int value);
ASTNode *new_ast_float_lit(double value);
ASTNode *new_ast_string_lit(const char *value);

void free_ast(ASTNode *node);
void print_ast(ASTNode *node, int indent);

#endif // AST_H
