#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include "fileio.h"
#include "datatypes.h"
#include "input.h"
#include "lifetime.h"
#include "row_operations.h"
#include "drawing.h"
#include "syntax_highlighting.h"

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