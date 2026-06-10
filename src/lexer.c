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

typedef struct Macro {
    char *name;
    char *value;
    bool in_expansion;
    struct Macro *next;
} Macro;

static Macro *macros = NULL;

static void add_macro(const char *name, const char *value) {
    for (Macro *m = macros; m; m = m->next) {
        if (strcmp(m->name, name) == 0) {
            free(m->value);
            m->value = xstrdup(value);
            return;
        }
    }
    Macro *m = xmalloc(sizeof(Macro));
    m->name = xstrdup(name);
    m->value = xstrdup(value);
    m->in_expansion = false;
    m->next = macros;
    macros = m;
}

static Macro *find_macro(const char *name) {
    for (Macro *m = macros; m; m = m->next) {
        if (strcmp(m->name, name) == 0) {
            return m;
        }
    }
    return NULL;
}

typedef struct {
    bool cond;
    bool parent_active;
} CondLevel;

static char *get_directory_path(const char *filepath) {
    const char *last_slash = strrchr(filepath, '/');
    if (!last_slash) {
        return xstrdup("");
    }
    int len = last_slash - filepath;
    char *dir = xmalloc(len + 1);
    strncpy(dir, filepath, len);
    dir[len] = '\0';
    return dir;
}

static char *read_file_or_null(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buf = xmalloc(size + 1);
    size_t read_bytes = fread(buf, 1, size, fp);
    buf[read_bytes] = '\0';
    fclose(fp);
    return buf;
}

typedef struct {
    char *name;
    TokenType type;
} Keyword;

static Keyword keywords[] = {
    {"i32", TOKEN_I32}, {"i64", TOKEN_I64}, {"f32", TOKEN_F32}, {"f64", TOKEN_F64},
    {"bool", TOKEN_BOOL_TYPE}, {"ptr", TOKEN_PTR_TYPE}, {"void", TOKEN_VOID},
    {"let", TOKEN_LET}, {"mut", TOKEN_MUT}, {"const", TOKEN_CONST}, {"fn", TOKEN_FN},
    {"struct", TOKEN_STRUCT}, {"enum", TOKEN_ENUM}, {"union", TOKEN_UNION},
    {"if", TOKEN_IF}, {"else", TOKEN_ELSE}, {"while", TOKEN_WHILE}, {"for", TOKEN_FOR},
    {"in", TOKEN_IN}, {"return", TOKEN_RETURN}, {"break", TOKEN_BREAK}, {"continue", TOKEN_CONTINUE},
    {"switch", TOKEN_SWITCH}, {"case", TOKEN_CASE},
    {"alloc", TOKEN_ALLOC}, {"free", TOKEN_FREE}, {"defer", TOKEN_DEFER}, {"sizeof", TOKEN_SIZEOF},
    {"import", TOKEN_IMPORT}, {"pub", TOKEN_PUB}, {"asm", TOKEN_ASM}, {"extern", TOKEN_EXTERN},
    {"null", TOKEN_NULL}, {"nullptr", TOKEN_NULLPTR},
};

static TokenType find_keyword(const char *name) {
    int count = sizeof(keywords) / sizeof(Keyword);
    for (int i = 0; i < count; i++) {
        if (strcmp(keywords[i].name, name) == 0) {
            return keywords[i].type;
        }
    }
    return TOKEN_IDENT;
}

