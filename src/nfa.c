#include "nfa.h"
#include "orderedset.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* initialize an epsnfa of one start states, one final states, and one
   transition between them
*/
epsnfa
epsnfa_one_transition(matcher_t m)
{
    static const int init_final_state = 1;
    epsnfa res = {
        .state_num = 2,
        .start_state = 0,
        .final_states = ORDEREDSET(int),
        .state_transitions = dynarr_new(sizeof(dynarr_t)),
    };
    dynarr_t transition_set_0 = dynarr_new(sizeof(transition_t)),
             transition_set_1 = dynarr_new(sizeof(transition_t));
    transition_t t;
    orderedset_add(&res.final_states, &init_final_state);
    append(&res.state_transitions, &transition_set_0);
    append(&res.state_transitions, &transition_set_1);
    t = (transition_t) { .matcher = m, .to_state = 1 };
    append((dynarr_t*)at(&res.state_transitions, 0), &t);
    return res;
}

int
epsnfa_print(epsnfa* self)
{
    int i, byte_count = 0;
    byte_count += printf("--- PRINT EPSNFA ---\n");
    byte_count += printf(
        "Number of state: %d\nStarting state: %d\nFinal states: ",
        self->state_num, self->start_state
    );
    for (i = 0; i < self->final_states.size; i++) {
        byte_count += printf("%d", ((int*)self->final_states.data)[i]);
        if (i != self->final_states.size - 1) {
            byte_count += printf(", ");
        }
    }
    byte_count += printf("\nTransitions:\nstateDiagram\n");
    for (i = 0; i < self->state_transitions.size; i++) {
        int j;
        dynarr_t* transitions = at(&self->state_transitions, i);
        for (j = 0; j < transitions->size; j++) {
            transition_t* t = at(transitions, j);
            char* flag_str;
            char payload_str[8] = {0};
            if (t->matcher.flag == MATCHER_FLAG_EPS) {
                flag_str = "EPS";
            } else if (t->matcher.flag == MATCHER_FLAG_WC) {
                flag_str = "WC";
                sprintf(payload_str, " %d", t->matcher.payload);
            } else if (t->matcher.flag == MATCHER_FLAG_BYTE) {
                flag_str = "BYTE";
                if (isprint(t->matcher.payload)) {
                    sprintf(payload_str, " '%c'", t->matcher.payload);
                } else {
                    sprintf(payload_str, " \\x%x", t->matcher.payload);
                }
            } else {
                flag_str = "UNK";
            }
            byte_count += printf(
                "  %d --> %d: [%s%s]\n", i, t->to_state, flag_str, payload_str
            );
        }
    }
    byte_count += printf("--------------------\n");
    return byte_count;
}

void
epsnfa_clear(epsnfa* self)
{
    int i;
    for (i = 0; i < self->state_transitions.size; i++) {
        dynarr_free((dynarr_t*)at(&self->state_transitions, i));
    }
    dynarr_free(&self->state_transitions);
    self->state_num = 0;
    self->start_state = -1;
    orderedset_free(&self->final_states);
}

void
epsnfa_add_state(epsnfa* self)
{
    self->state_num++;
    dynarr_t transition_set = dynarr_new(sizeof(transition_t));
    append(&self->state_transitions, &transition_set);
}

void
epsnfa_add_transition(epsnfa* self, int from, matcher_t matcher, int to)
{
    dynarr_t* from_transition_set = at(&self->state_transitions, from);
    int i;
    /* check repetition */
    for (i = 0; i < from_transition_set->size; i++) {
        transition_t* t = at(from_transition_set, i);
        if (t->to_state == to
            && memcmp(&t->matcher, &matcher, sizeof(matcher_t)) == 0) {
            return;
        }
    }
    transition_t t = { .matcher = matcher, .to_state = to };
    append(from_transition_set, &t);
}

