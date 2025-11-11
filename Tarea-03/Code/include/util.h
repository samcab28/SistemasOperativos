#ifndef UTIL_H
#define UTIL_H

#include "memsim.h"

void die(const char *msg);
void *xmalloc(size_t n);
char *xstrdup(const char *s);
FitStrategy parse_strategy(const char *s);

#endif
