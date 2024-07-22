#include <stdio.h>

#include "frontend.h"

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

    HIR_Proc* ast = parse_source(arena, source, source_path);
    hir_print(ast, "main");

    return 0;
}