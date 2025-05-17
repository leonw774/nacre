#include "nfa.h"
#include "bitmask.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* initialize an epsnfa of one start states, one final states, and one
   transition between them
*/
tepsnfa
tepsnfa_one_transition(matcher_t m)
{
    static const int init_final_state = 1;
    tepsnfa res = { .state_num = 2,
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

size_t
tepsnfa_print(tepsnfa* self)
{
    size_t i, j, byte_count = 0;
    byte_count += printf("--- PRINT T-EPSNFA ---\n");
    byte_count += printf(
        "Number of state: %lu\nStarting state: %lu\nFinal states: %lu\n"
        "Transitions:\nstateDiagram\n",
        self->state_num, self->start_state, self->final_state
    );
    for (i = 0; i < self->state_transitions.size; i++) {
        dynarr_t* transitions = at(&self->state_transitions, i);
        for (j = 0; j < transitions->size; j++) {
            transition_t* t = at(transitions, j);
            byte_count += printf(
                "  %lu --> %lu: %s\n", i, t->to_state, get_matcher_str(t->matcher)
            );
        }
    }
    byte_count += printf("----------------------\n");
    fflush(stdout);
    return byte_count;
}

void
tepsnfa_clear(tepsnfa* self)
{
    size_t i;
    for (i = 0; i < self->state_transitions.size; i++) {
        dynarr_free((dynarr_t*)at(&self->state_transitions, i));
    }
    dynarr_free(&self->state_transitions);
    self->state_num = 0;
    self->start_state = -1;
    self->final_state = -1;
}

void
tepsnfa_add_state(tepsnfa* self)
{
    self->state_num++;
    dynarr_t transition_set = dynarr_new(sizeof(transition_t));
    append(&self->state_transitions, &transition_set);
}

void
tepsnfa_add_transition(tepsnfa* self, size_t from, matcher_t matcher, size_t to)
{
    dynarr_t* from_transition_set = at(&self->state_transitions, from);
    size_t i;
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

tepsnfa
tepsnfa_deepcopy(const tepsnfa* self)
{
    size_t i, j;
    tepsnfa copy = {
        .state_num = self->state_num,
        .start_state = self->start_state,
        .final_state = self->final_state,
        .state_transitions = dynarr_new(sizeof(dynarr_t)),
    };
    for (i = 0; i < self->state_transitions.size; i++) {
        dynarr_t* from_transition_set = at(&self->state_transitions, i);
        dynarr_t to_transition_set = dynarr_new(sizeof(transition_t));
        for (j = 0; j < from_transition_set->size; j++) {
            append(&to_transition_set, at(from_transition_set, j));
        }
        append(&copy.state_transitions, &to_transition_set);
    }
    return copy;
}

void
tepsnfa_concat(tepsnfa* self, const tepsnfa* right)
{
    size_t i, j;
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
tepsnfa_union(tepsnfa* self, const tepsnfa* right)
{
    size_t i, j;
    size_t new_start = self->state_num + right->state_num;
    size_t new_final = new_start + 1;
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
    tepsnfa_add_transition(self, new_start, eps_matcher(), self->start_state);
    tepsnfa_add_transition(
        self, new_start, eps_matcher(), right->start_state + self->state_num
    );
    /* add eps from self final and right final to new final */
    tepsnfa_add_transition(self, self->final_state, eps_matcher(), new_final);
    tepsnfa_add_transition(
        self, right->final_state + self->state_num, eps_matcher(), new_final
    );
    /* set new start and final state */
    self->final_state = new_final;
    self->start_state = new_start;
    self->state_num += right->state_num + 2;
}

void
tepsnfa_fast_union(tepsnfa* self, const tepsnfa* right)
{
    size_t i, j;
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
    tepsnfa_add_transition(
        self, self->start_state, eps_matcher(),
        right->start_state + self->state_num
    );
    tepsnfa_add_transition(
        self, right->final_state + self->state_num, eps_matcher(),
        self->final_state
    );
    self->state_num += 2;
}

void
tepsnfa_to_star(tepsnfa* self)
{
    size_t star_start = self->state_num;
    size_t star_final = self->state_num + 1;
    tepsnfa_add_state(self);
    tepsnfa_add_state(self);
    /* add eps from star-start to self-start */
    tepsnfa_add_transition(self, star_start, eps_matcher(), self->start_state);
    /* add eps from self-finals to self-start */
    tepsnfa_add_transition(
        self, self->final_state, eps_matcher(), self->start_state
    );
    /* add eps from self-finals to star-final */
    tepsnfa_add_transition(self, self->final_state, eps_matcher(), star_final);
    /* add eps from star-start to star-final */
    tepsnfa_add_transition(self, star_start, eps_matcher(), star_final);
    /* set new start and final state */
    self->start_state = star_start;
    self->final_state = star_final;
}

void
tepsnfa_to_opt(tepsnfa* self)
{
    /* add an eps-transition from start state to final state */
    tepsnfa_add_transition(
        self, self->start_state, eps_matcher(), self->final_state
    );
}

epsnfa
tepsnfa_to_epsnfa_and_reduce_eps(tepsnfa* input)
{
    size_t i, j;
    epsnfa temp, output;
    int* state_map = malloc(input->state_num * sizeof(int));
    int* state_degrees = calloc(input->state_num, sizeof(int));
    // tepsnfa_print(input);

    /* input --> remove states with zero degree --> temp */
    for (i = 0; i < input->state_transitions.size; i++) {
        dynarr_t* transition_set = at(&input->state_transitions, i);
        for (j = 0; j < transition_set->size; j++) {
            transition_t* t = at(transition_set, j);
            state_degrees[i]++;
            state_degrees[t->to_state]++;
        }
    }
    {
        int nonzero_deg_count = 0;
        memset(state_map, -1, sizeof(int) * input->state_num);
        for (i = 0; i < input->state_num; i++) {
            if (state_degrees[i] != 0) {
                // printf(
                //     "%d -> %d (%d)\n", i, nonzero_deg_count, state_degrees[i]
                // );
                state_map[i] = nonzero_deg_count;
                nonzero_deg_count++;
            }
        }
        temp = epsnfa_new(nonzero_deg_count);
    }

    /* temp --> set start & finish states */
    bitmask_add(&temp.is_finish, state_map[input->final_state]);
    bitmask_add(&temp.is_start, state_map[input->start_state]);

    /* temp --> fill transition table */
    for (i = 0; i < input->state_num; i++) {
        dynarr_t* transition_set = at(&input->state_transitions, i);
        for (j = 0; j < transition_set->size; j++) {
            transition_t* t = at(transition_set, j);
            int k = state_map[i] * temp.state_num + state_map[t->to_state];
            temp.transition_table[k] = t->matcher;
        }
    }
    free(state_degrees);
    free(state_map);
    // epsnfa_print(&temp);

    /* temp --> reduce epsilon */
    for (i = 0; i < temp.state_num; i++) {
        epsnfa_reduce_eps(&temp, i);
    }
    // epsnfa_print(&temp);

    /* temp --> remove all out edges from zero in-degree non-starts state */
    assert(temp.state_num > 0);
    state_map = malloc(temp.state_num * sizeof(int));
    state_degrees = calloc(temp.state_num, sizeof(int));
    for (i = 0; i < temp.state_num; i++) {
        for (j = 0; j < temp.state_num; j++) {
            int k = i * temp.state_num + j;
            if (temp.transition_table[k].flag != MATCHER_FLAG_NIL) {
                state_degrees[j]++; /* we count only in-degree this time */
            }
        }
    }
    for (i = 0; i < temp.state_num; i++) {
        if (!bitmask_contains(&temp.is_start, i) && state_degrees[i] == 0) {
            /* remove the whole row */
            memset(
                &temp.transition_table[i * temp.state_num], 0,
                sizeof(matcher_t) * temp.state_num
            );
        }
    }

    /* temp --> remove states with zero degree again --> output */
    memset(state_degrees, 0, temp.state_num * sizeof(int));
    for (i = 0; i < temp.state_num; i++) {
        for (j = 0; j < temp.state_num; j++) {
            int k = i * temp.state_num + j;
            if (temp.transition_table[k].flag != MATCHER_FLAG_NIL) {
                state_degrees[i]++;
                state_degrees[j]++;
            }
        }
    }
    {
        int nonzero_deg_count = 0;
        memset(state_map, -1, sizeof(int) * temp.state_num);
        for (i = 0; i < temp.state_num; i++) {
            if (bitmask_contains(&temp.is_start, i) || state_degrees[i] != 0) {
                // printf(
                //     "%d -> %d (%d)\n", i, nonzero_deg_count, state_degrees[i]
                // );
                state_map[i] = nonzero_deg_count;
                nonzero_deg_count++;
            }
        }
        output = epsnfa_new(nonzero_deg_count);
    }

    /* output --> set start & finish states */
    for (i = 0; i < temp.state_num; i++) {
        if (bitmask_contains(&temp.is_start, i)) {
            bitmask_add(&output.is_start, state_map[i]);
        }
        if (bitmask_contains(&temp.is_finish, i)) {
            bitmask_add(&output.is_finish, state_map[i]);
        }
    }

    /* output --> fill transition table */
    for (i = 0; i < temp.state_num; i++) {
        if (state_map[i] == -1) {
            continue;
        }
        for (j = 0; j < temp.state_num; j++) {
            if (state_map[j] == -1) {
                continue;
            }
            int k = state_map[i] * output.state_num + state_map[j];
            int l = i * temp.state_num + j;
            output.transition_table[k] = temp.transition_table[l];
        }
    }
    free(state_degrees);
    free(state_map);
    epsnfa_clear(&temp);
    // epsnfa_print(&output);
    return output;
}

epsnfa
epsnfa_new(const size_t state_num)
{
    assert(state_num > 0);
    return (epsnfa) {
        .state_num = state_num,
        .is_start = bitmask_new(state_num),
        .is_finish = bitmask_new(state_num),
        .transition_table = calloc(state_num * state_num, sizeof(matcher_t)),
        .char_class_pool = (dynarr_t) {
            .size = 0,
            .cap = 0,
            .data = NULL,
            .elem_size = 0,
        },
    };
}

void
epsnfa_get_eps_closure(epsnfa* self, size_t state, bitmask_t* closure)
{
    if (bitmask_contains(closure, state)) {
        return;
    }
    bitmask_add(closure, state);
    size_t i;
    for (i = 0; i < self->state_num; i++) {
        matcher_t m = self->transition_table[state * self->state_num + i];
        /* use "==" because need to exclude anchor */
        if (m.flag == MATCHER_FLAG_EPS) {
            epsnfa_get_eps_closure(self, i, closure);
        }
    }
}

void
epsnfa_reduce_eps(epsnfa* self, size_t state)
{
    size_t i, j;
    bitmask_t closure = bitmask_new(self->state_num);

    /* get epsilon closure of s */
    epsnfa_get_eps_closure(self, state, &closure);
    // printf("closure %d:", state);
    // for (i = 0; i < self->state_num; i++) {
    //     if (bitmask_contains(&closure, i)) {
    //         printf(" %d", i);
    //     }
    // }
    // printf("\n");

    /* for each transition (s_from, p, s_to) in EPSNFA */
    for (i = 0; i < self->state_num; i++) {
        /* if s_from != s and s_from is in closure */
        if (i != state && bitmask_contains(&closure, i)) {
            for (j = 0; j < self->state_num; j++) {
                matcher_t m = self->transition_table[i * self->state_num + j];
                if (m.flag == MATCHER_FLAG_NIL) {
                    continue;
                }
                /* add (s, p, s_to) to EPSNFA */
                // printf("add (%d, %s, %d)\n", state, get_matcher_str(m), j);
                self->transition_table[state * self->state_num + j] = m;
            }
        }
    }

    /* if s is start state then for s' in closure, set s' as
       a start state */
    if (bitmask_contains(&self->is_start, state)) {
        for (j = 0; j < self->state_num; j++) {
            if (bitmask_contains(&closure, j)) {
                bitmask_add(&self->is_start, state);
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
                self->transition_table[k] = nil_matcher();
            }
        }
    }
    // epsnfa_print(self);

    bitmask_free(&closure);
}

void
epsnfa_print(epsnfa* self)
{
    size_t i, j;
    printf("---- PRINT EPSNFA ----\n");
    printf("Number of state: %lu\nStarting state:\n", self->state_num);
    for (i = 0; i < self->state_num; i++) {
        if (bitmask_contains(&self->is_start, i)) {
            printf(" %lu", i);
        }
    }
    printf("\nFinal states:\n");
    for (i = 0; i < self->state_num; i++) {
        if (bitmask_contains(&self->is_finish, i)) {
            printf(" %lu", i);
        }
    }
    printf("\nCharactor classes:\n");
    for (i = 0; i < self->char_class_pool.size; i++) {
        printf("#%lu ", i);
    }
    printf("\nTransitions:\n\nstateDiagram\n");
    for (i = 0; i < self->state_num; i++) {
        for (j = 0; j < self->state_num; j++) {
            int k = i * self->state_num + j;
            matcher_t m = self->transition_table[k];
            if (m.flag) {
                printf("  %lu --> %lu: %s\n", i, j, get_matcher_str(m));
            }
        }
    }
    printf("----------------------\n");
}

void
epsnfa_clear(epsnfa* self)
{
    self->state_num = 0;
    bitmask_free(&self->is_start);
    bitmask_free(&self->is_finish);
    free(self->transition_table);
}

/* return n if n is the largest integer such that input_str[0:n] matches
   return 0 if no match found */
size_t
epsnfa_find_initial_match(
    const epsnfa* self, const char* input_str, const size_t intput_len,
    const size_t start_offset
)
{
    typedef struct memory {
        size_t cur_state;
        size_t depth;
        size_t pos; /* the position in input string */
    } memory_t;
    size_t i, matched_len = 0;
    dynarr_t stack = dynarr_new(sizeof(memory_t));
#ifdef VERBOSE_MATCH
    printf("start_offset: %lu\n", start_offset);
#endif

    /* init from start states */
    for (i = 0; i < self->state_num; i++) {
        if (bitmask_contains(&self->is_start, i)) {
            memory_t init_mem = {
                .pos = 0,
                .depth = 0,
                .cur_state = i,
            };
            append(&stack, &init_mem);
        }
    }

    /* depth-first search */
    while (stack.size > 0) {
        memory_t cur_mem = *(memory_t*)back(&stack);
        size_t cur_pos = cur_mem.pos;
        char cur_char = input_str[start_offset + cur_pos];
        pop(&stack);

#ifdef VERBOSE_MATCH
        printf("---\n");
        printf("input pos : %lu\n", cur_pos);
        printf("cur char  : %c\n", cur_char);
        printf("stack size: %d\n", stack.size + 1);
        printf("cur state : %*d\n", stack.size + 1, cur_mem.cur_state);
        printf("---\n");
#endif

        if (cur_pos > matched_len
            && bitmask_contains(&self->is_finish, cur_mem.cur_state)) {
            matched_len = cur_pos;
        }

        if (cur_mem.depth == MATCH_SEARCH_DEPTH_LIMIT) {
            continue;
        }

        for (i = 0; i < self->state_num; i++) {
            anchor_byte behind, ahead;
            int is_matched = 0;
            size_t k = cur_mem.cur_state * self->state_num + i;
            matcher_t m = self->transition_table[k];
            if (m.flag == MATCHER_FLAG_NIL) {
                continue;
            }
#ifdef VERBOSE_MATCH
            printf("%d %s\n", i, get_matcher_str(m));
#endif
            if (m.flag & MATCHER_FLAG_ANCHOR) {
                if (cur_pos == 0 && start_offset == 0) {
                    behind = ANCHOR_BYTE_START;
                } else {
                    behind = input_str[start_offset + cur_pos - 1];
                }
                if (cur_pos + start_offset > intput_len - 1) {
                    ahead = ANCHOR_BYTE_END;
                } else {
                    ahead = cur_char;
                }
                is_matched = match_anchor(m.payload, behind, ahead);
            } else if (m.flag & MATCHER_FLAG_CLASS) {
                char_class_t* c = at(&self->char_class_pool, m.payload);
                assert(c != NULL);
                is_matched = match_class(*c, cur_char);
            } else if (cur_pos + start_offset <= intput_len - 1) {
                is_matched = match_byte(m, cur_char);
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
#ifdef VERBOSE_MATCH
                    printf(" matched archor \"%c%c\"\n", behind, cur_char);
#endif
                } else {
                    next_mem.pos++;
#ifdef VERBOSE_MATCH
                    printf(" matched byte %c\n", cur_char);
#endif
                }
                append(&stack, &next_mem);
            }
        }
    }
    dynarr_free(&stack);
    return matched_len;
}

dynarr_t
epsnfa_find_matches(
    const epsnfa* epsnfa, const char* input, const int is_global
)
{
    const size_t input_len = strlen(input);
    size_t start = 0, line_num = 1, col_num = 1, match_len = 0, i = 0;
    dynarr_t matches = dynarr_new(sizeof(match_t));
    for (start = 0; start < input_len; start += (match_len ? match_len : 1)) {
        match_len = epsnfa_find_initial_match(epsnfa, input, input_len, start);
        if (match_len) {
            match_t m = {
                .offset = start,
                .length = match_len,
                .line = line_num,
                .col = col_num,
            };
            append(&matches, &m);
            if (!is_global) {
                break;
            }
        }
        /* update line and col between start to start + match_len */
        for (i = 0; i < (match_len ? match_len : 1); i++) {
            if (start && input[i + start - 1] == '\n') {
                line_num++;
                col_num = 1;
            } else {
                col_num++;
            }
        }
    }
    return matches;
}

dynarr_t
epsnfa_find_matches_multiline(
    const epsnfa* epsnfa, const char* input, const int is_global
)
{
    const size_t input_len = strlen(input);
    size_t line_start = 0, line_end = 0, line_num = 1, line_len = 0;
    size_t match_len = 0, i = 0;
    dynarr_t matches = dynarr_new(sizeof(match_t));
    while (line_start < input_len) {
        while (input[line_end] != '\0' && input[line_end] != '\n') {
            line_end++;
        }
        for (i = 0; i < line_len; i++) {
            match_len = epsnfa_find_initial_match(epsnfa, input, line_len, i);
            if (match_len) {
                match_t m = {
                    .offset = i,
                    .length = match_len,
                    .line = line_num,
                    .col = i + 1,
                };
                append(&matches, &m);
                if (!is_global) {
                    break;
                }
            }
        }
        line_num++;
        line_start = line_end + 1;
    }
    return matches;
}
