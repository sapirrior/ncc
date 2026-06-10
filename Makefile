CC = clang
CFLAGS = -std=c11 -Wall -Wextra -Iinclude -g -D_GNU_SOURCE
LDFLAGS = -ldl -lm

# Source files
SRCS = src/main.c \
       src/common.c \
       src/lexer.c \
       src/parser.c \
       src/codegen/codegen.c \
       src/codegen/emitter.c \
       src/codegen/arm64.c

BIN = ncc

all: $(BIN)

$(BIN): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(BIN)

.PHONY: all clean
