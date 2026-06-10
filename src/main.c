#include "common.h"
#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "emitter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>
#include <ucontext.h>

static void *g_exec_mem = NULL;
static size_t g_exec_size = 0;

static void sigsegv_handler(int sig, siginfo_t *info, void *context) {
    ucontext_t *uc = (ucontext_t *)context;
    uint64_t pc = uc->uc_mcontext.pc;
    fprintf(stderr, "Caught signal %d (SEGV) at PC = %p\n", sig, (void *)pc);
    
    // Print CPU registers
    fprintf(stderr, "Registers:\n");
    fprintf(stderr, "  X0:  0x%llx\n", (unsigned long long)uc->uc_mcontext.regs[0]);
    fprintf(stderr, "  X1:  0x%llx\n", (unsigned long long)uc->uc_mcontext.regs[1]);
    fprintf(stderr, "  X2:  0x%llx\n", (unsigned long long)uc->uc_mcontext.regs[2]);
    fprintf(stderr, "  X29: 0x%llx\n", (unsigned long long)uc->uc_mcontext.regs[29]);
    fprintf(stderr, "  X30: 0x%llx\n", (unsigned long long)uc->uc_mcontext.regs[30]);
    fprintf(stderr, "  SP:  0x%llx\n", (unsigned long long)uc->uc_mcontext.sp);
    
    if (g_exec_mem) {
        fprintf(stderr, "exec_mem range: %p - %p\n", g_exec_mem, (char *)g_exec_mem + g_exec_size);
        if (pc >= (uint64_t)g_exec_mem && pc < (uint64_t)g_exec_mem + g_exec_size) {
            fprintf(stderr, "Crash occurred at JIT offset %ld (0x%lx)\n", pc - (uint64_t)g_exec_mem, pc - (uint64_t)g_exec_mem);
        }
    }
    exit(128 + sig);
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
    // Register crash signal handler
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sigsegv_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);

    if (argc != 2) {
        fprintf(stderr, "Nano C Compiler (ncc)\n");
        fprintf(stderr, "Usage: %s <source_file.c>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    char *source = read_file(filename);

    // 1. Tokenize
    Token *tokens = tokenize(filename, source);

    // 2. Parse
    ASTNode *ast = parse(tokens);

    // 3. Set codegen target to arm64
    if (!set_codegen_target("arm64")) {
        error("Failed to set codegen target to arm64");
    }

    // 4. Compile to binary in memory
    Emitter e;
    emitter_init(&e, NULL, true); // true = binary_mode
    generate_code_binary(ast, &e);

    // Print JIT buffer bytes
    printf("[DEBUG] JIT code bytes:\n");
    for (int i = 0; i < e.size; i++) {
        printf("%02x ", e.buffer[i]);
        if ((i + 1) % 4 == 0) printf(" | ");
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");

    // 5. Find main function offset
    int main_offset = emitter_find_symbol_offset(&e, "main");
    if (main_offset == -1) {
        error("JIT error: main function not found");
    }

    // 6. Allocate executable memory aligned to page size
    size_t page_size = sysconf(_SC_PAGESIZE);
    size_t alloc_size = (e.size + page_size - 1) & ~(page_size - 1);

    void *exec_mem = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (exec_mem == MAP_FAILED) {
        perror("mmap failed");
        emitter_free(&e);
        free_ast(ast);
        free_tokens(tokens);
        free(source);
        return 1;
    }

    g_exec_mem = exec_mem;
    g_exec_size = e.size;

    // Copy compiled bytes into executable memory
    memcpy(exec_mem, e.buffer, e.size);

    // Flush the instruction cache for ARM64 compatibility
    __builtin___clear_cache((char *)exec_mem, (char *)exec_mem + e.size);

    // Apply execute protection
    if (mprotect(exec_mem, alloc_size, PROT_READ | PROT_EXEC) == -1) {
        perror("mprotect failed");
        munmap(exec_mem, alloc_size);
        emitter_free(&e);
        free_ast(ast);
        free_tokens(tokens);
        free(source);
        return 1;
    }

    // 7. Call the compiled main function
    int (*main_fn)(void) = (int (*)(void))((char *)exec_mem + main_offset);
    int exit_code = main_fn();

    // 8. Cleanup resources
    munmap(exec_mem, alloc_size);
    emitter_free(&e);
    free_ast(ast);
    free_tokens(tokens);
    free(source);

    return exit_code;
}
