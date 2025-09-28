#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "product.h"
#include "station.h"

// Evitar conflicto con <sched.h>
typedef enum {
    MY_SCHED_FCFS,
    MY_SCHED_RR
} sched_type_t;

typedef struct scheduler scheduler_t;

scheduler_t *create_scheduler(sched_type_t type, int quantum);
void add_to_scheduler(scheduler_t *sched, product_t *p);
void run_scheduler(scheduler_t *sched, station_t *start);
void free_scheduler(scheduler_t *sched);

#endif
