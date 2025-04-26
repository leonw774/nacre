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
        .final_state = 1,
        .state_transitions = dynarr_new(sizeof(dynarr_t)),
    };
    dynarr_t transition_set_0 = dynarr_new(sizeof(transition_t)),
             transition_set_1 = dynarr_new(sizeof(transition_t));
    transition_t t;
    res.final_state = init_final_state;
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
        "Number of state: %d\nStarting state: %d\nFinal states: %d\n"
        "Transitions:\nstateDiagram\n",
        self->state_num, self->start_state, self->final_state
    );
    for (i = 0; i < self->state_transitions.size; i++) {
        int j;
        dynarr_t* transitions = at(&self->state_transitions, i);
        for (j = 0; j < transitions->size; j++) {
            transition_t* t = at(transitions, j);
            char* flag_str;
            char payload_str[8] = { 0 };
            if (t->matcher.flag & MATCHER_FLAG_EPS) {
                if (t->matcher.flag & MATCHER_FLAG_ANCHOR) {
                    flag_str = "ANCH";
                    if (t->matcher.payload == ANCHOR_START) {
                        payload_str[0] = ANCHOR_START_CHAR;
                    } else if (t->matcher.payload == ANCHOR_END) {
                        payload_str[0] = ANCHOR_END_CHAR;
                    } else if (t->matcher.flag) {
                        payload_str[0] = ANCHOR_WEDGE_CHAR;
                    }
                    payload_str[1] = '\0';
                } else {
                    flag_str = "EPS";
                }
            } else if (t->matcher.flag & MATCHER_FLAG_WC) {
                flag_str = "WC";
                sprintf(payload_str, "%d", t->matcher.payload);
            } else if (t->matcher.flag & MATCHER_FLAG_BYTE) {
                flag_str = "BYTE";
                if (isprint(t->matcher.payload)) {
                    sprintf(payload_str, "'%c'", t->matcher.payload);
                } else {
                    sprintf(payload_str, "\\x%x", t->matcher.payload);
                }
            } else {
                flag_str = "UNK";
                sprintf(payload_str, "%d", t->matcher.flag);
            }
            byte_count += printf(
                "  %d --> %d: [%s %s]\n", i, t->to_state, flag_str, payload_str
            );
        }
    }
    byte_count += printf("--------------------\n");
    fflush(stdout);
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
    self->final_state = -1;
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
        .final_state = self->final_state,
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
    int i, j;
    transition_t new_t;
    /* append right's transition to self */
    for (i = 0; i < right->state_transitions.size; i++) {
        dynarr_t* from_transition_set = at(&right->state_transitions, i);
        dynarr_t to_transition_set = dynarr_new(sizeof(transition_t));
        for (j = 0; j < from_transition_set->size; j++) {
            transition_t* t = at(from_transition_set, j);
            new_t = (transition_t) {
                .matcher = t->matcher,
                .to_state = self->state_num + t->to_state,
            };
            append(&to_transition_set, &new_t);
        }
        append(&self->state_transitions, &to_transition_set);
    }
    /* transition to self's final become to right's start */
    for (i = 0; i < self->state_transitions.size; i++) {
        dynarr_t* transition_set = at(&self->state_transitions, i);
        for (j = 0; j < transition_set->size; j++) {
            transition_t* t = at(transition_set, j);
            if (t->to_state == self->final_state) {
                t->to_state = self->state_num + right->start_state;
            }
        }
    }
    /* transform self's final to right's final by adding self->state_num */
    self->final_state = right->final_state + self->state_num;
    self->state_num += right->state_num;
}

