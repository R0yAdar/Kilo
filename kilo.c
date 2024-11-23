/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>

#include "lifetime.h"
#include "datatypes.h"
#include "syntax_highlighting.h"
#include "row_operations.h"
#include "drawing.h"
#include "fileio.h"
#include "input.h"
#include "io.h"

/*** defines ***/
#define KILO_FORCE_QUIT_TIMES 3
#define CTRL_KEY(k) ((k) & 0x1f)

/*** global variables ***/

struct termios original_settings;

/*** terminal ***/

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_settings) == -1)
        die("tcsetattr");
}

void enableRawMode(){
    if (tcgetattr(STDIN_FILENO, &original_settings) == -1)
        die("tcsetattr");
    atexit(disableRawMode);
    
    struct termios raw = original_settings;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

/*** init ***/

void set_window_size(editorConfig* const editor){
    if(getWindowSize(&editor->screenrows, &editor->screencols) == - 1) die("getWindowSize");

    editor->screenrows -= 2; // Save one for status bar
    if (editor->screenrows <= 0) die ("screenrows <= 0");
}

editorConfig initEditor(){
    editorConfig editor;
    editor.cx = 0;
    editor.cy = 0;
    editor.rx = 0;
    editor.rowoff = 0;
    editor.coloff = 0;
    editor.numrows = 0;
    editor.row = NULL;
    editor.dirty = 0;
    editor.filename = NULL;
    editor.statusmsg[0] = '\0';
    editor.statusmsg_time = 0;
    editor.syntax = NULL;

    set_window_size(&editor);

    return editor;
}


int main(int argc, char* argv[]){
    editorConfig editor = initEditor();
    enableRawMode();

    if (argc >= 2)
        editorOpen(&editor, argv[1]);

    editorSetStatusMessage(&editor, "HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

    while (true) {
        editorRefreshScreen(&editor);
        editorProcessKeypress(&editor);
        set_window_size(&editor); // resize if screen changes
    }
    
    return 0;
}