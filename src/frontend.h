#pragma once

#include "int128.h"
#include "core.h"

typedef enum {
    HIR_OP_ILLEGAL,
    HIR_OP_INT_CONST,
    HIR_OP_ADD,
    HIR_OP_SUB,
    HIR_OP_MUL,
    HIR_OP_DIV,
    NUM_HIR_OPS,
} HIR_Op;

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

typedef struct HIR_Block HIR_Block;
typedef struct HIR_Node HIR_Node;

struct HIR_Node {
    HIR_Block* block;
    HIR_Node* prev;
    HIR_Node* next;

    HIR_Op op;
    union {
        int128_t int_const;
        HIR_Node* binary[2];
    } as;

    int tid;
};

struct HIR_Block {
    HIR_Block* next;
    HIR_Node* start;
    HIR_Node* end;
    int tid;
};

typedef struct {
    HIR_Block* control_flow_head;
} HIR_Proc;

HIR_Proc* parse_source(Arena* arena, char* source, char* source_path);

void report_error_token(char* source, char* source_path, Token token, char* fmt, ...);

void hir_append(HIR_Block* block, HIR_Node* node);
void hir_print(HIR_Proc* proc, char* name);