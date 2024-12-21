#include "regex.h"

extern regex_ast_t parse_regex(char* input_str, const int is_debug);
extern void precalc_ast(regex_ast_t* ast);
