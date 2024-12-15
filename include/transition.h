#include <stdint.h>

typedef struct matcher {
    uint8_t eps;
    uint8_t byte;
} matcher_t;

int match_byte (matcher_t m, unsigned char byte);

#define EPS_MATCHER ((matcher_t) {.eps = 1, .byte = 0})
#define BYTE_MATCHER(b) ((matcher_t) {.eps = 0, .byte = b})

typedef struct transition {
    matcher_t matcher;
    int to_state;
} transition_t;

