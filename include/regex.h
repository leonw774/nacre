#include <stdlib.h>
#include <stdint.h>
#include "dynarr.h"

#ifndef REGEX_PARSER_H
#define REGEX_PARSER_H

enum regex_token_type {
    TYPE_LIT, /* literal */
    TYPE_WC, /* wildcard */
    TYPE_UOP, /* unary operators */
    TYPE_DUP, /* duplicator */
    TYPE_BOP, /* binary operators */
    TYPE_LP, /* left and right parenthese */
    TYPE_RP
};
#define REGEX_TYPES_NUM (TYPE_RP + 1)
extern const char* TYPE_NAME_STRS[REGEX_TYPES_NUM];

#define LIT_ESC_CHARS "+*?|(){\\"
#define WC_ESC_CHARS ".dDwWsS"
#define ISWCESC(c) (strchr(WC_ESC_CHARS, c))
#define ISLITESC(c) (strchr(LIT_ESC_CHARS, c))

enum WILDCARD_NAME {
    WC_ANY,
    WC_DIGIT, /* 0-9 */
    WC_NONDIGIT,
    WC_WORD, /* 0-9A-Za-z_ */
    WC_NONWORD,
    WC_SPACE, /* ' ', '\t', '\n', '\v', \f', '\r' */
    WC_NONSPACE,
};
#define WC_ANY_CHAR '.'

enum OPERATOR_NAME {
    OP_PLUS, OP_STAR, OP_OPT, OP_DUP,
    OP_CONCAT, OP_ALTER
};
#define OP_CHARS "+*?{ |"
#define OP_NAMES_NUM (OP_ALTER + 1)
extern const char* OP_NAME_STRS[];
#define ISUOP(c) (c == '+' || c == '*' || c == '?')
#define ISBOP(c) (c == '|')
extern const int OPERATOR_PRECED[OP_NAMES_NUM];
/* is a < b in precedence? */
#define OP_PRECED_LT(a, b) (OPERATOR_PRECED[a] < OPERATOR_PRECED[b])

#define DUP_NUM_MAX 0xFF
#define DUP_STR_MAX_LEN 3
#define DUP_START '{'
#define DUP_SEP ','
#define DUP_END '}'

typedef struct regex_token {
    uint8_t type;
    /* type | payload
       -----|-----
       LIT  | byte
       WC   | wildcard enum
       DUP  | low byte: min, high byte: max
       OP   | op enum */
    uint16_t payload;
} regex_token_t;
#define token_size sizeof(regex_token_t)
extern int print_regex_token(regex_token_t token);

typedef struct regex_ast {
    regex_token_t* tokens;
    int* lefts;
    int* rights;
    int size;
    int root;
} regex_ast_t;

#endif
