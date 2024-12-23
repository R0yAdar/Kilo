#ifndef FILEIO_H
#define FILEIO_H

#include "datatypes.h"

char* editorRowToString(editorConfig* const E, int* buflen);

void editorOpen(editorConfig* const E, char* filename);

int editorSave(editorConfig* const E);

#endif