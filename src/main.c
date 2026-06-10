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
#include <time.h>

#define NEOC_VERSION "0.1.0"

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

static char *read_source_file(const char *path) {
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

static void jit_run(ASTNode *ast) {
    if (!set_codegen_target("arm64")) {
        error("Failed to set codegen target to arm64");
    }

    Emitter e;
    emitter_init(&e, NULL, true);
    generate_code_binary(ast, &e);

    int main_offset = emitter_find_symbol_offset(&e, "main");
    if (main_offset == -1) {
        error("JIT error: main function not found");
    }

    size_t page_size = sysconf(_SC_PAGESIZE);
    size_t alloc_size = (e.size + page_size - 1) & ~(page_size - 1);

    void *exec_mem = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (exec_mem == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }

    g_exec_mem = exec_mem;
    g_exec_size = e.size;

    memcpy(exec_mem, e.buffer, e.size);
    __builtin___clear_cache((char *)exec_mem, (char *)exec_mem + e.size);

    if (mprotect(exec_mem, alloc_size, PROT_READ | PROT_EXEC) == -1) {
        perror("mprotect failed");
        exit(1);
    }

    int (*main_fn)(void) = (int (*)(void))((char *)exec_mem + main_offset);
    int exit_code = main_fn();

    munmap(exec_mem, alloc_size);
    emitter_free(&e);
    exit(exit_code);
}

static void compile_to_binary(ASTNode *ast, const char *out_name) {
    char asm_name[256];
    sprintf(asm_name, "%s.s", out_name);
    
    FILE *out_fp = fopen(asm_name, "w");
    if (!out_fp) error("Cannot open output assembly file");
    
    generate_arm64(ast, out_fp);
    fclose(out_fp);
    
    char cmd[512];
    sprintf(cmd, "clang -o %s %s src/libs/os.c src/libs/console.c -ldl -lm", out_name, asm_name);
    int res = system(cmd);
    
    remove(asm_name);
    
    if (res != 0) {
        error("Assembly/Linking failed");
    }
}

int main(int argc, char **argv) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sigsegv_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);

    if (argc < 2) {
        printf("NeoC Compiler (ncc) v%s\n", NEOC_VERSION);
        printf("Usage:\n");
        printf("  ncc -v              : Print version\n");
        printf("  ncc -r <file.nc>    : Compile and run in memory\n");
        printf("  ncc <file.nc> -o <n>: Compile to standalone binary\n");
        printf("  ncc <file.nc>       : Safe default build (random filename)\n");
        return 0;
    }

    bool run_mode = false;
    const char *in_file = NULL;
    const char *out_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            printf("NeoC v%s\n", NEOC_VERSION);
            return 0;
        } else if (strcmp(argv[i], "-r") == 0) {
            run_mode = true;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                out_file = argv[++i];
            } else {
                error("Expected output name after -o");
            }
        } else {
            in_file = argv[i];
        }
    }

    if (!in_file) {
        error("No input file provided");
    }

    g_filename = in_file;
    char *source = read_source_file(in_file);
    Token *tokens = tokenize(in_file, source);
    ASTNode *ast = parse(tokens);

    if (run_mode) {
        jit_run(ast);
    } else {
        if (!out_file) {
            srand(time(NULL));
            char rand_name[16];
            sprintf(rand_name, "%c.out", 'a' + (rand() % 26));
            out_file = rand_name;
            printf("Compiling to default output: %s\n", out_file);
        }
        compile_to_binary(ast, out_file);
    }

    free_ast(ast);
    free_tokens(tokens);
    free(source);

    return 0;
}
