#include "dynarr.h"
#include <stdint.h>
#include <stdlib.h>

#ifndef RE_H
#define RE_H

enum re_token_type {
    TYPE_LIT, /* literal */
    TYPE_WC, /* wildcard */
    TYPE_UOP, /* unary operators */
    TYPE_DUP, /* duplicator */
    TYPE_BOP, /* binary operators */
    TYPE_LP, /* left and right parenthese */
    TYPE_RP,
    TYPE_ANCHOR, /* anchor */
    TYPE_END
};
#define RE_TYPES_NUM (TYPE_END)
extern const char* TYPE_NAME_STRS[RE_TYPES_NUM];

/* literals that need to be escaped in regex */
extern const char* LIT_ESC_CHARS;
#define IS_LIT_ESC(c) (strchr(LIT_ESC_CHARS, c))

/* non-printables that need to be escaped in regex */
extern const char* NONPRINT_ESC_CHARS;
extern const char* NONPRINT_ESC_CHARS_TO;
#define IS_NONPRINT_ESC(c) (strchr(NONPRINT_ESC_CHARS, c))

/* wildcards that need to be expressed as escaped character */
extern const char* WC_ESC_CHARS;
#define IS_WC_ESC(c) (strchr(WC_ESC_CHARS, c))

enum WILDCARD_NAME {
    WC_DIGIT, /* 0-9 */
    WC_NONDIGIT,
    WC_WORD, /* A-Za-z0-9_ */
    WC_NONWORD,
    WC_SPACE, /* ' ', '\t', '\n', '\v', \f', '\r' */
    WC_NONSPACE,
    WC_ANY,
};
#define WC_ANY_CHAR '.'

/* characters that need to be escaped in bracket expression */
extern const char* BRACKET_ESC_CHARS;
extern const char* BRACKET_ESC_CHARS_TO;
#define BRACKET_STR_MAX_LEN 127

enum ANCHOR_NAME {
    ANCHOR_START,
    ANCHOR_END,
    ANCHOR_WEDGE,
};
#define ANCHOR_WEDGE_CHAR 'b'
#define ANCHOR_START_CHAR '^'
#define ANCHOR_END_CHAR '$'

enum OPERATOR_NAME {
    OP_PLUS,
    OP_STAR,
    OP_OPT,
    OP_DUP,
    OP_CONCAT,
    OP_ALTER,
    OP_BRK_ALTER,
    OP_END
};
#define OP_NAMES_NUM (OP_END + 1)
extern const char* OP_CHARS;
extern const char* OP_NAME_STRS[];
extern const int OP_PRECED[OP_NAMES_NUM];
#define IS_UOP(c) (c == '+' || c == '*' || c == '?')
#define IS_BOP(c) (c == '|')
/* is a < b in precedence? */
#define OP_PRECED_LT(a, b) (OP_PRECED[a] < OP_PRECED[b])


#define DUP_NUM_MAX 0xFF
#define DUP_NO_MAX 0
#define DUP_STR_MAX_LEN 3
#define DUP_START '{'
#define DUP_SEP ','
#define DUP_END '}'

/* type | payload       | payload2
   -----|---------------|---------
   LIT  | byte          | -
   WC   | wildcard enum | -
   DUP  | min           | max
   OP   | op enum       | -
*/
typedef struct re_token {
    uint8_t type;
    uint8_t payload;
    uint8_t payload2;
} re_token_t;
#define token_size sizeof(re_token_t)

extern int re_token_print(re_token_t token);

typedef struct re_ast {
    re_token_t* tokens;
    int* lefts;
    int* rights;
    int size;
    int root;
} re_ast_t;

#endif
