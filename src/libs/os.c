#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
