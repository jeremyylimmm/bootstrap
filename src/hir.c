#include <stdio.h>

#include "containers.h"
#include "frontend.h"

#define foreach_block(it, proc) for (HIR_Block* it = proc->control_flow_head; it; it = it->next)
#define foreach_node(it, block) for (HIR_Node* it = block->start; it; it = it->next)

static void fix_links(HIR_Node* node) {
    if (node->prev) {
        node->prev->next = node;
    }
    else {
        node->block->start = node;
    }

    if (node->next) {
        node->next->prev = node;
    }
    else {
        node->block->end = node;
    }
}

void hir_append(HIR_Block* block, HIR_Node* node) {
    assert(!node->block);
    node->block = block;
    node->next = 0;
    node->prev = block->end;
    fix_links(node);
}

typedef struct {
    int num_blocks;
    int num_nodes;
} BlockNodeCount;

static BlockNodeCount assign_tids(HIR_Proc* proc) {
    int nb = 0;
    int nn = 0;

    foreach_block(block, proc) {
        block->tid = nb++;

        foreach_node(n, block) {
            n->tid = nn++;
        }
    }

    return (BlockNodeCount) {
        .num_blocks = nb,
        .num_nodes = nn
    };
}

void hir_print(HIR_Proc* proc, char* name) {
    printf("-- proc %s --\n", name);

    assign_tids(proc);

    foreach_block(block, proc) {
        printf("bb_%d:\n", block->tid);

        foreach_node(n, block) {
            printf("  %%%-3d = ", n->tid);

            static_assert(NUM_HIR_OPS == 9, "handle print ops");
            switch (n->op) {
                default:
                    assert(false);
                    break;
                case HIR_OP_INT_CONST:
                    printf("$%lld", n->as.int_const.low);
                    break;
                case HIR_OP_ADD:
                    printf("add %%%d, %%%d", n->as.binary[0]->tid, n->as.binary[1]->tid);
                    break;                                  
                case HIR_OP_SUB:                            
                    printf("sub %%%d, %%%d", n->as.binary[0]->tid, n->as.binary[1]->tid);
                    break;                                  
                case HIR_OP_MUL:                            
                    printf("mul %%%d, %%%d", n->as.binary[0]->tid, n->as.binary[1]->tid);
                    break;                                  
                case HIR_OP_DIV:                            
                    printf("div %%%d, %%%d", n->as.binary[0]->tid, n->as.binary[1]->tid);
                    break;
                case HIR_OP_JUMP:
                    printf("jmp bb_%d", n->as.jump.loc->tid);
                    break;
                case HIR_OP_BRANCH:
                    printf("branch %%%d [bb_%d, bb_%d]", n->as.branch.predicate->tid, n->as.branch.loc_then->tid, n->as.branch.loc_else->tid);
                    break;
                case HIR_OP_RET:
                    printf("ret");
                    if (n->as.ret.value) {
                        printf(" %%%d", n->as.ret.value->tid);
                    }
            }

            printf("\n");
        }
    }

    printf("\n");
}

typedef struct {
    int count;
    HIR_Block* arr[2];
} Successors;

static Successors get_successors(HIR_Block* block) {
    Successors result = {0};

    if (block->end) {
        switch (block->end->op) {
            case HIR_OP_JUMP:
                result.arr[result.count++] = block->end->as.jump.loc;
                break;
            case HIR_OP_BRANCH:
                result.arr[result.count++] = block->end->as.branch.loc_then;
                result.arr[result.count++] = block->end->as.branch.loc_else;
                break;
        }
    }

    return result;
}

static Bitset* walk_cfg(Arena* arena, HIR_Proc* proc, int num_blocks) {
    Bitset* reachable = bitset_alloc(arena, num_blocks);

    Vec(HIR_Block*) stack = 0;
    vec_push(stack, proc->control_flow_head);
    
    while (vec_len(stack)) {
        HIR_Block* block = vec_pop(stack);

        if (bitset_get(reachable, block->tid)) { continue; }
        bitset_set(reachable, block->tid);

        Successors successors = get_successors(block);
        for (int i = 0; i < successors.count; ++i) {
            vec_push(stack, successors.arr[i]);
        }
    }

    vec_destroy(stack);

    return reachable;
}

typedef struct {
    SB_Node* region;
    SB_Node* mem_phi;

    Vec(SB_Node*) ctrl_ins;
    Vec(SB_Node*) mem_ins;
} BlockHead;

typedef struct {
    Vec(SB_Node*) ctrl;
    Vec(SB_Node*) mem;
    Vec(SB_Node*) ret_val;
} EndPaths;

typedef struct {
    SB_Node* ctrl;
    SB_Node* mem;
    SB_Node* ret_val;
} State;

