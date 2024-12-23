#ifndef EDITOR_OPERATIONS_H
#define EDITOR_OPERATIONS_H

#include "datatypes.h"

void editorInsertChar(editorConfig* const E, int c);

int editorCountIndentation(const char* from, int until);

void editorCopyIndentation(const char* indentation_source, const char* text_source, char* to, int indent_count);

void editorInsertNewline(editorConfig* const E);

void editorDelWord(editorConfig* const E);

void editorDelChar(editorConfig* const E);

/*** find ***/

void editorFindCallback(editorConfig* const E, char* query, int key);

void editorFind(editorConfig* const E);
#endif