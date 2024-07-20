#include <stdio.h>
#include <stdlib.h>

#include "core.h"
#include "int128.h"

int main() {
    Arena* arena = arena_new();

    char* source_path = "examples/test.bs";

    FILE* file;
    if (fopen_s(&file, source_path, "r")) {
        printf("Failed to read '%s'\n", source_path);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    size_t file_length = ftell(file);
    rewind(file);

    char* source = arena_push(arena, (file_length + 1) * sizeof(char));
    size_t source_length = fread(source, 1, file_length, file);
    source[source_length] = '\0';

    printf("%s\n", source);

    int128_t a = int128_from_uint64(23982392383);
    int128_t b = int128_from_uint64(239833);
    Int128DivResult d = int128_div(a, b);

    printf("%lld r %lld\n", d.quotient.low, d.remainder.low);

    return 0;
}