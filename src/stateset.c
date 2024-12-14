#include <stdlib.h>

unsigned char*
new_stateset(int size) {
    if (size <= 0) {
        return NULL;
    }
    return calloc((size - 1) / 8 + 1, 1);
}