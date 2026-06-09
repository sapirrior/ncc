#ifndef LEXER_H
#define LEXER_H

#include "common.h"

typedef enum {
    TOKEN_EOF = 0,
    TOKEN_INT,       // 'int' keyword
    TOKEN_BOOL,      // '_Bool' keyword
    TOKEN_RETURN,    // 'return' keyword
    TOKEN_IF,        // 'if' keyword
    TOKEN_ELSE,      // 'else' keyword
    TOKEN_WHILE,     // 'while' keyword
    TOKEN_DO,        // 'do' keyword
    TOKEN_FOR,       // 'for' keyword
    TOKEN_BREAK,     // 'break' keyword
    TOKEN_CONTINUE,  // 'continue' keyword
    TOKEN_IDENT,     // Identifier
    TOKEN_NUM,       // Number literal
    TOKEN_LPAREN,    // '('
    TOKEN_RPAREN,    // ')'
    TOKEN_LBRACE,    // '{'
    TOKEN_RBRACE,    // '}'
    TOKEN_SEMI,      // ';'
    TOKEN_COMMA,     // ','

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
    char *value;     // Raw string value (for identifiers/literals)
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
