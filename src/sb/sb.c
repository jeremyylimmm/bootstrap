#include <stdio.h>
#include <string.h>

#include "internal.h"
#include "containers.h"

SB_Context* sb_init() {
    Arena* arena = arena_new();
    SB_Context* ctx = arena_type(arena, SB_Context);
    ctx->arena = arena;
    ctx->scratch_lib = scratch_library_new();
    return ctx;
}

void sb_cleanup(SB_Context* ctx) {
    scratch_library_destroy(&ctx->scratch_lib);
    arena_destroy(ctx->arena);
}

static void alloc_inputs(SB_Context* ctx, SB_Node* node, int num_ins) {
    assert(!node->num_ins);
    node->num_ins = num_ins;
    node->ins = arena_array(ctx->arena, SB_Node*, num_ins);
}

static SB_Node* new_node(SB_Context* ctx, SB_Op op, int num_ins, SB_NodeFlags flags) {
    SB_Node* node = arena_type(ctx->arena, SB_Node);
    node->op = op;
    node->flags = flags;
    alloc_inputs(ctx, node, num_ins);
    return node;
}

static void alloc_data_raw(SB_Context* ctx, SB_Node* node, int size) {
    node->data_size = size;
    node->data = arena_zero(ctx->arena, size);
}

static void set_input(SB_Context* ctx, SB_Node* node, int index, SB_Node* input) {
    assert(index < node->num_ins && !node->ins[index]);
    node->ins[index] = input;

    SB_User* u = arena_type(ctx->arena, SB_User);
    u->index = index;
    u->node = node;

    u->next = input->users;
    input->users = u;
}

#define ALLOC_DATA(ctx, node, type) alloc_data_raw(ctx, node, sizeof(type))
#define SET_INPUT(node, index, input) set_input(ctx, node, index, input)

SB_Node* sb_node_null(SB_Context* ctx) {
    return new_node(ctx, SB_OP_NULL, 0, SB_NODE_FLAG_NONE);
}

SB_Node* sb_node_int_const(SB_Context* ctx, uint64_t value) {
    SB_Node* n = new_node(ctx, SB_OP_INT_CONST, 0, SB_NODE_FLAG_NONE);    
    ALLOC_DATA(ctx, n, uint64_t);
    VIEW_DATA(n, uint64_t) = value;
    return n;
}

SB_Node* sb_node_alloca(SB_Context* ctx) {
    return new_node(ctx, SB_OP_ALLOCA, 0, SB_NODE_FLAG_NONE);
}

SB_Node* new_binary(SB_Context* ctx, SB_Op op, SB_Node* left, SB_Node* right) {
    SB_Node* n = new_node(ctx, op, NUM_BINARY_INS, SB_NODE_FLAG_NONE);
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
    return new_node(ctx, SB_OP_START, 0, SB_NODE_FLAG_STARTS_BASIC_BLOCK | SB_NODE_FLAG_TRANSFERS_CONTROL);
}

SB_Node* sb_node_end(SB_Context* ctx, SB_Node* ctrl, SB_Node* mem, SB_Node* ret_val) {
    SB_Node* node = new_node(ctx, SB_OP_END, NUM_END_INS, SB_NODE_FLAG_NONE);
    SET_INPUT(node, END_CTRL, ctrl);
    SET_INPUT(node, END_MEM, mem);
    SET_INPUT(node, END_RET_VAL, ret_val);
    return node;
}

static SB_Node* new_proj(SB_Context* ctx, SB_Op op, SB_Node* input, SB_NodeFlags flags) {
    SB_Node* node = new_node(ctx, op, NUM_PROJ_INS, SB_NODE_FLAG_PROJECTION | flags);
    SET_INPUT(node, PROJ_INPUT, input);
    return node;
}

SB_Node* sb_node_start_mem(SB_Context* ctx, SB_Node* start) {
    assert(start->op == SB_OP_START);
    return new_proj(ctx, SB_OP_START_MEM, start, SB_NODE_FLAG_NONE);
}

SB_Node* sb_node_start_ctrl(SB_Context* ctx, SB_Node* start) {
    assert(start->op == SB_OP_START);
    return new_proj(ctx, SB_OP_START_CTRL, start, SB_NODE_FLAG_TRANSFERS_CONTROL);
}

SB_Node* sb_node_region(SB_Context* ctx) {
    return new_node(ctx, SB_OP_REGION, 0, SB_NODE_FLAG_STARTS_BASIC_BLOCK | SB_NODE_FLAG_TRANSFERS_CONTROL);
}

SB_Node* sb_node_phi(SB_Context* ctx) {
    return new_node(ctx, SB_OP_PHI, 0, SB_NODE_FLAG_NONE);
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
    assert(num_ins == region->num_ins);

    alloc_inputs(ctx, phi, num_ins + 1);
    SET_INPUT(phi, 0, region);

    for (int i = 0; i < num_ins; ++i) {
        SET_INPUT(phi, i + 1, ins[i]);
    }
}

SB_Node* sb_node_branch(SB_Context* ctx, SB_Node* ctrl, SB_Node* predicate) {
    SB_Node* node = new_node(ctx, SB_OP_BRANCH, NUM_BRANCH_INS, SB_NODE_FLAG_TRANSFERS_CONTROL);
    SET_INPUT(node, BRANCH_CTRL, ctrl);
    SET_INPUT(node, BRANCH_PREDICATE, predicate);
    return node;
}

