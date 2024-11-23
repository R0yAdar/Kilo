#include "row_operations.h"
#include "syntax_highlighting.h"

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
