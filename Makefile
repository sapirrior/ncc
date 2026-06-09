CC = clang
CFLAGS = -std=c11 -Wall -Wextra -Iinclude -g

# Source files
SRCS = src/main.c \
       src/argparse.c \
       src/compiler.c \
       src/runner.c

BIN = ncc

all: $(BIN)

$(BIN): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(BIN)

.PHONY: all clean
