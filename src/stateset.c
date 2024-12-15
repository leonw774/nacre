#include <stdlib.h>
#include "stateset.h"

unsigned char*
new_stateset(int size) {
    if (size <= 0) {
        return NULL;
    }
    return calloc((size - 1) / 8 + 1, 1);
}

inline void
stateset_add(unsigned char* self, int pos) {
    self[pos / 8] |= (unsigned char) (1 << (pos % 8));
}

inline void
stateset_remove(unsigned char* self, int pos) {
    self[pos / 8] &= ~((unsigned char) (1 << (pos % 8)));
}

int
stateset_find(unsigned char* self, int pos) {
    return self[pos / 8] & (unsigned char) (1 << (pos % 8));
}

void
stateset_union(
    unsigned char* self, unsigned char* right, int self_size
) {
    return;
}

void
stateset_intersection(
    unsigned char* self, unsigned char* right, int self_size
) {
    return;
}