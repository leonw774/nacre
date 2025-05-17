#include "re_parser.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/*
    parse the bracket expression (the thing in side "[...]" or "[^...]")

    ```
        BRACKET_EXPR = CHAR_EXPR
                    | BRACKET_EXPR CHAR_EXPR
                    | BRACKET_EXPR RANGE_EXPR
                    ;
        RANGE_EXPR   = CHAR_EXPR '-' CHAR_EXPR;
        CHAR_EXPR    = CHAR | '\' ESC_CHAR;
        ESC_CHAR     = '-' | ']' | '^' | 'n' | 'r' | 't' | 'v' | 'f';
        CHAR         = [all printables]
    ```

    into an class matcher
*/
char_class_t
parse_bracket_expr(const char* input_str)
{
    typedef struct bracket_token {
        uint8_t is_range_char; // 1 if is range charactor "-", else 0
        uint8_t byte;
        int wc; // -1 if not wildcard
    } bracket_token_t;
    dynarr_t intermidiate = dynarr_new(sizeof(bracket_token_t));
    char_class_t char_class = {
        .bitmap = { 0 },
        .is_negated = 0,
    };
    size_t i, input_len = strlen(input_str);
    int j = 0, is_esc = 0, is_range = 0;
    char range_from = 0;

    /* transform escaped char to intermidiate bracket_token token */
    for (i = 0; i < input_len; i++) {
        char c = input_str[i];
        bracket_token_t t;
        if (is_esc) {
            char* esc_char = strchr(BRACKET_ESC_CHARS, c);
            char* wc_char = strchr(WC_ESC_CHARS, c);
            if (esc_char) {
                t = (bracket_token_t) {
                    .is_range_char = 0,
                    .byte = BRACKET_ESC_CHARS_TO[esc_char - BRACKET_ESC_CHARS],
                    .wc = -1,
                };
                append(&intermidiate, &t);
            } else if (wc_char) {
                t = (bracket_token_t) {
                    .is_range_char = 0,
                    .byte = 0,
                    .wc = wc_char - WC_ESC_CHARS,
                };
                append(&intermidiate, &t);
            } else {
                printf("parse_bracket_expr: bad escape: '%c'", c);
                exit(1);
            }
        } else if (c == '\\') {
            is_esc = 1;
        } else if (c == '-') {
            t = (bracket_token_t) {
                .is_range_char = 1,
                .byte = 0,
                .wc = -1,
            };
            append(&intermidiate, &t);
        } else {
            t = (bracket_token_t) {
                .is_range_char = 0,
                .byte = c,
                .wc = -1,
            };
            append(&intermidiate, &t);
        }
    }

    memset(char_class.bitmap, 0, 32 * sizeof(unsigned char));
    for (i = 0; i < intermidiate.size; i++) {
        bracket_token_t* t = at(&intermidiate, i);
        if (is_range) {
            if (t->wc != -1 || t->is_range_char) {
                fprintf(stderr, "bracket expr: bad range\n");
                exit(1);
            }
            if (t->byte < range_from) {
                printf(
                    "bracket expr: bad range: left '%c' < right '%c'\n",
                    range_from, t->byte
                );
                exit(1);
            }
            for (j = range_from; j <= t->byte; j++) {
                char_class_set(&char_class, j);
            }
            is_range = 0;
        } else if (t->is_range_char) {
            if (i == 0 || i == input_len - 1) {
                printf("bracket expr: incomplete range expression\n");
                exit(1);
            } else {
                bracket_token_t* prev_t = at(&intermidiate, i - 1);
                if (prev_t->wc != -1 || prev_t->is_range_char) {
                    fprintf(stderr, "bracket expr: bad range\n");
                    exit(1);
                }
                range_from = t->byte;
                is_range = 1;
            }
        } else if (t->wc != -1) {
            for (j = 0; j < 256; j++) {
                if (match_wc(t->wc, j)) {
                    char_class_set(&char_class, j);
                }
            }
        } else {
            char_class_set(&char_class, t->byte);
        }
    }
    return char_class;
}

