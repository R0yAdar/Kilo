#include "datatypes.h"

#ifndef SYNTAX_HIGHLIGHTING_H
#define SYNTAX_HIGHLIGHTING_H


/*** filetypes ***/
#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

int is_separator(int c);

void editorUpdateSyntax(editorConfig* const E, erow* row);

int editorSyntaxToColor(int hl);

void editorSelectSyntaxHighlight(editorConfig* const E);

#endif