#include "re_parser.h"
#include "orderedset.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* parse the bracket expression (the thing in side "[]")
        BRACKET_EXPR = CHAR_EXPR
                    | BRACKET_EXPR CHAR_EXPR
                    | BRACKET_EXPR RANGE_EXPR
                    ;
        RANGE_EXPR   = CHAR_EXPR '-' CHAR_EXPR;
        CHAR_EXPR    = CHAR | '\' ESC_CHAR;
        ESC_CHAR     = '-' | ']' | '^' | 'n' | 'r' | 't' | 'v' | 'f';
        CHAR         = [all printables]
   into an equivalent alter expression
        ALTER_EXPR = ALTER_EXPR '|' CHAR_EXPR | CHAR_EXPR;
        CHAR_EXPR  = CHAR | '\' ESC_EXPR;
        ESC_EXPR   = '.' | '+' | '*' | '?' | '|' | '(' | ')' | '{' | '\'
                    | '^' | '$';
*/
dynarr_t
parse_bracket_expr(const char* input_str)
{
    dynarr_t output = dynarr_new(sizeof(re_token_t));
    orderedset_t charset = orderedset_new(sizeof(char), NULL);
    size_t i, input_len = strlen(input_str);
    int j = 0, is_esc = 0, is_range = 0;
    char range_from = 0;
    for (i = 0; i < input_len; i++) {
        char c = input_str[i];
        if (is_range) {
            if (c < range_from) {
                printf(
                    "parse_bracket_expr: bad range: left '%c' is less than "
                    "right '%c'",
                    range_from, c
                );
                exit(1);
            }
            for (j = range_from; j <= c; j++) {
                orderedset_add(&charset, &j);
            }
            is_range = 0;
            continue;
        } else if (is_esc) {
            char* esc_char = strchr(BRACKET_ESC_CHARS, c);
            if (esc_char) {
                orderedset_add(
                    &charset,
                    &BRACKET_ESC_CHARS_TO[esc_char - BRACKET_ESC_CHARS]
                );
            } else {
                printf("parse_bracket_expr: bad escape: '%c'", c);
                exit(1);
            }
            is_esc = 0;
            continue;
        }

        if (c == '\\') {
            is_esc = 1;
        } else if (c == '-') {
            if (i == 0 || i == input_len - 1) {
                printf("parse_bracket_expr: incomplete range expression\n");
                exit(1);
            }
            range_from = input_str[i - 1];
            is_range = 1;
        } else {
            orderedset_add(&charset, &c);
        }
    }

    for (j = 0; j < charset.size; j++) {
        re_token_t t;
        if (j != 0) {
            t = (re_token_t) { .type = TYPE_BOP, .payload = OP_ALTER };
            append(&output, &t);
        }
        t = (re_token_t) {
            .type = TYPE_LIT,
            .payload = ((char*)charset.data)[j],
        };
        append(&output, &t);
    }

    return output;
}

