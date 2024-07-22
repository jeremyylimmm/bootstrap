#include <stdio.h>

#include "frontend.h"

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

void hir_print(HIR_Proc* proc, char* name) {
    printf("-- proc %s --\n", name);

    int nb = 0;
    int nn = 0;

    for (HIR_Block* block = proc->control_flow_head; block; block = block->next) {
        block->tid = nb++;

        for (HIR_Node* n = block->start; n; n = n->next) {
            n->tid = nn++;
        }
    }

    for (HIR_Block* block = proc->control_flow_head; block; block = block->next) {
        printf("bb_%d:\n", block->tid);

        for (HIR_Node* n = block->start; n; n = n->next) {
            printf("  %%%-4d =  ", n->tid);

            static_assert(NUM_HIR_OPS == 6, "handle print ops");
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
            }

            printf("\n");
        }
    }
}