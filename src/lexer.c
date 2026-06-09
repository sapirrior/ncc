#include "lexer.h"

static Token *new_token(TokenType type, const char *value, int line, int col) {
    Token *tok = xmalloc(sizeof(Token));
    tok->type = type;
    tok->value = value ? xstrdup(value) : NULL;
    tok->line = line;
    tok->col = col;
    tok->next = NULL;
    return tok;
}

Token *tokenize(const char *filename, const char *source) {
    Token head = {0};
    Token *cur = &head;

    int line = 1;
    int col = 1;
    const char *p = source;

    while (*p) {
        // Skip whitespace
        if (isspace((unsigned char)*p)) {
            if (*p == '\n') {
                line++;
                col = 1;
            } else {
                col++;
            }
            p++;
            continue;
        }

        // Skip C99 single-line comments
        if (p[0] == '/' && p[1] == '/') {
            p += 2;
            col += 2;
            while (*p && *p != '\n') {
                p++;
                col++;
            }
            continue;
        }

        // Parentheses and Braces
        if (*p == '(') {
            cur->next = new_token(TOKEN_LPAREN, NULL, line, col);
            cur = cur->next;
            p++; col++;
            continue;
        }
        if (*p == ')') {
            cur->next = new_token(TOKEN_RPAREN, NULL, line, col);
            cur = cur->next;
            p++; col++;
            continue;
        }
        if (*p == '{') {
            cur->next = new_token(TOKEN_LBRACE, NULL, line, col);
            cur = cur->next;
            p++; col++;
            continue;
        }
        if (*p == '}') {
            cur->next = new_token(TOKEN_RBRACE, NULL, line, col);
            cur = cur->next;
            p++; col++;
            continue;
        }
        if (*p == ';') {
            cur->next = new_token(TOKEN_SEMI, NULL, line, col);
            cur = cur->next;
            p++; col++;
            continue;
        }

        // Multi-character Operators
        if (p[0] == '=' && p[1] == '=') {
            cur->next = new_token(TOKEN_EQ, NULL, line, col);
            cur = cur->next;
            p += 2; col += 2;
            continue;
        }
        if (p[0] == '!' && p[1] == '=') {
            cur->next = new_token(TOKEN_NE, NULL, line, col);
            cur = cur->next;
            p += 2; col += 2;
            continue;
        }
        if (p[0] == '<' && p[1] == '=') {
            cur->next = new_token(TOKEN_LE, NULL, line, col);
            cur = cur->next;
            p += 2; col += 2;
            continue;
        }
        if (p[0] == '>' && p[1] == '=') {
            cur->next = new_token(TOKEN_GE, NULL, line, col);
            cur = cur->next;
            p += 2; col += 2;
            continue;
        }

        // Single-character Operators
        if (*p == '=') {
            cur->next = new_token(TOKEN_ASSIGN, NULL, line, col);
            cur = cur->next;
            p++; col++;
            continue;
        }
        if (*p == '!') {
            cur->next = new_token(TOKEN_BANG, NULL, line, col);
            cur = cur->next;
            p++; col++;
            continue;
        }
        if (*p == '<') {
            cur->next = new_token(TOKEN_LT, NULL, line, col);
            cur = cur->next;
            p++; col++;
            continue;
        }
        if (*p == '>') {
            cur->next = new_token(TOKEN_GT, NULL, line, col);
            cur = cur->next;
            p++; col++;
            continue;
        }
        if (*p == '+') {
            cur->next = new_token(TOKEN_PLUS, NULL, line, col);
            cur = cur->next;
            p++; col++;
            continue;
        }
        if (*p == '-') {
            cur->next = new_token(TOKEN_MINUS, NULL, line, col);
            cur = cur->next;
            p++; col++;
            continue;
        }
        if (*p == '*') {
            cur->next = new_token(TOKEN_STAR, NULL, line, col);
            cur = cur->next;
            p++; col++;
            continue;
        }
        if (*p == '/') {
            cur->next = new_token(TOKEN_SLASH, NULL, line, col);
            cur = cur->next;
            p++; col++;
            continue;
        }
        if (*p == '~') {
            cur->next = new_token(TOKEN_TILDE, NULL, line, col);
            cur = cur->next;
            p++; col++;
            continue;
        }

        // Numeric literal
        if (isdigit((unsigned char)*p)) {
            const char *start = p;
            int start_col = col;
            while (isdigit((unsigned char)*p)) {
                p++; col++;
            }
            int len = p - start;
            char *val = xmalloc(len + 1);
            strncpy(val, start, len);
            val[len] = '\0';
            cur->next = new_token(TOKEN_NUM, val, line, start_col);
            free(val);
            cur = cur->next;
            continue;
        }

        // Identifier or Keyword
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p;
            int start_col = col;
            while (isalnum((unsigned char)*p) || *p == '_') {
                p++; col++;
            }
            int len = p - start;
            char *val = xmalloc(len + 1);
            strncpy(val, start, len);
            val[len] = '\0';

            TokenType type = TOKEN_IDENT;
            if (strcmp(val, "int") == 0) {
                type = TOKEN_INT;
            } else if (strcmp(val, "_Bool") == 0) {
                type = TOKEN_BOOL;
            } else if (strcmp(val, "return") == 0) {
                type = TOKEN_RETURN;
            }

            cur->next = new_token(type, type == TOKEN_IDENT ? val : NULL, line, start_col);
            free(val);
            cur = cur->next;
            continue;
        }

        error_at(filename, line, col, "invalid character: '%c'", *p);
    }

    cur->next = new_token(TOKEN_EOF, NULL, line, col);
    return head.next;
}

