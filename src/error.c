#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

#include "frontend.h"

void report_error_token(char* source, char* source_path, Token token, char* fmt, ...) {
    char* line_start = token.start;
    while (line_start != source && *line_start != '\n') {
        --line_start;
    }

    while (isspace(*line_start)) {
        ++line_start;
    }

    int line_length = 0;
    while (line_start[line_length] != '\n' && line_start[line_length] != '\0') {
        ++line_length;
    }

    int prefix = printf("%s(%d): error: ", source_path, token.line);
    printf("%.*s\n", line_length, line_start);

    va_list ap;
    va_start(ap, fmt);

    int offset = prefix + (int)(token.start - line_start);
    printf("%*s^ ", offset, "");

    vprintf(fmt, ap);
    printf("\n");

    va_end(ap);
}