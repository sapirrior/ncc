#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>

int _NC7console6printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);
    return result;
}

void _NC7console9printchar(int ch) {
    putchar(ch);
    fflush(stdout);
}

int _NC7console5width(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
        return 80;
    }
    return w.ws_col;
}

int _NC7console6height(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
        return 24;
    }
    return w.ws_row;
}

void _NC7console5clear(void) {
    printf("\033[2J\033[H");
    fflush(stdout);
}

void _NC7console9set_color(int fg, int bg) {
    if (fg >= 0 && bg >= 0) {
        printf("\033[%d;%dm", fg, bg);
    } else if (fg >= 0) {
        printf("\033[%dm", fg);
    } else if (bg >= 0) {
        printf("\033[%dm", bg);
    }
    fflush(stdout);
}

void _NC7console11reset_color(void) {
    printf("\033[0m");
    fflush(stdout);
}

int _NC7console5getch(void) {
    struct termios oldt, newt;
    int ch;
    if (tcgetattr(STDIN_FILENO, &oldt) != 0) {
        return getchar();
    }
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

void _NC7console11cursor_move(int row, int col) {
    printf("\033[%d;%dH", row, col);
    fflush(stdout);
}
