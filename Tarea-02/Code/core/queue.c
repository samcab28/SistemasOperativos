#include "queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

void queue_init(queue_t *q) {
    q->head = q->tail = NULL;
    pthread_mutex_init(&q->lock, NULL);
    // Semáforo inicializado en cero: no hay elementos disponibles
    // y las operaciones pop bloquearán hasta que se haga post.
    sem_init(&q->items, 0, 0);
}

void queue_push(queue_t *q, product_t *p) {
    node_t *n = malloc(sizeof(node_t));
    n->prod = p;
    n->next = NULL;
    pthread_mutex_lock(&q->lock);
    if (!q->tail) q->head = q->tail = n;
    else { q->tail->next = n; q->tail = n; }
    pthread_mutex_unlock(&q->lock);
    sem_post(&q->items);
}

product_t* queue_pop(queue_t *q) {
    // Esperar hasta que exista al menos un elemento disponible
    sem_wait(&q->items);
    pthread_mutex_lock(&q->lock);
    node_t *n = q->head;
    if (!n) {
        // Reponer el semáforo si detectamos condición de carrera
        pthread_mutex_unlock(&q->lock);
        sem_post(&q->items);
        return NULL;
    }
    q->head = n->next;
    if (!q->head) q->tail = NULL;
    pthread_mutex_unlock(&q->lock);
    product_t *p = n->prod;
    free(n);
    return p;
}

product_t* queue_try_pop(queue_t *q) {
    if (sem_trywait(&q->items) != 0) {
        return NULL;
    }

    pthread_mutex_lock(&q->lock);
    node_t *n = q->head;
    if (!n) {
        pthread_mutex_unlock(&q->lock);
        // No reponemos el semáforo en la versión no bloqueante: el caller asume fallo.
        return NULL;
    }
    q->head = n->next;
    if (!q->head) q->tail = NULL;
    pthread_mutex_unlock(&q->lock);

    product_t *p = n->prod;
    free(n);
    return p;
}

void queue_destroy(queue_t *q) {
    // consumir todo para liberar nodos (no libera productos)
    pthread_mutex_lock(&q->lock);
    node_t *cur = q->head;
    while (cur) {
        node_t *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    q->head = q->tail = NULL;
    pthread_mutex_unlock(&q->lock);
    sem_destroy(&q->items);
    pthread_mutex_destroy(&q->lock);
}