dynarr_t
tokenization(const char* input_str)
{
    size_t i, input_size = strlen(input_str);
    int can_add_concat = 0;

    enum state { ST_ESC, ST_DUP, ST_BRK, ST_NORM };
    int cur_state = ST_NORM;

    int dup_min = -1, dup_max = -1, dup_str_len = 0, is_dup_have_sep = 0;
    char dup_str[DUP_STR_MAX_LEN + 1];

    int brk_str_len = 0, brk_is_esc = 0;
    char brk_str[BRACKET_STR_MAX_LEN + 1];

    re_token_t t;
    const re_token_t CONCAT_OP = { .type = TYPE_BOP, .payload = OP_CONCAT };
    dynarr_t input = dynarr_new(sizeof(re_token_t));

    for (i = 0; i < input_size; i++) {
        char c = input_str[i];
        /* ESCAPE STATE */
        if (cur_state == ST_ESC) {
            if (IS_LIT_ESC(c)) {
                t = (re_token_t) { .type = TYPE_LIT, .payload = c };
            } else if (c == ANCHOR_WEDGE_CHAR) {
                t = (re_token_t) {
                    .type = TYPE_ANCHOR,
                    .payload = ANCHOR_WEDGE,
                };
            } else {
                char* nonprint = IS_NONPRINT_ESC(c);
                char* wc = IS_WC_ESC(c);
                if (nonprint) {
                    t = (re_token_t) {
                        .type = TYPE_LIT,
                        .payload
                        = NONPRINT_ESC_CHARS_TO[nonprint - NONPRINT_ESC_CHARS],
                    };
                } else if (wc) {
                    t = (re_token_t) {
                        .type = TYPE_WC,
                        .payload = wc - WC_ESC_CHARS,
                    };
                } else {
                    printf("error: bad escape sequence '%c'\n", c);
                    exit(1);
                }
            }
            if (can_add_concat) {
                append(&input, &CONCAT_OP);
            }
            append(&input, &t);
            can_add_concat = 1;
            cur_state = ST_NORM;
            continue;
        }
        /* DUPLICATION STATE */
        else if (cur_state == ST_DUP) {
            if (c == DUP_END) {
                t = (re_token_t) { .type = TYPE_DUP, .payload = 0 };
                dup_str[dup_str_len] = '\0';
                if (is_dup_have_sep == 1) {
                    t.payload = dup_min;
                    /* if dup_str is empty (the "{m,}" format),
                    dup_max should be 0 to indicate "no maximum" */
                    t.payload2 = (dup_str_len == 0)
                        ? DUP_NO_MAX
                        : atoi_check_dup_max(dup_str);
                } else {
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
                cur_state = ST_NORM;
            } else if (c == DUP_SEP) {
                /* "{,n}" is not allowed" */
                if (dup_str_len == 0) {
                    printf("error: duplication min is empty\n");
                    exit(1);
                }
                dup_str[dup_str_len] = '\0';
                dup_min = atoi_check_dup_max(dup_str);
                dup_str_len = 0;
                is_dup_have_sep = 1;
            } else if (c == ' ') {
                continue;
            } else {
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
                } else {
                    printf("error: duplication is not number: %c\n", c);
                    exit(1);
                }
            }
            continue;
        }
        /* BRACKET STATE */
        else if (cur_state == ST_BRK) {
            if (c == '\\') {
                brk_is_esc = 1;
            } else if (c == ']') {
                if (brk_is_esc) {
                    brk_str_len++;
                    continue;
                }
                if (brk_str_len > BRACKET_STR_MAX_LEN) {
                    printf(
                        "bracket expression too long (>%d)", BRACKET_STR_MAX_LEN
                    );
                    exit(1);
                }
                if (brk_str_len == 0) {
                    printf("bracket expression is empty");
                    exit(1);
                }
                strncpy(brk_str, input_str + i - brk_str_len, brk_str_len);
                brk_str[brk_str_len] = '\0';
                {
                    dynarr_t alter_exprs = parse_bracket_expr(brk_str);
                    size_t j;
                    if (can_add_concat) {
                        append(&input, &CONCAT_OP);
                    }

                    t = (re_token_t) {
                        .type = TYPE_LP,
                    };
                    append(&input, &t);
                    for (j = 0; j < alter_exprs.size; j++) {
                        append(&input, at(&alter_exprs, j));
                    }
                    t = (re_token_t) {
                        .type = TYPE_RP,
                    };
                    append(&input, &t);

                    can_add_concat = 1;
                    cur_state = ST_NORM;
                }
                cur_state = ST_NORM;
            } else {
                brk_str_len++;
            }
            continue;
        }

        /* NORMAL STATE */
        if (c == '\\') {
            cur_state = ST_ESC;
        } else if (c == '[') {
            cur_state = ST_BRK;
            brk_str_len = 0;
        } else if (c == WC_ANY_CHAR) {
            t = (re_token_t) { .type = TYPE_WC, .payload = WC_ANY };
            if (can_add_concat) {
                append(&input, &CONCAT_OP);
            }
            append(&input, &t);
        } else if (c == ANCHOR_START_CHAR) {
            t = (re_token_t) { .type = TYPE_ANCHOR, .payload = ANCHOR_START };
            if (can_add_concat) {
                append(&input, &CONCAT_OP);
            }
            append(&input, &t);
        } else if (c == ANCHOR_END_CHAR) {
            t = (re_token_t) { .type = TYPE_ANCHOR, .payload = ANCHOR_END };
            if (can_add_concat) {
                append(&input, &CONCAT_OP);
            }
            append(&input, &t);
        } else if (c == DUP_START) {
            cur_state = ST_DUP;
            dup_min = dup_max = -1;
            dup_str_len = is_dup_have_sep = 0;
            memset(dup_str, 0, DUP_STR_MAX_LEN);
        } else if (IS_UOP(c)) {
            t = (re_token_t) {
                .type = TYPE_UOP,
                .payload = strchr(OP_CHARS, c) - OP_CHARS,
            };
            append(&input, &t);
        } else if (IS_BOP(c)) {
            t = (re_token_t) { .type = TYPE_BOP, .payload = OP_ALTER };
            append(&input, &t);
            can_add_concat = 0;
        } else if (c == '(') {
            t = (re_token_t) { .type = TYPE_LP };
            if (can_add_concat) {
                append(&input, &CONCAT_OP);
            }
            append(&input, &t);
        } else if (c == ')') {
            t = (re_token_t) { .type = TYPE_RP };
            append(&input, &t);
        } else {
            t = (re_token_t) { .type = TYPE_LIT, .payload = c };
            if (can_add_concat) {
                append(&input, &CONCAT_OP);
            }
            append(&input, &t);
        }

        /* update can add concat */
        if (c == '\\' || c == DUP_START) {
            /* change state, do nothing */
        } else {
            can_add_concat = !(IS_BOP(c) || c == '(');
        }
    }
    return input;
}

