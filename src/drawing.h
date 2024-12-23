#ifndef DRAWING_H
#define DRAWING_H

#include "datatypes.h"

/*** escape room ***/

int escapeSetColor(abuf* ab, int current_color);

/*** output ***/
void editorScroll(editorConfig* const E);

void editorAppendWelcome(editorConfig* const E, abuf* ab);

void editorDrawRows(editorConfig* const E, abuf* ab);

void editorDrawStatusBar(editorConfig* const E, abuf* ab);

void editorDrawMessageBar(editorConfig* const E, abuf* ab);

void editorRefreshScreen(editorConfig* const editor);

void editorSetStatusMessage(editorConfig* const editor, const char* fmt, ...);

#endif