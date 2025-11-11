#include "block.h"
#include "util.h"

BlockHeader* new_block(size_t offset, size_t size, int free) {
    BlockHeader *b = (BlockHeader*)xmalloc(sizeof(*b));
    b->offset = offset;
    b->size = size;
    b->free = free;
    return b;
}

void insert_after(BlockHeader *pos, BlockHeader *node) {
    node->prev = pos;
    node->next = pos->next;
    if (pos->next) pos->next->prev = node;
    pos->next = node;
}

void remove_block(MemSim *ms, BlockHeader *b) {
    if (b->prev) b->prev->next = b->next;
    else ms->blocks = b->next;
    if (b->next) b->next->prev = b->prev;
    free(b);
}

void try_coalesce(MemSim *ms, BlockHeader *b) {
    if (b->prev && b->prev->free &&
        b->prev->offset + b->prev->size == b->offset) {
        b->prev->size += b->size;
        remove_block(ms, b);
        b = b->prev;
    }
    if (b->next && b->next->free &&
        b->offset + b->size == b->next->offset) {
        b->size += b->next->size;
        remove_block(ms, b->next);
    }
}