void print_tokens(Token *tok) {
    while (tok) {
        printf("[%d:%d] ", tok->line, tok->col);
        switch (tok->type) {
            case TOKEN_EOF: printf("EOF\n"); break;
            case TOKEN_INT: printf("KEYWORD: int\n"); break;
            case TOKEN_BOOL: printf("KEYWORD: _Bool\n"); break;
            case TOKEN_RETURN: printf("KEYWORD: return\n"); break;
            case TOKEN_IDENT: printf("IDENTIFIER: %s\n", tok->value); break;
            case TOKEN_NUM: printf("NUMBER: %s\n", tok->value); break;
            case TOKEN_LPAREN: printf("LPAREN: (\n"); break;
            case TOKEN_RPAREN: printf("RPAREN: )\n"); break;
            case TOKEN_LBRACE: printf("LBRACE: {\n"); break;
            case TOKEN_RBRACE: printf("RBRACE: }\n"); break;
            case TOKEN_SEMI: printf("SEMI: ;\n"); break;
            case TOKEN_PLUS: printf("PLUS: +\n"); break;
            case TOKEN_MINUS: printf("MINUS: -\n"); break;
            case TOKEN_STAR: printf("STAR: *\n"); break;
            case TOKEN_SLASH: printf("SLASH: /\n"); break;
            case TOKEN_TILDE: printf("TILDE: ~\n"); break;
            case TOKEN_BANG: printf("BANG: !\n"); break;
            case TOKEN_ASSIGN: printf("ASSIGN: =\n"); break;
            case TOKEN_EQ: printf("EQ: ==\n"); break;
            case TOKEN_NE: printf("NE: !=\n"); break;
            case TOKEN_LT: printf("LT: <\n"); break;
            case TOKEN_LE: printf("LE: <=\n"); break;
            case TOKEN_GT: printf("GT: >\n"); break;
            case TOKEN_GE: printf("GE: >=\n"); break;
        }
        tok = tok->next;
    }
}

void free_tokens(Token *tok) {
    while (tok) {
        Token *next = tok->next;
        if (tok->value) {
            free(tok->value);
        }
        free(tok);
        tok = next;
    }
}
