#include "common.h"
#include <stdarg.h>

const char *g_filename = NULL;


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
