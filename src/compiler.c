#include "driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

char *find_in_path(const char *cmd) {
    char *path_env = getenv("PATH");
    if (!path_env) return NULL;
    char *path = strdup(path_env);
    char *dir = strtok(path, ":");
    static char full_path[PATH_MAX];
    while (dir != NULL) {
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd);
        if (access(full_path, X_OK) == 0) {
            free(path);
            return full_path;
        }
        dir = strtok(NULL, ":");
    }
    free(path);
    return NULL;
}

const char *select_compiler(bool is_cpp) {
    if (is_cpp) {
        const char *env_cxx = getenv("NCC_CXX");
        if (env_cxx) return env_cxx;

        const char *cpp_compilers[] = {"clang++", "g++", "c++"};
        for (size_t i = 0; i < sizeof(cpp_compilers)/sizeof(cpp_compilers[0]); i++) {
            if (find_in_path(cpp_compilers[i])) {
                return cpp_compilers[i];
            }
        }
    } else {
        const char *env_cc = getenv("NCC_CC");
        if (env_cc) return env_cc;

        const char *c_compilers[] = {"clang", "gcc", "cc"};
        for (size_t i = 0; i < sizeof(c_compilers)/sizeof(c_compilers[0]); i++) {
            if (find_in_path(c_compilers[i])) {
                return c_compilers[i];
            }
        }
    }
    return NULL;
}

int compile_sources(const Options *opts, const char *compiler, const char *effective_output) {
    // We allocate extra space for compiler name, default flags, output flag, and NULL terminator
    int cmd_argc = 0;
    char **cmd_argv = malloc((opts->flags_count + opts->source_count + 15) * sizeof(char *));

    cmd_argv[cmd_argc++] = (char *)compiler;

    // Suppress implicit declaration errors for C legacy code compatibility
    if (!opts->is_cpp) {
        cmd_argv[cmd_argc++] = "-Wno-error=implicit-function-declaration";
    }

    // Determine if we compile to assembly (.s / .S)
    bool compile_to_assembly = false;
    if (effective_output) {
        const char *ext = strrchr(effective_output, '.');
        if (ext && (strcmp(ext, ".s") == 0 || strcmp(ext, ".S") == 0)) {
            compile_to_assembly = true;
        }
    }


    // Check if user specified optimization
    bool has_opt_flag = false;
    for (int i = 0; i < opts->flags_count; i++) {
        if (strncmp(opts->compiler_flags[i], "-O", 2) == 0) {
            has_opt_flag = true;
            break;
        }
    }
    if (!has_opt_flag) {
        cmd_argv[cmd_argc++] = "-O3"; // Default high optimization
    }

    if (compile_to_assembly) {
        bool has_S = false;
        for (int i = 0; i < opts->flags_count; i++) {
            if (strcmp(opts->compiler_flags[i], "-S") == 0) {
                has_S = true;
                break;
            }
        }
        if (!has_S) {
            cmd_argv[cmd_argc++] = "-S";
        }
    }

    // Add user compiler flags
    for (int i = 0; i < opts->flags_count; i++) {
        cmd_argv[cmd_argc++] = opts->compiler_flags[i];
    }

    // Add source files
    for (int i = 0; i < opts->source_count; i++) {
        cmd_argv[cmd_argc++] = opts->source_files[i];
    }

    // Add output file flags
    cmd_argv[cmd_argc++] = "-o";
    cmd_argv[cmd_argc++] = (char *)effective_output;
    cmd_argv[cmd_argc++] = NULL;

    if (opts->verbose) {
        printf("ncc: executing command:");
        for (int i = 0; i < cmd_argc - 1; i++) {
            printf(" %s", cmd_argv[i]);
        }
        printf("\n");
        fflush(stdout);
    }

    // Run the compiler using fork & exec
    pid_t compile_pid = fork();
    int compile_status = 0;
    if (compile_pid == 0) {
        execvp(compiler, cmd_argv);
        perror("execvp compiler");
        exit(127);
    } else if (compile_pid > 0) {
        waitpid(compile_pid, &compile_status, 0);
    } else {
        perror("fork compiler");
        free(cmd_argv);
        return -1;
    }

    free(cmd_argv);

    if (WIFEXITED(compile_status)) {
        return WEXITSTATUS(compile_status);
    }
    return -1;
}
