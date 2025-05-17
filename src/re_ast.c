#include "re_token.h"
#include "re_ast.h"
#include <stdio.h>
#include <string.h>

void
re_ast_free(re_ast_t* ast)
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

epsnfa
re_ast_to_nfa(const re_ast_t* re_ast, const int is_debug)
{
    epsnfa result;
    tepsnfa* nfas;
    unsigned char* is_visited;
    dynarr_t index_stack;
    dynarr_t char_class_pool = dynarr_new(sizeof(char_class_t));
    int i;
    /* if ast is empty */
    if (re_ast->size == 0) {
        tepsnfa empty = tepsnfa_one_transition(eps_matcher());
        result = tepsnfa_to_epsnfa_and_reduce_eps(&empty);
        tepsnfa_clear(&empty);
        return result;
    }
    /* if ast is not empty */
    nfas = calloc(re_ast->size, sizeof(tepsnfa));
    is_visited = calloc(re_ast->size, sizeof(unsigned char));
    index_stack = dynarr_new(sizeof(int));
    append(&index_stack, &re_ast->root);
    while (index_stack.size > 0) {
        int cur_index = *(int*)back(&index_stack);
        int left_index = re_ast->lefts[cur_index];
        int right_index = re_ast->rights[cur_index];
        re_token_t cur_token = re_ast->tokens[cur_index];

        if (is_debug) {
            printf("cur index: %d ", cur_index);
            printf("left index: %d ", left_index);
            printf("right index: %d ", right_index);
            re_token_print(cur_token);
        }

        if (is_visited[cur_index] == 0
            && (left_index != -1 || right_index != -1)) {
            if (left_index != -1) {
                append(&index_stack, &left_index);
            }
            if (right_index != -1) {
                append(&index_stack, &right_index);
            }
            is_visited[cur_index] = 1;
            continue;
        }
        pop(&index_stack);

        switch (cur_token.type) {
        case TYPE_BYTE:
            nfas[cur_index]
                = tepsnfa_one_transition(byte_matcher(cur_token.payload.byte));
            break;
        case TYPE_ANCHOR:
            nfas[cur_index]
                = tepsnfa_one_transition(anchor_matcher(cur_token.payload.anch)
                );
            break;
        case TYPE_CLASS:
            nfas[cur_index]
                = tepsnfa_one_transition(class_matcher(char_class_pool.size));
            append(&char_class_pool, &cur_token.payload.class);
            break;
        case TYPE_DUP: {
            /* expand the dup operation with concat, alter, opt, and star.
               for example:
               - "a{3,}" become "aaa(a*)"
               - "a{4}" become "aaaa"
               - "a{2,5}" become "aa(a(a(a)?)?)?"
            */
            tepsnfa* cur_nfa = &nfas[cur_index];
            tepsnfa* left_nfa = &nfas[left_index];
            int min = cur_token.payload.dup.min;
            int max = cur_token.payload.dup.max;
            int i;
            *cur_nfa = tepsnfa_deepcopy(left_nfa);
            for (i = 1; i < min; i++) {
                tepsnfa_concat(cur_nfa, left_nfa);
            }
            if (max == DUP_NO_MAX) {
                /* at least min depulication */
                tepsnfa left_star = tepsnfa_deepcopy(left_nfa);
                tepsnfa_to_star(&left_star);
                tepsnfa_concat(cur_nfa, &left_star);
                tepsnfa_clear(&left_star);
            } else if (min == max) {
                /* extact duplication */
                /* do nothing */
            } else {
                tepsnfa tail_opt = tepsnfa_deepcopy(left_nfa);
                int j;
                for (j = 1; j < max - min; j++) {
                    tepsnfa head = tepsnfa_deepcopy(left_nfa);
                    tepsnfa_to_opt(&tail_opt);
                    tepsnfa_concat(&head, &tail_opt);
                    tepsnfa_clear(&tail_opt);
                    tail_opt = head;
                }
                tepsnfa_to_opt(&tail_opt);
                tepsnfa_concat(cur_nfa, &tail_opt);
                tepsnfa_clear(&tail_opt);
            }
            break;
        }
        case TYPE_WC:
            nfas[cur_index]
                = tepsnfa_one_transition(wc_matcher(cur_token.payload.wc));
            break;
        case TYPE_UOP:
            nfas[cur_index] = tepsnfa_deepcopy(&nfas[left_index]);
            switch (cur_token.payload.op) {
            case OP_PLUS:
                tepsnfa_concat(&nfas[cur_index], &nfas[left_index]);
                tepsnfa_to_star(&nfas[cur_index]);
                break;
            case OP_STAR:
                tepsnfa_to_star(&nfas[cur_index]);
                break;
            case OP_OPT:
                tepsnfa_to_opt(&nfas[cur_index]);
                break;
            default:
                printf("error: bad unary operator %d\n", cur_token.payload.op);
                exit(1);
            }
            break;
        case TYPE_BOP:
            if (cur_token.payload.op == OP_CONCAT) {
                nfas[cur_index] = tepsnfa_deepcopy(&nfas[left_index]);
                tepsnfa_concat(&nfas[cur_index], &nfas[right_index]);
            } else if (cur_token.payload.op == OP_ALTER) {
                int is_left_one = nfas[left_index].state_num == 2;
                int is_right_one = nfas[right_index].state_num == 2;
                if (is_left_one) {
                    nfas[cur_index] = tepsnfa_deepcopy(&nfas[right_index]);
                    tepsnfa_fast_union(&nfas[cur_index], &nfas[left_index]);
                } else if (is_right_one) {
                    nfas[cur_index] = tepsnfa_deepcopy(&nfas[left_index]);
                    tepsnfa_fast_union(&nfas[cur_index], &nfas[right_index]);
                } else {
                    nfas[cur_index] = tepsnfa_deepcopy(&nfas[left_index]);
                    tepsnfa_union(&nfas[cur_index], &nfas[right_index]);
                }
            } else {
                printf("error: bad binary operator %d\n", cur_token.payload.op);
                exit(1);
            }
            break;
        default:
            printf("error: bad token type\n");
            exit(1);
        }

        if (is_debug) {
            tepsnfa_print(&nfas[cur_index]);
        }
    }
    result = tepsnfa_to_epsnfa_and_reduce_eps(&nfas[re_ast->root]);
    result.char_class_pool = char_class_pool;
    for (i = 0; i < re_ast->size; i++) {
        tepsnfa_clear(&nfas[i]);
    }
    if (is_debug) {
        epsnfa_print(&result);
    }
    dynarr_free(&index_stack);
    free(is_visited);
    free(nfas);
    return result;
}
