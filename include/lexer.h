#ifndef LEXER_H
#define LEXER_H

#include "common.h"

typedef enum {
    TOKEN_EOF = 0,

    // NeoC Keywords (The 32 Budget)
    TOKEN_I32, TOKEN_I64, TOKEN_F32, TOKEN_F64, TOKEN_BOOL_TYPE, TOKEN_PTR_TYPE, TOKEN_VOID,
    TOKEN_LET, TOKEN_MUT, TOKEN_CONST, TOKEN_FN, TOKEN_STRUCT, TOKEN_ENUM, TOKEN_UNION,
    TOKEN_IF, TOKEN_ELSE, TOKEN_WHILE, TOKEN_FOR, TOKEN_IN, TOKEN_RETURN, TOKEN_BREAK, TOKEN_CONTINUE, TOKEN_SWITCH, TOKEN_CASE,
    TOKEN_ALLOC, TOKEN_FREE, TOKEN_DEFER, TOKEN_SIZEOF,
    TOKEN_IMPORT, TOKEN_PUB, TOKEN_ASM, TOKEN_EXTERN,

    // Literals and Identifiers
    TOKEN_IDENT,
    TOKEN_NUM,       // Integer literal
    TOKEN_FLOAT_LIT, // Float literal
    TOKEN_STRING,    // String literal
    TOKEN_NULL,      // 'null'
    TOKEN_NULLPTR,   // 'nullptr'

    // Delimiters and Punctuations
    TOKEN_LPAREN,    // '('
    TOKEN_RPAREN,    // ')'
    TOKEN_LBRACE,    // '{'
    TOKEN_RBRACE,    // '}'
    TOKEN_LBRACK,    // '['
    TOKEN_RBRACK,    // ']'
    TOKEN_SEMI,      // ';'
    TOKEN_COMMA,     // ','
    TOKEN_DOT,       // '.'
    TOKEN_COLON,     // ':'
    TOKEN_AMP,       // '&'
    TOKEN_ARROW,     // '->'

    // Operators
    TOKEN_PLUS,      // '+'
    TOKEN_MINUS,     // '-'
    TOKEN_STAR,      // '*'
    TOKEN_SLASH,     // '/'
    TOKEN_TILDE,     // '~'
    TOKEN_BANG,      // '!'
    TOKEN_ASSIGN,    // '='
    TOKEN_EQ,        // '=='
    TOKEN_NE,        // '!='
    TOKEN_LT,        // '<'
    TOKEN_LE,        // '<='
    TOKEN_GT,        // '>'
    TOKEN_GE,        // '>='
} TokenType;

typedef struct Token {
    TokenType type;
    char *value;     // Raw string value
    int line;
    int col;
    struct Token *next;
} Token;

// Lexer function interface
Token *tokenize(const char *filename, const char *source);

// Helper to print token list (for debugging)
void print_tokens(Token *tok);

// Helper to free token list memory
void free_tokens(Token *tok);

#endif // LEXER_H