Token *tokenize(const char *filename, const char *source) {
    Token head = {0};
    Token *cur = &head;

    int line = 1;
    int col = 1;
    const char *p = source;
    bool bol = true;

    CondLevel cond_stack[100];
    int cond_depth = 0;

    #define SHOULD_EMIT() ((cond_depth == 0) || (cond_stack[cond_depth - 1].cond && cond_stack[cond_depth - 1].parent_active))

    while (*p) {
        // Skip whitespace
        if (isspace((unsigned char)*p)) {
            if (*p == '\n') {
                line++;
                col = 1;
                bol = true;
            } else {
                col++;
            }
            p++;
            continue;
        }

        // Skip comments (both // and /* */ could be supported, keeping // for now)
        if (p[0] == '/' && p[1] == '/') {
            p += 2;
            col += 2;
            while (*p && *p != '\n') {
                p++;
                col++;
            }
            continue;
        }

        // Handle preprocessor directive (keeping for hybrid C/NeoC support during transition)
        if (bol && *p == '#') {
            p++; col++; // Consume '#'
            while (*p && (*p == ' ' || *p == '\t')) {
                p++; col++;
            }
            const char *dir_start = p;
            while (*p && isalpha((unsigned char)*p)) {
                p++; col++;
            }
            int dir_len = p - dir_start;
            char *dir = xmalloc(dir_len + 1);
            strncpy(dir, dir_start, dir_len);
            dir[dir_len] = '\0';

            if (strcmp(dir, "ifdef") == 0) {
                while (*p && (*p == ' ' || *p == '\t')) {
                    p++; col++;
                }
                const char *m_start = p;
                while (*p && (isalnum((unsigned char)*p) || *p == '_')) {
                    p++; col++;
                }
                int m_len = p - m_start;
                char *m_name = xmalloc(m_len + 1);
                strncpy(m_name, m_start, m_len);
                m_name[m_len] = '\0';

                bool parent_active = SHOULD_EMIT();
                bool cond = (find_macro(m_name) != NULL);
                cond_stack[cond_depth++] = (CondLevel){ cond, parent_active };
                free(m_name);
            } else if (strcmp(dir, "ifndef") == 0) {
                while (*p && (*p == ' ' || *p == '\t')) {
                    p++; col++;
                }
                const char *m_start = p;
                while (*p && (isalnum((unsigned char)*p) || *p == '_')) {
                    p++; col++;
                }
                int m_len = p - m_start;
                char *m_name = xmalloc(m_len + 1);
                strncpy(m_name, m_start, m_len);
                m_name[m_len] = '\0';

                bool parent_active = SHOULD_EMIT();
                bool cond = (find_macro(m_name) == NULL);
                cond_stack[cond_depth++] = (CondLevel){ cond, parent_active };
                free(m_name);
            } else if (strcmp(dir, "else") == 0) {
                if (cond_depth == 0) {
                    error_at(filename, line, col, "#else without #ifdef/#ifndef");
                }
                cond_stack[cond_depth - 1].cond = !cond_stack[cond_depth - 1].cond;
            } else if (strcmp(dir, "endif") == 0) {
                if (cond_depth == 0) {
                    error_at(filename, line, col, "#endif without #ifdef/#ifndef");
                }
                cond_depth--;
            } else if (SHOULD_EMIT()) {
                if (strcmp(dir, "include") == 0) {
                    while (*p && (*p == ' ' || *p == '\t')) {
                        p++; col++;
                    }
                    if (*p == '"') {
                        p++; col++;
                        const char *fn_start = p;
                        while (*p && *p != '"' && *p != '\n') {
                            p++; col++;
                        }
                        if (*p == '"') {
                            int fn_len = p - fn_start;
                            char *fn = xmalloc(fn_len + 1);
                            strncpy(fn, fn_start, fn_len);
                            fn[fn_len] = '\0';
                            p++; col++;

                            char *dir_path = get_directory_path(filename);
                            char *full_path = xmalloc(strlen(dir_path) + strlen(fn) + 2);
                            if (strlen(dir_path) > 0) {
                                sprintf(full_path, "%s/%s", dir_path, fn);
                            } else {
                                sprintf(full_path, "%s", fn);
                            }
                            free(dir_path);
                            free(fn);

                            char *inc_source = read_file_or_null(full_path);
                            if (!inc_source) {
                                inc_source = read_file_or_null(fn);
                            }
                            if (!inc_source) {
                                error_at(filename, line, col, "Cannot open include file");
                            }

                            Token *inc_tokens = tokenize(full_path, inc_source);
                            free(full_path);
                            free(inc_source);

                            if (inc_tokens) {
                                cur->next = inc_tokens;
                                while (cur->next && cur->next->type != TOKEN_EOF) {
                                    cur = cur->next;
                                }
                                Token *eof_tok = cur->next;
                                if (eof_tok && eof_tok->type == TOKEN_EOF) {
                                    cur->next = NULL;
                                    free_tokens(eof_tok);
                                }
                            }
                        } else {
                            error_at(filename, line, col, "Expected closing quote in #include");
                        }
                    }
                } else if (strcmp(dir, "define") == 0) {
                    while (*p && (*p == ' ' || *p == '\t')) {
                        p++; col++;
                    }
                    const char *m_start = p;
                    while (*p && (isalnum((unsigned char)*p) || *p == '_')) {
                        p++; col++;
                    }
                    int m_len = p - m_start;
                    if (m_len == 0) {
                        error_at(filename, line, col, "Expected macro name");
                    }
                    char *m_name = xmalloc(m_len + 1);
                    strncpy(m_name, m_start, m_len);
                    m_name[m_len] = '\0';

                    while (*p && (*p == ' ' || *p == '\t')) {
                        p++; col++;
                    }
                    const char *val_start = p;
                    while (*p && *p != '\n') {
                        p++; col++;
                    }
                    int val_len = p - val_start;
                    char *m_val = xmalloc(val_len + 1);
                    strncpy(m_val, val_start, val_len);
                    m_val[val_len] = '\0';

                    add_macro(m_name, m_val);
                    free(m_name);
                    free(m_val);
                }
            }

            free(dir);
            while (*p && *p != '\n') {
                p++; col++;
            }
            if (*p == '\n') {
                line++;
                col = 1;
                bol = true;
                p++;
            }
            continue;
        }

        // If we are in an inactive block, skip ordinary characters
        if (!SHOULD_EMIT()) {
            p++; col++;
            bol = false;
            continue;
        }

        bol = false;

        // String literals
        if (*p == '"') {
            p++; col++;
            const char *start = p;
            int start_col = col;
            while (*p && *p != '"') {
                if (*p == '\n') {
                    line++;
                    col = 1;
                } else {
                    col++;
                }
                p++;
            }
            int len = p - start;
            char *val = xmalloc(len + 1);
            strncpy(val, start, len);
            val[len] = '\0';

            if (*p == '"') {
                p++; col++; // Consume closing quote
            } else {
                error_at(filename, line, col, "Unterminated string literal");
            }

            cur->next = new_token(TOKEN_STRING, val, line, start_col);
            free(val);
            cur = cur->next;
            continue;
        }

        // Parentheses, Braces, Brackets, Comma, Semicolon, Dot, Amp, Colon
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
        if (*p == '[') {
            cur->next = new_token(TOKEN_LBRACK, NULL, line, col);
            cur = cur->next;
            p++; col++;
            continue;
        }
        if (*p == ']') {
            cur->next = new_token(TOKEN_RBRACK, NULL, line, col);
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
        if (*p == ',') {
            cur->next = new_token(TOKEN_COMMA, NULL, line, col);
            cur = cur->next;
            p++; col++;
            continue;
        }
        if (*p == '.') {
            cur->next = new_token(TOKEN_DOT, NULL, line, col);
            cur = cur->next;
            p++; col++;
            continue;
        }
        if (*p == ':') {
            cur->next = new_token(TOKEN_COLON, NULL, line, col);
            cur = cur->next;
            p++; col++;
            continue;
        }
        if (*p == '&') {
            cur->next = new_token(TOKEN_AMP, NULL, line, col);
            cur = cur->next;
            p++; col++;
            continue;
        }

        // Multi-character Operators
        if (p[0] == '-' && p[1] == '>') {
            cur->next = new_token(TOKEN_ARROW, NULL, line, col);
            cur = cur->next;
            p += 2; col += 2;
            continue;
        }
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

        // Numeric literal (integer or float)
        if (isdigit((unsigned char)*p) || (*p == '.' && isdigit((unsigned char)p[1]))) {
            const char *start = p;
            int start_col = col;
            bool is_float = false;
            while (isdigit((unsigned char)*p) || *p == '.') {
                if (*p == '.') {
                    is_float = true;
                }
                p++; col++;
            }
            int len = p - start;
            char *val = xmalloc(len + 1);
            strncpy(val, start, len);
            val[len] = '\0';

            TokenType type = is_float ? TOKEN_FLOAT_LIT : TOKEN_NUM;
            cur->next = new_token(type, val, line, start_col);
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

            Macro *m = find_macro(val);
            if (m && !m->in_expansion) {
                m->in_expansion = true;
                Token *macro_tokens = tokenize(filename, m->value);
                m->in_expansion = false;
                if (macro_tokens) {
                    cur->next = macro_tokens;
                    while (cur->next && cur->next->type != TOKEN_EOF) {
                        cur = cur->next;
                    }
                    Token *eof_tok = cur->next;
                    if (eof_tok && eof_tok->type == TOKEN_EOF) {
                        cur->next = NULL;
                        free_tokens(eof_tok);
                    }
                }
                free(val);
                continue;
            }

            TokenType type = find_keyword(val);
            cur->next = new_token(type, type == TOKEN_IDENT ? val : NULL, line, start_col);
            free(val);
            cur = cur->next;
            continue;
        }

        error_at(filename, line, col, "invalid character: '%c'", *p);
    }

    cur->next = new_token(TOKEN_EOF, NULL, line, col);
    #undef SHOULD_EMIT
    return head.next;
}

void print_tokens(Token *tok) {
    while (tok) {
        printf("[%d:%d] ", tok->line, tok->col);
        switch (tok->type) {
            case TOKEN_EOF: printf("EOF\n"); break;
            case TOKEN_I32: printf("KEYWORD: i32\n"); break;
            case TOKEN_I64: printf("KEYWORD: i64\n"); break;
            case TOKEN_F32: printf("KEYWORD: f32\n"); break;
            case TOKEN_F64: printf("KEYWORD: f64\n"); break;
            case TOKEN_BOOL_TYPE: printf("KEYWORD: bool\n"); break;
            case TOKEN_PTR_TYPE: printf("KEYWORD: ptr\n"); break;
            case TOKEN_VOID: printf("KEYWORD: void\n"); break;
            case TOKEN_LET: printf("KEYWORD: let\n"); break;
            case TOKEN_MUT: printf("KEYWORD: mut\n"); break;
            case TOKEN_CONST: printf("KEYWORD: const\n"); break;
            case TOKEN_FN: printf("KEYWORD: fn\n"); break;
            case TOKEN_STRUCT: printf("KEYWORD: struct\n"); break;
            case TOKEN_ENUM: printf("KEYWORD: enum\n"); break;
            case TOKEN_UNION: printf("KEYWORD: union\n"); break;
            case TOKEN_IF: printf("KEYWORD: if\n"); break;
            case TOKEN_ELSE: printf("KEYWORD: else\n"); break;
            case TOKEN_WHILE: printf("KEYWORD: while\n"); break;
            case TOKEN_FOR: printf("KEYWORD: for\n"); break;
            case TOKEN_IN: printf("KEYWORD: in\n"); break;
            case TOKEN_RETURN: printf("KEYWORD: return\n"); break;
            case TOKEN_BREAK: printf("KEYWORD: break\n"); break;
            case TOKEN_CONTINUE: printf("KEYWORD: continue\n"); break;
            case TOKEN_SWITCH: printf("KEYWORD: switch\n"); break;
            case TOKEN_CASE: printf("KEYWORD: case\n"); break;
            case TOKEN_ALLOC: printf("KEYWORD: alloc\n"); break;
            case TOKEN_FREE: printf("KEYWORD: free\n"); break;
            case TOKEN_DEFER: printf("KEYWORD: defer\n"); break;
            case TOKEN_SIZEOF: printf("KEYWORD: sizeof\n"); break;
            case TOKEN_IMPORT: printf("KEYWORD: import\n"); break;
            case TOKEN_PUB: printf("KEYWORD: pub\n"); break;
            case TOKEN_ASM: printf("KEYWORD: asm\n"); break;
            case TOKEN_EXTERN: printf("KEYWORD: extern\n"); break;
            case TOKEN_NULL: printf("KEYWORD: null\n"); break;
            case TOKEN_NULLPTR: printf("KEYWORD: nullptr\n"); break;
            case TOKEN_IDENT: printf("IDENTIFIER: %s\n", tok->value); break;
            case TOKEN_NUM: printf("NUMBER: %s\n", tok->value); break;
            case TOKEN_FLOAT_LIT: printf("FLOAT_LITERAL: %s\n", tok->value); break;
            case TOKEN_STRING: printf("STRING_LITERAL: \"%s\"\n", tok->value); break;
            case TOKEN_LPAREN: printf("LPAREN: (\n"); break;
            case TOKEN_RPAREN: printf("RPAREN: )\n"); break;
            case TOKEN_LBRACE: printf("LBRACE: {\n"); break;
            case TOKEN_RBRACE: printf("RBRACE: }\n"); break;
            case TOKEN_LBRACK: printf("LBRACK: [\n"); break;
            case TOKEN_RBRACK: printf("RBRACK: ]\n"); break;
            case TOKEN_SEMI: printf("SEMI: ;\n"); break;
            case TOKEN_COMMA: printf("COMMA: ,\n"); break;
            case TOKEN_DOT: printf("DOT: .\n"); break;
            case TOKEN_COLON: printf("COLON: :\n"); break;
            case TOKEN_AMP: printf("AMP: &\n"); break;
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
            case TOKEN_ARROW: printf("ARROW: ->\n"); break;
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
