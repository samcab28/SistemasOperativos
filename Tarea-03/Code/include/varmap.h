#ifndef VARMAP_H
#define VARMAP_H

#include "memsim.h"

VarMap* find_var(VarMap *head, const char *name);
void set_var(MemSim *ms, const char *name, BlockHeader *b);
void unset_var(MemSim *ms, const char *name);

#endif
