#ifndef BLOCK_H
#define BLOCK_H

#include "memsim.h"

BlockHeader* new_block(size_t offset, size_t size, int free);
void insert_after(BlockHeader *pos, BlockHeader *node);
void remove_block(MemSim *ms, BlockHeader *b);
void try_coalesce(MemSim *ms, BlockHeader *b);

#endif
