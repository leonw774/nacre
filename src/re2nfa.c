#include "nfa.h"
#include "re.h"
#include <stdio.h>

epsnfa
re2nfa(const re_ast_t* re_ast, const int is_debug)
{
    epsnfa result;
    epsnfa* nfas;
    unsigned char* is_visited;
    dynarr_t index_stack;
    /* if ast is empty */
    if (re_ast->size == 0) {
        return epsnfa_one_transition(EPS_MATCHER());
    }
    /* if ast is not empty */
    nfas = calloc(re_ast->size, sizeof(epsnfa));
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
        case TYPE_LIT:
            nfas[cur_index]
                = epsnfa_one_transition(BYTE_MATCHER(cur_token.payload));
            break;
        case TYPE_ANCHOR:
            nfas[cur_index]
                = epsnfa_one_transition(ANCHOR_MATCHER(cur_token.payload));
            break;
        case TYPE_DUP: {
            /* expand the dup operation with concat, alter, opt, and star.
               for example:
                - "a{2,5}" become "aa(a(a(a)?)?)?"
                - "a{3,}" become "aaa(a*)"
                - "a{4}" become "aaaa"
            */
            epsnfa* cur_nfa = &nfas[cur_index];
            epsnfa* left_nfa = &nfas[left_index];
            int min = cur_token.payload;
            int max = cur_token.payload2;
            int i;
            *cur_nfa = epsnfa_deepcopy(left_nfa);
            for (i = 1; i < min; i++) {
                epsnfa_concat(cur_nfa, left_nfa);
            }
            if (max == DUP_NO_MAX) {
                epsnfa left_star = epsnfa_deepcopy(left_nfa);
                epsnfa_to_star(&left_star);
                epsnfa_concat(cur_nfa, &left_star);
                epsnfa_clear(&left_star);
            } else {
                epsnfa tail_opt = epsnfa_deepcopy(left_nfa);
                int j;
                for (j = 1; j < max - min; j++) {
                    epsnfa head = epsnfa_deepcopy(left_nfa);
                    epsnfa_to_opt(&tail_opt);
                    epsnfa_concat(&head, &tail_opt);
                    epsnfa_clear(&tail_opt);
                    tail_opt = head;
                }
                epsnfa_to_opt(&tail_opt);
                epsnfa_concat(cur_nfa, &tail_opt);
                epsnfa_clear(&tail_opt);
            }
            break;
        }
        case TYPE_WC:
            nfas[cur_index]
                = epsnfa_one_transition(WC_MATCHER(cur_token.payload));
            break;
        case TYPE_UOP:
            nfas[cur_index] = epsnfa_deepcopy(&nfas[left_index]);
            switch (cur_token.payload) {
            case OP_PLUS:
                epsnfa_concat(&nfas[cur_index], &nfas[left_index]);
                epsnfa_to_star(&nfas[cur_index]);
                break;
            case OP_STAR:
                epsnfa_to_star(&nfas[cur_index]);
                break;
            case OP_OPT:
                epsnfa_to_opt(&nfas[cur_index]);
                break;
            }
            break;
        case TYPE_BOP:
            assert(
                cur_token.payload == OP_CONCAT || cur_token.payload == OP_ALTER
            );
            nfas[cur_index] = epsnfa_deepcopy(&nfas[left_index]);
            if (cur_token.payload == OP_CONCAT) {
                epsnfa_concat(&nfas[cur_index], &nfas[right_index]);
            } else {
                epsnfa_union(&nfas[cur_index], &nfas[right_index]);
            }
            break;
        default:
            printf("error: bad token type\n");
            exit(1);
        }

        if (is_debug) {
            epsnfa_print(&nfas[cur_index]);
        }
    }
    result = nfas[re_ast->root];
    free(nfas);
    return result;
}
