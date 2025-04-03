#include <stdint.h>
#include "matcher.h"

typedef struct transition {
    matcher_t matcher;
    int to_state;
} transition_t;
