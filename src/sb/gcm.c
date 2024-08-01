#include "internal.h"
#include "containers.h"

typedef struct {
    HashMap map;
} BlockMap;

static BlockMap block_map_new() {
    return (BlockMap) {
        .map = hash_map_new(sizeof(SB_Node*), sizeof(SB_Block*), pointer_hash, pointer_cmp)
    };
}

static void block_map_destroy(BlockMap* map) {
    hash_map_destroy(&map->map);
}

static void block_map_insert(BlockMap* map, SB_Node* node, SB_Block* block) {
    hash_map_insert(&map->map, &node, &block);
}

static SB_Block* block_map_get(BlockMap* map, SB_Node* node) {
    return *(SB_Block**)hash_map_get(&map->map, &node);
}

typedef struct {
    SB_Block* head;
    BlockMap block_map;
} CFG;

static SB_Block* new_block(Arena* arena) {
    return arena_type(arena, SB_Block);
}

static Vec(SB_Node*) get_postorder(SB_Proc* proc) {
    Vec(SB_Node*) stack = 0;
    NodeSet visited = node_set_new();

    vec_push(stack, proc->start);

    Vec(SB_Node*) result = 0;

    while(vec_len(stack)) {
        SB_Node* node = vec_pop(stack);

        if (node_set_contains(&visited, node)) { continue; }
        node_set_add(&visited, node);

        for (SB_User* u = node->users; u; u = u->next) {
            if (!(u->node->flags & SB_NODE_FLAG_TRANSFERS_CONTROL)) { continue; }
            vec_push(stack, u->node);
        }

        vec_push(result, node);
    }

    vec_destroy(stack);
    node_set_destroy(&visited);

    return postorder;
}

static CFG build_cfg(Arena* arena, SB_Proc* proc) {
    Vec(SB_Node*) postorder = get_postorder(proc);

    SB_Block* head = 0;
    BlockMap block_map = block_map_new();

    for (size_t i = 0; i < vec_len(postorder); ++i) {
        SB_Node* node = postorder[i];

        SB_Block* block = head;

        if (node->flags & SB_NODE_FLAG_STARTS_BASIC_BLOCK) {
            block = new_block(arena);
        }

        block_map_insert(&block_map, node, block);

        if (block != head) {
            block->next = head;
            head = block;
        }
    }

    vec_destroy(postorder);

    return (CFG) {
        .block_map = block_map,
        .head = head
    };
}

SB_Schedule* schedule(Arena* arena, SB_Proc* proc) {
    CFG cfg = build_cfg(arena, proc);
    (void)cfg;
    return 0;
}