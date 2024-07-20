#include <stdio.h>

#include "frontend.h"

static int128_t eval(AST* ast) {
    switch (ast->kind) {
        default:
            assert(false);
            return int128_zero();

        case AST_INT_CONST:
            return ast->as.int_const;
        case AST_ADD:
            return int128_add(eval(ast->as.binary[0]), eval(ast->as.binary[1]));
        case AST_SUB:
            return int128_sub(eval(ast->as.binary[0]), eval(ast->as.binary[1]));
        case AST_MUL:
            return int128_mul(eval(ast->as.binary[0]), eval(ast->as.binary[1]));
        case AST_DIV:
            return int128_div(eval(ast->as.binary[0]), eval(ast->as.binary[1])).quotient;
    }
}

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

    AST* ast = parse_source(arena, source, source_path);
    int128_t result = eval(ast);

    printf("Result: %lld\n", result.low);

    return 0;
}