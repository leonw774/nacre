#include "dynarr.h"
#include "bitmask.h"
#include "matcher.h"

#ifndef NFA_H
#define NFA_H

#define NFA_STATE_NUM_LIMIT 65535

typedef struct transition {
    matcher_t matcher;
    int to_state;
} transition_t;

/* Thompson epsilon-NFA: only one start and one finish.
   edge stored as linked-lists */
typedef struct tepsnfa {
    int state_num;
    int start_state;
    int final_state;
    /* type: dynarr of transtion. index is starting state. */
    dynarr_t state_transitions;
} tepsnfa;

/* epsilon NFA: edges stored as an adjacent matrix */
typedef struct epsnfa {
    int state_num;
    matcher_t* transition_table;
    bitmask_t is_start;
    bitmask_t is_finish;
    dynarr_t char_class_pool; /* type: char_class_t */
} epsnfa;


/* Thompson epsilon-NFA methods */

/* initialize an Thompson epsnfa of one start states, one final states, and one
   transition between them */
tepsnfa tepsnfa_one_transition(matcher_t m);

int tepsnfa_print(tepsnfa* self);

void tepsnfa_clear(tepsnfa* self);

void tepsnfa_add_state(tepsnfa* self);

void tepsnfa_add_transition(tepsnfa* self, int from, matcher_t matcher, int to);

tepsnfa tepsnfa_deepcopy(const tepsnfa* self);

/* a, b -> ab */
void tepsnfa_concat(tepsnfa* self, const tepsnfa* right);

/* a, b -> a|b */
void tepsnfa_union(tepsnfa* self, const tepsnfa* right);

/* a, b -> a|b where b is one-transition epsnfa */
void tepsnfa_fast_union(tepsnfa* self, const tepsnfa* right);

/* r -> r* */
void tepsnfa_to_star(tepsnfa* self);

/* r -> r? */
void tepsnfa_to_opt(tepsnfa* self);

epsnfa tepsnfa_to_epsnfa_and_reduce_eps(tepsnfa* input);


/* Epsilon-NFA methods */

epsnfa epsnfa_new(const int state_num);

void epsnfa_get_eps_closure(epsnfa* self, int state, bitmask_t* closure);

void epsnfa_reduce_eps(epsnfa* self, int state);

void epsnfa_print(epsnfa* self);

void epsnfa_clear(epsnfa* self);

typedef struct match {
    size_t offset;
    size_t length;
    size_t line;
    size_t col;
} match_t;

#define MATCH_SEARCH_DEPTH_LIMIT 100000

/* return n if n is the smallest integer such that input_str[0:n] matches
   return -1 if no match found */
size_t epsnfa_find_initial_match(
    const epsnfa* self, const char* input_str, const size_t intput_len,
    const size_t start_offset
);

dynarr_t
epsnfa_find_matches(
    const epsnfa* epsnfa, const char* input, const int is_global
);

dynarr_t
epsnfa_find_matches_multiline(
    const epsnfa* epsnfa, const char* input, const int is_global
);

#endif
