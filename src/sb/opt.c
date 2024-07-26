#include "internal.h"
#include "containers.h"

typedef struct {
    HashMap map;
} IndexMap;

static IndexMap index_map_new() { return (IndexMap) { .map = hash_map_new(sizeof(SB_Node*), sizeof(size_t), pointer_hash, pointer_cmp) }; }
static void index_map_destroy(IndexMap* map) { hash_map_destroy(&map->map); } 
static void index_map_insert(IndexMap* map, SB_Node* node, size_t index) { hash_map_insert(&map->map, &node, &index); }
static size_t index_map_get(IndexMap* map, SB_Node* node) { return *(size_t*)hash_map_get(&map->map, &node); }
static bool index_map_contains(IndexMap* map, SB_Node* node) { return hash_map_contains(&map->map, &node); }
static void index_map_remove(IndexMap* map, SB_Node* node) { hash_map_remove(&map->map, &node); }

typedef struct {
    Vec(SB_Node*) stack;
    IndexMap index_map;
} Worklist;

static Worklist worklist_new() {
    return (Worklist) {
        .index_map = index_map_new()
    };
}

static void worklist_destroy(Worklist* wl) {
    index_map_destroy(&wl->index_map);
    vec_destroy(wl->stack);
}

static void worklist_push(Worklist* wl, SB_Node* node) {
    if (index_map_contains(&wl->index_map, node)) { return; }

    size_t i = vec_len(wl->stack);
    vec_push(wl->stack, node);

    index_map_insert(&wl->index_map, node, i);
}

static SB_Node* worklist_pop(Worklist* wl) {
    SB_Node* node = vec_pop(wl->stack);
    assert(index_map_get(&wl->index_map, node) == vec_len(wl->stack));
    index_map_remove(&wl->index_map, node);
    return node;
}

static bool worklist_empty(Worklist* wl) {
    return vec_len(wl->stack) == 0;
}

static void worklist_remove(Worklist* wl, SB_Node* node) {
    if (!index_map_contains(&wl->index_map, node)) { return; }

    size_t index = index_map_get(&wl->index_map, node);
    SB_Node* last = wl->stack[index] = vec_pop(wl->stack);
    index_map_insert(&wl->index_map, last, index);
    index_map_remove(&wl->index_map, node);
}

typedef struct {
    Worklist* wl;
} WorklistInitContext;

static void worklist_init_fn(SB_Node* node, void* _ctx) {
    WorklistInitContext* ctx = _ctx;
    worklist_push(ctx->wl, node);
}

static void init_worklist(Worklist* wl, SB_Proc* proc) {
    WorklistInitContext init_ctx = {
        .wl = wl
    };

    walk_graph(proc->end, 0, worklist_init_fn, &init_ctx);
}

static void remove_user(SB_Node* node, SB_Node* user, int index) {
    for (SB_User** u = &node->users; *u; u = &(*u)->next) {
        if ((*u)->node == user && (*u)->index == index) {
            *u = (*u)->next;
            return;
        }
    }

    assert("not in user list" && false);
}

static void delete_node(Worklist* wl, SB_Node* node) {
    assert("trying to remove a node that has users" && !node->users);

    worklist_remove(wl, node);

    for (int i = 0; i < node->num_ins; ++i) {
        node->ins[i];
        remove_user(node->ins[i], node, i);

        if (!node->ins[i]->users) {
            delete_node(wl, node->ins[i]);
        }
    }
}

static void replace_node(Worklist* wl, SB_Node* dest, SB_Node* src) {
    while (dest->users) {
        SB_User* u = dest->users;
        dest->users = u->next;

        u->node->ins[u->index] = src;

        u->next = src->users;
        src->users = u;
    }

    delete_node(wl, dest);
}

typedef SB_Node*(*IdealizeFn)(SB_Context*, Worklist*, SB_Node*);

static SB_Node* idealize_phi(SB_Context* ctx, Worklist* wl, SB_Node* node) {
    (void)ctx;

    SB_Node* same = 0;

    for (int i = 1; i < node->num_ins; ++i) {
        if (node->ins[i] == node) { continue; } 

        if (!same) {
            same = node->ins[i];
        }

        if (same != node->ins[i]) { return node; }
    }

    assert(same);
    worklist_push(wl, node->ins[0]); // Region may be able to be collapsed
    return same;
}

static SB_Node* idealize_region(SB_Context* ctx, Worklist* wl, SB_Node* node) {
    (void)wl;
    (void)ctx;

    for (SB_User* u = node->users; u; u = u->next) {
        if (u->node->op == SB_OP_PHI && u->index == 0) {
            return node;
        }
    }

    SB_Node* same = 0;

    for (int i = 0; i < node->num_ins; ++i) {
        if (!same) {
            same = node->ins[i];
        }

        if (same != node->ins[i]) {
            return node;
        }
    }

    assert(same);
    return same;
}

static IdealizeFn idealize_table[NUM_SB_OPS] = {
    [SB_OP_PHI] = idealize_phi,
    [SB_OP_REGION] = idealize_region,
};

void sb_opt(SB_Context* ctx, SB_Proc* proc) {
    (void)ctx;

    Worklist wl = worklist_new();
    init_worklist(&wl, proc);

    while (!worklist_empty(&wl)) {
        SB_Node* node = worklist_pop(&wl);

        if (idealize_table[node->op]) {
            IdealizeFn fn = idealize_table[node->op];
            SB_Node* ideal = fn(ctx, &wl, node);

            if (ideal != node) {
                replace_node(&wl, node, ideal);
                node = ideal;

                for (SB_User* u = ideal->users; u; u = u->next) {
                    worklist_push(&wl, u->node);
                }
            }
        }
    }

    worklist_destroy(&wl);
} 