#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>

struct scheduler {
    sched_type_t type;
    int quantum;
    product_t *queue[100];
    int front, rear;
};

scheduler_t *create_scheduler(sched_type_t type, int quantum) {
    scheduler_t *s = malloc(sizeof(scheduler_t));
    s->type = type;
    s->quantum = quantum;
    s->front = 0;
    s->rear = 0;
    return s;
}

void add_to_scheduler(scheduler_t *sched, product_t *p) {
    sched->queue[sched->rear++] = p;
}

void run_scheduler(scheduler_t *sched, station_t *start) {
    printf("Ejecutando scheduler...\n");
    for (int i = sched->front; i < sched->rear; i++) {
        product_t *p = sched->queue[i];
        process_product(start, p);
    }
}

void free_scheduler(scheduler_t *sched) {
    free(sched);
}
