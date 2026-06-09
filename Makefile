CC = clang
CFLAGS = -std=c99 -Wall -Wextra -Iinclude -g

# Source files
SRCS = src/main.c \
       src/lexer.c \
       src/parser.c \
       src/codegen/codegen.c \
       src/codegen/arm64.c

BIN = ncc

all: $(BIN)

$(BIN): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(BIN)

.PHONY: all clean
