#include "dynarr.h"
#include "orderedset.h"
#include "matcher.h"
#include "transition.h"

/* epsilon-NFA */
typedef struct epsnfa {
    int state_num;
    int start_state;
    orderedset_t final_states;
    /* type: dynarr of transtion. index is starting state. */
    dynarr_t state_transitions;
} epsnfa;

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

/* r -> r* */
void epsnfa_to_star(epsnfa* self);

/* r -> r? */
void epsnfa_to_opt(epsnfa* self);

/* check if the input string matches the eps-NFA */
int epsnfa_match(const epsnfa* self, const char* input_str);