dynarr_t
tokenize(const char* input_str)
{
    size_t i, input_size = strlen(input_str);
    if (input_size > RE_STR_LEN_LIMIT) {
        printf("regexp str too long: %lu > %d\n", input_size, RE_STR_LEN_LIMIT);
        exit(1);
    }
    int can_add_concat = 0;

    enum state { ST_ESC, ST_DUP, ST_BRK, ST_NORM };
    int cur_state = ST_NORM;

    int dup_min = -1, dup_max = -1, dup_str_len = 0, is_dup_have_sep = 0;
    char dup_str[DUP_STR_MAX_LEN + 1];

    int brk_str_len = 0, brk_is_esc = 0, brk_is_neg = 0;
    char brk_str[BRACKET_STR_MAX_LEN + 1];

    re_token_t t;
    const re_token_t CONCAT_OP = {
        .type = TYPE_BOP,
        .payload = { .op = OP_CONCAT },
    };
    dynarr_t tokens = dynarr_new(sizeof(re_token_t));

    for (i = 0; i < input_size; i++) {
        char c = input_str[i];
        /* ESCAPE STATE */
        if (cur_state == ST_ESC) {
            if (strchr(BYTE_ESC_CHARS, c)) {
                t = (re_token_t) {
                    .type = TYPE_BYTE,
                    .payload = { .byte = c },
                };
            } else if (c == ANCHOR_WEDGE_CHAR) {
                t = (re_token_t) {
                    .type = TYPE_ANCHOR,
                    .payload = { .anch = ANCHOR_WEDGE },
                };
            } else {
                char* np = IS_NONPRINT_ESC(c);
                char* wc = IS_WC_ESC(c);
                if (np) {
                    uint8_t b = NONPRINT_ESC_CHARS_TO[np - NONPRINT_ESC_CHARS];
                    t = (re_token_t) {
                        .type = TYPE_BYTE,
                        .payload = { .byte = b },
                    };
                } else if (wc) {
                    t = (re_token_t) {
                        .type = TYPE_WC,
                        .payload = { .wc = wc - WC_ESC_CHARS },
                    };
                } else {
                    printf("error: bad escape sequence '%c'\n", c);
                    exit(1);
                }
            }
            if (can_add_concat) {
                append(&tokens, &CONCAT_OP);
            }
            append(&tokens, &t);
            can_add_concat = 1;
            cur_state = ST_NORM;
            continue;
        }
        /* DUPLICATION STATE */
        else if (cur_state == ST_DUP) {
            if (c == DUP_END) {
                t = (re_token_t) {
                    .type = TYPE_DUP,
                    .payload = { .dup = { .min = 0, .max = 0 } },
                };
                dup_str[dup_str_len] = '\0';
                if (is_dup_have_sep == 1) {
                    t.payload.dup.min = dup_min;
                    /* if dup_str is empty (the "{m,}" format),
                    dup_max should be 0 to indicate "no maximum" */
                    t.payload.dup.max = (dup_str_len == 0)
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
                    t.payload.dup.min = t.payload.dup.max = dup_min;
                }
                append(&tokens, &t);
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
            if (c == '^') {
                if (brk_str_len == 0) {
                    brk_is_neg = 1;
                } else if (brk_is_esc) {
                    brk_str_len++;
                    brk_is_esc = 0;
                } else {
                    printf(
                        "bracket expr: \"^\" should appear at the "
                        "begining of braket or escaped\n"
                    );
                    exit(1);
                }
            } else if (c == '\\') {
                brk_is_esc = 1;
            } else if (c == ']') {
                if (brk_str_len > BRACKET_STR_MAX_LEN) {
                    printf(
                        "bracket expr too long (>%d)\n", BRACKET_STR_MAX_LEN
                    );
                    exit(1);
                }
                if (brk_str_len == 0) {
                    printf("bracket expr: is empty\n");
                    exit(1);
                }
                if (brk_is_esc) {
                    brk_str_len++;
                    brk_is_esc = 0;
                    continue;
                }
                strncpy(brk_str, input_str + i - brk_str_len, brk_str_len);
                brk_str[brk_str_len] = '\0';
                {
                    char_class_t char_class = parse_bracket_expr(brk_str);
                    char_class.is_negated = brk_is_neg;
                    if (can_add_concat) {
                        append(&tokens, &CONCAT_OP);
                    }
                    t = (re_token_t) {
                        .type = TYPE_CLASS,
                        .payload = { .class = char_class },
                    };
                    append(&tokens, &t);
                    can_add_concat = 1;
                    cur_state = ST_NORM;
                }
                cur_state = ST_NORM;
            } else {
                brk_str_len++;
                brk_is_esc = 0;
            }
            continue;
        }

        /* NORMAL STATE */
        if (c == '\\') {
            cur_state = ST_ESC;
        } else if (c == '[') {
            cur_state = ST_BRK;
            brk_str_len = 0;
            brk_is_neg = 0;
        } else if (c == WC_ANY_CHAR) {
            t = (re_token_t) { .type = TYPE_WC, .payload = { .wc = WC_ANY } };
            if (can_add_concat) {
                append(&tokens, &CONCAT_OP);
            }
            append(&tokens, &t);
        } else if (c == ANCHOR_START_CHAR) {
            t = (re_token_t) {
                .type = TYPE_ANCHOR,
                .payload = { .anch = ANCHOR_START },
            };
            if (can_add_concat) {
                append(&tokens, &CONCAT_OP);
            }
            append(&tokens, &t);
        } else if (c == ANCHOR_END_CHAR) {
            t = (re_token_t) {
                .type = TYPE_ANCHOR,
                .payload = { .anch = ANCHOR_END },
            };
            if (can_add_concat) {
                append(&tokens, &CONCAT_OP);
            }
            append(&tokens, &t);
        } else if (c == DUP_START) {
            cur_state = ST_DUP;
            dup_min = dup_max = -1;
            dup_str_len = is_dup_have_sep = 0;
            memset(dup_str, 0, DUP_STR_MAX_LEN);
        } else if (IS_UOP(c)) {
            t = (re_token_t) {
                .type = TYPE_UOP,
                .payload = { .op = strchr(OP_CHARS, c) - OP_CHARS },
            };
            append(&tokens, &t);
        } else if (IS_BOP(c)) {
            t = (re_token_t) {
                .type = TYPE_BOP,
                .payload = { .op = OP_ALTER },
            };
            append(&tokens, &t);
            can_add_concat = 0;
        } else if (c == '(') {
            t = (re_token_t) { .type = TYPE_LP };
            if (can_add_concat) {
                append(&tokens, &CONCAT_OP);
            }
            append(&tokens, &t);
        } else if (c == ')') {
            t = (re_token_t) { .type = TYPE_RP };
            append(&tokens, &t);
        } else {
            t = (re_token_t) { .type = TYPE_BYTE, .payload = { .byte = c } };
            if (can_add_concat) {
                append(&tokens, &CONCAT_OP);
            }
            append(&tokens, &t);
        }

        /* update can add concat */
        if (cur_state == ST_NORM) {
            can_add_concat = !(IS_BOP(c) || c == '(');
        }
    }
    return tokens;
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
    /* list of re_token_t */
    dynarr_t input_tokens, postfix_tokens;
    dynarr_t op_stack, index_stack;
    re_ast_t ast = {
        .tokens = NULL, .lefts = NULL, .rights = NULL, .size = 0, .root = -1
    };

    /* tokenization */
    input_tokens = tokenize(input_str);

    if (is_debug) {
        printf("--- input_tokens ---\n");
        for (i = 0; i < input_tokens.size; i++) {
            re_token_print(*(re_token_t*)at(&input_tokens, i));
        }
    }

    /* transform to postfix notation with shunting yard algorithm */
    postfix_tokens = dynarr_new(sizeof(re_token_t));
    op_stack = dynarr_new(sizeof(re_token_t));
    for (i = 0; i < input_tokens.size; i++) {
        re_token_t* cur_token = at(&input_tokens, i);
        if (cur_token->type == TYPE_BOP || cur_token->type == TYPE_LP) {
            /* binary operator and left parenthese */
            /* pop stack to output until top is left parenthese or
               the precedence of stack top is not lower */
            re_token_t* stack_top = back(&op_stack);
            while (stack_top != NULL
                   && stack_top->type != TYPE_LP
                   /* parenthese's precedence < all operators */
                   && cur_token->type != TYPE_LP
                   && OP_PRECED_LT(stack_top->payload.op, cur_token->payload.op)
            ) {
                append(&postfix_tokens, stack_top);
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
                append(&postfix_tokens, stack_top);
                pop(&op_stack);
                stack_top = back(&op_stack);
            }

            /* pop out the left bracket */
            if (stack_top != NULL) {
                pop(&op_stack);
            }
        } else {
            /* unary operator, wildcard, and literal */
            append(&postfix_tokens, cur_token);
        }

        if (is_debug) {
            printf("output:\n");
            for (j = 0; j < postfix_tokens.size; j++) {
                printf("  ");
                re_token_print(*(re_token_t*)at(&postfix_tokens, j));
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
        append(&postfix_tokens, back(&op_stack));
        pop(&op_stack);
    }
    dynarr_free(&input_tokens);
    dynarr_free(&op_stack);

    if (is_debug) {
        printf("--- postfix result ---\n");
        for (i = 0; i < postfix_tokens.size; i++) {
            re_token_print(*(re_token_t*)at(&postfix_tokens, i));
        }
    }

    // expanded_output = expand_dups(&output);

    if (is_debug) {
        printf("--- build ast tree ---\n");
    }

    ast.tokens = postfix_tokens.data;
    ast.size = postfix_tokens.size;
    ast.lefts = malloc(postfix_tokens.size * sizeof(int));
    ast.rights = malloc(postfix_tokens.size * sizeof(int));
    memset(ast.lefts, 0xFF, postfix_tokens.size * sizeof(int));
    memset(ast.rights, 0xFF, postfix_tokens.size * sizeof(int));
    index_stack = dynarr_new(sizeof(int));
    for (i = 0; i < postfix_tokens.size; i++) {
        re_token_t* cur_token = &ast.tokens[i];
        switch (cur_token->type) {
        case TYPE_BYTE:
        case TYPE_WC:
        case TYPE_ANCHOR:
        case TYPE_CLASS:
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
