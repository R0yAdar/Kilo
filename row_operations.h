#include "datatypes.h"

#ifndef ROW_OPERATIONS_H
#define ROW_OPERATIONS_H
#define KILO_TAB_STOP 4

int editorRowCxToRx(erow* row, int cx);

int editorRowRxToCx(erow* row, int rx);

void editorUpdateRow(editorConfig* const E, erow* row);

void editorInsertRow(editorConfig* const E, int at, char* s, size_t len);

void editorFreeRow(erow* row);

void editorDelRow(editorConfig* const E, int at);

void editorRowInsertChar(editorConfig* const E, erow* row, int at, int c);

void editorRowAppendString(editorConfig* const E, erow* row, char* s, size_t len);

void editorRowDelChar(editorConfig* const E, erow* row, int at);

#endif