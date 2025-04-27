#include "nfa.h"
#include "bitmask.h"
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
    epsnfa res = { .state_num = 2,
                   .start_state = 0,
                   .final_state = 1,
                   .state_transitions = dynarr_new(sizeof(dynarr_t)) };
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
    int i, j, byte_count = 0;
    byte_count += printf("--- PRINT EPSNFA ---\n");
    byte_count += printf(
        "Number of state: %d\nStarting state: %d\nFinal states: %d\n"
        "Transitions:\nstateDiagram\n",
        self->state_num, self->start_state, self->final_state
    );
    for (i = 0; i < self->state_transitions.size; i++) {
        dynarr_t* transitions = at(&self->state_transitions, i);
        for (j = 0; j < transitions->size; j++) {
            transition_t* t = at(transitions, j);
            byte_count += printf(
                "  %d --> %d: %s\n", i, t->to_state, get_matcher_str(t->matcher)
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
    int i, j;
    assert(right->state_transitions.size == 2);
    dynarr_t* right_start_transition_set
        = at(&right->state_transitions, right->start_state);
    assert(right_start_transition_set->size == 1);
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
    epsnfa_add_transition(
        self, self->start_state, EPS_MATCHER(),
        right->start_state + self->state_num
    );
    epsnfa_add_transition(
        self, right->final_state + self->state_num, EPS_MATCHER(),
        self->final_state
    );
    self->state_num += 2;
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

nfa
epsnfa_reduce(epsnfa* input)
{
    int i, j;
    nfa output;
    int* state_map = malloc(sizeof(int) * input->state_num);
    int* state_degrees = calloc(input->state_num, sizeof(int));
    /* zero empty state */
    for (i = 0; i < input->state_transitions.size; i++) {
        dynarr_t* transition_set = at(&input->state_transitions, i);
        for (j = 0; j < transition_set->size; j++) {
            transition_t* t = at(transition_set, j);
            state_degrees[i]++;
            state_degrees[t->to_state]++;
        }
    }
    {
        int nonempty_state_count = 0;
        memset(state_map, -1, sizeof(int) * input->state_num);
        for (i = 0; i < input->state_num; i++) {
            if (state_degrees[i] != 0) {
                // printf(
                //     "%d -> %d (%d)\n", i, nonempty_state_count,
                //     state_degrees[i]
                // );
                state_map[i] = nonempty_state_count;
                nonempty_state_count++;
            }
        }
        output = (nfa) {
            .state_num = nonempty_state_count,
            .start_state = state_map[input->start_state],
            .is_finish = bitmask_new(input->state_num),
            .transition_table = malloc(
                nonempty_state_count * nonempty_state_count * sizeof(matcher_t)
            ),
        };
    }
    // epsnfa_print(input);
    /* set finish state set */
    bitmask_add(&output.is_finish, state_map[input->final_state]);
    /* fill transition table */
    for (i = 0; i < input->state_num; i++) {
        dynarr_t* transition_set = at(&input->state_transitions, i);
        for (j = 0; j < transition_set->size; j++) {
            transition_t* t = at(transition_set, j);
            int k = state_map[i] * output.state_num + state_map[t->to_state];
            output.transition_table[k] = t->matcher;
        }
    }
    // nfa_print(&output);
    free(state_degrees);
    free(state_map);
    /* reduce epsilon */
    bitmask_t visited = bitmask_new(output.state_num);
    for (i = 0; i < output.state_num; i++) {
        nfa_reduce_state(&output, i, &visited);
    }
    bitmask_free(&visited);
    /* TODO: remove all out edge from zero in-degree state (except starts) */

    /* TODO: remove empty state */

    return output;
}

void
nfa_get_eps_closure(nfa* self, int state, bitmask_t* closure)
{
    if (bitmask_contains(closure, state)) {
        return;
    }
    bitmask_add(closure, state);
    for (int i = 0; i < self->state_num; i++) {
        matcher_t m = self->transition_table[state * self->state_num + i];
        /* use "==" because need to exclude anchor */
        if (m.flag == MATCHER_FLAG_EPS) {
            nfa_get_eps_closure(self, i, closure);
        }
    }
}

void
nfa_reduce_state(nfa* self, int state, bitmask_t* visited)
{
    int i, j;
    bitmask_t closure = bitmask_new(self->state_num);
    /* get epsilon closure of s */
    nfa_get_eps_closure(self, state, &closure);
    // printf("closure %d:", state);
    // for (i = 0; i < self->state_num; i++) {
    //     if (bitmask_contains(&closure, i)) {
    //         printf(" %d", i);
    //     }
    // }
    // printf("\n");
    /* for each transition (s_from, p, s_to) in NFA */
    for (i = 0; i < self->state_num; i++) {
        /* if s_from != s and s_from is in closure */
        if (i != state && bitmask_contains(&closure, i)) {
            for (j = 0; j < self->state_num; j++) {
                matcher_t m = self->transition_table[i * self->state_num + j];
                if (m.flag == MATCHER_FLAG_NIL) {
                    continue;
                }
                /* add (s, p, s_to) to NFA */
                // printf("add (%d, %s, %d)\n", state, get_matcher_str(m), j);
                self->transition_table[state * self->state_num + j] = m;
            }
        }
    }
    /* if exists (s, eps, s') where s' is in closure and s' is finish state then
       also set s as finish state */
    for (j = 0; j < self->state_num; j++) {
        if (bitmask_contains(&closure, j)
            && bitmask_contains(&self->is_finish, j)) {
            bitmask_add(&self->is_finish, state);
        }
    }
    /* remove eps transitions from s to another states in the closure */
    for (j = 0; j < self->state_num; j++) {
        if (bitmask_contains(&closure, j)) {
            int k = state * self->state_num + j;
            matcher_t m = self->transition_table[k];
            if (m.flag == MATCHER_FLAG_EPS) {
                // printf("remove (%d, ?, %d)\n", state, j);
                self->transition_table[k] = NIL_MATCHER();
            }
        }
    }
    // nfa_print(self);
    /* for all transition (s, p, s') in NFA */
    for (j = 0; j < self->state_num; j++) {
        /* if s' is not visited do recude(s') */
        matcher_t m = self->transition_table[state * self->state_num + j];
        if (m.flag != MATCHER_FLAG_NIL && !bitmask_contains(visited, j)) {
            nfa_reduce_state(self, j, visited);
        }
        bitmask_add(visited, j);
    }
    bitmask_free(&closure);
}

void
nfa_print(nfa* self)
{
    int i, j;
    printf("----- PRINT NFA -----\n");
    printf(
        "Number of state: %d\nStarting state: %d\nFinal states:\n",
        self->state_num, self->start_state
    );
    for (i = 0; i < self->state_num; i++) {
        if (bitmask_contains(&self->is_finish, i)) {
            printf(" %d", i);
        }
    }
    printf("\nTransitions:\n\nstateDiagram\n");
    for (i = 0; i < self->state_num; i++) {
        for (j = 0; j < self->state_num; j++) {
            int k = i * self->state_num + j;
            matcher_t m = self->transition_table[k];
            if (m.flag) {
                printf("  %d --> %d: %s\n", i, j, get_matcher_str(m));
            }
        }
    }
    printf("--------------------\n");
}

void
nfa_clear(nfa* self)
{
    bitmask_free(&self->is_finish);
    free(self->transition_table);
}

/* return n if n is the largest integer such that input_str[0:n] matches
   return 0 if no match found */
size_t
nfa_match(
    const nfa* self, const char* input_str, const size_t intput_len,
    const size_t start_offset
)
{
    typedef struct memory {
        int cur_state;
        int depth;
        /* the position in input string */
        size_t pos;
    } memory_t;
    int i;
    size_t matched_len = 0;
    dynarr_t stack = dynarr_new(sizeof(memory_t));
    memory_t init_mem = {
        .pos = 0,
        .depth = 0,
        .cur_state = self->start_state,
    };

    /* depth-first search */
    append(&stack, &init_mem);
    while (stack.size > 0) {
        memory_t cur_mem = *(memory_t*)back(&stack);
        size_t cur_pos = cur_mem.pos;
        pop(&stack);

        // printf("input pos : %lu\n", cur_pos);
        // printf("cur char  : %c\n", input_str[start_offset + cur_pos]);
        // printf("stack size: %d\n", stack.size + 1);
        // printf("cur state : %*d\n", stack.size + 1, cur_mem.cur_state);
        // // printf("---\n");

        if (bitmask_contains(&self->is_finish, cur_mem.cur_state)) {
            matched_len = cur_pos;
            break;
        }

        if (cur_mem.depth == MATCH_SEARCH_DEPTH_LIMIT) {
            continue;
        }

        matcher_t m;
        int is_matched;
        anchor_byte behind, ahead;
        for (i = 0; i < self->state_num; i++) {
            m = self->transition_table[cur_mem.cur_state * self->state_num + i];
            if (m.flag == MATCHER_FLAG_NIL) {
                continue;
            }
            // printf("%d %s\n", i, get_matcher_str(m));
            if (m.flag & MATCHER_FLAG_ANCHOR) {
                if (cur_pos == 0 && start_offset == 0) {
                    behind = ANCHOR_BYTE_START;
                } else {
                    behind = input_str[start_offset + cur_pos - 1];
                }
                if (cur_pos + start_offset > intput_len - 1) {
                    ahead = ANCHOR_BYTE_END;
                } else {
                    ahead = input_str[start_offset + cur_pos];
                }
                is_matched = match_anchor(m.payload, behind, ahead);
            } else if (cur_pos + start_offset <= intput_len - 1) {
                is_matched = match_byte(m, input_str[start_offset + cur_pos]);
            } else {
                continue;
            }

            if (is_matched) {
                memory_t next_mem = {
                    .pos = cur_pos,
                    .depth = cur_mem.depth,
                    .cur_state = i,
                };
                if ((m.flag & MATCHER_FLAG_ANCHOR)) {
                    // printf("anchor %c%c\n", behind, input_str[start_offset +
                    // cur_pos]);
                } else {
                    next_mem.pos++;
                    // printf("match %c\n", input_str[start_offset + cur_pos]);
                }
                append(&stack, &next_mem);
            }
        }
        // bitmask_free(&cur_mem.eps_visited);
    }
    // for (i = 0; i < stack.size; i++) {
    //     bitmask_free(&((memory_t*)at(&stack, i))->eps_visited);
    // }
    dynarr_free(&stack);
    return matched_len;
}
