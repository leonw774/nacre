#include <stdint.h>
#include "matcher.h"

#ifndef TRANSITION_H
#define TRANSITION_H

typedef struct transition {
    matcher_t matcher;
    int to_state;
} transition_t;

#endif
