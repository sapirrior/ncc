#ifndef DRIVER_H
#define DRIVER_H

#include <stdbool.h>
#include <limits.h>

// Struct to hold parsed command-line options
typedef struct {
    char **compiler_flags;
    int flags_count;

    char **source_files;
    int source_count;

    char **run_args;
    int run_count;

    char *output_file;
    bool verbose;
    bool is_cpp;
} Options;

// Initialize and free options
void options_init(Options *opts);
void options_free(Options *opts);

// Command line argument parser
bool parse_arguments(int argc, char **argv, Options *opts);

// Helper file extension checks
bool is_source_file(const char *filename);
bool is_cpp_source(const char *filename);

// Utility to find command paths
char *find_in_path(const char *cmd);

// Compiler selection and execution
const char *select_compiler(bool is_cpp);
int compile_sources(const Options *opts, const char *compiler, const char *effective_output);

// Target program execution and signal cleanup
void setup_signal_handlers(char *temp_path);
void cleanup_temp_file(void);
int run_target(const char *exe_path, char **run_args, int run_count, bool verbose);

#endif // DRIVER_H
