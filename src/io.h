#ifndef IO_H
#define IO_H

int mapCharToMovementKey(char c);

int editorReadKey();

int getCursorPosition(int* rows, int* cols);

int getWindowSize(int* rows, int* cols);

#endif