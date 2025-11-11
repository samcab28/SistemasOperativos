#ifndef MEMSIM_H
#define MEMSIM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>

typedef enum { FIRST_FIT=0, BEST_FIT=1, WORST_FIT=2 } FitStrategy;

typedef struct BlockHeader {
    size_t offset;
    size_t size;
    int free;
    struct BlockHeader *prev;
    struct BlockHeader *next;
} BlockHeader;

typedef struct VarMap {
    char *name;
    BlockHeader *block;
    struct VarMap *next;
} VarMap;

typedef struct {
    uint8_t *pool;
    size_t pool_size;
    BlockHeader *blocks;
    FitStrategy strat;
    VarMap *vars;
} MemSim;

void init_memsim(MemSim *ms, FitStrategy s, size_t pool_size);
void run_script(MemSim *ms, const char *path);
void cleanup_memsim(MemSim *ms);
void expand_pool(MemSim *ms, size_t extra_bytes);

#endif
