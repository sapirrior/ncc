#include "driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    Options opts;
    options_init(&opts);

    if (!parse_arguments(argc, argv, &opts)) {
        if (argc < 2) {
            fprintf(stderr, "Nano C Compiler (ncc) driver - C11 version\n");
            fprintf(stderr, "Usage:\n");
            fprintf(stderr, "  Compile: %s [options] <source-files> -o <output-binary>\n", argv[0]);
            fprintf(stderr, "  Run (JIT): %s [options] <source-files> [--] [program-arguments]\n", argv[0]);
        }
        options_free(&opts);
        return 1;
    }

    // Select the appropriate compiler (clang/clang++ or gcc/g++)
    const char *compiler = select_compiler(opts.is_cpp);
    if (!compiler) {
        fprintf(stderr, "Error: No suitable C/C++ compiler found in PATH\n");
        options_free(&opts);
        return 1;
    }

    bool is_run_mode = (opts.output_file == NULL);
    char tmp_exe_path[PATH_MAX] = {0};
    char *effective_output = opts.output_file;

    if (is_run_mode) {
        // Setup temporary file inside workspace/current directory
        snprintf(tmp_exe_path, sizeof(tmp_exe_path), "./.ncc_tmp_run_XXXXXX");
        int fd = mkstemp(tmp_exe_path);
        if (fd == -1) {
            perror("Error: Failed to create temporary file");
            options_free(&opts);
            return 1;
        }
        close(fd);
        effective_output = tmp_exe_path;

        // Register signal handlers for cleanup
        setup_signal_handlers(tmp_exe_path);
    }

    // Compile the source files
    int compile_code = compile_sources(&opts, compiler, effective_output);
    if (compile_code != 0) {
        cleanup_temp_file();
        options_free(&opts);
        return compile_code;
    }

    int exit_code = 0;
    if (is_run_mode) {
        // Run the compiled target forwarding arguments
        exit_code = run_target(tmp_exe_path, opts.run_args, opts.run_count, opts.verbose);
        cleanup_temp_file();
    }

    options_free(&opts);
    return exit_code;
}
