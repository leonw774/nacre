#include <stdlib.h>
#include <stdint.h>
#include "dynarr.h"

#ifndef RE_H
#define RE_H

enum re_token_type {
    TYPE_LIT, /* literal */
    TYPE_WC, /* wildcard */
    TYPE_UOP, /* unary operators */
    TYPE_DUP, /* duplicator */
    TYPE_BOP, /* binary operators */
    TYPE_LP, /* left and right parenthese */
    TYPE_RP
};
#define RE_TYPES_NUM (TYPE_RP + 1)
extern const char* TYPE_NAME_STRS[RE_TYPES_NUM];

/* literals that need to be escaped in regex */
#define LIT_ESC_CHARS "+*?|(){\\"
/* non-printables that need to be escaped in regex */
#define NONPRINT_ESC_CHARS "nrtvf"
#define NONPRINT_ESC_CHARS_MAP_TO "\n\r\t\v\f"
/* wildcards that need to be expressed as escaped character */
#define WC_ESC_CHARS ".dDwWsS"
#define IS_WC_ESC(c) (strchr(WC_ESC_CHARS, c))
#define IS_NONPRINT_ESC(c) (strchr(NONPRINT_ESC_CHARS, c))
#define IS_LIT_ESC(c) (strchr(LIT_ESC_CHARS, c))

enum WILDCARD_NAME {
    WC_ANY,
    WC_DIGIT, /* 0-9 */
    WC_NONDIGIT,
    WC_WORD, /* A-Za-z0-9_ */
    WC_NONWORD,
    WC_SPACE, /* ' ', '\t', '\n', '\v', \f', '\r' */
    WC_NONSPACE,
};
#define WC_ANY_CHAR '.'

/* characters that need to be escaped in bracket expression */
#define BRACKET_ESC_CHARS "-]"

enum OPERATOR_NAME {
    OP_PLUS, OP_STAR, OP_OPT, OP_DUP,
    OP_CONCAT, OP_ALTER
};
#define OP_CHARS "+*?{ |"
#define OP_NAMES_NUM (OP_ALTER + 1)
extern const char* OP_NAME_STRS[];
#define IS_UOP(c) (c == '+' || c == '*' || c == '?')
#define IS_BOP(c) (c == '|')
extern const int OPERATOR_PRECED[OP_NAMES_NUM];
/* is a < b in precedence? */
#define OP_PRECED_LT(a, b) (OPERATOR_PRECED[a] < OPERATOR_PRECED[b])

#define DUP_NUM_MAX 0xFF
#define DUP_STR_MAX_LEN 3
#define DUP_START '{'
#define DUP_SEP ','
#define DUP_END '}'

typedef struct re_token {
    uint8_t type;
    /* type | payload       | payload2
       -----|---------------|---------
       LIT  | byte          | -
       WC   | wildcard enum | -
       DUP  | min           | max
       OP   | op enum       | -        */
    uint8_t payload;
    uint8_t payload2;
} re_token_t;
#define token_size sizeof(re_token_t)
extern int print_re_token(re_token_t token);

typedef struct re_ast {
    re_token_t* tokens;
    int* lefts;
    int* rights;
    int size;
    int root;
} re_ast_t;

#endif