inline int
atoi_check_dup_max(const char dup_str[DUP_STR_MAX_LEN])
{
    int d = atoi(dup_str);
    if (d > DUP_NUM_MAX) {
        printf(
            "error: duplication number over limit(%d): %d\n", DUP_NUM_MAX, d
        );
        exit(1);
    }
    return d;
}

re_ast_t
parse_regex(const char* input_str, const int is_debug)
{
    int i, j;
    dynarr_t input, output;
    dynarr_t op_stack, index_stack;
    re_ast_t ast = {
        .tokens = NULL, .lefts = NULL, .rights = NULL, .size = 0, .root = -1
    };

    /* tokenization */
    input = tokenization(input_str);

    if (is_debug) {
        printf("--- input ---\n");
        for (i = 0; i < input.size; i++) {
            re_token_print(*(re_token_t*)at(&input, i));
        }
    }

    /* transform to postfix notation with shunting yard algorithm */
    output = dynarr_new(sizeof(re_token_t));
    op_stack = dynarr_new(sizeof(re_token_t));
    for (i = 0; i < input.size; i++) {
        re_token_t* cur_token = at(&input, i);
        if (cur_token->type == TYPE_BOP || cur_token->type == TYPE_LP) {
            /* binary operator and left parenthese */
            /* pop stack to output until top is left parenthese or
               the precedence of stack top is not lower */
            re_token_t* stack_top = back(&op_stack);
            while (stack_top != NULL
                   && stack_top->type != TYPE_LP
                   /* parenthese's precedence < all operators */
                   && cur_token->type != TYPE_LP
                   && OP_PRECED_LT(stack_top->payload, cur_token->payload)) {
                append(&output, stack_top);
                pop(&op_stack);
                stack_top = back(&op_stack);
            }
            /* push into op stack */
            append(&op_stack, cur_token);
        } else if (cur_token->type == TYPE_RP) {
            /* left parenthese */
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
        } else {
            /* unary operator, wildcard, and literal */
            append(&output, cur_token);
        }

        if (is_debug) {
            printf("output:\n");
            for (j = 0; j < output.size; j++) {
                printf("  ");
                re_token_print(*(re_token_t*)at(&output, j));
            }
            printf("op_stack:\n");
            for (j = 0; j < op_stack.size; j++) {
                printf("  ");
                re_token_print(*(re_token_t*)at(&op_stack, j));
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
            re_token_print(*(re_token_t*)at(&output, i));
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
        case TYPE_ANCHOR:
            append(&index_stack, &i);
            break;
        case TYPE_UOP:
        case TYPE_DUP:
            ast.lefts[i] = *(int*)back(&index_stack);
            pop(&index_stack);
            append(&index_stack, &i);
            break;
        case TYPE_BOP:
            ast.rights[i] = *(int*)back(&index_stack);
            pop(&index_stack);
            ast.lefts[i] = *(int*)back(&index_stack);
            pop(&index_stack);
            append(&index_stack, &i);
            break;
        default:
            printf("error building tree: bad token type\n");
            exit(1);
            break;
        }

        if (is_debug) {
            printf("%d cur_token: ", i);
            re_token_print(*cur_token);
            printf("index_stack:\n");
            for (j = 0; j < index_stack.size; j++) {
                printf(" %d", *(int*)at(&index_stack, j));
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
    ast.root = *(int*)index_stack.data;
    dynarr_free(&index_stack);

    return ast;
}
