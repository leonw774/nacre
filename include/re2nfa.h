#include "nfa.h"
#include "re.h"

extern epsnfa re2nfa(const re_ast_t* re_ast, const int is_debug);