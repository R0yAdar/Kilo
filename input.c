#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#include "editor_operations.h"
#include "fileio.h"
#include "io.h"
#include "drawing.h"

#define KILO_FORCE_QUIT_TIMES 3
#define CTRL_KEY(k) ((k) & 0x1f)

char* editorPrompt(editorConfig* const editor, char* prompt, void (*callback)(editorConfig* const, char*, int)) {
    size_t bufsize = 128;
    char* buf = malloc(bufsize);

    size_t insize = 0;
    buf[0] = '\0';

    while (1){
        editorSetStatusMessage(editor, prompt, buf);
        editorRefreshScreen(editor);
        int c = editorReadKey();
        if (c == '\x1b') {
            editorSetStatusMessage(editor, "");
            if (callback) callback(editor, buf, c);
            free(buf);
            return NULL;
        } else if (c == '\r') {
            if (insize != 0) {
                editorSetStatusMessage(editor, "");
                if (callback) callback(editor, buf, c);
                return buf;
            }
        } else if (c == BACKSPACE || c == CTRL_KEY('h')) {
            if (insize != 0) buf[--insize]  = '\0';
        } else if (!iscntrl(c) && c < 128) {
            if (insize == bufsize - 1) {
                bufsize *= 2;
                buf = realloc(buf, bufsize);
            }
            buf[insize++] = c;
            buf[insize] = '\0';
        }

        if (callback) callback(editor, buf, c);
    }
}

void editorMoveCursor(editorConfig* const E, int key){
    erow* row = (E->cy >= E->numrows) ? NULL : &E->row[E->cy];

    switch (key) {
        case ARROW_LEFT:
            if (E->cx != 0) {
                --E->cx;
            } else if (E->cy > 0) {
                --E->cy;
                E->cx = E->row[E->cy].size;
            }
            break;
        case ARROW_RIGHT:
            if (row && E->cx < row->size){
                ++E->cx;
            } else if (E->cy < E->numrows) {
                ++E->cy;
                E->cx = 0;
            }
            break;
        case ARROW_UP:
            if (E->cy != 0) --E->cy;
            break;
        case ARROW_DOWN:
            if (E->cy < E->numrows) ++E->cy;
            break;
    }

    row = (E->cy >= E->numrows) ? NULL : &E->row[E->cy];
    int rowlen = row ? row->size : 0;
    if (E->cx > rowlen) {
        E->cx = rowlen;
    }
}

void editorProcessKeypress(editorConfig* const editor) {
    static int quit_times = KILO_FORCE_QUIT_TIMES;

    int c = editorReadKey();

    switch (c)
    {
    case '\r':
        editorInsertNewline(editor);
        break;
    
    case CTRL_KEY ('s'):
        {
            int result = editorSave(editor);
            if (result != -1) 
                editorSetStatusMessage(editor, "Saved file %.20s", editor->filename);
            else if (result == 0)
                editorSetStatusMessage(editor, "Can't save! I/O error: %.40s", strerror(errno));
        }
        break;

    case CTRL_KEY('q'):
        if (editor->dirty && quit_times > 0) {
            editorSetStatusMessage(editor, "WARNING!!! File has unsaved changes. "
                "Press Ctrl-Q %d more times to quit.", quit_times--);
            return;
        }
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    
    case HOME_KEY:
        editor->cx = 0;
        break;
    
    case END_KEY:
        if (editor->cy < editor->numrows)
            editor->cx = editor->row[editor->cy].size;
        break;
    
    case CTRL_KEY('f'):
        editorFind(editor);
        break;
    case CTRL_KEY('h'):
        break;
    case BACKSPACE:
    case DEL_KEY:
        if (c == DEL_KEY) editorMoveCursor(editor, ARROW_RIGHT);
        editorDelChar(editor);
        break;

    case PAGE_DOWN:
    case PAGE_UP:
        {
        editor->cy = c == PAGE_UP ? editor->rowoff : editor->rowoff + editor->screenrows - 1;
        if (editor->cy > editor->numrows) editor->cy = editor->numrows; // cap E.cy

        int times = editor->screenrows;
        while (times--)
          editorMoveCursor(editor, c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
        }
      break;
    
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
        editorMoveCursor(editor, c);
        break;
    
    case CTRL_KEY('l'):
    case '\x1b':
        break;
    
    default:
        editorInsertChar(editor, c);
        break;
    }

    quit_times = KILO_FORCE_QUIT_TIMES;
}
