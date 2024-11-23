#ifndef INPUT_H
#define INPUT_H

#include "datatypes.h"

#define KILO_FORCE_QUIT_TIMES 3
#define CTRL_KEY(k) ((k) & 0x1f)

char* editorPrompt(editorConfig* const editor, char* prompt, void (*callback)(editorConfig* const, char*, int));

void editorMoveCursor(editorConfig* const E, int key);

void editorProcessKeypress(editorConfig* const editor);

#endif