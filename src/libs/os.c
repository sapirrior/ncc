#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

void _NC2os5print(const char *message) {
    if (message) {
        printf("%s", message);
        fflush(stdout);
    }
}

void _NC2os4exit(int code) {
    exit(code);
}

void _NC2os5sleep(int milliseconds) {
    if (milliseconds > 0) {
        usleep(milliseconds * 1000);
    }
}

int _NC2os6system(const char *command) {
    if (command) {
        return system(command);
    }
    return -1;
}

// ------------------------------------------------------------
// Newly Added Standard OS Library Functions
// ------------------------------------------------------------

int _NC2os6exists(const char *path) {
    if (!path) return 0;
    return access(path, F_OK) == 0;
}

int _NC2os6remove(const char *path) {
    if (!path) return 0;
    return remove(path) == 0;
}

int _NC2os5mkdir(const char *path) {
    if (!path) return 0;
#ifdef _WIN32
    return mkdir(path) == 0;
#else
    return mkdir(path, 0777) == 0;
#endif
}

int _NC2os5rmdir(const char *path) {
    if (!path) return 0;
    return rmdir(path) == 0;
}

int _NC2os6rename(const char *oldpath, const char *newpath) {
    if (!oldpath || !newpath) return 0;
    return rename(oldpath, newpath) == 0;
}

char *_NC2os6getenv(const char *name) {
    if (!name) return NULL;
    return getenv(name);
}

int _NC2os6setenv(const char *name, const char *value) {
    if (!name || !value) return 0;
    return setenv(name, value, 1) == 0;
}

int _NC2os6getpid(void) {
    return (int)getpid();
}

int _NC2os6getuid(void) {
#ifdef _WIN32
    return 0; // Standard fallback for Windows if compiled there
#else
    return (int)getuid();
#endif
}

char *_NC2os6getcwd(void) {
    static char buf[4096];
    if (getcwd(buf, sizeof(buf)) != NULL) {
        return buf;
    }
    return NULL;
}

int _NC2os5chdir(const char *path) {
    if (!path) return -1;
    return chdir(path) == 0;
}

int _NC2os7getppid(void) {
#ifdef _WIN32
    return 0;
#else
    return (int)getppid();
#endif
}

void _NC2os8printerr(const char *message) {
    if (message) {
        fprintf(stderr, "%s", message);
        fflush(stderr);
    }
}

const char *_NC2os8platform(void) {
#if defined(_WIN32) || defined(_WIN64)
    return "windows";
#elif defined(__APPLE__) || defined(__MACH__)
    return "darwin";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

