#include <stdlib.h>
#include <string.h>
#include "dynarr.h"
#include "stateset.h"
#include "nfa.h"

epsnfa epsnfa_one_state() {
    epsnfa a = {
        .state_num = 1,
        .start_state = 0,
        .final_state = 0,
        .state_transitions = new_dynarr(sizeof(dynarr_t))
    };
    dynarr_t transition_set = new_dynarr(sizeof(transition_t));
    append(&a.state_transitions, &transition_set);
    return a;
}

epsnfa epsnfa_one_transition(matcher_t m) {
    epsnfa a = {
        .state_num = 2,
        .start_state = 0,
        .final_state = 1,
        .state_transitions = new_dynarr(sizeof(dynarr_t))
    };
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
    self->final_state = -1;
}

void
epsnfa_add_state(epsnfa* self) {
    self->state_num++;
    dynarr_t transition_set = new_dynarr(sizeof(transition_t));
    append(&self->state_transitions, &transition_set);
}

int
epsnfa_match(epsnfa* self, const char* input_str, const unsigned long size) {
    typedef struct memory {
        /* the position in input string */
        int pos;
        int cur_state;
        /* is the state visited by current eps path */
        unsigned char* eps_visited;
    } memory_t;

    dynarr_t stack = new_dynarr(sizeof(memory_t));
    int i, res = 0, stateset_size = (self->state_num - 1) / 8 + 1;
    memory_t init_mem = {
        .pos = 0,
        .cur_state = self->start_state,
        .eps_visited = new_stateset(self->state_num)
    };

    append(&stack, &init_mem);
    while (stack.size > 0) {
        const memory_t* cur_mem = back(&stack);
        if (cur_mem->cur_state == self->final_state) {
            res = 1;
            break;
        }

        char raw_input = input_str[cur_mem->pos];
        dynarr_t cur_state_trans =
            ((dynarr_t*) self->state_transitions.data)[cur_mem->cur_state];
        for (i = 0; i < cur_state_trans.size; i++) {
            transition_t* t = at(&cur_state_trans, i);
            if (match_byte(t->matcher, raw_input)) {
                memory_t next_mem = {
                    .pos = 0,
                    .cur_state = t->to_state,
                    .eps_visited = NULL
                };
                if (t->matcher.eps) {
                    int is_eps_visited = stateset_find(
                        next_mem.eps_visited,
                        t->to_state
                    );
                    if (is_eps_visited) {
                        continue;
                    }
                    else {
                        next_mem.pos = cur_mem->pos;
                        next_mem.eps_visited = new_stateset(self->state_num);
                        memcpy(
                            next_mem.eps_visited,
                            cur_mem->eps_visited,
                            stateset_size
                        );
                        stateset_add(next_mem.eps_visited, t->to_state);
                    }
                }
                else if (t->matcher.byte == raw_input) {
                    /* reset the eps visited mask */
                    next_mem.eps_visited = new_stateset(self->state_num);
                    next_mem.pos = cur_mem->pos + 1;
                }
                append(&stack, &next_mem);
            }
        }

        free(cur_mem->eps_visited);
        pop(&stack);
    }

    for (i = 0; i < stack.size; i++) {
        free(((memory_t*) stack.data)[i].eps_visited);
    }
    free_dynarr(&stack);
    return res;
}
