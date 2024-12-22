#include <stdio.h>
#include <string.h>
#include "re.h"

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
print_re_token(re_token_t token) {
    int printed_length = 0, dup_min, dup_max;
    char* nonprint_pos;
    printed_length + printf("{type=%s, ", TYPE_NAME_STRS[token.type]);
    switch (token.type) {
    case TYPE_LIT:
        nonprint_pos = strchr(NONPRINT_ESC_CHARS_MAP_TO, token.payload);
        if (nonprint_pos != NULL) {
            printed_length += printf("'\\%c'", NONPRINT_ESC_CHARS[
                (int) (nonprint_pos - NONPRINT_ESC_CHARS_MAP_TO)
            ]);
        }
        else {
            printed_length += printf("'%c'", token.payload);
        }
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
        dup_min = token.payload;
        dup_max = token.payload2;
        printed_length +=
            (dup_min == dup_max)
            ? printf("{%d}", dup_min)
            : printf("{%d,%d}", dup_min, dup_max);
        break;
    }
    printed_length += printf("}\n");
};