epsnfa
epsnfa_deepcopy(const epsnfa* self)
{
    int i;
    epsnfa copy = {
        .state_num = self->state_num,
        .start_state = self->start_state,
        .final_states = orderedset_copy(&self->final_states),
        .state_transitions = dynarr_new(sizeof(dynarr_t)),
    };
    for (i = 0; i < self->state_transitions.size; i++) {
        dynarr_t* from_transition_set = at(&self->state_transitions, i);
        dynarr_t to_transition_set = dynarr_new(sizeof(transition_t));
        int j;
        for (j = 0; j < from_transition_set->size; j++) {
            append(&to_transition_set, at(from_transition_set, j));
        }
        append(&copy.state_transitions, &to_transition_set);
    }
    return copy;
}

void
epsnfa_concat(epsnfa* self, const epsnfa* right)
{
    int i;
    orderedset_t new_final_states;
    /* append right's transition to self */
    for (i = 0; i < right->state_transitions.size; i++) {
        dynarr_t* from_transition_set = at(&right->state_transitions, i);
        dynarr_t to_transition_set = dynarr_new(sizeof(transition_t));
        int j;
        for (j = 0; j < from_transition_set->size; j++) {
            transition_t* t = at(from_transition_set, j);
            transition_t new_t = {
                .matcher = t->matcher,
                .to_state = self->state_num + t->to_state,
            };
            append(&to_transition_set, &new_t);
        }
        append(&self->state_transitions, &to_transition_set);
    }
    /* transform final states of right to new by adding self->state_num */
    new_final_states = orderedset_copy(&right->final_states);
    for (i = 0; i < new_final_states.size; i++) {
        ((int*)new_final_states.data)[i] += self->state_num;
    }
    /* add epsilon transition from self's final states to right's start state */
    for (i = 0; i < self->final_states.size; i++) {
        int final = ((int*)self->final_states.data)[i];
        dynarr_t* from_transition_set = at(&self->state_transitions, final);
        transition_t t = {
            .matcher = EPS_MATCHER(),
            .to_state = self->state_num + right->start_state,
        };
        append(from_transition_set, &t);
    }
    /* update final states to new */
    orderedset_free(&self->final_states);
    self->final_states = new_final_states;
    /* update state num */
    self->state_num += right->state_num;
}

void
epsnfa_union(epsnfa* self, const epsnfa* right)
{
    int i;
    orderedset_t new_final_states;
    /* append right's transition to self */
    for (i = 0; i < right->state_transitions.size; i++) {
        dynarr_t* from_transition_set = at(&right->state_transitions, i);
        dynarr_t to_transition_set = dynarr_new(sizeof(transition_t));
        int j;
        for (j = 0; j < from_transition_set->size; j++) {
            transition_t* t = at(from_transition_set, j);
            transition_t new_t = {
                .matcher = t->matcher,
                .to_state = self->state_num + t->to_state,
            };
            append(&to_transition_set, &new_t);
        }
        append(&self->state_transitions, &to_transition_set);
    }
    /* transform final states of right to new by adding self->state_num */
    new_final_states = orderedset_copy(&right->final_states);
    for (i = 0; i < new_final_states.size; i++) {
        ((int*)new_final_states.data)[i] += self->state_num;
    }
    /* add right's final states to new final states */
    orderedset_union(&self->final_states, &new_final_states);
    orderedset_free(&new_final_states);
    /* add new start state and update state num */
    {
        int right_start_state_in_new = self->state_num + right->start_state;
        self->state_num += right->state_num;
        /* add new start state that eps-transition to self and right's start */
        epsnfa_add_state(self);
        /* add new transition from new start state to old start state */
        epsnfa_add_transition(
            self, self->state_num - 1, EPS_MATCHER(), self->start_state
        );
        epsnfa_add_transition(
            self, self->state_num - 1, EPS_MATCHER(), right_start_state_in_new
        );
        self->start_state = self->state_num - 1;
    }
}

