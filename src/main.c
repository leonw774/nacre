#include "nfa.h"
#include "re2nfa.h"
#include "re_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
find_matches(const epsnfa* nfa, const char* buffer)
{
    size_t start, match_length, len = strlen(buffer);
    size_t line_num = 1, col_num = 1, tmp, check_newline_to;
    for (start = 0; start < len; start += match_length) {
        const char* substring = buffer + start;
        if (!substring) {
            fprintf(stderr, "Error: Memory allocation failed.\n");
            exit(1);
        }
        match_length = epsnfa_match(nfa, substring);
        if (match_length) {
            size_t print_start = start;
            while (print_start != 0 && buffer[print_start - 1] != '\n') {
                print_start--;
            }
            size_t print_end = start + match_length;
            while (buffer[print_end] != '\n') {
                print_end++;
            }
            char* print_buffer = malloc(print_end - print_start + 1);
            strncpy(
                print_buffer, buffer + print_start, print_end - print_start
            );
            print_buffer[print_end - print_start] = '\0';
            print_match(print_buffer, line_num, col_num, match_length);
            free(print_buffer);
        } else {
            /* start still need to increase by one */
            start++;
        }
        /* update line and col between start to start + match_length */
        check_newline_to
            = start + (match_length == 0 ? 1 : match_length);
        for (tmp = start; tmp < check_newline_to; tmp++) {
            if (tmp && buffer[tmp - 1] == '\n') {
                line_num++;
                col_num = 1;
            } else {
                col_num++;
            }
        }
    }
}

int
main(int argc, char* argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s {re} {input_file}\n", argv[0]);
        return 1;
    }

    const char* regex = argv[1];
    const char* input_file = argv[2];

    // Parse the regex into an AST
    re_ast_t ast = parse_regex(regex, 0);
    if (ast.size == 0) {
        fprintf(stderr, "Error: Failed to parse regex.\n");
        return 1;
    }

    // Convert the AST to an epsilon-NFA
    epsnfa nfa = re2nfa(&ast, 0);

    // Open the input file
    FILE* file = fopen(input_file, "r");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    // Read the file into a buffer with a size limit (1MB) and find matches
    // byte-by-byte
    const size_t MAX_BUFFER_SIZE = 1024 * 1024; // 1MB
    char* buffer = malloc(MAX_BUFFER_SIZE + 1);
    if (!buffer) {
        perror("Error allocating memory for buffer");
        fclose(file);
        return 1;
    }

    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, MAX_BUFFER_SIZE, file)) > 0) {
        if (bytes_read < MAX_BUFFER_SIZE) {
            buffer[bytes_read] = '\0'; // Null-terminate the buffer
        } else {
            buffer[MAX_BUFFER_SIZE]
                = '\0'; // Ensure null-termination for full buffer
        }
        find_matches(&nfa, buffer);
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