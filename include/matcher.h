#include "re.h"

#ifndef _MATCHER_H
#define _MATCHER_H

#define MATCHER_FLAG_EPS 0x01
#define MATCHER_FLAG_WC 0x02
#define MATCHER_FLAG_BYTE 0x04
#define MATCHER_FLAG_ANCHOR 0x08

typedef struct matcher {
    uint8_t flag;
    /*  flag | payload
       ------|-----------
        eps  | undefined
        wc   | wc enum
        byte | byte
    */
    uint8_t payload;
} matcher_t;

typedef int anchor_byte;
#define ANCHOR_BYTE_START ((anchor_byte)(-1))
#define ANCHOR_BYTE_END ((anchor_byte)(-2))

static inline int
isword(anchor_byte byte)
{
    return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z')
        || (byte >= '0' && byte <= '9') || (byte == '_');
}

static inline int
match_wc(enum WILDCARD_NAME wc, unsigned char byte)
{
    switch (wc) {
    case WC_ANY:
        return byte != '\n';
    case WC_DIGIT:
        return (byte >= '0' && byte <= '9');
    case WC_NONDIGIT:
        return !(byte >= '0' && byte <= '9');
    case WC_WORD:
        return isword(byte);
    case WC_NONWORD:
        return !isword(byte);
    case WC_SPACE:
        return (
            byte == ' ' || byte == '\t' || byte == '\n' || byte == '\v'
            || byte == '\f' || byte == '\r'
        );
    case WC_NONSPACE:
        return !(
            byte == ' ' || byte == '\t' || byte == '\n' || byte == '\v'
            || byte == '\f' || byte == '\r'
        );
    default:
        return 0;
    }
}

static inline int
match_byte(matcher_t m, unsigned char byte)
{
    return (
        (m.flag & MATCHER_FLAG_EPS)
        || ((m.flag & MATCHER_FLAG_WC) && match_wc(m.payload, byte))
        || ((m.flag & MATCHER_FLAG_BYTE) && m.payload == byte)
    );
}

static inline int
match_anchor(enum ANCHOR_NAME anchor, anchor_byte behind, anchor_byte ahead)
{
    return (anchor == ANCHOR_START && behind == ANCHOR_BYTE_START)
        || (anchor == ANCHOR_END && ahead == ANCHOR_BYTE_END)
        || (anchor == ANCHOR_WEDGE && (isword(behind) != isword(ahead)));
}

#define EPS_MATCHER()                                                          \
    ((matcher_t) {                                                             \
        .flag = MATCHER_FLAG_EPS,                                              \
        .payload = 0,                                                          \
    })
#define WC_MATCHER(w)                                                          \
    ((matcher_t) {                                                             \
        .flag = MATCHER_FLAG_WC,                                               \
        .payload = w,                                                          \
    })
#define BYTE_MATCHER(b)                                                        \
    ((matcher_t) {                                                             \
        .flag = MATCHER_FLAG_BYTE,                                             \
        .payload = b,                                                          \
    })
#define ANCHOR_MATCHER(a)                                                      \
    ((matcher_t) {                                                             \
        .flag = MATCHER_FLAG_ANCHOR | MATCHER_FLAG_EPS,                        \
        .payload = a,                                                          \
    })

#endif
