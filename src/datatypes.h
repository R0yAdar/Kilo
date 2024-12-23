#include <time.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef DATATYPES_H
#define DATATYPES_H

enum editorHighlight {
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_MLCOMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH
};

enum editorKey {
    ESCAPE = 0x1b,
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

typedef struct editorSyntax {
    char* filetype;
    char** filematch;
    char** keywords;
    char* singleline_comment_start;
    char* multiline_comment_start;
    char* multiline_comment_end;
    int flags;
} editorSyntax;

typedef struct erow {
    int idx;
    int size;
    int rsize;
    char* chars;
    char* render;
    unsigned char* hl;
    int hl_open_comment;
} erow;

typedef struct editorConfig {
    int cx, cy;
    int rx;
    int rowoff;
    int coloff;
    int screenrows;
    int screencols;
    int numrows;
    erow* row;
    int dirty;
    char* filename;
    char statusmsg[80];
    time_t statusmsg_time;
    editorSyntax* syntax;
    struct termios orig_termios;
} editorConfig;

/*** append buffer ***/

typedef struct abuf {
    char* data;
    int len;

} abuf;

int abAppend(abuf* ab, const char* s, int len);

void abFree(abuf* ab);

#define ABUF_INIT {NULL , 0}

#endif