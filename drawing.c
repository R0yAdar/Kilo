#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>

#include "syntax_highlighting.h"
#include "row_operations.h"
#include "drawing.h"

#define KILO_VERSION "0.0.1"

/*** escape room ***/

int escapeSetColor(abuf* ab, int current_color){
    char buf[16];
    int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
    return abAppend(ab, buf, clen);
}

/*** output ***/
void editorScroll(editorConfig* const E) {
    E->rx = 0;
    if (E->cy < E->numrows) {
        E->rx = editorRowCxToRx(&E->row[E->cy], E->cx);
    }

    if (E->cy < E->rowoff) {
        E->rowoff = E->cy;
    }
    if (E->cy >= E->rowoff + E->screenrows) {
        E->rowoff = E->cy - E->screenrows + 1;
    }
    if (E->rx < E->coloff){
        E->coloff = E->rx;
    }
    if (E->rx >= E->coloff + E->screencols) {
        E->coloff = E->rx - E->screencols + 1;
    }
}

void editorAppendWelcome(editorConfig* const E, abuf* ab){
    char welcome[80];
    int welcomelen = snprintf(welcome, sizeof(welcome),
    "Kilo editor -- version %s", KILO_VERSION);

    if (welcomelen > E->screencols) welcomelen = E->screencols;
    int padding = (E->screencols - welcomelen) / 2;
    if (padding){
        abAppend(ab, "~", 1);
        while (--padding) abAppend(ab, " ", 1);
    }

    abAppend(ab, welcome, welcomelen);
}

void editorDrawRows(editorConfig* const E, abuf* ab){
    for (int y = 0; y < E->screenrows; ++y){
        int filerow = y + E->rowoff;
        if (filerow >= E->numrows) {
            if (E->numrows == 0 && y == E->screenrows / 3){
                editorAppendWelcome(E, ab);
            } else {
                abAppend(ab, "~", 1);
            }
        } else {
            int len = E->row[filerow].rsize - E->coloff;
            if (len < 0) len = 0;
            if (len > E->screencols) len = E->screencols;
            char* c = &E->row[filerow].render[E->coloff];
            unsigned char* hl = &E->row[filerow].hl[E->coloff];

            int current_color = -1;
            
            for(int j = 0; j < len; ++j) {
                if (iscntrl((int)c[j])) {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    abAppend(ab, "\x1b[7m", 4);
                    abAppend(ab, &sym, 1);
                    abAppend(ab, "\x1b[m", 3);
                    if (current_color != -1) escapeSetColor(ab, current_color);
                }
                else if (hl[j] == HL_NORMAL) {
                    if (current_color != - 1) {
                        abAppend(ab, "\x1b[39m", 5);
                        current_color = -1;
                    }
                    abAppend(ab, &c[j], 1);
                } else {
                    int color = editorSyntaxToColor(hl[j]);
                    if (color != current_color) {
                        current_color = color;
                        escapeSetColor(ab, current_color);
                    }
                    abAppend(ab, &c[j], 1);
                }
            }

            abAppend(ab, "\x1b[39m", 5);
        }

        abAppend(ab, "\x1b[K", 3);
        abAppend(ab, "\r\n", 2);
    }
}

void editorDrawStatusBar(editorConfig* const E, abuf* ab){
    abAppend(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof status, "%.20s - %d lines %s",
        E->filename ? E->filename : "[No Name]", E->numrows, 
        E->dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof rstatus, "%s | %d/%d", 
        E->syntax ? E->syntax->filetype : "no ft", E->cy + 1, E->numrows);

    if (len > E->screencols) len = E->screencols;
    abAppend(ab, status, len);
    while (len < E->screencols) {
        if (E->screencols - len == rlen) {
            abAppend(ab, rstatus, rlen);
            break;
        } else {
            abAppend(ab, " ", 1);
            ++len;
        }
    }
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(editorConfig* const E, abuf* ab){
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E->statusmsg);
    if (msglen > E->screencols) msglen = E->screencols;
    int i = 0;
    while(i++ < (E->screencols - msglen) / 2) abAppend(ab, " ", 1);
    
    abAppend(ab, "\x1b[4m", 4);
    if (msglen && time(NULL) - E->statusmsg_time < 5)
        abAppend(ab, E->statusmsg, msglen);
    abAppend(ab, "\x1b[m", 4);
}

void editorRefreshScreen(editorConfig* const editor) {
    // unhandeld oom (abAppend)
    editorScroll(editor);

    abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(editor, &ab);
    editorDrawStatusBar(editor, &ab);
    editorDrawMessageBar(editor, &ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", 
        (editor->cy - editor->rowoff) + 1, (editor->rx - editor->coloff) + 1);
    
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.data, ab.len);
    abFree(&ab);
}

void editorSetStatusMessage(editorConfig* const editor, const char* fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(editor->statusmsg, sizeof editor->statusmsg, fmt, ap);
    va_end(ap);
    editor->statusmsg_time = time(NULL);
}
