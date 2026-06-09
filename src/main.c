#include "driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    Options opts;
    options_init(&opts);

    if (!parse_arguments(argc, argv, &opts)) {
        if (argc < 2) {
            fprintf(stderr, "NeoC Compiler (ncc) driver - C11 version\n");
            fprintf(stderr, "Usage:\n");
            fprintf(stderr, "  Compile: %s [options] <source-files> -o <output-binary>\n", argv[0]);
            fprintf(stderr, "  Run (JIT): %s [options] <source-files> [--] [program-arguments]\n", argv[0]);
        }
        options_free(&opts);
        return 1;
    }

    // Preprocess any source files that start with shebang "#!"
    char **temp_shebang_files = calloc(opts.source_count, sizeof(char *));
    for (int i = 0; i < opts.source_count; i++) {
        const char *src = opts.source_files[i];
        if (strcmp(src, "-") == 0) continue;

        FILE *fp = fopen(src, "r");
        if (!fp) continue;

        char first_two[2] = {0};
        size_t read_bytes = fread(first_two, 1, 2, fp);
        if (read_bytes == 2 && first_two[0] == '#' && first_two[1] == '!') {
            // It has a shebang! Strip it by copying to a temp file
            char tmp_shebang[PATH_MAX];
            snprintf(tmp_shebang, sizeof(tmp_shebang), "./.ncc_tmp_shebang_XXXXXX");
            int fd = mkstemp(tmp_shebang);
            if (fd != -1) {
                close(fd);
                char tmp_shebang_ext[PATH_MAX];
                snprintf(tmp_shebang_ext, sizeof(tmp_shebang_ext), "%s%s", tmp_shebang, is_cpp_source(src) ? ".cpp" : ".c");
                if (rename(tmp_shebang, tmp_shebang_ext) == 0) {
                    FILE *out_fp = fopen(tmp_shebang_ext, "w");
                    if (out_fp) {
                        char line[4096];
                        // Skip shebang line but output a newline to preserve error line numbering
                        if (fgets(line, sizeof(line), fp)) {
                            fprintf(out_fp, "\n");
                        }
                        while (fgets(line, sizeof(line), fp)) {
                            fputs(line, out_fp);
                        }
                        fclose(out_fp);
                        opts.source_files[i] = strdup(tmp_shebang_ext);
                        temp_shebang_files[i] = opts.source_files[i];
                    }
                }
            }
        }
        fclose(fp);
    }

    // If one of the source files is "-", we read from stdin and write to a temp file
    char tmp_stdin_path[PATH_MAX] = {0};
    bool has_stdin_source = false;
    for (int i = 0; i < opts.source_count; i++) {
        if (strcmp(opts.source_files[i], "-") == 0) {
            has_stdin_source = true;
            break;
        }
    }

    if (has_stdin_source) {
        char tmp_base[PATH_MAX] = "./.ncc_tmp_stdin_XXXXXX";
        int fd = mkstemp(tmp_base);
        if (fd == -1) {
            perror("Error: Failed to create temporary file for stdin");
            for (int i = 0; i < opts.source_count; i++) {
                if (temp_shebang_files[i]) {
                    unlink(temp_shebang_files[i]);
                    free(temp_shebang_files[i]);
                }
            }
            free(temp_shebang_files);
            options_free(&opts);
            return 1;
        }
        close(fd);

        snprintf(tmp_stdin_path, sizeof(tmp_stdin_path), "%s%s", tmp_base, opts.is_cpp ? ".cpp" : ".c");
        if (rename(tmp_base, tmp_stdin_path) != 0) {
            perror("Error: Failed to rename temporary file for stdin");
            unlink(tmp_base);
            for (int i = 0; i < opts.source_count; i++) {
                if (temp_shebang_files[i]) {
                    unlink(temp_shebang_files[i]);
                    free(temp_shebang_files[i]);
                }
            }
            free(temp_shebang_files);
            options_free(&opts);
            return 1;
        }

        FILE *tmp_fp = fopen(tmp_stdin_path, "w");
        if (!tmp_fp) {
            perror("Error: Failed to open temporary file for stdin");
            unlink(tmp_stdin_path);
            for (int i = 0; i < opts.source_count; i++) {
                if (temp_shebang_files[i]) {
                    unlink(temp_shebang_files[i]);
                    free(temp_shebang_files[i]);
                }
            }
            free(temp_shebang_files);
            options_free(&opts);
            return 1;
        }

        if (opts.stdin_code) {
            fwrite(opts.stdin_code, 1, strlen(opts.stdin_code), tmp_fp);
        } else {
            char buf[4096];
            size_t bytes;
            while ((bytes = fread(buf, 1, sizeof(buf), stdin)) > 0) {
                fwrite(buf, 1, bytes, tmp_fp);
            }
        }
        fclose(tmp_fp);

        // Replace "-" with the temp file path in source_files
        for (int i = 0; i < opts.source_count; i++) {
            if (strcmp(opts.source_files[i], "-") == 0) {
                opts.source_files[i] = tmp_stdin_path;
            }
        }
    }

    // Select the appropriate compiler (clang/clang++ or gcc/g++)
    const char *compiler = select_compiler(opts.is_cpp);
    if (!compiler) {
        fprintf(stderr, "Error: No suitable C/C++ compiler found in PATH\n");
        if (has_stdin_source) unlink(tmp_stdin_path);
        for (int i = 0; i < opts.source_count; i++) {
            if (temp_shebang_files[i]) {
                unlink(temp_shebang_files[i]);
                free(temp_shebang_files[i]);
            }
        }
        free(temp_shebang_files);
        options_free(&opts);
        return 1;
    }

    bool is_run_mode = (opts.output_file == NULL);
    char tmp_exe_path[PATH_MAX] = {0};
    char *effective_output = opts.output_file;
    bool use_cache = false;

    if (is_run_mode) {
        if (find_cached_binary(&opts, compiler, tmp_exe_path, sizeof(tmp_exe_path))) {
            use_cache = true;
        }
        effective_output = tmp_exe_path;

        if (!use_cache) {
            // Register signal handlers for cleanup of the corrupt cached binary if compile gets interrupted
            setup_signal_handlers(tmp_exe_path);
        }
    }

    int compile_code = 0;
    if (!use_cache) {
        // Compile the source files directly to cache
        compile_code = compile_sources(&opts, compiler, effective_output);

        // Disable signal handlers because compilation successfully finished
        if (is_run_mode) {
            setup_signal_handlers(NULL);
        }
    }

    if (has_stdin_source) {
        unlink(tmp_stdin_path);
    }
    // Clean up shebang files if any
    for (int i = 0; i < opts.source_count; i++) {
        if (temp_shebang_files[i]) {
            unlink(temp_shebang_files[i]);
            free(temp_shebang_files[i]);
        }
    }
    free(temp_shebang_files);

    if (compile_code != 0) {
        // Compilation failed, delete the partial/failed cached file
        if (is_run_mode) {
            unlink(tmp_exe_path);
        }
        options_free(&opts);
        return compile_code;
    }

    int exit_code = 0;
    if (is_run_mode) {
        // Run the compiled target forwarding arguments
        exit_code = run_target(tmp_exe_path, opts.run_args, opts.run_count, opts.verbose);
    }

    options_free(&opts);
    return exit_code;
}
