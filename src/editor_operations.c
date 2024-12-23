#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "editor_operations.h"
#include "row_operations.h"
#include "row_operations.h"
#include "datatypes.h"
#include "input.h"

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

void editorDelWord(editorConfig* const E) {
    if (E->cy == E->numrows) return;
    if (E->cx == 0 && E->cy == 0) return;

    erow* row = &E->row[E->cy];

    if (E->cx > 0) {
        int is_space = isspace((int)row->chars[E->cx - 1]);
        while(E->cx > 0){
            if (isspace((int)row->chars[E->cx - 1]) != is_space)
                break;
            editorRowDelChar(E, row, E->cx - 1);
            --E->cx;
        }

    } else {
        E->cx = E->row[E->cy - 1].size;
        editorRowAppendString(E, &E->row[E->cy - 1], row->chars, row->size);
        editorDelRow(E, E->cy);
        --E->cy;
    }
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
