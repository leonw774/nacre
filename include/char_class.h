#include <stdint.h>

#ifndef CHAR_CLASS_H
#define CHAR_CLASS_H

typedef struct char_class {
    uint8_t bitmap[32]; /* 256 bits (32 bytes) are for bytes */
    int is_negated;
} char_class_t;

static inline void
char_class_set(char_class_t* c, uint8_t i)
{
    c->bitmap[i / 8] |= (uint8_t)(1 << (i % 8));
}

#endif
