#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "regex_parser.h"

/* for parsing the bracket expression */
regex_ast_t parse_bracket_expr(char* input_str, const int is_debug) {

}

regex_ast_t parse_regex(char* input_str, const int is_debug) {
    enum state { ST_ESC, ST_DUP, ST_EXP};
    int i, j, l, r, cur_state = ST_EXP, can_add_concat = 0;

    int dup_min = -1, dup_max = -1, dup_str_len = 0;
    char dup_num_str[DUP_STR_MAX_LEN + 1];


    size_t input_size = strlen(input_str);
    dynarr_t input = new_dynarr(sizeof(regex_token_t));
    dynarr_t output = new_dynarr(sizeof(regex_token_t));
    dynarr_t op_stack = new_dynarr(sizeof(regex_token_t));
    dynarr_t index_stack = new_dynarr(sizeof(regex_token_t));
    regex_ast_t ast = {
        .tokens = NULL,
        .lefts = NULL,
        .rights = NULL,
        .size = 0,
        .root = -1
    };
    const regex_token_t CONCAT_OP = {
        .type = TYPE_BOP,
        .payload = OP_CONCAT
    };

    /* tokenization */
    for (i = 0; i < input_size; i++) {
        char c = input_str[i];
        regex_token_t t;
        if (cur_state == ST_ESC) {
            if (ISLITESC(c)) {
                t = (regex_token_t) {
                    .type = TYPE_LIT,
                    .payload = c
                };
            }
            else {
                char* is_wc = ISWCESC(c);
                if (is_wc) {
                    t = (regex_token_t) {
                        .type = TYPE_WC,
                        .payload = is_wc - WC_ESC_CHARS
                    };
                }
            }
            if (can_add_concat) {
                append(&input, &CONCAT_OP);
            }
            append(&input, &t);
            can_add_concat = 1;
            cur_state = ST_EXP;
            continue;
        }
        else if (cur_state == ST_DUP) {
            if (c == DUP_END) {
                t = (regex_token_t) {
                    .type = TYPE_DUP,
                    .payload = 0
                };

                if (dup_min != -1) {
                    dup_max = atoi(dup_num_str);
                    if (dup_max > DUP_NUM_MAX) {
                        printf(
                            "error: duplication number over limit(%d): %d\n",
                            DUP_NUM_MAX,
                            dup_max
                        );
                        exit(1);
                    }
                    t.payload = dup_min | (dup_max << 8);
                }
                else {
                    dup_min = atoi(dup_num_str);
                    if (dup_min > DUP_NUM_MAX) {
                        printf(
                            "error: duplication number over limit(%d): %d\n",
                            DUP_NUM_MAX,
                            dup_min
                        );
                        exit(1);
                    }
                    t.payload = dup_min | (dup_min << 8);
                }

                append(&input, &t);
                can_add_concat = 1;
                cur_state = ST_EXP;
            }
            else if (c == DUP_SEP) {
                dup_min = atoi(dup_num_str);
                if (dup_min > DUP_NUM_MAX) {
                    printf(
                        "error: duplication number over limit(%d): %d\n",
                        DUP_NUM_MAX,
                        dup_min
                    );
                    exit(1);
                }
                dup_str_len = 0;
                memset(dup_num_str, 0, DUP_STR_MAX_LEN);
            }
            else if (c == ' ') {
                continue;
            }
            else {
                if (dup_str_len >= DUP_STR_MAX_LEN) {
                    printf(
                        "error: duplication number length over limit(%d): %d\n",
                        DUP_STR_MAX_LEN,
                        dup_str_len
                    );
                    exit(1);
                }
                if (isdigit(c)) {
                    dup_num_str[dup_str_len] = c;
                    dup_str_len++;
                }
                else {
                    printf("error: duplication is not number: %c\n", c);
                    exit(1);
                }
            }
            continue;
        }

        if (c == '\\') {
            cur_state = ST_ESC;
        }
        else if (c == WC_ANY_CHAR) {
            t = (regex_token_t) {
                .type = TYPE_WC,
                .payload = WC_ANY
            };
            if (can_add_concat) {
                append(&input, &CONCAT_OP);
            }
            append(&input, &t);
            can_add_concat = 1;
        }
        else if (c == DUP_START) {
            cur_state = ST_DUP;
            dup_min = -1;
            dup_max = -1;
            dup_str_len = 0;
            memset(dup_num_str, 0, DUP_STR_MAX_LEN);
        }
        else if (ISUOP(c)) {
            t = (regex_token_t) {
                .type = TYPE_UOP,
                .payload =
                    (c == '+') ? OP_PLUS : (
                        (c == '*') ? OP_STAR : OP_OPT
                    )
            };
            append(&input, &t);
            can_add_concat = 1;
        }
        else if (ISBOP(c)) {
            t = (regex_token_t) {
                .type = TYPE_BOP,
                .payload = OP_ALTER
            };
            append(&input, &t);
            can_add_concat = 0;
        }
        else if (c == '(') {
            t = (regex_token_t) {
                .type = TYPE_LP,
                .payload = 0
            };
            if (can_add_concat) {
                append(&input, &CONCAT_OP);
            }
            append(&input, &t);
            can_add_concat = 0;
        }
        else if (c == ')') {
            t = (regex_token_t) {
                .type = TYPE_RP,
                .payload = 0
            };
            append(&input, &t);
            can_add_concat = 1;
        }
        else {
            t = (regex_token_t) {
                .type = TYPE_LIT,
                .payload = c
            };
            if (can_add_concat) {
                append(&input, &CONCAT_OP);
            }
            append(&input, &t);
            can_add_concat = 1;
        }
    }

    if (is_debug) {
        printf("--- input ---\n");
        for (i = 0; i < input.size; i++) {
            print_regex_token(*(regex_token_t*) at(&input, i));
        }
    }

    /* transform to postfix notation */
    for (i = 0; i < input.size; i++) {
        regex_token_t* cur_token = at(&input, i);
        if (cur_token->type == TYPE_BOP || cur_token->type == TYPE_LP) {
            /* pop stack to output until top is left parenthese or
               the precedence of stack top is not lower */
            regex_token_t* stack_top = back(&op_stack);
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
        else if (cur_token->type == TYPE_RP) {
            /* pop stack and push to output until top is left braclet */
            regex_token_t* stack_top = back(&op_stack);
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
        else {
            append(&output, cur_token);
        }
        if (is_debug) {
            printf("output:\n");
            for (j = 0; j < output.size; j++) {
                printf("  ");
                print_regex_token(*(regex_token_t*) at(&output, j));
            }
            printf("op_stack:\n");
            for (j = 0; j < op_stack.size; j++) {
                printf("  ");
                print_regex_token(*(regex_token_t*) at(&op_stack, j));
            }
        }
    }
    /* pop op stack to output until it is empty */
    while (op_stack.size != 0) {
        append(&output, back(&op_stack));
        pop(&op_stack);
    }
    free_dynarr(&input);
    free_dynarr(&op_stack);

    if (is_debug) {
        printf("--- postfix result ---\n");
        for (i = 0; i < output.size; i++) {
            print_regex_token(*(regex_token_t*) at(&output, i));
        }
        printf("--- build ast tree ---\n");
    }

    ast.tokens = output.data;
    ast.size = output.size;
    ast.lefts = malloc(output.size * sizeof(int));
    ast.rights = malloc(output.size * sizeof(int));
    memset(ast.lefts, 0xFF, output.size * sizeof(int));
    memset(ast.rights, 0xFF, output.size * sizeof(int));
    for (i = 0; i < output.size; i++) {
        regex_token_t* cur_token = &ast.tokens[i];
        switch (cur_token->type) {
        case TYPE_LIT:
        case TYPE_WC:
            append(&index_stack, &i);
            break;
        case TYPE_UOP:
        case TYPE_DUP:
            ast.rights[i] = *(int*) back(&index_stack);
            pop(&index_stack);
            append(&index_stack, &i);
            break;
        case TYPE_BOP:
            ast.lefts[i] = *(int*) back(&index_stack);
            pop(&index_stack);
            ast.rights[i] = *(int*) back(&index_stack);
            pop(&index_stack);
            append(&index_stack, &i);
            break;
        }

        if (is_debug) {
            printf("cur_token: "); print_regex_token(*cur_token);
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
    free_dynarr(&index_stack);
    return ast;
}

/* pre-calculate some operations:
    if the oparend(s) of DUP and CONCAT are the pure literals
    do real string duplication and concat to them */
void precalc_ast(regex_ast_t* ast) {

}
