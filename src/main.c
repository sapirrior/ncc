#include "common.h"
#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include <stdarg.h>

void error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

void error_at(const char *filename, int line, int col, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%s:%d:%d: Error: ", filename, line, col);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        error("Out of memory");
    }
    return ptr;
}

char *xstrdup(const char *s) {
    char *ptr = strdup(s);
    if (!ptr) {
        error("Out of memory");
    }
    return ptr;
}

static char *read_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        error("Cannot open file: %s", path);
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

int main(int argc, char **argv) {
    char *input_path = NULL;
    char *output_path = NULL;
    char *target = "arm64";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                error("Missing argument for -o");
            }
            output_path = argv[++i];
        } else if (strcmp(argv[i], "-t") == 0) {
            if (i + 1 >= argc) {
                error("Missing argument for -t");
            }
            target = argv[++i];
        } else if (argv[i][0] == '-') {
            error("Unknown option: %s", argv[i]);
        } else {
            if (input_path) {
                error("Multiple input files specified");
            }
            input_path = argv[i];
        }
    }

    if (!input_path) {
        error("No input file specified");
    }

    if (!set_codegen_target(target)) {
        error("Unsupported target: %s", target);
    }

    char *source = read_file(input_path);
    Token *tokens = tokenize(input_path, source);

    ASTNode *ast = parse(tokens);

    FILE *out = stdout;
    if (output_path) {
        out = fopen(output_path, "w");
        if (!out) {
            error("Cannot open output file: %s", output_path);
        }
    }

    generate_code(ast, out);

    if (output_path) {
        fclose(out);
    }

    free_ast(ast);
    free_tokens(tokens);
    free(source);

    return 0;
}
