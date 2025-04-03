#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "re_parser.h"

/* parse the bracket expression (the thing in side "[]")
    BRACKET_EXPR = CHAR_EXPR
                 | BRACKET_EXPR CHAR_EXPR
                 | BRACKET_EXPR RANGE_EXPR
                 ;
    RANGE_EXPR   = CHAR_EXPR '-' CHAR_EXPR;
    CHAR_EXPR    = CHAR | '\' ESC_CHAR;
    ESC_CHAR     = '-' | ']' |  | 'n' | 'r' | 't' | 'v' | 'f';
    CHAR         = [all printables]
*/
// re_ast_t
// parse_bracket_expr(char* input_str, const int is_debug) {

// }

dynarr_t tokenization(const char* input_str) {
    enum state { ST_ESC, ST_DUP, ST_BRK, ST_NOR};
    int i, can_add_concat = 0, cur_state = ST_NOR;
    int dup_min = -1, dup_max = -1, dup_str_len = 0, is_dup_have_sep = 0;
    char c;
    char dup_str[DUP_STR_MAX_LEN];
    size_t input_size = strlen(input_str);
    dynarr_t input = dynarr_new(sizeof(re_token_t));
    re_token_t t;
    const re_token_t CONCAT_OP = {
        .type = TYPE_BOP,
        .payload = OP_CONCAT
    };

    for (i = 0; i < input_size; i++) {
        c = input_str[i];
        /* ESCAPE STATE */
        if (cur_state == ST_ESC) {
            if (IS_LIT_ESC(c)) {
                t = (re_token_t) {.type = TYPE_LIT, .payload = c};
            }
            else {
                char* is_nonprint = IS_NONPRINT_ESC(c);
                char* is_wc = IS_WC_ESC(c);
                if (is_nonprint) {
                    t = (re_token_t) {
                        .type = TYPE_LIT,
                        .payload = NONPRINT_ESC_CHARS_MAP_TO[
                            is_nonprint - NONPRINT_ESC_CHARS
                        ]
                    };
                }
                else if (is_wc) {
                    t = (re_token_t) {
                        .type = TYPE_WC,
                        .payload = is_wc - WC_ESC_CHARS
                    };
                }
                else {
                    printf("error: bad escape sequence '%c'\n", c);
                    exit(1);
                }
            }
            if (can_add_concat) {
                append(&input, &CONCAT_OP);
            }
            append(&input, &t);
            can_add_concat = 1;
            cur_state = ST_NOR;
            continue;
        }
        /* DUPLICATION STATE */
        else if (cur_state == ST_DUP) {
            if (c == DUP_END) {
                t = (re_token_t) {.type = TYPE_DUP, .payload = 0};
                dup_str[dup_str_len] = '\0';
                if (is_dup_have_sep == 1) {
                    t.payload = dup_min;
                    /* if dup_str is empty (the "{m,}" format),
                    dup_max should be 0 to indicate "no maximum" */
                    t.payload2 = (dup_str_len == 0)
                        ? DUP_NO_MAX : atoi_check_dup_max(dup_str);
                }
                else {
                    /* "{}" is not allowed" */
                    if (dup_str_len == 0) {
                        printf("error: duplication min is empty\n");
                        exit(1);
                    }
                    /* the "{m}" format */
                    dup_min = atoi_check_dup_max(dup_str);
                    t.payload = t.payload2 = dup_min;
                }
                append(&input, &t);
                can_add_concat = 1;
                cur_state = ST_NOR;
            }
            else if (c == DUP_SEP) {
                /* "{,n}" is not allowed" */
                if (dup_str_len == 0) {
                    printf("error: duplication min is empty\n");
                    exit(1);
                }
                dup_str[dup_str_len] = '\0';
                dup_min = atoi_check_dup_max(dup_str);
                dup_str_len = 0;
                is_dup_have_sep = 1;
            }
            else if (c == ' ') {
                continue;
            }
            else {
                if (dup_str_len >= DUP_STR_MAX_LEN) {
                    printf(
                        "error: duplication number length over limit(%d): %d\n",
                        DUP_STR_MAX_LEN, dup_str_len
                    );
                    exit(1);
                }
                if (isdigit(c)) {
                    dup_str[dup_str_len] = c;
                    dup_str_len++;
                }
                else {
                    printf("error: duplication is not number: %c\n", c);
                    exit(1);
                }
            }
            continue;
        }

        /* NORMAL STATE */
        if (c == '\\') {
            cur_state = ST_ESC;
        }
        else if (c == WC_ANY_CHAR) {
            t = (re_token_t) {.type = TYPE_WC, .payload = WC_ANY};
            if (can_add_concat) {
                append(&input, &CONCAT_OP);
            }
            append(&input, &t);
            can_add_concat = 1;
        }
        else if (c == DUP_START) {
            cur_state = ST_DUP;
            dup_min = dup_max = -1;
            dup_str_len = is_dup_have_sep = 0;
            memset(dup_str, 0, DUP_STR_MAX_LEN);
        }
        else if (IS_UOP(c)) {
            t = (re_token_t) {
                .type = TYPE_UOP, 
                .payload = strchr(OP_CHARS, c) - OP_CHARS
            };
            append(&input, &t);
            can_add_concat = 1;
        }
        else if (IS_BOP(c)) {
            t = (re_token_t) {.type = TYPE_BOP, .payload = OP_ALTER};
            append(&input, &t);
            can_add_concat = 0;
        }
        else if (c == '(') {
            t = (re_token_t) {.type = TYPE_LP};
            if (can_add_concat) {
                append(&input, &CONCAT_OP);
            }
            append(&input, &t);
            can_add_concat = 0;
        }
        else if (c == ')') {
            t = (re_token_t) {.type = TYPE_RP};
            append(&input, &t);
            can_add_concat = 1;
        }
        else {
            t = (re_token_t) {.type = TYPE_LIT, .payload = c};
            if (can_add_concat) {
                append(&input, &CONCAT_OP);
            }
            append(&input, &t);
            can_add_concat = 1;
        }
    }
    return input;
}