void
epsnfa_to_star(epsnfa* self)
{
    int i;
    int star_start = self->state_num;
    int star_final = self->state_num + 1;
    epsnfa_add_state(self);
    epsnfa_add_state(self);
    /* 1. add eps from star-start to self-start */
    epsnfa_add_transition(self, star_start, EPS_MATCHER(), self->start_state);
    for (i = 0; i < self->final_states.size; i++) {
        int final = ((int*)self->final_states.data)[i];
        /* 2. add eps from self-finals to self-start */
        epsnfa_add_transition(self, final, EPS_MATCHER(), self->start_state);
        /* 3. add eps from self-finals to star-final */
        epsnfa_add_transition(self, final, EPS_MATCHER(), star_final);
    }
    /* 4. add eps from star-start to star-final */
    epsnfa_add_transition(self, star_start, EPS_MATCHER(), star_final);
    /* 5. set new start and final state */
    self->start_state = star_start;
    orderedset_free(&self->final_states);
    self->final_states = ORDEREDSET(int);
    orderedset_add(&self->final_states, &star_final);
}

void
epsnfa_to_opt(epsnfa* self)
{
    /* add an eps-transition from start state to each final state */
    int i;
    for (i = 0; i < self->final_states.size; i++) {
        epsnfa_add_transition(
            self, self->start_state, EPS_MATCHER(),
            ((int*)self->final_states.data)[i]
        );
    }
}

int
epsnfa_match(const epsnfa* self, const char* input_str)
{
    typedef struct memory {
        /* the position in input string */
        int pos;
        int cur_state;
        /* is the state visited by current eps path */
        orderedset_t eps_visited;
    } memory_t;
    unsigned long long input_size = strlen(input_str);
    dynarr_t stack = dynarr_new(sizeof(memory_t));
    int i, res = 0;
    memory_t init_mem = {
        .pos = 0,
        .cur_state = self->start_state,
        .eps_visited = ORDEREDSET(int),
    };

    // printf("input string: %s\n", input_str);
    // printf("input size: %lld\n", input_size);
    // printf("---\n");

    append(&stack, &init_mem);
    while (stack.size > 0) {
        memory_t cur_mem = *(memory_t*)back(&stack); /* copy to stack */
        pop(&stack);

        // printf("input pos : %d\n", cur_mem.pos);
        // printf("stack size: %d\n", stack.size+1);
        // printf("cur state : %*d\n", stack.size+1, cur_mem.cur_state);
        // printf("---\n");

        if (cur_mem.pos == input_size
            && orderedset_contains(&self->final_states, &cur_mem.cur_state)) {
            orderedset_free(&cur_mem.eps_visited);
            res = 1;
            break;
        }

        char raw_input = input_str[cur_mem.pos];
        dynarr_t* cur_transitions
            = at(&self->state_transitions, cur_mem.cur_state);
        for (i = 0; i < cur_transitions->size; i++) {
            transition_t* t = at(cur_transitions, i);
            if (match_byte(t->matcher, raw_input)) {
                memory_t next_mem = {
                    .pos = cur_mem.pos,
                    .cur_state = t->to_state,
                };
                if (t->matcher.flag == MATCHER_FLAG_EPS) {
                    /* if the matcher is eps, check if the state is visited */
                    if (orderedset_contains(
                            &cur_mem.eps_visited, &t->to_state
                        )) {
                        /* end this path if visited */
                        continue;
                    }
                    /* otherwise, add this state to visited */
                    next_mem.eps_visited
                        = orderedset_copy(&cur_mem.eps_visited);
                    orderedset_add(&next_mem.eps_visited, &t->to_state);
                } else {
                    /* reset the eps visited set */
                    next_mem.eps_visited = ORDEREDSET(int);
                    next_mem.pos++;
                }
                append(&stack, &next_mem);
            }
        }
        orderedset_free(&cur_mem.eps_visited);
    }

    for (i = 0; i < stack.size; i++) {
        orderedset_free(&((memory_t*)at(&stack, i))->eps_visited);
    }
    dynarr_free(&stack);
    return res;
}
