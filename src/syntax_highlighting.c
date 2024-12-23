#include <ctype.h>

#include "datatypes.h"
#include "syntax_highlighting.h"

/*** filetypes ***/
#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

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
