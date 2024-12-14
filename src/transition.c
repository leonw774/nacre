#include "transition.h"

inline int
match_byte (matcher_t m, unsigned char byte) {
    return (
        m.eps
        || m.byte == byte
    );
}