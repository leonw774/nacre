#include "matcher.h"
#include "re.h"

#define RE_STR_LEN_LIMIT 65535

extern char_class_t parse_bracket_expr(const char* input_str);
extern int atoi_check_dup_max(const char dup_str[DUP_STR_MAX_LEN]);
extern dynarr_t expand_dups(dynarr_t* ast);
extern re_ast_t parse_regex(const char* input_str, const int is_debug);
