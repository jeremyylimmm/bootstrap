#pragma once

#define X(name, ...) SB_OP_##name,
typedef enum {
    SB_OP_INVALID,
    #include "ops.inc"
    NUM_SB_OPS,
} SB_Op;
#undef X

#define X(name, mnemonic, ...) mnemonic,
static const char* sb_op_mnemonic[] = {
    "<error>",
    #include "ops.inc"
};
#undef X

typedef struct SB_Node SB_Node;
typedef struct SB_User SB_User;

struct SB_Node {
    SB_Op op;

    int num_ins;
    SB_Node** ins;

    SB_User* users;

    int data_size;
    void* data;
};

struct SB_User{ 
    SB_User* next;
    int index;
    SB_Node* node;
};

typedef struct {
    SB_Node* start;
    SB_Node* end;
} SB_Proc;

typedef struct SB_Context SB_Context;

SB_Context* sb_init();
void sb_cleanup(SB_Context* ctx);

SB_Node* sb_node_int_const(SB_Context* ctx, uint64_t value);

SB_Node* sb_node_add (SB_Context* ctx, SB_Node* left, SB_Node* right);
SB_Node* sb_node_sub (SB_Context* ctx, SB_Node* left, SB_Node* right);
SB_Node* sb_node_mul (SB_Context* ctx, SB_Node* left, SB_Node* right);
SB_Node* sb_node_sdiv(SB_Context* ctx, SB_Node* left, SB_Node* right);

SB_Node* sb_node_start(SB_Context* ctx);
SB_Node* sb_node_end(SB_Context* ctx, SB_Node* ctrl, SB_Node* mem, SB_Node* ret_val);

SB_Node* sb_node_start_mem(SB_Context* ctx, SB_Node* start);
SB_Node* sb_node_start_ctrl(SB_Context* ctx, SB_Node* start);

SB_Node* sb_node_region(SB_Context* ctx);
SB_Node* sb_node_phi(SB_Context* ctx);

void sb_provide_region_inputs(SB_Context* ctx, SB_Node* region, int num_ins, SB_Node** ins);
void sb_provide_phi_inputs(SB_Context* ctx, SB_Node* phi, SB_Node* region, int num_ins, SB_Node** ins);

SB_Proc* sb_proc(SB_Context* ctx, SB_Node* start, SB_Node* end);