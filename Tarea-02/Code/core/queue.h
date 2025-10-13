#ifndef QUEUE_H
#define QUEUE_H

#include "product.h"
#include <pthread.h>
#include <semaphore.h>

typedef struct node {
    product_t *prod;
    struct node *next;
} node_t;

typedef struct queue {
    node_t *head;
    node_t *tail;
    pthread_mutex_t lock;
    sem_t items;
} queue_t;

/* Inicializar cola */
void queue_init(queue_t *q);

/* Push thread-safe (al final) */
void queue_push(queue_t *q, product_t *p);

/* Pop thread-safe (bloqueante) */
product_t* queue_pop(queue_t *q);

/* Pop no bloqueante: devuelve NULL si no hay elementos */
product_t* queue_try_pop(queue_t *q);

/* Vaciar cola y liberar nodos (no libera product_t) */
void queue_destroy(queue_t *q);

#endif
