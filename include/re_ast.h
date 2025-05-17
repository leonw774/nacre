#include "dynarr.h"
#include "char_class.h"
#include "nfa.h"
#include <stdint.h>
#include <stdlib.h>

#ifndef RE_AST_H
#define RE_AST_H

typedef struct re_ast {
    re_token_t* tokens;
    int* lefts;
    int* rights;
    int size;
    int root;
} re_ast_t;

void re_ast_free(re_ast_t* ast);

extern epsnfa re_ast_to_nfa(const re_ast_t* re_ast, const int is_debug);

#endif
