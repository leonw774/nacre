#include <stdio.h>
#include "nfa.h"

int main() {
    /* make the epsilon NFA example on Wikipedia that "determines if the input
       contains an even number of 0s or an even number of 1s" */
    epsnfa m = {
        .state_num = 5,
        .start_state = 0,
        .final_states = new_stateset(5),
        .state_transitions = new_dynarr(sizeof(dynarr_t))
    };
    /* add final states */
    stateset_add(m.final_states, 1);
    stateset_add(m.final_states, 3);
    /* init transition sets */
    int i;
    for (i = 0; i < m.state_num; i++) {
        dynarr_t transition_set = new_dynarr(sizeof(transition_t));
        append(&m.state_transitions, &transition_set);
    }
    /* add transitions */
    epsnfa_add_transition(&m, 0, EPS_MATCHER, 1);
    epsnfa_add_transition(&m, 0, EPS_MATCHER, 3);
    epsnfa_add_transition(&m, 1, BYTE_MATCHER('0'), 2);
    epsnfa_add_transition(&m, 1, BYTE_MATCHER('1'), 1);
    epsnfa_add_transition(&m, 2, BYTE_MATCHER('0'), 1);
    epsnfa_add_transition(&m, 2, BYTE_MATCHER('1'), 2);
    epsnfa_add_transition(&m, 3, BYTE_MATCHER('0'), 3);
    epsnfa_add_transition(&m, 3, BYTE_MATCHER('1'), 4);
    epsnfa_add_transition(&m, 4, BYTE_MATCHER('0'), 4);
    epsnfa_add_transition(&m, 4, BYTE_MATCHER('1'), 3);
    
    /* test cases */
    printf("%d\n", epsnfa_match(&m, "01")); /* 0 */ 
    printf("%d\n", epsnfa_match(&m, "001")); /* 1 */ 
    printf("%d\n", epsnfa_match(&m, "0101")); /* 1 */ 
    printf("%d\n", epsnfa_match(&m, "110010")); /* 0 */ 
    printf("%d\n", epsnfa_match(&m, "1110001")); /* 1 */ 
    return 0;
}