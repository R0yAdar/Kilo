/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>

#include "io.c"
#include "lifetime.h"
#include "datatypes.h"

/*** defines ***/
#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 4
#define KILO_FORCE_QUIT_TIMES 3
#define CTRL_KEY(k) ((k) & 0x1f)
#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

/*** global variables ***/

struct termios original_settings;

/*** filetypes ***/

char* CL_HL_extensions[] = { ".c", ".h", ".cpp", NULL };

char* C_HL_keywords[] = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "struct", "union", "typedef", "static", "enum", "class", "case",
    "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
    "void|", NULL
};

struct editorSyntax HLDB[] = {
    {
        "c",
        CL_HL_extensions,
        C_HL_keywords,
        "//", "/*", "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    }
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

/*** prototypes ***/

void editorSetStatusMessage(editorConfig* const editor, const char* fmt, ...);
void editorRefreshScreen();
char* editorPrompt(editorConfig* const editor, char* prompt, void (*callback)(editorConfig* const, char*, int));

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

/*** syntax highlighting ***/

int is_separator(int c) {
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editorUpdateSyntax(editorConfig* const E, erow* row) {
    row->hl = realloc(row->hl, row->rsize);
    memset(row->hl, HL_NORMAL, row->size); // turns out we need it ; )

    if (E->syntax == NULL) return;

    char** keywords = E->syntax->keywords;
    char* scs = E->syntax->singleline_comment_start;
    char* mcs = E->syntax->multiline_comment_start;
    char* mce = E->syntax->multiline_comment_end;

    int scs_len = scs ? strlen(scs) : 0;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;

    int prev_sep = 1;
    int in_string = 0;
    int in_comment = (row->idx > 0 && E->row[row->idx - 1].hl_open_comment);

    int i = 0;
    while (i < row->rsize) {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

        if (mcs_len && mce_len && !in_string) {
            if (in_comment) {
                row->hl[i] = HL_MLCOMMENT;
                if (strncmp(&row->render[i], mce, mce_len) == 0) {
                    memset(&row->hl[i], HL_MLCOMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                } else {
                    ++i;
                    continue;
                }
            } else if (strncmp(&row->render[i], mcs, mcs_len) == 0) {
                memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }

        if (scs_len && !in_string) {
            if (strncmp(&row->render[i], scs, scs_len) == 0) {
                memset(&row->hl[i], HL_COMMENT, row->rsize - i);
                break;
            }
        }

        if (E->syntax->flags & HL_HIGHLIGHT_STRINGS) {
            if (in_string) {
                row->hl[i] = HL_STRING;
                if (c == '\\' && i + 1 < row->size) {
                    row->hl[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }
                if (c == in_string) in_string = 0;
                ++i;
                prev_sep = 1;
                continue;
            } else if (c == '"' || c == '\''){
                in_string = c;
                row->hl[i] = HL_STRING;
                ++i;
                continue;
            }
        }

        if ((E->syntax->flags & HL_HIGHLIGHT_NUMBERS) &&
            ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) || 
            (c == '.' && prev_hl == HL_NUMBER))) {
            row->hl[i] = HL_NUMBER;
            ++i;
            prev_sep = 0;
            continue;
        }

        if (prev_sep) {
            int j;
            for(j = 0; keywords[j]; ++j){
                int klen = strlen(keywords[j]);
                int is_type2 = keywords[j][klen - 1] == '|';
                if (is_type2) --klen;

                if (strncmp(&row->render[i], keywords[j], klen) == 0 && 
                    is_separator(row->render[i + klen])) {
                        memset(&row->hl[i], is_type2 ? 
                        HL_KEYWORD2 : HL_KEYWORD1, klen);
                        i += klen;
                        break;
                }
            }

            if (keywords[j] != NULL) {
                prev_sep = 0;
                continue;
            }
        }

        prev_sep = is_separator(c);
        ++i;
    }

    int changed = (row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment;
    if (changed && row->idx + 1 < E->numrows)
        editorUpdateSyntax(E, &E->row[row->idx + 1]);
}

int editorSyntaxToColor(int hl) {
    switch (hl){
        case HL_MLCOMMENT:
        case HL_COMMENT: return 36;
        case HL_KEYWORD1: return 33;
        case HL_KEYWORD2: return 32;
        case HL_NUMBER: return 32;
        case HL_STRING: return 35;
        case HL_MATCH: return 34;
        default: return 37;
    }
}

void editorSelectSyntaxHighlight(editorConfig* const E) {
    E->syntax = NULL;
    if (E->filename == NULL) return;

    char* ext = strrchr(E->filename, '.');

    for (unsigned int i = 0; i < HLDB_ENTRIES; ++i) {
        editorSyntax* s = &HLDB[i];
        
        unsigned int e = 0;
        while (s->filematch[e]) {
            int is_ext = (s->filematch[e][0] == '.');
            if ((is_ext && ext && !strcmp(ext, s->filematch[e])) ||
                (!is_ext && strstr(E->filename, s->filematch[e]))) {
                    E->syntax = s;

                    int filerow;

                    for (filerow = 0; filerow < E->numrows; ++filerow) {
                        editorUpdateSyntax(E, &E->row[filerow]);
                    }

                    return;
            }
            ++e;
        }
    }
}

/*** row operations ***/

int editorRowCxToRx(erow* row, int cx) {
    int rx = 0;
    for (int i = 0; i < cx; i++){
        if (row->chars[i] == '\t')
            rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
        ++rx;
    }

    return rx;
}

int editorRowRxToCx(erow* row, int rx) {
    int cur_rx = 0;
    int cx;

    for (cx = 0; cx < row->size; cx++) {
        if (row->chars[cx] == '\t')
            cur_rx += (KILO_TAB_STOP - 1) - (cur_rx % KILO_TAB_STOP);
        ++cur_rx;
        if (cur_rx > rx) return cx;
    }

    return cx;
}

void editorUpdateRow(editorConfig* const E, erow* row) {
    int tabs = 0;

    for (int i = 0; i < row->size; i++)
        if (row->chars[i] == '\t') ++tabs;
    
    row->render = realloc(row->render, row->size + tabs * (KILO_TAB_STOP - 1) + 1);
    
    int idx = 0;
    for (int i = 0; i < row->size; ++i) {
        if (row->chars[i] == '\t'){
            row->render[idx++] = ' ';
            while (idx % KILO_TAB_STOP != 0) row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->chars[i];
        }
    }

    row->render[idx] = '\0';
    row->rsize = idx;
    editorUpdateSyntax(E, row);
}

void editorInsertRow(editorConfig* const E, int at, char* s, size_t len) {
    // doesnot handle oom
    if (at < 0 || at > E->numrows) return;

    E->row = realloc(E->row, sizeof(erow) * (E->numrows + 1));
    memmove(&E->row[at + 1], &E->row[at], sizeof(erow) * (E->numrows - at));
    for (int j = at + 1; j <= E->numrows; j++) E->row[j].idx++;

    E->row[at].idx = at;
    E->row[at].size = len;
    E->row[at].chars = malloc(len + 1);
    memcpy(E->row[at].chars, s, len);
    E->row[at].chars[len] = '\0';

    E->row[at].rsize = 0;
    E->row[at].render = NULL;
    E->row[at].hl = NULL;
    E->row[at].hl_open_comment = 0;
    editorUpdateRow(E, &E->row[at]);

    ++E->numrows;
    ++E->dirty;
}

void editorFreeRow(erow* row) {
    free(row->render);
    free(row->chars);
    free(row->hl);
}

void editorDelRow(editorConfig* const E, int at) {
    if (at < 0 || at >= E->numrows) return;
    editorFreeRow(&E->row[at]);
    memmove(&E->row[at], &E->row[at + 1], sizeof(erow) * (E->numrows - at - 1));
    for (int j = at; j < E->numrows - 1; j++) E->row[j].idx--;
    --E->numrows;
    ++E->dirty;
}

void editorRowInsertChar(editorConfig* const E, erow* row, int at, int c) {
    if (at < 0 || at > row->size) at = row->size;
    row->chars = realloc(row->chars, row->size + 2);
    //memcpy isn't safe to use on the same memory
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    ++row->size;
    row->chars[at] = c;
    editorUpdateRow(E, row);
    ++E->dirty;
}

void editorRowAppendString(editorConfig* const E, erow* row, char* s, size_t len) {
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size]  = '\0';
    editorUpdateRow(E, row);
    ++E->dirty;
}

void editorRowDelChar(editorConfig* const E, erow* row, int at) {
    if (at < 0 || at >= row->size) return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    --row->size;
    editorUpdateRow(E, row);
    ++E->dirty;
}

/*** editor operations ***/

void editorInsertChar(editorConfig* const E, int c) {
    if (E->cy == E->numrows) {
        editorInsertRow(E, E->numrows, "", 0);
    }
    editorRowInsertChar(E, &E->row[E->cy], E->cx, c);
    ++E->cx;
}

int editorCountIndentation(const char* from, int until) {
    int indent_count = 0;

    while (indent_count < until && from[indent_count] != '\0'){
        if (from[indent_count] == ' ') ++indent_count;
        else if (from[indent_count] == '\t') ++indent_count;
        else break;
    }

    return indent_count;
}


void editorCopyIndentation(const char* indentation_source, const char* text_source, char* to, int indent_count) {
    int text_len = strlen(text_source);
    
    memcpy(to, indentation_source, indent_count);
    memcpy(&to[indent_count], text_source, text_len);
}

void editorInsertNewline(editorConfig* const E) {
    int new_cx = 0;
    if (E->cx == 0) {
        editorInsertRow(E, E->cy, "", 0);
    } else {
        erow* row = &E->row[E->cy];

        int indentation = editorCountIndentation(row->chars, E->cx);
        if (indentation > 0) {
            new_cx = indentation;
            int indented_line_chars_count = indentation + row->size - E->cx;
            char* indented_line = malloc(indented_line_chars_count);
            editorCopyIndentation(row->chars, &row->chars[E->cx], indented_line, indentation);

            editorInsertRow(E, E->cy + 1, indented_line, indented_line_chars_count);
            
            free(indented_line);
        } else {
            // may invalidate row because of realloc:
            editorInsertRow(E, E->cy + 1, &row->chars[E->cx], row->size - E->cx);
        }

        row = &E->row[E->cy]; // reassign
        row->size = E->cx;
        row->chars[row->size] = '\0';
        editorUpdateRow(E, row);
    }

    ++E->cy;
    E->cx = new_cx;
}

void editorDelChar(editorConfig* const E) {
    if (E->cy == E->numrows) return;
    if (E->cx == 0 && E->cy == 0) return;

    erow* row = &E->row[E->cy];
    if (E->cx > 0) {
        editorRowDelChar(E, row, E->cx - 1);
        --E->cx;
    } else {
        E->cx = E->row[E->cy - 1].size;
        editorRowAppendString(E, &E->row[E->cy - 1], row->chars, row->size);
        editorDelRow(E, E->cy);
        --E->cy;
    }
}

/*** file i/o ***/

char* editorRowToString(editorConfig* const E, int* buflen){
    int totalen = 0;

    for (int i = 0; i < E->numrows; ++i)
        totalen += E->row[i].size + 1;
    *buflen = totalen;

    char* buf = malloc(totalen);
    char* p = buf;
    for(int i = 0; i < E->numrows; ++i) {
        memcpy(p, E->row[i].chars, E->row[i].size);
        p += E->row[i].size;
        *p = '\n';
        ++p;
    }

    return buf;
}

void editorOpen(editorConfig* const E, char* filename) {
    free(E->filename);
    E->filename = strdup(filename);

    editorSelectSyntaxHighlight(E);

    FILE* fp = fopen(filename, "r");
    if (!fp) die("fopen");

    char* line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1){
        while(linelen > 0 && (line[linelen - 1] == '\n' || 
            line[linelen - 1] == '\r')) --linelen;
        editorInsertRow(E, E->numrows, line, linelen);
    }
    free(line);
    fclose(fp);
    E->dirty = 0;
}

int editorSave(editorConfig* const E) {
    // should check return vals
    if (E->filename == NULL) {
        E->filename = editorPrompt(E, "Save as: %s", NULL);

        if (E->filename == NULL) {
            editorSetStatusMessage(E, "Save aborted");
            return -2;
        }

        editorSelectSyntaxHighlight(E);
    }

    int len;
    char* buf = editorRowToString(E, &len);

    int fd = open(E->filename, O_RDWR | O_CREAT, 0644);
    if (fd != -1) {
        if (ftruncate(fd, len) != -1) {
            if (write(fd, buf, len) == len) {
                // if write fails, we retain data up to len.
                close(fd);
                free(buf);
                E->dirty = 0;
                return 0;
            }
        }
        close(fd);
    }

    free(buf);
    return -1;
}

/*** find ***/

void editorFindCallback(editorConfig* const E, char* query, int key) {
    static int last_match = -1;
    static int direction = 1;

    static int saved_hl_line;
    static char* saved_hl = NULL;

    if (saved_hl) {
        memcpy(E->row[saved_hl_line].hl, saved_hl, E->row[saved_hl_line].rsize);
        free(saved_hl);
        saved_hl = NULL;
    }

    if (key == '\r' || key == '\x1b') { 
        last_match = -1;
        direction = 1;
        return;
    } else if (key == ARROW_RIGHT || key == ARROW_DOWN){
        direction = 1;
    } else if (key == ARROW_LEFT  || key == ARROW_UP){
        direction = -1;
    } else {
        last_match = -1;
        direction = 1;
    }

    if (last_match == -1) direction = 1;
    int current = last_match;

    for (int i = 0; i < E->numrows; ++i) {
        current += direction;
        if (current == -1) current = E->numrows - 1;
        else if (current == E->numrows) current = 0;

        erow* row = &E->row[current];
        char* match = strstr(row->render, query);
        if (match) {
            last_match = current;
            E->cy = current;
            E->cx = editorRowRxToCx(row, match - row->render);
            E->rowoff = E->cy; // brah why did they make it E.numrows they are stupid

            saved_hl_line = current;
            saved_hl = malloc(row->rsize);
            memcpy(saved_hl, row->hl, row->rsize);
            memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
            break;
        }
    }
}

void editorFind(editorConfig* const E) {
    int saved_cx = E->cx;
    int saved_cy = E->cy;
    int saved_coloff = E->coloff;
    int saved_rowoff = E->rowoff;

    char* query = editorPrompt(E, "Search: %s (Use ESC/Arrows/Enter)", editorFindCallback);
    
    if(!query) {
        E->cx = saved_cx;
        E->cy = saved_cy;
        E->coloff = saved_coloff;
        E->rowoff = saved_rowoff;
    }

    free(query); // tutorial checks if not null, but we can free null.
}


/*** append buffer ***/

typedef struct abuf {
    char* data;
    int len;

} abuf;

int abAppend(abuf* ab, const char* s, int len){
    char* new = realloc(ab->data, ab->len +len);

    if (new == NULL) return -1;
    memcpy(&new[ab->len], s, len);
    ab->data = new;
    ab->len += len;
    return 0;
}

void abFree(abuf* ab){
    free(ab->data);
}

#define ABUF_INIT {NULL , 0}

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

/*** input ***/

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
    
    case BACKSPACE:
    case CTRL_KEY('h'):
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