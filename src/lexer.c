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

        // Single character tokens
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
            case TOKEN_RETURN: printf("KEYWORD: return\n"); break;
            case TOKEN_IDENT: printf("IDENTIFIER: %s\n", tok->value); break;
            case TOKEN_NUM: printf("NUMBER: %s\n", tok->value); break;
            case TOKEN_LPAREN: printf("LPAREN: (\n"); break;
            case TOKEN_RPAREN: printf("RPAREN: )\n"); break;
            case TOKEN_LBRACE: printf("LBRACE: {\n"); break;
            case TOKEN_RBRACE: printf("RBRACE: }\n"); break;
            case TOKEN_SEMI: printf("SEMI: ;\n"); break;
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
