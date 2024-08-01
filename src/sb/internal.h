#include "sb.h"
#include "core.h"
#include "containers.h"

#define VIEW_DATA(n, type) (*(type*)((n)->data))

struct SB_Context {
    Arena* arena;
    ScratchLibrary scratch_lib;
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

enum {
    BRANCH_CTRL,
    BRANCH_PREDICATE,
    NUM_BRANCH_INS
};

enum {
    LOAD_CTRL,
    LOAD_MEM,
    LOAD_ADDR,
    NUM_LOAD_INS
};

enum {
    STORE_CTRL,
    STORE_MEM,
    STORE_ADDR,
    STORE_VALUE,
    NUM_STORE_INS
};

typedef struct {
    HashSet set;
} NodeSet;

static NodeSet node_set_new() {
    return (NodeSet) {
        .set = hash_set_new(sizeof(SB_Node*), pointer_hash, pointer_cmp)
    };
}

static void node_set_destroy(NodeSet* set) {
    hash_set_destroy(&set->set);
}

static void node_set_add(NodeSet* set, SB_Node* node) {
    hash_set_insert(&set->set, &node);
}

static bool node_set_contains(NodeSet* set, SB_Node* node) {
    return hash_set_contains(&set->set, &node);
}

static void node_set_remove(NodeSet* set, SB_Node* node) {
    hash_set_remove(&set->set, &node);
}

typedef void(*VisitNodeFn)(SB_Node*, void*);

static void walk_graph(SB_Node* end, NodeSet* visited_out, VisitNodeFn visit_fn, void* visit_ctx) {
    Vec(SB_Node*) stack = 0;
    vec_push(stack, end);

    NodeSet visited = node_set_new();

    while (vec_len(stack)) {
        SB_Node* node = vec_pop(stack);

        if (node_set_contains(&visited, node)) { continue; }
        node_set_add(&visited, node);

        if (visit_fn) {
            visit_fn(node, visit_ctx);
        }

        for (int i = 0; i < node->num_ins; ++i) {
            if (node->ins[i]) {
                vec_push(stack, node->ins[i]);
            }
        }
    }

    if (visited_out) {
        *visited_out = visited;
    }
    else {
        node_set_destroy(&visited);
    }

    vec_destroy(stack);
}

SB_Schedule* schedule(Arena* arena, SB_Proc* proc);