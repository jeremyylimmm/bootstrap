#include "core.h"
#include "sb.h"

#define VIEW_DATA(n, type) (*(type*)((n)->data))

struct SB_Context {
    Arena* arena;
};

enum {
    BINARY_LEFT,
    BINARY_RIGHT,
    NUM_BINARY_INS
};

enum {
    END_CTRL,
    END_MEM,
    END_RET_VAL,
    NUM_END_INS
};

enum {
    PROJ_INPUT,
    NUM_PROJ_INS
};

SB_Context* sb_init() {
    Arena* arena = arena_new();
    SB_Context* ctx = arena_type(arena, SB_Context);
    ctx->arena = arena;
    return ctx;
}

void sb_cleanup(SB_Context* ctx) {
    arena_destroy(ctx->arena);
}

static void alloc_inputs(SB_Context* ctx, SB_Node* node, int num_ins) {
    assert(!node->num_ins);
    node->num_ins = num_ins;
    node->ins = arena_array(ctx->arena, SB_Node*, num_ins);
}

static SB_Node* new_node(SB_Context* ctx, SB_Op op, int num_ins) {
    SB_Node* node = arena_type(ctx->arena, SB_Node);
    node->op = op;
    alloc_inputs(ctx, node, num_ins);
    return node;
}

static void alloc_data_raw(SB_Context* ctx, SB_Node* node, int size) {
    node->data_size = size;
    node->data = arena_zero(ctx->arena, size);
}

static void set_input(SB_Context* ctx, SB_Node* node, int index, SB_Node* input) {
    assert(index < node->num_ins && !node->ins[index]);
    node->ins[index] = node;

    SB_User* u = arena_type(ctx->arena, SB_User);
    u->index = index;
    u->node = node;

    u->next = input->users;
    input->users = u;
}

#define ALLOC_DATA(ctx, node, type) alloc_data_raw(ctx, node, sizeof(type))
#define SET_INPUT(node, index, input) set_input(ctx, node, index, input)

SB_Node* sb_node_int_const(SB_Context* ctx, uint64_t value) {
    SB_Node* n = new_node(ctx, SB_OP_INT_CONST, 0);    
    ALLOC_DATA(ctx, n, uint64_t);
    VIEW_DATA(n, uint64_t) = value;
    return n;
}

SB_Node* new_binary(SB_Context* ctx, SB_Op op, SB_Node* left, SB_Node* right) {
    SB_Node* n = new_node(ctx, op, NUM_BINARY_INS);
    SET_INPUT(n, BINARY_LEFT, left);
    SET_INPUT(n, BINARY_RIGHT, right);
    return n;
}

SB_Node* sb_node_add(SB_Context* ctx, SB_Node* left, SB_Node* right) {
    return new_binary(ctx, SB_OP_ADD, left, right);
}

SB_Node* sb_node_sub(SB_Context* ctx, SB_Node* left, SB_Node* right) {
    return new_binary(ctx, SB_OP_SUB, left, right);
}

SB_Node* sb_node_mul(SB_Context* ctx, SB_Node* left, SB_Node* right) {
    return new_binary(ctx, SB_OP_MUL, left, right);
}

SB_Node* sb_node_sdiv(SB_Context* ctx, SB_Node* left, SB_Node* right) {
    return new_binary(ctx, SB_OP_SDIV, left, right);
}

SB_Node* sb_node_start(SB_Context* ctx) {
    return new_node(ctx, SB_OP_START, 0);
}

SB_Node* sb_node_end(SB_Context* ctx, SB_Node* ctrl, SB_Node* mem, SB_Node* ret_val) {
    SB_Node* node = new_node(ctx, SB_OP_END, NUM_END_INS);
    SET_INPUT(node, END_CTRL, ctrl);
    SET_INPUT(node, END_MEM, mem);
    SET_INPUT(node, END_RET_VAL, ret_val);
    return node;
}

static SB_Node* new_proj(SB_Context* ctx, SB_Op op, SB_Node* input) {
    SB_Node* node = new_node(ctx, op, NUM_PROJ_INS);
    SET_INPUT(node, PROJ_INPUT, input);
    return node;
}

SB_Node* sb_node_start_mem(SB_Context* ctx, SB_Node* start) {
    assert(start->op == SB_OP_START);
    return new_proj(ctx, SB_OP_START_MEM, start);
}

SB_Node* sb_node_start_ctrl(SB_Context* ctx, SB_Node* start) {
    assert(start->op == SB_OP_START);
    return new_proj(ctx, SB_OP_START_CTRL, start);
}

SB_Node* sb_node_region(SB_Context* ctx) {
    return new_node(ctx, SB_OP_REGION, 0);
}

SB_Node* sb_node_phi(SB_Context* ctx) {
    return new_node(ctx, SB_OP_PHI, 0);
}

void sb_provide_region_inputs(SB_Context* ctx, SB_Node* region, int num_ins, SB_Node** ins) {
    assert(region->op == SB_OP_REGION);
    alloc_inputs(ctx, region, num_ins);

    for (int i = 0; i < num_ins; ++i) {
        SET_INPUT(region, i, ins[i]);
    }
}

void sb_provide_phi_inputs(SB_Context* ctx, SB_Node* phi, SB_Node* region, int num_ins, SB_Node** ins) {
    assert(phi->op == SB_OP_PHI && region->op == SB_OP_REGION);

    alloc_inputs(ctx, phi, num_ins + 1);
    SET_INPUT(phi, 0, region);

    for (int i = 0; i < num_ins; ++i) {
        SET_INPUT(phi, i + 1, ins[i]);
    }
}

SB_Proc* sb_proc(SB_Context* ctx, SB_Node* start, SB_Node* end) {
    SB_Proc* proc = arena_type(ctx->arena, SB_Proc);
    proc->start = start;
    proc->end = end;
    return proc;
}