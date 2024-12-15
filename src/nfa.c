#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "nfa.h"

epsnfa epsnfa_one_state() {
    epsnfa a = {
        .state_num = 1,
        .start_state = 0,
        .final_states = new_stateset(1),
        .state_transitions = new_dynarr(sizeof(dynarr_t))
    };
    stateset_add(a.final_states, 0);
    dynarr_t transition_set = new_dynarr(sizeof(transition_t));
    append(&a.state_transitions, &transition_set);
    return a;
}

epsnfa epsnfa_one_transition(matcher_t m) {
    epsnfa a = {
        .state_num = 2,
        .start_state = 0,
        .final_states = new_stateset(2),
        .state_transitions = new_dynarr(sizeof(dynarr_t))
    };
    stateset_add(a.final_states, 1);
    dynarr_t transition_set_0 = new_dynarr(sizeof(transition_t)),
        transition_set_1 = new_dynarr(sizeof(transition_t));
    append(&a.state_transitions, &transition_set_0);
    append(&a.state_transitions, &transition_set_1);
    transition_t t = {
        .matcher = m,
        .to_state = 1
    };
    append((dynarr_t*) at(&a.state_transitions, 0), &t);
    return a;
}

void epsnfa_clear(epsnfa* self) {
    int i;
    for (i = 0; i < self->state_transitions.size; i++) {
        free_dynarr((dynarr_t*) at(&self->state_transitions, i));
    }
    free_dynarr(&self->state_transitions);
    self->state_num = 0;
    self->start_state = -1;
    self->final_states = NULL;
}

void
epsnfa_add_state(epsnfa* self) {
    self->state_num++;
    dynarr_t transition_set = new_dynarr(sizeof(transition_t));
    append(&self->state_transitions, &transition_set);
}

void
epsnfa_add_transition(epsnfa* self, int from, matcher_t matcher, int to) {
    dynarr_t* from_transition_set = at(&self->state_transitions, from);
    int i;
    /* check repetition */
    for (i = 0; i < from_transition_set->size; i++) {
        transition_t* t = at(from_transition_set, i);
        if (t->matcher.byte == matcher.byte
            && t->matcher.eps == matcher.eps
            && t->to_state == to
        ) {
            return;
        }
    }
    transition_t t = {
        .matcher = matcher,
        .to_state = to
    };
    append(from_transition_set, &t);
}

int
epsnfa_match(
    const epsnfa* self,
    const char* input_str
) {
    typedef struct memory {
        /* the position in input string */
        int pos;
        int cur_state;
        /* is the state visited by current eps path */
        unsigned char* eps_visited;
    } memory_t;
    unsigned long long input_size = strlen(input_str);
    dynarr_t stack = new_dynarr(sizeof(memory_t));
    int i, res = 0, stateset_size = (self->state_num - 1) / 8 + 1;
    memory_t init_mem = {
        .pos = 0,
        .cur_state = self->start_state,
        .eps_visited = new_stateset(self->state_num)
    };

    // printf("input string: %s\n", input_str);
    // printf("input size: %d\n", input_size);
    // printf("---\n");

    append(&stack, &init_mem);
    while (stack.size > 0) {
        memory_t cur_mem = *(memory_t*) back(&stack); /* copy to stack */
        pop(&stack);
        
        // printf("input pos : %d\n", cur_mem.pos);
        // printf("stack size: %d\n", stack.size+1);
        // printf("cur state : %*d\n", stack.size+1, cur_mem.cur_state);
        // printf("---\n");
       
        if (cur_mem.pos == input_size
            && stateset_find(self->final_states, cur_mem.cur_state)
        ) {
            res = 1;
            break;
        }

        char raw_input = input_str[cur_mem.pos];
        dynarr_t* cur_state_trans =
            at(&self->state_transitions, cur_mem.cur_state); 
        for (i = 0; i < cur_state_trans->size; i++) {
            transition_t* t = at(cur_state_trans, i);
            if (match_byte(t->matcher, raw_input)) {
                memory_t next_mem = {
                    .pos = cur_mem.pos,
                    .cur_state = t->to_state,
                    .eps_visited = NULL
                };
                if (t->matcher.eps) {
                    if (stateset_find(cur_mem.eps_visited, t->to_state)) {
                        continue;
                    }
                    else {
                        next_mem.eps_visited = new_stateset(self->state_num);
                        memcpy(
                            next_mem.eps_visited,
                            cur_mem.eps_visited,
                            stateset_size
                        );
                        stateset_add(next_mem.eps_visited, t->to_state);
                    }
                }
                else if (t->matcher.byte == raw_input) {
                    /* reset the eps visited mask */
                    next_mem.eps_visited = new_stateset(self->state_num);
                    next_mem.pos++;
                }
                else {
                    continue;
                }
                append(&stack, &next_mem);
            }
        }

        free(cur_mem.eps_visited);
    }

    for (i = 0; i < stack.size; i++) {
        free(((memory_t*) at(&stack, i))->eps_visited);
    }
    free_dynarr(&stack);
    return res;
}
