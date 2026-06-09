#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

// Error reporting helpers
void error(const char *fmt, ...);
void error_at(const char *filename, int line, int col, const char *fmt, ...);

// Simple memory allocations that check for out-of-memory
void *xmalloc(size_t size);
char *xstrdup(const char *s);

#endif // COMMON_H
