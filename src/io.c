#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>

#include "lifetime.h"
#include "datatypes.h"
#include "io.h"

int mapCharToMovementKey(char c){
    switch (c)
    {
        // type 1
        case '1': return HOME_KEY;
        case '3': return DEL_KEY;
        case '4': return END_KEY;
        case '5': return PAGE_UP;
        case '6': return PAGE_DOWN;
        case '7': return HOME_KEY;
        case '8': return END_KEY;

        // type 2 & 3
        case 'A': return ARROW_UP;
        case 'B': return ARROW_DOWN;
        case 'C': return ARROW_RIGHT;
        case 'D': return ARROW_LEFT;
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
    }
        
    return ESCAPE;
}

int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if (nread == -1 && errno != EAGAIN) die("read");
    }

    if ( c == '\x1b'){
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '['){
            if (seq[1] >= '0' && seq[1] <= '9'){
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') return mapCharToMovementKey(seq[1]);
            } else {
                return mapCharToMovementKey(seq[1]);
            }
        } else if (seq[0] == 'O') {
            return mapCharToMovementKey(seq[1]);
        }

        return '\x1b';
    }

    return c;
}

int getCursorPosition(int* rows, int* cols){
    char buf[32];
    unsigned int i = 0;

    if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }

    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

int getWindowSize(int* rows, int* cols){
    struct winsize ws;

    if (!rows | !cols) return -1;
    
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 
        || ws.ws_col == 0 || ws.ws_row == 0){
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;
        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}