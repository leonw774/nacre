#include "re.h"
#include <stdio.h>
#include <string.h>

const char* TYPE_NAME_STRS[] = {
    "LIT", "WC", "UOP", "DUP", "BOP", "LP", "RP", "ANCHOR",
};

const char* LIT_ESC_CHARS = ".+*?|(){\\";
const char* NONPRINT_ESC_CHARS = "nrtvf";
const char* NONPRINT_ESC_CHARS_TO = "\n\r\t\v\f";
const char* WC_ESC_CHARS = "dDwWsS";
const char* BRACKET_ESC_CHARS = "-]nrtvf";
const char* BRACKET_ESC_CHARS_TO = "-]\n\r\t\v\f";

const char* OP_CHARS = "+*?{ ||";
const char* OP_NAME_STRS[] = {
    "PLUS", "STAR", "OPT", "DUP", "CONCAT", "ALTER", "BRACKET_ALTER",
};
const int OP_PRECED[] = { 1, 1, 1, 1, 2, 3, 3 };

int
re_token_print(re_token_t token)
{
    int byte_count = 0, dup_min, dup_max;
    char* nonprint_pos;
    byte_count += printf("{type=%s, ", TYPE_NAME_STRS[token.type]);
    switch (token.type) {
    case TYPE_LIT:
        nonprint_pos = strchr(NONPRINT_ESC_CHARS_TO, token.payload);
        if (nonprint_pos != NULL) {
            byte_count += printf(
                "'\\%c'",
                NONPRINT_ESC_CHARS[(int)(nonprint_pos - NONPRINT_ESC_CHARS_TO)]
            );
        } else {
            byte_count += printf("'%c'", token.payload);
        }
        break;
    case TYPE_WC:
        byte_count += (token.payload == WC_ANY)
            ? printf(".")
            : printf("\\%c", WC_ESC_CHARS[token.payload]);
        break;
    case TYPE_UOP:
    case TYPE_BOP:
        byte_count += printf(OP_NAME_STRS[token.payload]);
        break;
    case TYPE_DUP:
        dup_min = token.payload;
        dup_max = token.payload2;
        if (dup_min == dup_max) {
            byte_count += printf("{%d}", dup_min);
        } else if (dup_max == DUP_NO_MAX) {
            byte_count += printf("{%d,}", dup_min);
        } else {
            printf("{%d,%d}", dup_min, dup_max);
        }
        break;
    case TYPE_ANCHOR:
        if (token.payload <= ANCHOR_WEDGE_CHAR) {
            printf("%c", "^$b"[token.payload]);
        }
        break;
    case TYPE_LP:
    case TYPE_RP:
        break;
    default:
        printf("invalid type");
    }
    byte_count += printf("}\n");
    return byte_count;
};

void re_ast_free(re_ast_t* ast)
{
    if (ast == NULL) {
        return;
    }
    if (ast->tokens) {
        free(ast->tokens);
    }
    if (ast->lefts != NULL) {
        free(ast->lefts);
    }
    if (ast->rights != NULL) {
        free(ast->rights);
    }
}