void
epsnfa_union(epsnfa* self, const epsnfa* right)
{
    int i, j;
    int new_start = self->state_num + right->state_num;
    int new_final = new_start + 1;
    /* append right's transition to self */
    for (i = 0; i < right->state_transitions.size; i++) {
        dynarr_t* from_transition_set = at(&right->state_transitions, i);
        dynarr_t to_transition_set = dynarr_new(sizeof(transition_t));
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
    /* add transition set for new state */
    {
        dynarr_t new_start_transition_set = dynarr_new(sizeof(transition_t));
        dynarr_t new_final_transition_set = dynarr_new(sizeof(transition_t));
        append(&self->state_transitions, &new_start_transition_set);
        append(&self->state_transitions, &new_final_transition_set);
    }
    /* add eps from new start to self start and right start */
    epsnfa_add_transition(self, new_start, EPS_MATCHER(), self->start_state);
    epsnfa_add_transition(
        self, new_start, EPS_MATCHER(), right->start_state + self->state_num
    );
    /* add eps from self final and right final to new final */
    epsnfa_add_transition(self, self->final_state, EPS_MATCHER(), new_final);
    epsnfa_add_transition(
        self, right->final_state + self->state_num, EPS_MATCHER(), new_final
    );
    /* set new start and final state */
    self->final_state = new_final;
    self->start_state = new_start;
    self->state_num += right->state_num + 2;
}

void
epsnfa_bracket_union(epsnfa* self, const epsnfa* right)
{
    assert(right->state_transitions.size == 2);
    dynarr_t* right_start_transition_set
        = at(&right->state_transitions, right->start_state);
    assert(right_start_transition_set->size == 1);
    epsnfa_add_transition(
        self, self->start_state,
        ((transition_t*)at(right_start_transition_set, 0))->matcher,
        self->final_state
    );
}

void
epsnfa_to_star(epsnfa* self)
{
    int star_start = self->state_num;
    int star_final = self->state_num + 1;
    epsnfa_add_state(self);
    epsnfa_add_state(self);
    /* add eps from star-start to self-start */
    epsnfa_add_transition(self, star_start, EPS_MATCHER(), self->start_state);
    /* add eps from self-finals to self-start */
    epsnfa_add_transition(
        self, self->final_state, EPS_MATCHER(), self->start_state
    );
    /* add eps from self-finals to star-final */
    epsnfa_add_transition(self, self->final_state, EPS_MATCHER(), star_final);
    /* add eps from star-start to star-final */
    epsnfa_add_transition(self, star_start, EPS_MATCHER(), star_final);
    /* set new start and final state */
    self->start_state = star_start;
    self->final_state = star_final;
}

void
epsnfa_to_opt(epsnfa* self)
{
    /* add an eps-transition from start state to final state */
    epsnfa_add_transition(
        self, self->start_state, EPS_MATCHER(), self->final_state
    );
}

/* return n if n is the smallest integer such that input_str[0:n] matches
   return 0 if no match found */
size_t
epsnfa_match(
    const epsnfa* self, const char* input_str, const anchor_byte behind_input
)
{
    typedef struct memory {
        /* the position in input string */
        int cur_state;
        size_t pos;
        /* is the state visited by current eps path */
        orderedset_t eps_visited;
    } memory_t;
    size_t matched_len = 0;
    size_t input_size = strlen(input_str);
    dynarr_t stack = dynarr_new(sizeof(memory_t));
    int i;
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
        size_t cur_pos = cur_mem.pos;
        pop(&stack);

        // printf("input pos : %d\n", cur_pos);
        // printf("stack size: %d\n", stack.size+1);
        // printf("cur state : %*d\n", stack.size+1, cur_mem.cur_state);
        // printf("---\n");

        if (self->final_state == cur_mem.cur_state) {
            orderedset_free(&cur_mem.eps_visited);
            if (cur_pos > matched_len) {
                matched_len = cur_pos;
            }
        }

        dynarr_t* cur_transitions
            = at(&self->state_transitions, cur_mem.cur_state);
        for (i = 0; i < cur_transitions->size; i++) {
            transition_t* t = at(cur_transitions, i);
            matcher_t m = t->matcher;
            int is_matched = 0;
            if (m.flag & MATCHER_FLAG_ANCHOR) {
                anchor_byte behind
                    = (cur_pos == 0) ? behind_input : input_str[cur_pos - 1];
                anchor_byte ahead = (cur_pos >= input_size - 1)
                    ? ANCHOR_BYTE_END
                    : input_str[cur_pos];
                is_matched = match_anchor(m.payload, behind, ahead);
            } else if (cur_pos >= input_size - 1) {
                continue;
            } else {
                is_matched = match_byte(m, input_str[cur_pos]);
            }

            if (is_matched) {
                memory_t next_mem = {
                    .pos = cur_pos,
                    .cur_state = t->to_state,
                };
                if (m.flag & MATCHER_FLAG_EPS) {
                    /* if the matcher is eps, check if the state is visited */
                    int is_loopback = orderedset_contains(
                        &cur_mem.eps_visited, &t->to_state
                    );
                    /* end this path if visited */
                    if (is_loopback) {
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
    return matched_len;
}
