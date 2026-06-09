#include "driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void options_init(Options *opts) {
    opts->compiler_flags = NULL;
    opts->flags_count = 0;
    opts->source_files = NULL;
    opts->source_count = 0;
    opts->run_args = NULL;
    opts->run_count = 0;
    opts->output_file = NULL;
    opts->verbose = false;
    opts->is_cpp = false;
}

void options_free(Options *opts) {
    if (opts->compiler_flags) free(opts->compiler_flags);
    if (opts->source_files) free(opts->source_files);
    if (opts->run_args) free(opts->run_args);
}

bool is_source_file(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return false;
    return (strcmp(ext, ".c") == 0 ||
            strcmp(ext, ".cpp") == 0 ||
            strcmp(ext, ".cc") == 0 ||
            strcmp(ext, ".cxx") == 0 ||
            strcmp(ext, ".C") == 0 ||
            strcmp(ext, ".s") == 0 ||
            strcmp(ext, ".S") == 0);
}

bool is_cpp_source(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return false;
    return (strcmp(ext, ".cpp") == 0 ||
            strcmp(ext, ".cc") == 0 ||
            strcmp(ext, ".cxx") == 0 ||
            strcmp(ext, ".C") == 0);
}

bool parse_arguments(int argc, char **argv, Options *opts) {
    opts->compiler_flags = malloc(argc * sizeof(char *));
    opts->source_files = malloc(argc * sizeof(char *));
    opts->run_args = malloc(argc * sizeof(char *));

    bool collecting_run_args = false;

    for (int i = 1; i < argc; i++) {
        if (collecting_run_args) {
            opts->run_args[opts->run_count++] = argv[i];
            continue;
        }

        if (strcmp(argv[i], "--") == 0) {
            collecting_run_args = true;
            continue;
        }

        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: Missing argument for -o\n");
                return false;
            }
            opts->output_file = argv[++i];
            continue;
        }

        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            opts->verbose = true;
            opts->compiler_flags[opts->flags_count++] = argv[i];
            continue;
        }

        if (argv[i][0] == '-') {
            opts->compiler_flags[opts->flags_count++] = argv[i];
        } else {
            if (is_source_file(argv[i])) {
                opts->source_files[opts->source_count++] = argv[i];
            } else {
                collecting_run_args = true;
                opts->run_args[opts->run_count++] = argv[i];
            }
        }
    }

    if (opts->source_count == 0) {
        fprintf(stderr, "Error: No input source files specified\n");
        return false;
    }

    // Determine if we need a C++ compiler
    for (int i = 0; i < opts->source_count; i++) {
        if (is_cpp_source(opts->source_files[i])) {
            opts->is_cpp = true;
            break;
        }
    }

    return true;
}