static SB_Node* lower_node(SB_Context* ctx, SB_Node** conv, State* state, SB_Node** ctrl_out, HIR_Node* n) {
    static_assert(NUM_HIR_OPS == 9, "handle hir instruction lowering");
    switch (n->op) {
        default:
            assert(false);
            return 0;
        case HIR_OP_INT_CONST:
            return sb_node_int_const(ctx, n->as.int_const.low);
        case HIR_OP_ADD:
            return sb_node_add(ctx, conv[n->as.binary[0]->tid], conv[n->as.binary[1]->tid]);
        case HIR_OP_SUB:
            return sb_node_sub(ctx, conv[n->as.binary[0]->tid], conv[n->as.binary[1]->tid]);
        case HIR_OP_MUL:
            return sb_node_mul(ctx, conv[n->as.binary[0]->tid], conv[n->as.binary[1]->tid]);
        case HIR_OP_DIV:
            return sb_node_sdiv(ctx, conv[n->as.binary[0]->tid], conv[n->as.binary[1]->tid]);
        case HIR_OP_JUMP:
            return 0;
        case HIR_OP_BRANCH: {
            SB_Node* branch = state->ctrl = sb_node_branch(ctx, state->ctrl, conv[n->as.branch.predicate->tid]);
            ctrl_out[0] = sb_node_branch_then(ctx, branch);
            ctrl_out[1] = sb_node_branch_else(ctx, branch);
            return 0;
        } 
        case HIR_OP_RET:
            if (n->as.ret.value) {
                state->ret_val = conv[n->as.ret.value->tid];
            }
            return 0;
    }
}

SB_Proc* hir_lower(SB_Context* ctx, HIR_Proc* hir_proc) {
    Scratch* scratch = scratch_get(get_global_scratch_library(), 0, 0);

    BlockNodeCount bnc = assign_tids(hir_proc);
    Bitset* reachable = walk_cfg(scratch->arena, hir_proc, bnc.num_blocks);

    // Initialize regions and memory phis for each basic block

    BlockHead* heads = arena_array(scratch->arena, BlockHead, bnc.num_blocks);

    foreach_block(block, hir_proc) {
        if (!bitset_get(reachable, block->tid)) { continue; }

        heads[block->tid] = (BlockHead) {
            .region = sb_node_region(ctx),
            .mem_phi = sb_node_phi(ctx),
        };
    }

    // Generate the graph for each basic block

    SB_Node** conv = arena_array(scratch->arena, SB_Node*, bnc.num_nodes); // Convert HIR_Node to SB_Node
    EndPaths end_paths = {0}; // Stores each return pathway

    foreach_block(block, hir_proc) {
        if (!bitset_get(reachable, block->tid)) { continue; }

        State state = {
            .ctrl = heads[block->tid].region,
            .mem = heads[block->tid].mem_phi,
        };
       
        SB_Node* ctrl_out[2] = {0};

        foreach_node(n, block) {
            conv[n->tid] = lower_node(ctx, conv, &state, ctrl_out, n);
        }

        for (int i = 0; i < ARRAY_LENGTH(ctrl_out); ++i) {
            if (!ctrl_out[i]) {
                ctrl_out[i] = state.ctrl;
            }
        }

        Successors successors = get_successors(block);

        for (int i = 0; i < successors.count; ++i) {
            HIR_Block* s = successors.arr[i];
            vec_push(heads[s->tid].mem_ins, state.mem);
            vec_push(heads[s->tid].ctrl_ins, ctrl_out[i]);
        }

        if (!successors.count) {
            SB_Node* ret_val = state.ret_val ? state.ret_val : sb_node_null(ctx);
            vec_push(end_paths.ctrl, state.ctrl);
            vec_push(end_paths.mem, state.mem);
            vec_push(end_paths.ret_val, ret_val);
        }
    }

    SB_Node* start = sb_node_start(ctx);
    SB_Node* start_mem = sb_node_start_mem(ctx, start);
    SB_Node* start_ctrl = sb_node_start_ctrl(ctx, start);

    // Entry-point gets start memory and control inputs

    assert(!vec_len(heads[0].ctrl_ins));
    vec_push(heads[0].ctrl_ins, start_ctrl);
    vec_push(heads[0].mem_ins, start_mem);
    
    // Add the memory and control inputs into the regions and phis

    foreach_block(block, hir_proc) {
        if (!bitset_get(reachable, block->tid)) { continue; }

        BlockHead* h = &heads[block->tid];

        sb_provide_region_inputs(ctx, h->region, (int)vec_len(h->ctrl_ins), h->ctrl_ins);
        sb_provide_phi_inputs(ctx, h->mem_phi, h->region, (int)vec_len(h->mem_ins), h->mem_ins);

        vec_destroy(h->ctrl_ins);
        vec_destroy(h->mem_ins);
    }

    // Construct join-nodes for end node

    SB_Node* end_region = sb_node_region(ctx);
    SB_Node* end_mem_phi = sb_node_phi(ctx);
    SB_Node* end_ret_val_phi = sb_node_phi(ctx);

    assert(vec_len(end_paths.ctrl)); // At least one exiting path - otherwise malformed IR

    sb_provide_region_inputs(ctx, end_region, (int)vec_len(end_paths.ctrl), end_paths.ctrl);
    sb_provide_phi_inputs(ctx, end_mem_phi, end_region, (int)vec_len(end_paths.mem), end_paths.mem);
    sb_provide_phi_inputs(ctx, end_ret_val_phi, end_region, (int)vec_len(end_paths.ret_val), end_paths.ret_val);

    vec_destroy(end_paths.ctrl);
    vec_destroy(end_paths.mem);
    vec_destroy(end_paths.ret_val);

    // Make proc

    SB_Node* end = sb_node_end(ctx, end_region, end_mem_phi, end_ret_val_phi);
    SB_Proc* proc = sb_proc(ctx, start, end);

    scratch_release(scratch);

    return proc;
}