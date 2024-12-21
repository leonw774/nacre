#include <stdio.h>
#include "regex.h"

const char* TYPE_NAME_STRS[] = {
    "LIT",
    "WC",
    "UOP",
    "DUP",
    "BOP",
    "LP",
    "RP",
};

const int OPERATOR_PRECED[] = {
    1, 1, 1, 1,
    2, 3
};

const char* OP_NAME_STRS[] = {
    "PLUS", "STAR", "OPT", "DUP",
    "CONCAT", "ALTER"
};

int
print_regex_token(regex_token_t token) {
    int printed_length = 0, dup_min, dup_max;
    printed_length + printf("{type=%s, ", TYPE_NAME_STRS[token.type]);
    switch (token.type) {
    case TYPE_LIT:
        printed_length += printf("'%c'", token.payload);
        break;
    case TYPE_WC:
        printed_length +=
            (token.payload == WC_ANY)
            ? printf(".")
            : printf("\\%c", WC_ESC_CHARS[token.payload]);
        break;
    case TYPE_UOP:
    case TYPE_BOP:
        printed_length += printf(OP_NAME_STRS[token.payload]);
        break;
    case TYPE_DUP:
        dup_min = token.payload & 0xFF;
        dup_max = token.payload >> 8;
        printed_length +=
            (dup_min == dup_max)
            ? printf("{%d}", dup_min)
            : printf("{%d,%d}", dup_min, dup_max);
        break;
    }
    printed_length += printf("}\n");
};
