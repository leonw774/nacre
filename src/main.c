#include "nfa.h"
#include "re2nfa.h"
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

void
print_match(
    const char* buffer, size_t start_line, size_t start_col, size_t length
)
{
    int is_match_started = 0;
    size_t print_match_count = 0;
    size_t line_start = 0, line_end = 0;
    printf("%lu, %lu (%lu)\n", start_line, start_col, length);
    while (buffer[line_end]) {
        size_t i;
        while (buffer[line_end] != '\0' && buffer[line_end] != '\n') {
            line_end++;
        }
        if (line_end > line_start) {
            fwrite(buffer + line_start, 1, line_end - line_start, stdout);
        }
        printf("\n");
        if (is_match_started == 0) {
            for (i = 0; i < start_col - 1; i++) {
                printf(" ");
            }
            is_match_started = 1;
        }
        for (; i < line_end - line_start; i++) {
            if (print_match_count < length) {
                printf("^");
                print_match_count++;
            }
        }
        printf("\n");
        if (buffer[line_end] == '\0') {
            break;
        }
        line_start = line_end + 1;
        line_end = line_start;
    }
}

void
find_matches(const epsnfa* epsnfa, const char* input, const int is_global)
{
    char print_buffer[MAX_PRINT_BUFFER_SIZE];
    size_t start, match_length, len = strlen(input);
    size_t line_num = 1, col_num = 1, tmp, check_newline_to;
    for (start = 0; start < len; start += match_length) {
        match_length = epsnfa_match(epsnfa, input, len, start);
        if (match_length) {
            size_t print_start = start - col_num + 1;
            size_t print_end = start + match_length - 1;
            size_t print_size;
            while (input[print_end] != '\n') {
                print_end++;
            }
            print_size = print_end - print_start;
            if (print_size >= MAX_PRINT_BUFFER_SIZE) {
                print_size = MAX_PRINT_BUFFER_SIZE - 1;
            }
            strncpy(print_buffer, input + print_start, print_size);
            print_buffer[print_size] = '\0';
            print_match(print_buffer, line_num, col_num, match_length);
            if (!is_global) {
                break;
            }
        } else {
            /* start still need to increase by one */
            start++;
        }
        /* update line and col between start to start + match_length */
        check_newline_to = start + (match_length == 0 ? 1 : match_length);
        for (tmp = start; tmp < check_newline_to; tmp++) {
            if (tmp && input[tmp - 1] == '\n') {
                line_num++;
                col_num = 1;
            } else {
                col_num++;
            }
        }
    }
}

void
find_matches_multiline(
    const epsnfa* epsnfa, const char* input, const int is_global
)
{
    size_t line_start = 0, match_length, len = strlen(input);
    size_t line_num = 1;
    char line[MAX_PRINT_BUFFER_SIZE];
    while (line_start < len) {
        size_t i, line_end = line_start, line_len;
        while (input[line_end] != '\0' && input[line_end] != '\n') {
            line_end++;
        }
        line_len = line_end - line_start;
        if (line_len >= MAX_PRINT_BUFFER_SIZE) {
            line_len = MAX_PRINT_BUFFER_SIZE - 1;
        }
        strncpy(line, input + line_start, line_len);
        line[line_len] = '\0';
        for (i = 0; i < line_len; i++) {
            match_length = epsnfa_match(epsnfa, line, line_len, i);
            if (match_length) {
                print_match(line, line_num, i + 1, match_length);
                if (!is_global) {
                    break;
                }
            }
        }
        line_num++;
        line_start = line_end + 1;
    }
}

int
main(int argc, char* argv[])
{

    match_flags_t match_flag;
    const char* regex = NULL;
    const char* input_file = NULL;
    const char* opt_def = "gmi";
    const char* usage = "Usage: %s [OPTION] PATTERN INPUT_FILE\n";
    int c;
    extern int optind, optopt;

    re_ast_t ast;
    epsnfa nfa;
    FILE* file;

    memset(&match_flag, 0, sizeof(match_flags_t));
    while ((c = getopt(argc, argv, opt_def)) != -1) {
        switch (c) {
        case 'g':
            match_flag.global = 1;
            break;
        case 'm':
            match_flag.multiline = 1;
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
    nfa = re2nfa(&ast, IS_DEBUG_FLAG);

    // ppen the input file
    file = fopen(input_file, "r");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    /* read the file into a buffer with a size limit and find matches */
    {
        char* buffer = malloc(MAX_INPUT_BUF_SIZE + 1);
        size_t bytes_read;
        if (!buffer) {
            perror("Error allocating memory for buffer");
            fclose(file);
            return 1;
        }
        while ((bytes_read = fread(buffer, 1, MAX_INPUT_BUF_SIZE, file)) > 0) {
            // Null-terminate the buffer
            buffer[bytes_read] = '\0';
            buffer[MAX_INPUT_BUF_SIZE] = '\0';
            if (match_flag.multiline) {
                find_matches_multiline(&nfa, buffer, match_flag.global);
            } else {
                find_matches(&nfa, buffer, match_flag.global);
            }
        }
        free(buffer);
    }

    fclose(file);
    epsnfa_clear(&nfa);
    re_ast_free(&ast);
    return 0;
}