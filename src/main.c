#include "nfa.h"
#include "re2nfa.h"
#include "re_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef IS_DEBUG
#define IS_DEBUG_FLAG 1
#else
#define IS_DEBUG_FLAG 0
#endif

const size_t MAX_SOURCE_BUFFER_SIZE = 10 * 1024 * 1024; // 10MB
const size_t MAX_PRINT_BUFFER_SIZE = 1024;

void
print_match(
    const char* buffer, size_t start_line, size_t start_col, size_t length
)
{
    int is_first_line = 1;
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
        if (is_first_line) {
            for (i = 0; i < start_col - 1; i++) {
                printf(" ");
            }
            is_first_line = 0;
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
find_matches(const epsnfa* epsnfa, const char* input)
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
find_matches_multiline(const epsnfa* epsnfa, const char* input)
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
                break;
            }
        }
        line_num++;
        line_start = line_end + 1;
    }
}

int
main(int argc, char* argv[])
{
    const char* multiline_flag = NULL;
    const char* regex = NULL;
    const char* input_file = NULL;

    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Usage: %s [-m] {re} {input_file}\n", argv[0]);
        return 1;
    }
    if (argc == 3) {
        regex = argv[1];
        input_file = argv[2];
    } else {
        multiline_flag = argv[1];
        if (strcmp(multiline_flag, "-m") != 0) {
            fprintf(stderr, "Usage: %s [-m] {re} {input_file}\n", argv[0]);
            return 1;
        }
        regex = argv[2];
        input_file = argv[3];
    }

    // Parse the regex into an AST
    re_ast_t ast = parse_regex(regex, IS_DEBUG_FLAG);
    if (ast.size == 0) {
        fprintf(stderr, "Error: Failed to parse regex.\n");
        return 1;
    }

    // Convert the AST to an reduced epsilon-NFA
    epsnfa nfa = re2nfa(&ast, IS_DEBUG_FLAG);

    // Open the input file
    FILE* file = fopen(input_file, "r");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    // Read the file into a buffer with a size limit (128MB) and find matches
    // byte-by-byte
    char* buffer = malloc(MAX_SOURCE_BUFFER_SIZE + 1);
    if (!buffer) {
        perror("Error allocating memory for buffer");
        fclose(file);
        return 1;
    }

    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, MAX_SOURCE_BUFFER_SIZE, file)) > 0) {
        if (bytes_read < MAX_SOURCE_BUFFER_SIZE) {
            buffer[bytes_read] = '\0'; // Null-terminate the buffer
        } else {
            buffer[MAX_SOURCE_BUFFER_SIZE]
                = '\0'; // Ensure null-termination for full buffer
        }
        if (multiline_flag) {
            find_matches_multiline(&nfa, buffer);
        } else {
            find_matches(&nfa, buffer);
        }
    }

    // Clean up
    free(buffer);
    fclose(file);
    epsnfa_clear(&nfa);
    free(ast.tokens);
    free(ast.lefts);
    free(ast.rights);

    return 0;
}