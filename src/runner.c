#include "driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

static char *g_tmp_exe = NULL;

void cleanup_temp_file(void) {
    if (g_tmp_exe) {
        unlink(g_tmp_exe);
    }
}

static void signal_handler(int sig) {
    cleanup_temp_file();
    signal(sig, SIG_DFL);
    raise(sig);
}

void setup_signal_handlers(char *temp_path) {
    g_tmp_exe = temp_path;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGSEGV, signal_handler);
    atexit(cleanup_temp_file);
}

int run_target(const char *exe_path, char **run_args, int run_count, bool verbose) {
    // Build arguments for execution
    char **run_argv = malloc((run_count + 2) * sizeof(char *));
    run_argv[0] = (char *)exe_path;
    for (int i = 0; i < run_count; i++) {
        run_argv[i + 1] = run_args[i];
    }
    run_argv[run_count + 1] = NULL;

    if (verbose) {
        printf("ncc: running target:");
        for (int i = 0; i < run_count + 1; i++) {
            printf(" %s", run_argv[i]);
        }
        printf("\n");
        fflush(stdout);
    }

    int exit_code = 0;
    pid_t run_pid = fork();
    if (run_pid == 0) {
        execvp(exe_path, run_argv);
        perror("execvp target");
        exit(127);
    } else if (run_pid > 0) {
        int run_status;
        waitpid(run_pid, &run_status, 0);
        if (WIFEXITED(run_status)) {
            exit_code = WEXITSTATUS(run_status);
        } else if (WIFSIGNALED(run_status)) {
            int sig = WTERMSIG(run_status);
            fprintf(stderr, "ncc JIT: Program terminated by signal %d\n", sig);
            exit_code = 128 + sig;
        }
    } else {
        perror("fork target");
        exit_code = -1;
    }

    free(run_argv);
    return exit_code;
}
