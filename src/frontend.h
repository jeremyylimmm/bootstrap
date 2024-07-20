#pragma once

#include "int128.h"
#include "core.h"

#define X(name, ...) AST_##name,
typedef enum {
    AST_ILLEGAL,
    #include "ast_kinds.inc"
    NUM_AST_KINDS,
} AST_Kind;
#undef X

#define X(name, mnemonic, ...) mnemonic,
static char* ast_mnemonic_table[NUM_AST_KINDS] = {
    "<error>",
    #include "ast_kinds.inc"
};
#undef X

enum {
    TOKEN_EOF = 0,
    TOKEN_IDENT = 256,
    TOKEN_INT_LITERAL
};

typedef struct {
    int kind;
    int length;
    int line;
    char* start;
} Token;

typedef struct AST AST;
struct AST {
    AST_Kind kind;

    union {
        int128_t int_const;
        AST* binary[2];
    } as;
};

AST* parse_source(Arena* arena, char* source, char* source_path);

void report_error_token(char* source, char* source_path, Token token, char* fmt, ...);