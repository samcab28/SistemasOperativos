#include "varmap.h"
#include "util.h"

VarMap* find_var(VarMap *head, const char *name) {
    for (VarMap *p=head; p; p=p->next)
        if (strcmp(p->name, name)==0) return p;
    return NULL;
}

void set_var(MemSim *ms, const char *name, BlockHeader *b) {
    VarMap *v = find_var(ms->vars, name);
    if (v) { v->block = b; return; }
    v = (VarMap*)xmalloc(sizeof(*v));
    v->name = xstrdup(name);
    v->block = b;
    v->next = ms->vars;
    ms->vars = v;
}

void unset_var(MemSim *ms, const char *name) {
    VarMap **pp = &ms->vars;
    while (*pp) {
        if (strcmp((*pp)->name, name)==0) {
            VarMap *t = *pp;
            *pp = (*pp)->next;
            free(t->name);
            free(t);
            return;
        }
        pp = &((*pp)->next);
    }
}
