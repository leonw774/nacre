#include "dynarr.h"
#include "bitmask.h"
#include "matcher.h"
#include "orderedset.h"
#include "transition.h"

#ifndef NFA_H
#define NFA_H

/* epsilon-NFA */
typedef struct epsnfa {
    int state_num;
    int start_state;
    int final_state;
    /* type: dynarr of transtion. index is starting state. */
    dynarr_t state_transitions;
} epsnfa;

/* NFA */
typedef struct nfa {
    int state_num;
    int start_state;
    matcher_t* transition_table;
    bitmask_t is_finish;
} nfa;

/* initialize an epsnfa of one start states, one final states, and one
   transition between them
*/
epsnfa epsnfa_one_transition(matcher_t m);

int epsnfa_print(epsnfa* self);

void epsnfa_clear(epsnfa* self);

void epsnfa_add_state(epsnfa* self);

void epsnfa_add_transition(epsnfa* self, int from, matcher_t matcher, int to);

epsnfa epsnfa_deepcopy(const epsnfa* self);

/* a, b -> ab */
void epsnfa_concat(epsnfa* self, const epsnfa* right);

/* a, b -> a|b */
void epsnfa_union(epsnfa* self, const epsnfa* right);

/* a, b -> a|b where b is one-transition nfa */
void epsnfa_bracket_union(epsnfa* self, const epsnfa* right);

/* r -> r* */
void epsnfa_to_star(epsnfa* self);

/* r -> r? */
void epsnfa_to_opt(epsnfa* self);


/* nfa methods */

void nfa_get_eps_closure(nfa* self, int state, bitmask_t* closure);

void nfa_reduce_state(nfa* self, int state, bitmask_t* visited);

nfa epsnfa_reduce(epsnfa* input);

void nfa_print(nfa* self);

void nfa_clear(nfa* self);

#define MATCH_SEARCH_DEPTH_LIMIT 100000

/* return n if n is the smallest integer such that input_str[0:n] matches
   return -1 if no match found */
size_t nfa_match(
    const nfa* self, const char* input_str, const size_t intput_len,
    const size_t start_offset
);

#endif