inline int
atoi_check_dup_max(const char dup_str[DUP_STR_MAX_LEN]) {
    int d = atoi(dup_str);
    if (d > DUP_NUM_MAX) {
        printf(
            "error: duplication number over limit(%d): %d\n",
            DUP_NUM_MAX,
            d
        );
        exit(1);
    }
    return d;
}

re_ast_t
parse_regex(const char* input_str, const int is_debug) {
    int i, j;
    dynarr_t input, output;
    dynarr_t op_stack, index_stack;
    re_ast_t ast = {
        .tokens = NULL,
        .lefts = NULL,
        .rights = NULL,
        .size = 0,
        .root = -1
    };

    /* tokenization */
    input = tokenization(input_str);

    if (is_debug) {
        printf("--- input ---\n");
        for (i = 0; i < input.size; i++) {
            re_token_print(*(re_token_t*) at(&input, i));
        }
    }

    /* transform to postfix notation with shunting yard algorithm */
    output = dynarr_new(sizeof(re_token_t));
    op_stack = dynarr_new(sizeof(re_token_t));
    for (i = 0; i < input.size; i++) {
        re_token_t* cur_token = at(&input, i);
        /* binary operator and left parenthese */
        if (cur_token->type == TYPE_BOP || cur_token->type == TYPE_LP) {
            /* pop stack to output until top is left parenthese or
               the precedence of stack top is not lower */
            re_token_t* stack_top = back(&op_stack);
            while (
                stack_top != NULL && stack_top->type != TYPE_LP
                /* parenthese's precedence < all operators */
                && cur_token->type != TYPE_LP
                && OP_PRECED_LT(stack_top->payload, cur_token->payload)
            ) {
                append(&output, stack_top);
                pop(&op_stack);
                stack_top = back(&op_stack);
            }
            /* push into op stack */
            append(&op_stack, cur_token);
        }
        /* left parenthese */
        else if (cur_token->type == TYPE_RP) {
            /* pop stack and push to output until top is left braclet */
            re_token_t* stack_top = back(&op_stack);
            while (stack_top != NULL && stack_top->type != TYPE_LP) {
                append(&output, stack_top);
                pop(&op_stack);
                stack_top = back(&op_stack);
            }

            /* pop out the left bracket */
            if (stack_top != NULL) {
                pop(&op_stack);
            }
        }
        /* unary operator, wildcard, and literal */
        else {
            append(&output, cur_token);
        }
        if (is_debug) {
            printf("output:\n");
            for (j = 0; j < output.size; j++) {
                printf("  ");
                re_token_print(*(re_token_t*) at(&output, j));
            }
            printf("op_stack:\n");
            for (j = 0; j < op_stack.size; j++) {
                printf("  ");
                re_token_print(*(re_token_t*) at(&op_stack, j));
            }
        }
    }
    /* pop op stack to output until it is empty */
    while (op_stack.size != 0) {
        append(&output, back(&op_stack));
        pop(&op_stack);
    }
    dynarr_free(&input);
    dynarr_free(&op_stack);

    if (is_debug) {
        printf("--- postfix result ---\n");
        for (i = 0; i < output.size; i++) {
            re_token_print(*(re_token_t*) at(&output, i));
        }
    }

    // expanded_output = expand_dups(&output);

    if (is_debug) {
        printf("--- build ast tree ---\n");
    }

    ast.tokens = output.data;
    ast.size = output.size;
    ast.lefts = malloc(output.size * sizeof(int));
    ast.rights = malloc(output.size * sizeof(int));
    memset(ast.lefts, 0xFF, output.size * sizeof(int));
    memset(ast.rights, 0xFF, output.size * sizeof(int));
    index_stack = dynarr_new(sizeof(int));
    for (i = 0; i < output.size; i++) {
        re_token_t* cur_token = &ast.tokens[i];
        switch (cur_token->type) {
        case TYPE_LIT:
        case TYPE_WC:
            append(&index_stack, &i);
            break;
        case TYPE_UOP:
        case TYPE_DUP:
            ast.lefts[i] = *(int*) back(&index_stack);
            pop(&index_stack);
            append(&index_stack, &i);
            break;
        case TYPE_BOP:
            ast.rights[i] = *(int*) back(&index_stack);
            pop(&index_stack);
            ast.lefts[i] = *(int*) back(&index_stack);
            pop(&index_stack);
            append(&index_stack, &i);
            break;
        default:
            printf("error building tree: bad token type\n");
            exit(1);
            break;
        }

        if (is_debug) {
            printf("%d cur_token: ", i); re_token_print(*cur_token);
            printf("index_stack:\n");
            for (j = 0; j < index_stack.size; j++) {
                printf(" %d", *(int*) at(&index_stack, j));
            }
            printf("\n");
        }
    }
    if (index_stack.size != 1) {
        printf(
            "error when making ast: index stack size is %d at the end\n",
            index_stack.size
        );
        exit(1);
    }
    ast.root = *(int*) index_stack.data;
    dynarr_free(&index_stack);

    return ast;
}
