#include "datatypes.h"

int abAppend(abuf* ab, const char* s, int len){
    char* new = realloc(ab->data, ab->len +len);

    if (new == NULL) return -1;
    memcpy(&new[ab->len], s, len);
    ab->data = new;
    ab->len += len;
    return 0;
}

void abFree(abuf* ab){
    free(ab->data);
}