SB_Node* sb_node_branch_then(SB_Context* ctx, SB_Node* branch) {
    assert(branch->op == SB_OP_BRANCH);
    return new_proj(ctx, SB_OP_BRANCH_THEN, branch, SB_NODE_FLAG_STARTS_BASIC_BLOCK | SB_NODE_FLAG_TRANSFERS_CONTROL);
}

SB_Node* sb_node_branch_else(SB_Context* ctx, SB_Node* branch) {
    assert(branch->op == SB_OP_BRANCH);
    return new_proj(ctx, SB_OP_BRANCH_ELSE, branch, SB_NODE_FLAG_STARTS_BASIC_BLOCK | SB_NODE_FLAG_TRANSFERS_CONTROL);
}

SB_Node* sb_node_load(SB_Context* ctx, SB_Node* ctrl, SB_Node* mem, SB_Node* addr) {
    SB_Node* node = new_node(ctx, SB_OP_LOAD, NUM_LOAD_INS, SB_NODE_FLAG_NONE); 
    SET_INPUT(node, LOAD_CTRL, ctrl);
    SET_INPUT(node, LOAD_MEM, mem);
    SET_INPUT(node, LOAD_ADDR, addr);
    return node;
}

SB_Node* sb_node_store(SB_Context* ctx, SB_Node* ctrl, SB_Node* mem, SB_Node* addr, SB_Node* value) {
    SB_Node* node = new_node(ctx, SB_OP_STORE, NUM_STORE_INS, SB_NODE_FLAG_NONE); 
    SET_INPUT(node, STORE_CTRL, ctrl);
    SET_INPUT(node, STORE_MEM, mem);
    SET_INPUT(node, STORE_ADDR, addr);
    SET_INPUT(node, STORE_VALUE, value);
    return node;
}

typedef struct {
    NodeSet* useful;
} TrimUselessContext;

static void trim_useless(SB_Node* node, void* _ctx) {
    TrimUselessContext* ctx = _ctx;
    
    for (SB_User** u = &node->users; *u;) {
        if (node_set_contains(ctx->useful, (*u)->node)) {
            u = &(*u)->next;
        }
        else {
            *u = (*u)->next;
        }
    }
}

SB_Proc* sb_proc(SB_Context* ctx, SB_Node* start, SB_Node* end) {
    NodeSet useful = {0};
    walk_graph(end, &useful, 0, 0);

    assert("the procedure never reaches the end node" && node_set_contains(&useful, start));

    TrimUselessContext trim_useless_ctx = {
        .useful = &useful
    };

    walk_graph(end, 0, trim_useless, &trim_useless_ctx);

    node_set_destroy(&useful);

    SB_Proc* proc = arena_type(ctx->arena, SB_Proc);
    proc->start = start;
    proc->end = end;
    return proc;
}

static char* proj_name(SB_Op op) {
    const char* mnemonic = sb_op_mnemonic[op];
    char* start = strrchr(mnemonic, '.');
    assert(start);
    return start + 1;
}

static void graphviz_node(NodeSet* visited, SB_Node* node, char* out, size_t out_sz) {
    if (node->flags & SB_NODE_FLAG_PROJECTION) {
        char temp[512];
        graphviz_node(visited, node->ins[PROJ_INPUT], temp, sizeof(temp));
        sprintf_s(out, out_sz, "%s:p_%s", temp, proj_name(node->op));
        return;
    }

    sprintf_s(out, out_sz, "n%p", node);

    if (node_set_contains(visited, node)) { return; }
    node_set_add(visited, node);

    printf("  %s [shape=\"record\",label=\"{", out);

    if (node->num_ins) {
        printf("{");

        for (int i = 0; i < node->num_ins; ++i) {
            if (i > 0) { printf("|"); }
            printf("<i%d>%d", i, i);
        }

        printf("}|");
    }
    
    printf("{%s}", sb_op_mnemonic[node->op]);

    bool has_projections = false;
    for (SB_User* u = node->users; u; u = u->next) {
        if (u->node->flags & SB_NODE_FLAG_PROJECTION) {
            has_projections = true;
            break;
        }
    }

    if (has_projections) {
        printf("|{");
        int count = 0;
        for (SB_User* u = node->users; u; u = u->next) {
            if (count++ > 0) {
                printf("|");
            }
            char* n = proj_name(u->node->op);
            printf("<p_%s>%s", n, n);
        }
        printf("}");
    }

    printf("}\"];\n");

    for (int i = 0; i < node->num_ins; ++i) {
        if (node->ins[i]) {
            char input_name[512];
            graphviz_node(visited, node->ins[i], input_name, sizeof(input_name));
            printf("  %s -> %s:i%d;\n", input_name, out, i);
        }
    }
}

void sb_graphviz(SB_Proc* proc) {
    printf("digraph G {\n");

    NodeSet visited = node_set_new();
    char temp[512];
    graphviz_node(&visited, proc->end, temp, sizeof(temp));

    node_set_destroy(&visited);
        
    printf("}\n\n");
}