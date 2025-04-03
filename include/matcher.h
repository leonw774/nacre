#include "re.h"

#ifndef _MATCHER_H
#define _MATCHER_H

#define MATCHER_FLAG_EPS 0x01
#define MATCHER_FLAG_WC 0x02
#define MATCHER_FLAG_BYTE 0x04

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

static inline int
match_wc(enum WILDCARD_NAME wc, unsigned char byte)
{
    switch (wc) {
    case WC_ANY:
        return 1;
    case WC_DIGIT:
        return (byte >= '0' && byte <= '9');
    case WC_NONDIGIT:
        return !(byte >= '0' && byte <= '9');
    case WC_WORD:
        return (
            (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z')
            || (byte >= '0' && byte <= '9') || (byte == '_')
        );
    case WC_NONWORD:
        return !(
            (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z')
            || (byte >= '0' && byte <= '9') || (byte == '_')
        );
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
        m.flag == MATCHER_FLAG_EPS
        || (m.flag == MATCHER_FLAG_WC && match_wc(m.payload, byte))
        || (m.flag == MATCHER_FLAG_BYTE && m.payload == byte)
    );
}

#define EPS_MATCHER() ((matcher_t){ .flag = MATCHER_FLAG_EPS, .payload = 0, })
#define WC_MATCHER(w) ((matcher_t){ .flag = MATCHER_FLAG_WC, .payload = w, })
#define BYTE_MATCHER(b) ((matcher_t){ .flag = MATCHER_FLAG_BYTE, .payload = b, })

#endif
