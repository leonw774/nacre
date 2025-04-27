#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef BITMASK_H
#define BITMASK_H

typedef struct bitmask {
    unsigned char* mask;
    size_t byte_size;
} bitmask_t;

static bitmask_t
bitmask_new(size_t size)
{
    assert(size != 0);
    return (bitmask_t) {
        .mask = calloc((size - 1) / 8 + 1, 1),
        .byte_size = (size - 1) / 8 + 1,
    };
}

static void
bitmask_add(bitmask_t* self, const int key)
{
    self->mask[key / 8] |= (unsigned char)(1 << (key % 8));
}

static int
bitmask_contains(const bitmask_t* self, const int key)
{
    return self->mask[key / 8] & (unsigned char)(1 << (key % 8));
}

static bitmask_t
bitmask_copy(const bitmask_t* src)
{
    bitmask_t dst = {
        .mask = malloc(src->byte_size),
        .byte_size = src->byte_size,
    };
    memcpy(dst.mask, src->mask, src->byte_size);
    return dst;
}

static void
bitmask_free(bitmask_t* self)
{
    if (self->mask) {
        free(self->mask);
    }
    self->mask = NULL;
    self->byte_size = 0;
}

#endif