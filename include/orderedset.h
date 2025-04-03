#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef ORDEREDSET_H
#define ORDEREDSET_H

#define NEW_SET_CAP 4

typedef struct orderedset {
    int elem_size;
    int size;
    int cap;
    /* can be NULL to use default (cmp_int) */
    int (*cmp_func)(const void* a, const void* b);
    void* data;
} orderedset_t;

static int
cmp_int(const void* a, const void* b)
{
    return *(int*)a - *(int*)b;
}

static orderedset_t
orderedset_new(int elem_size, int (*cmp_func)(const void* a, const void* b))
{
    return (orderedset_t) {
        .elem_size = elem_size,
        .size = 0,
        .cap = NEW_SET_CAP,
        .data = calloc(NEW_SET_CAP, elem_size),
    };
}

#define ORDEREDSET(type) orderedset_new(sizeof(type), NULL)
#define ORDEREDSET_CMPFUNC(type, cmpfunc) orderedset_new(sizeof(type), cmpfunc)

static void
orderedset_free(orderedset_t* self)
{
    if (self->data) {
        free(self->data);
        self->data = NULL;
    }
    self->size = self->cap = 0;
}

static orderedset_t
orderedset_copy(const orderedset_t* self)
{
    orderedset_t copy = *self;
    copy.data = calloc(self->cap, self->elem_size);
    memcpy(copy.data, self->data, self->size * self->elem_size);
    return copy;
}

static int
orderedset_find_index(const orderedset_t* self, const void* key)
{
    int low = 0, high = self->size;
    int mid;
    if (self->size == 0) {
        return -1;
    }
    while (low <= high) {
        mid = (low + high) / 2;
        int cmp = self->cmp_func
            ? self->cmp_func(self->data + mid * self->elem_size, key)
            : cmp_int(self->data + mid * self->elem_size, key);
        if (cmp == 0) {
            return mid;
        } else if (cmp < 0) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return -1;
}

static int
orderedset_contains(const orderedset_t* self, const void* key)
{
    return orderedset_find_index(self, key) >= 0;
}

static void
orderedset_add(orderedset_t* self, const void* key)
{
    if (orderedset_contains(self, key)) {
        return;
    }
    if (self->size == self->cap) {
        self->cap *= 2;
        self->data = realloc(self->data, self->cap);
    }
    memcpy(self->data + self->size * self->elem_size, key, self->elem_size);
    self->size += 1;
}

static void
orderedset_remove(orderedset_t* self, const void* key)
{
    int found_index = orderedset_find_index(self, key);
    if (found_index == -1) {
        return;
    }
    if (found_index != self->size - 1) {
        memcpy(
            self->data + found_index * self->elem_size,
            self->data + (found_index + 1) * self->elem_size,
            self->size - found_index - 1
        );
    }
    self->size--;
}

static void
orderedset_union(orderedset_t* self, orderedset_t* right)
{
    void* new_data = NULL;
    int i = 0, j = 0, repeat = 0, elem_size = self->elem_size;
    assert(self->elem_size == right->elem_size);
    new_data = malloc(self->elem_size * (self->size + right->size));
    while (i < self->size && j < right->size) {
        void* self_elem = self->data + i * elem_size;
        void* right_elem = right->data + j * elem_size;
        int k = i + j - repeat;
        if (j == right->size) {
            memcpy(new_data + k * elem_size, self_elem, elem_size);
            i++;
        } else if (i == self->size) {
            memcpy(new_data + k * elem_size, right_elem, elem_size);
            j++;
        } else {
            int cmp = self->cmp_func ? self->cmp_func(self_elem, right_elem)
                                     : cmp_int(self_elem, right_elem);
            if (cmp < 0) {
                memcpy(new_data + k * elem_size, self_elem, elem_size);
                i++;
            } else if (cmp == 0) {
                memcpy(new_data + k * elem_size, self_elem, elem_size);
                i++;
                j++;
                repeat++;
            } else {
                memcpy(new_data + k * elem_size, right_elem, elem_size);
                j++;
            }
        }
    }
    self->size = i + j - repeat;
    self->cap = self->size + right->size;
}

static void
orderedset_intersect(orderedset_t* self, orderedset_t* right)
{
    void* new_data = NULL;
    int i = 0, j = 0, repeat = 0, elem_size = self->elem_size;
    assert(self->elem_size == right->elem_size);
    new_data = malloc(self->elem_size * self->size);
    while (i < self->size && j < right->size) {
        void* self_elem = self->data + i * elem_size;
        void* right_elem = right->data + j * elem_size;
        if (j == right->size) {
            i++;
        } else if (i == self->size) {
            j++;
        } else {
            int cmp = self->cmp_func ? self->cmp_func(self_elem, right_elem)
                                     : cmp_int(self_elem, right_elem);
            if (cmp < 0) {
                i++;
            } else if (cmp == 0) {
                memcpy(new_data + repeat * elem_size, self_elem, elem_size);
                i++;
                j++;
                repeat++;
            } else {
                j++;
            }
        }
    }
    self->size = repeat;
}

#endif
