#include "nfa.h"
#include "re_ast.h"
#include "re_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef IS_DEBUG
#define IS_DEBUG_FLAG 1
#else
#define IS_DEBUG_FLAG 0
#endif

const size_t MAX_INPUT_BUF_SIZE = 128 * 1024 * 1024; // 128MB
const size_t MAX_PRINT_BUFFER_SIZE = 1024;

typedef struct match_flags {
    unsigned char global;
    unsigned char multiline;
} match_flags_t;

/* buffer the full input buffer */
void
print_match(const char* buffer, const match_t match)
{
    int is_match_started = 0;
    size_t print_match_count = 0;
    size_t line_start = 0, line_end = 0, i = 0;
    printf("%lu, %lu (%lu)\n", match.line, match.col, match.length);

    /* find the start of the line containing the match */
    line_start = match.offset;
    while (line_start > 0 && buffer[line_start - 1] != '\n') {
        line_start--;
    }
    line_end = line_start;

    while (buffer[line_end]) {
        while (buffer[line_end] != '\0' && buffer[line_end] != '\n') {
            line_end++;
        }
        if (line_end > line_start) {
            fwrite(buffer + line_start, 1, line_end - line_start, stdout);
        }
        printf("\n");
        if (is_match_started == 0) {
            for (i = 0; i < match.col - 1; i++) {
                printf(" ");
            }
            is_match_started = 1;
        }
        for (; i < line_end - line_start; i++) {
            if (print_match_count < match.length) {
                printf("^");
                print_match_count++;
            }
        }
        printf("\n");
        if (buffer[line_end] == '\0' || print_match_count >= match.length) {
            break;
        }
        line_start = line_end + 1;
        line_end = line_start;
    }
}

int
main(int argc, char* argv[])
{

    match_flags_t mflag;
    const char* regex = NULL;
    const char* input_file = NULL;
    const char* opt_def = "gmi";
    const char* usage = "Usage: %s [OPTION] PATTERN INPUT_FILE\n";
    int c;
    extern int optind, optopt;

    re_ast_t ast;
    epsnfa nfa;
    FILE* file;

    memset(&mflag, 0, sizeof(match_flags_t));
    while ((c = getopt(argc, argv, opt_def)) != -1) {
        switch (c) {
        case 'g':
            mflag.global = 1;
            break;
        case 'm':
            mflag.multiline = 1;
            break;
        case '?':
            if (isprint(optopt)) {
                fprintf(stderr, "Bad argument %c\n", (char)optopt);
            } else {
                fprintf(stderr, "Bad argument \\x%x\n", (char)optopt);
            }
            break;
        default:
            fprintf(stderr, usage, argv[0]);
            abort();
        }
    }
    if (argc - optind == 2) {
        regex = argv[optind];
        input_file = argv[optind + 1];
    } else {
        fprintf(stderr, usage, argv[0]);
    }

    // parse the regex into an AST
    ast = parse_regex(regex, IS_DEBUG_FLAG);
    if (ast.size == 0) {
        fprintf(stderr, "Error: Failed to parse regex.\n");
        return 1;
    }

    // convert AST to an reduced epsilon-NFA
    nfa = re_ast_to_nfa(&ast, IS_DEBUG_FLAG);

    // ppen the input file
    file = fopen(input_file, "r");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    /* read the file into a buffer with a size limit and find matches */
    {
        char* buffer = malloc(MAX_INPUT_BUF_SIZE + 1);
        size_t i, bytes_read;
        dynarr_t matches;
        if (!buffer) {
            perror("Error allocating memory for buffer");
            fclose(file);
            return 1;
        }
        buffer[MAX_INPUT_BUF_SIZE] = '\0';
        while ((bytes_read = fread(buffer, 1, MAX_INPUT_BUF_SIZE, file)) > 0) {
            buffer[bytes_read] = '\0';
            if (mflag.multiline) {
                matches
                    = epsnfa_find_matches_multiline(&nfa, buffer, mflag.global);
            } else {
                matches = epsnfa_find_matches(&nfa, buffer, mflag.global);
            }
            /* print all matches */
            for (i = 0; i < matches.size; i++) {
                print_match(buffer, *(match_t*)at(&matches, i));
            }
            dynarr_free(&matches);
        }
        free(buffer);
    }
    fclose(file);
    epsnfa_clear(&nfa);
    re_ast_free(&ast);
    return 0;
}