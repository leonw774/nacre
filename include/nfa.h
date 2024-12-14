#include "transition.h"

/* epsilon-NFA */
typedef struct epsnfa {
    int state_num;
    int start_state;
    int final_state;
    /* type: dynarr of transtion. index is starting state. */
    dynarr_t state_transitions;
} epsnfa;

/* initialize an epsnfa with one state and no transition */
epsnfa epsnfa_one_state();

/* initialize an epsnfa of one start states, one final states and
   a transition between them */
epsnfa epsnfa_one_transition(matcher_t m);

void epsnfa_add_state(epsnfa* self);

void epsnfa_clear(epsnfa* self);

void epsnfa_add_transition(epsnfa* self, int from, int to, matcher_t matcher);

/* a, b -> ab */
void epsnfa_concat(epsnfa* self, epsnfa* right);

/* a, b -> a|b */
void epsnfa_union(epsnfa* self, epsnfa* right);

/* r -> r* */
void epsnfa_to_star(epsnfa* self);

/* r -> r? */
void epsnfa_to_opt(epsnfa* self);

/* check if the input string matches the eps-NFA */
int epsnfa_match(
    epsnfa* self,
    const char* input_str,
    const unsigned long size
);
