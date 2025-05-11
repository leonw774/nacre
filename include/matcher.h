#include "bitmask.h"
#include "char_class.h"
#include "re.h"
#include <ctype.h>
#include <stdio.h>

#ifndef MATCHER_H
#define MATCHER_H

#define MATCHER_FLAG_NIL 0x00
#define MATCHER_FLAG_EPS 0x01
#define MATCHER_FLAG_WC 0x02
#define MATCHER_FLAG_BYTE 0x04
#define MATCHER_FLAG_ANCHOR 0x08
#define MATCHER_FLAG_CLASS 0x10

typedef struct matcher {
    uint8_t flag;
    /*   flag | payload
       -------|-------------
          nil | NULL
          eps | undefined
           wc | wc enum
         byte | byte
       anchor | anchor enum
        class | class index
    */
    uint32_t payload;
} matcher_t;
#define MATCHER_SIZE sizeof(mathcer_t)

static char*
get_matcher_str(matcher_t m)
{
    static char matcher_str[64];
    if (m.flag & MATCHER_FLAG_EPS) {
        if (m.flag & MATCHER_FLAG_ANCHOR) {
            char payload_char = '\0';
            if (m.payload == ANCHOR_START) {
                payload_char = ANCHOR_START_CHAR;
            } else if (m.payload == ANCHOR_END) {
                payload_char = ANCHOR_END_CHAR;
            } else if (m.flag) {
                payload_char = ANCHOR_WEDGE_CHAR;
            }
            sprintf(matcher_str, "[ANCH %c]", payload_char);
        } else {
            sprintf(matcher_str, "[EPS]");
        }
    } else if (m.flag & MATCHER_FLAG_WC) {
        sprintf(matcher_str, "[WC %d]", m.payload);
    } else if (m.flag & MATCHER_FLAG_BYTE) {
        if (isprint(m.payload)) {
            sprintf(matcher_str, "[BYTE '%c']", m.payload);
        } else {
            sprintf(matcher_str, "[BYTE \\x%x]", m.payload);
        }
    } else if (m.flag & MATCHER_FLAG_CLASS) {
        sprintf(matcher_str, "[CLASS #%d]", m.payload);
    } else {
        sprintf(matcher_str, "[UNK %d]", m.flag);
    }
    return matcher_str;
}

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
match_class(char_class_t c, unsigned char byte)
{
    return ((c.bitmap[byte / 8] & (unsigned char)(1 << (byte % 8))) != 0)
        != c.is_negated;
}

static inline int
match_byte(matcher_t m, unsigned char byte)
{
    assert(
        ((m.flag & MATCHER_FLAG_ANCHOR) == 0)
        && ((m.flag & MATCHER_FLAG_CLASS) == 0)
    );
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

static inline matcher_t
nil_matcher()
{
    return (matcher_t) { .flag = 0, .payload = 0 };
}

static inline matcher_t
eps_matcher()
{
    return (matcher_t) { .flag = MATCHER_FLAG_EPS, .payload = 0 };
}

static inline matcher_t
wc_matcher(enum WILDCARD_NAME wc)
{
    return (matcher_t) { .flag = MATCHER_FLAG_WC, .payload = wc };
}

static inline matcher_t
byte_matcher(unsigned char b)
{
    return (matcher_t) { .flag = MATCHER_FLAG_BYTE, .payload = b };
}

static inline matcher_t
anchor_matcher(enum ANCHOR_NAME anch)
{
    return (matcher_t) {
        .flag = MATCHER_FLAG_ANCHOR | MATCHER_FLAG_EPS,
        .payload = anch,
    };
}

static inline matcher_t
class_matcher(int class_index)
{
    return (matcher_t) {
        .flag = MATCHER_FLAG_CLASS,
        .payload = class_index,
    };
}

#endif
