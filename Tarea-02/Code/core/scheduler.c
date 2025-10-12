#include "scheduler.h"
#include "../metrics/logger.h"
#include "../metrics/metrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

// Nombres de los algoritmos
static const char *algorithm_names[] = {
    "FCFS",
    "Round Robin"
};

// Nombres de los estados
static const char *state_names[] = {
    "IDLE",
    "RUNNING",
    "PAUSED",
    "STOPPED"
};

// =============================================
// FUNCIONES AUXILIARES INTERNAS
// =============================================

static void get_current_time(struct timespec *ts) {
    clock_gettime(CLOCK_MONOTONIC, ts);
}

static long time_diff_ms(const struct timespec *start, const struct timespec *end) {
    long seconds = end->tv_sec - start->tv_sec;
    long nanoseconds = end->tv_nsec - start->tv_nsec;
    return (seconds * 1000) + (nanoseconds / 1000000);
}

static struct timespec timespec_add_ms(const struct timespec *start, long ms) {
    struct timespec result = *start;
    result.tv_sec += ms / 1000;
    result.tv_nsec += (ms % 1000) * 1000000L;
    if (result.tv_nsec >= 1000000000L) {
        result.tv_sec += result.tv_nsec / 1000000000L;
        result.tv_nsec %= 1000000000L;
    }
    return result;
}

typedef enum {
    SCHED_EVENT_NONE = 0,
    SCHED_EVENT_SLICE_COMPLETED,
    SCHED_EVENT_SLICE_PREEMPTED
} scheduler_event_type_t;

// =============================================
// FUNCIONES DE CREACIÓN Y DESTRUCCIÓN
// =============================================

scheduler_t *create_scheduler(scheduling_algorithm_t algorithm, int quantum_ms) {
    scheduler_t *scheduler = malloc(sizeof(scheduler_t));
    if (!scheduler) {
        SCHEDULER_ERROR("No se pudo crear scheduler");
        return NULL;
    }
    
    // Inicializar datos básicos
    scheduler->id = 0;
    strcpy(scheduler->name, "MainScheduler");
    
    // Threading
    scheduler->thread_running = 0;
    
    // Sincronización
    pthread_mutex_init(&scheduler->mutex, NULL);
    pthread_cond_init(&scheduler->products_available, NULL);
    sem_init(&scheduler->scheduling_semaphore, 0, 1);
    
    // Configuración
    scheduler->config.algorithm = algorithm;
    scheduler->config.quantum_ms = quantum_ms;
    scheduler->config.preemption_enabled = (algorithm == SCHED_ROUND_ROBIN);
    scheduler->config.max_products_in_system = 100;
    
    // Estrategia (se configura después)
    scheduler->strategy = NULL;
    
    // Colas
    scheduler->ready_queue = malloc(sizeof(queue_t));
    queue_init(scheduler->ready_queue);
    
    scheduler->waiting_queue = malloc(sizeof(queue_t));
    queue_init(scheduler->waiting_queue);
    
    // Estado
    scheduler->current_product = NULL;
    scheduler->first_station = NULL;
    scheduler->state = SCHED_IDLE;
    
    // Estadísticas
    memset(&scheduler->stats, 0, sizeof(scheduler->stats));
    get_current_time(&scheduler->stats.start_time);
    scheduler->stats.last_dispatched_product = NULL;
    
    // Control de quantum
    memset(&scheduler->quantum_control, 0, sizeof(scheduler->quantum_control));
    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
    pthread_cond_init(&scheduler->quantum_control.cond, &cond_attr);
    pthread_condattr_destroy(&cond_attr);
    scheduler->quantum_control.event_type = SCHED_EVENT_NONE;
    
    SCHEDULER_INFO("Scheduler creado (algoritmo: %s, quantum: %d ms)",
                   algorithm_names[algorithm], quantum_ms);
    
    return scheduler;
}

void destroy_scheduler(scheduler_t *scheduler) {
    if (!scheduler) return;
    
    SCHEDULER_INFO("Destruyendo scheduler");
    
    // Detener hilo si está corriendo
    if (scheduler->thread_running) {
        scheduler_stop_thread(scheduler);
    }
    
    // Destruir sincronización
    pthread_mutex_destroy(&scheduler->mutex);
    pthread_cond_destroy(&scheduler->products_available);
    sem_destroy(&scheduler->scheduling_semaphore);
    pthread_cond_destroy(&scheduler->quantum_control.cond);
    
    // Destruir colas
    if (scheduler->ready_queue) {
        queue_destroy(scheduler->ready_queue);
        free(scheduler->ready_queue);
    }
    
    if (scheduler->waiting_queue) {
        queue_destroy(scheduler->waiting_queue);
        free(scheduler->waiting_queue);
    }
    
    // Destruir estrategia
    if (scheduler->strategy) {
        free(scheduler->strategy);
    }
    
    free(scheduler);
}

// =============================================
// FUNCIONES DE CONFIGURACIÓN
// =============================================

void scheduler_set_first_station(scheduler_t *scheduler, station_t *station) {
    if (!scheduler) return;
    
    pthread_mutex_lock(&scheduler->mutex);
    scheduler->first_station = station;
    pthread_mutex_unlock(&scheduler->mutex);
    
    if (station) {
        station_set_scheduler(station, scheduler);
    }
    
    SCHEDULER_INFO("Primera estación configurada: %s", 
                   station ? station->name : "NULL");
}

void scheduler_set_ready_queue(scheduler_t *scheduler, queue_t *queue) {
    if (!scheduler) return;
    
    pthread_mutex_lock(&scheduler->mutex);
    
    // Destruir cola existente si hay
    if (scheduler->ready_queue) {
        queue_destroy(scheduler->ready_queue);
        free(scheduler->ready_queue);
    }
    
    scheduler->ready_queue = queue;
    pthread_mutex_unlock(&scheduler->mutex);
    
    SCHEDULER_DEBUG("Cola de listos configurada");
}

void scheduler_set_algorithm(scheduler_t *scheduler, scheduling_algorithm_t algorithm) {
    if (!scheduler) return;
    
    pthread_mutex_lock(&scheduler->mutex);
    scheduler->config.algorithm = algorithm;
    scheduler->config.preemption_enabled = (algorithm == SCHED_ROUND_ROBIN);
    pthread_mutex_unlock(&scheduler->mutex);
    
    SCHEDULER_INFO("Algoritmo cambiado a: %s", algorithm_names[algorithm]);
}

void scheduler_set_quantum(scheduler_t *scheduler, int quantum_ms) {
    if (!scheduler) return;
    
    pthread_mutex_lock(&scheduler->mutex);
    scheduler->config.quantum_ms = quantum_ms;
    pthread_mutex_unlock(&scheduler->mutex);
    
    SCHEDULER_INFO("Quantum configurado: %d ms", quantum_ms);
}

void scheduler_set_preemption(scheduler_t *scheduler, int enabled) {
    if (!scheduler) return;
    
    pthread_mutex_lock(&scheduler->mutex);
    scheduler->config.preemption_enabled = enabled;
    pthread_mutex_unlock(&scheduler->mutex);
    
    SCHEDULER_DEBUG("Preemption: %s", enabled ? "HABILITADO" : "DESHABILITADO");
}

void scheduler_set_max_products(scheduler_t *scheduler, int max_products) {
    if (!scheduler) return;
    
    pthread_mutex_lock(&scheduler->mutex);
    scheduler->config.max_products_in_system = max_products;
    pthread_mutex_unlock(&scheduler->mutex);
    
    SCHEDULER_DEBUG("Máximo de productos: %d", max_products);
}

// =============================================
// FUNCIONES DE CONTROL DEL HILO
// =============================================

int scheduler_start_thread(scheduler_t *scheduler) {
    if (!scheduler) return 0;
    
    if (scheduler->thread_running) {
        SCHEDULER_INFO("Hilo del scheduler ya está corriendo");
        return 1;
    }
    
    if (!scheduler->first_station) {
        SCHEDULER_ERROR("No hay primera estación configurada");
        return 0;
    }
    
    scheduler->thread_running = 1;
    scheduler_set_state(scheduler, SCHED_RUNNING);
    
    if (pthread_create(&scheduler->thread, NULL, scheduler_worker_thread, scheduler) != 0) {
        SCHEDULER_ERROR("Fallo al crear hilo del scheduler");
        scheduler->thread_running = 0;
        return 0;
    }
    
    SCHEDULER_INFO("Hilo del scheduler iniciado");
    return 1;
}

void scheduler_stop_thread(scheduler_t *scheduler) {
    if (!scheduler || !scheduler->thread_running) return;
    
    SCHEDULER_INFO("Deteniendo scheduler...");
    
    pthread_mutex_lock(&scheduler->mutex);
    scheduler->thread_running = 0;
    pthread_cond_broadcast(&scheduler->products_available);
    pthread_cond_broadcast(&scheduler->quantum_control.cond);
    pthread_mutex_unlock(&scheduler->mutex);

    // Desbloquear posibles waits en las colas internas
    if (scheduler->ready_queue) {
        sem_post(&scheduler->ready_queue->items);
    }
    if (scheduler->waiting_queue) {
        sem_post(&scheduler->waiting_queue->items);
    }
    
    pthread_join(scheduler->thread, NULL);
    
    scheduler_set_state(scheduler, SCHED_STOPPED);
    SCHEDULER_INFO("Scheduler detenido");
}

void scheduler_pause(scheduler_t *scheduler) {
    if (!scheduler) return;
    
    pthread_mutex_lock(&scheduler->mutex);
    if (scheduler->state == SCHED_RUNNING) {
        scheduler->state = SCHED_PAUSED;
        SCHEDULER_INFO("Scheduler pausado");
    }
    pthread_mutex_unlock(&scheduler->mutex);
}

void scheduler_resume(scheduler_t *scheduler) {
    if (!scheduler) return;
    
    pthread_mutex_lock(&scheduler->mutex);
    if (scheduler->state == SCHED_PAUSED) {
        scheduler->state = SCHED_RUNNING;
        pthread_cond_broadcast(&scheduler->products_available);
        SCHEDULER_INFO("Scheduler reanudado");
    }
    pthread_mutex_unlock(&scheduler->mutex);
}

int scheduler_is_running(const scheduler_t *scheduler) {
    return scheduler ? scheduler->thread_running : 0;
}

// =============================================
// FUNCIÓN PRINCIPAL DEL HILO (Worker)
// =============================================

void *scheduler_worker_thread(void *arg) {
    scheduler_t *scheduler = (scheduler_t *)arg;
    
    SCHEDULER_INFO("Worker thread del scheduler iniciado");
    
    // Seleccionar función de procesamiento según algoritmo
    while (scheduler->thread_running) {
        pthread_mutex_lock(&scheduler->mutex);
        
        // Esperar si está pausado
        while (scheduler->state == SCHED_PAUSED && scheduler->thread_running) {
            pthread_cond_wait(&scheduler->products_available, &scheduler->mutex);
        }
        
        pthread_mutex_unlock(&scheduler->mutex);
        
        if (!scheduler->thread_running) break;
        
        // Ejecutar algoritmo correspondiente
        if (scheduler->config.algorithm == SCHED_FCFS) {
            scheduler_fcfs_process(scheduler);
        } else if (scheduler->config.algorithm == SCHED_ROUND_ROBIN) {
            scheduler_rr_process(scheduler);
        }
    }
    
    SCHEDULER_INFO("Worker thread del scheduler finalizado");
    return NULL;
}

// =============================================
// FUNCIONES DE SCHEDULING
// =============================================

void scheduler_add_product(scheduler_t *scheduler, product_t *product) {
    if (!scheduler || !product) return;
    
    pthread_mutex_lock(&scheduler->mutex);

    if (!product->current_station) {
        product->current_station = scheduler->first_station;
    }
    
    queue_push(scheduler->ready_queue, product);
    set_product_state(product, STATE_IN_QUEUE);
    metrics_product_queued(product->id);
    
    scheduler->stats.total_products_scheduled++;
    
    pthread_cond_signal(&scheduler->products_available);
    pthread_mutex_unlock(&scheduler->mutex);
    
    SCHEDULER_DEBUG("Producto %d agregado a ready_queue", product->id);
}

void scheduler_add_batch(scheduler_t *scheduler, product_t **products, int count) {
    if (!scheduler || !products) return;
    
    for (int i = 0; i < count; i++) {
        scheduler_add_product(scheduler, products[i]);
    }
    
    SCHEDULER_INFO("Batch de %d productos agregado", count);
}

void scheduler_requeue_preempted_product(scheduler_t *scheduler, product_t *product) {
    if (!scheduler || !product) return;

    pthread_mutex_lock(&scheduler->mutex);
    if (!product->current_station) {
        product->current_station = scheduler->first_station;
    }
    queue_push(scheduler->ready_queue, product);
    set_product_state(product, STATE_IN_QUEUE);
    pthread_cond_signal(&scheduler->products_available);
    pthread_mutex_unlock(&scheduler->mutex);

    metrics_product_queued(product->id);
    SCHEDULER_DEBUG("Producto %d reencolado para replanificación", product->id);
}

void scheduler_notify_slice_end(scheduler_t *scheduler, product_t *product, int was_preempted) {
    if (!scheduler || !product) return;

    pthread_mutex_lock(&scheduler->mutex);

    if (scheduler->current_product == product) {
        scheduler->quantum_control.event_type = was_preempted ? SCHED_EVENT_SLICE_PREEMPTED
                                                              : SCHED_EVENT_SLICE_COMPLETED;
        scheduler->quantum_control.event_pending = 1;
        scheduler->quantum_control.waiting_for_event = 0;
        pthread_cond_signal(&scheduler->quantum_control.cond);
    }

    pthread_mutex_unlock(&scheduler->mutex);
}

product_t *scheduler_select_next_product(scheduler_t *scheduler) {
    if (!scheduler) return NULL;
    
    if (scheduler->config.algorithm == SCHED_FCFS) {
        return scheduler_fcfs_select_next(scheduler);
    } else if (scheduler->config.algorithm == SCHED_ROUND_ROBIN) {
        return scheduler_rr_select_next(scheduler);
    }
    
    return NULL;
}

void scheduler_dispatch_product(scheduler_t *scheduler, product_t *product) {
    if (!scheduler || !product) return;

    station_t *target_station = product->current_station;
    if (!target_station) {
        target_station = scheduler->first_station;
    }

    if (!target_station) {
        SCHEDULER_ERROR("No hay estación objetivo para despachar el producto %d", product->id);
        return;
    }

    product->current_station = target_station;

    SCHEDULER_INFO("Despachando Producto %d a estación '%s'",
                   product->id, target_station->name);

    queue_push(target_station->input_queue, product);

    metrics_product_processing_start(product->id, target_station->id);
}

void scheduler_context_switch(scheduler_t *scheduler, product_t *old_product, 
                             product_t *new_product) {
    if (!scheduler) return;
    
    pthread_mutex_lock(&scheduler->mutex);
    scheduler->stats.context_switches++;
    pthread_mutex_unlock(&scheduler->mutex);
    
    metrics_scheduler_context_switch(
        old_product ? old_product->id : -1,
        new_product ? new_product->id : -1
    );
    
    SCHEDULER_DEBUG("Context switch: %d -> %d",
                    old_product ? old_product->id : -1,
                    new_product ? new_product->id : -1);
}

// =============================================
// FUNCIONES ESPECÍFICAS DE FCFS
// =============================================

product_t *scheduler_fcfs_select_next(scheduler_t *scheduler) {
    if (!scheduler || !scheduler->ready_queue) return NULL;
    
    // FCFS: Simple FIFO
    return queue_pop(scheduler->ready_queue);
}

void scheduler_fcfs_process(scheduler_t *scheduler) {
    if (!scheduler) return;
    
    // Esperar por producto
    product_t *product = scheduler_fcfs_select_next(scheduler);
    
    if (product) {
        pthread_mutex_lock(&scheduler->mutex);
        scheduler->current_product = product;
        pthread_mutex_unlock(&scheduler->mutex);
        
        SCHEDULER_INFO("FCFS: Procesando Producto %d", product->id);
        
        // Despachar a primera estación
        scheduler_dispatch_product(scheduler, product);
        
        pthread_mutex_lock(&scheduler->mutex);
        scheduler->current_product = NULL;
        pthread_mutex_unlock(&scheduler->mutex);
    } else {
        // No hay productos, esperar un poco
        usleep(100000); // 100ms
    }
}

// =============================================
// FUNCIONES ESPECÍFICAS DE ROUND ROBIN
// =============================================

product_t *scheduler_rr_select_next(scheduler_t *scheduler) {
    if (!scheduler || !scheduler->ready_queue) return NULL;
    
    // Round Robin: También usa FIFO pero con quantum
    return queue_pop(scheduler->ready_queue);
}

void scheduler_rr_start_quantum(scheduler_t *scheduler, product_t *product) {
    if (!scheduler || !product) return;
    
    pthread_mutex_lock(&scheduler->mutex);
    
    get_current_time(&scheduler->quantum_control.quantum_start_time);
    scheduler->quantum_control.quantum_expired = 0;
    scheduler->quantum_control.quantum_deadline =
        timespec_add_ms(&scheduler->quantum_control.quantum_start_time,
                        scheduler->config.quantum_ms);
    scheduler->quantum_control.waiting_for_event = 0;
    scheduler->quantum_control.event_pending = 0;
    scheduler->quantum_control.event_type = SCHED_EVENT_NONE;
    pthread_mutex_unlock(&scheduler->mutex);
    
    SCHEDULER_DEBUG("Quantum iniciado para Producto %d (%d ms)",
                    product->id, scheduler->config.quantum_ms);
}

int scheduler_rr_is_quantum_expired(scheduler_t *scheduler) {
    if (!scheduler) return 1;
    
    struct timespec current_time;
    get_current_time(&current_time);
    
    long elapsed_ms = time_diff_ms(&scheduler->quantum_control.quantum_start_time,
                                   &current_time);
    
    return elapsed_ms >= scheduler->config.quantum_ms;
}

static station_t *scheduler_find_station_with_product(station_t *station, product_t *product) {
    while (station) {
        pthread_mutex_lock(&station->mutex);
        product_t *current = station->current_product;
        pthread_mutex_unlock(&station->mutex);
        if (current == product) {
            return station;
        }
        station = station->next_station;
    }
    return NULL;
}

void scheduler_rr_handle_quantum_expiration(scheduler_t *scheduler) {
    if (!scheduler) return;

    pthread_mutex_lock(&scheduler->mutex);
    product_t *product = scheduler->current_product;
    if (!product) {
        pthread_mutex_unlock(&scheduler->mutex);
        return;
    }

    scheduler->quantum_control.quantum_expired = 1;
    pthread_mutex_unlock(&scheduler->mutex);

    SCHEDULER_INFO("Quantum expirado - solicitando preempción de Producto %d", product->id);

    station_t *station = scheduler_find_station_with_product(scheduler->first_station, product);
    if (station) {
        station_request_preemption(station, product);
    } else {
        SCHEDULER_DEBUG("Producto %d no encontrado en ninguna estación al expirar quantum", product->id);
    }
}

void scheduler_rr_process(scheduler_t *scheduler) {
    if (!scheduler) return;
    
    // Obtener siguiente producto
    product_t *product = scheduler_rr_select_next(scheduler);
    
    if (product) {
        pthread_mutex_lock(&scheduler->mutex);
        product_t *previous = scheduler->stats.last_dispatched_product;
        pthread_mutex_unlock(&scheduler->mutex);

        if (previous && previous != product) {
            scheduler_context_switch(scheduler, previous, product);
        }

        pthread_mutex_lock(&scheduler->mutex);
        scheduler->stats.last_dispatched_product = product;
        scheduler->current_product = product;
        pthread_mutex_unlock(&scheduler->mutex);

        SCHEDULER_INFO("Round Robin: Procesando Producto %d", product->id);
        
        scheduler_rr_start_quantum(scheduler, product);
        scheduler_dispatch_product(scheduler, product);

        scheduler_event_type_t event_type = SCHED_EVENT_NONE;

        pthread_mutex_lock(&scheduler->mutex);
        if (scheduler->quantum_control.event_pending) {
            event_type = (scheduler_event_type_t)scheduler->quantum_control.event_type;
            scheduler->quantum_control.event_pending = 0;
            scheduler->quantum_control.waiting_for_event = 0;
            pthread_mutex_unlock(&scheduler->mutex);
        } else {
            scheduler->quantum_control.waiting_for_event = 1;
            struct timespec deadline = scheduler->quantum_control.quantum_deadline;

            while (scheduler->thread_running && scheduler->quantum_control.waiting_for_event) {
                int rc = pthread_cond_timedwait(&scheduler->quantum_control.cond,
                                                &scheduler->mutex,
                                                &deadline);

                if (rc == 0 && scheduler->quantum_control.event_pending) {
                    event_type = (scheduler_event_type_t)scheduler->quantum_control.event_type;
                    scheduler->quantum_control.waiting_for_event = 0;
                    scheduler->quantum_control.event_pending = 0;
                    break;
                }

                if (rc == ETIMEDOUT) {
                    scheduler->quantum_control.waiting_for_event = 0;
                    pthread_mutex_unlock(&scheduler->mutex);

                    scheduler_rr_handle_quantum_expiration(scheduler);

                    pthread_mutex_lock(&scheduler->mutex);
                    while (scheduler->thread_running && !scheduler->quantum_control.event_pending) {
                        pthread_cond_wait(&scheduler->quantum_control.cond, &scheduler->mutex);
                    }

                    if (scheduler->quantum_control.event_pending) {
                        event_type = (scheduler_event_type_t)scheduler->quantum_control.event_type;
                        scheduler->quantum_control.event_pending = 0;
                    }
                    break;
                }
            }

            scheduler->quantum_control.waiting_for_event = 0;
            pthread_mutex_unlock(&scheduler->mutex);
        }

        pthread_mutex_lock(&scheduler->mutex);
        scheduler->current_product = NULL;
        pthread_mutex_unlock(&scheduler->mutex);

        if (event_type == SCHED_EVENT_SLICE_COMPLETED) {
            SCHEDULER_DEBUG("Producto %d completó su porción antes del fin del quantum", product->id);
        } else if (event_type == SCHED_EVENT_SLICE_PREEMPTED) {
            pthread_mutex_lock(&scheduler->mutex);
            scheduler->stats.preemptions++;
            pthread_mutex_unlock(&scheduler->mutex);
            metrics_scheduler_preemption(product->id);
            SCHEDULER_DEBUG("Producto %d fue preemptado al expirar el quantum", product->id);
        }
    } else {
        // No hay productos, esperar
        usleep(100000); // 100ms
    }
}

// =============================================
// FUNCIONES DE ESTADO Y CONTROL
// =============================================

scheduler_state_t scheduler_get_state(const scheduler_t *scheduler) {
    return scheduler ? scheduler->state : SCHED_STOPPED;
}

void scheduler_set_state(scheduler_t *scheduler, scheduler_state_t state) {
    if (!scheduler) return;
    
    pthread_mutex_lock(&scheduler->mutex);
    scheduler->state = state;
    pthread_mutex_unlock(&scheduler->mutex);
}

int scheduler_has_products(const scheduler_t *scheduler) {
    if (!scheduler || !scheduler->ready_queue) return 0;
    
    // Verificar si la cola tiene elementos
    return scheduler->ready_queue->head != NULL;
}

int scheduler_get_queue_size(const scheduler_t *scheduler) {
    if (!scheduler || !scheduler->ready_queue) return 0;
    
    pthread_mutex_lock(&((scheduler_t*)scheduler)->ready_queue->lock);
    
    int count = 0;
    node_t *current = scheduler->ready_queue->head;
    while (current) {
        count++;
        current = current->next;
    }
    
    pthread_mutex_unlock(&((scheduler_t*)scheduler)->ready_queue->lock);
    
    return count;
}

void scheduler_wait_completion(scheduler_t *scheduler) {
    if (!scheduler) return;
    
    SCHEDULER_INFO("Esperando completitud de todos los productos...");
    
    while (scheduler_has_products(scheduler) && scheduler->thread_running) {
        sleep(1);
    }
    
    SCHEDULER_INFO("Todos los productos procesados");
}

void scheduler_record_product_completion(scheduler_t *scheduler, product_t *product) {
    if (!scheduler || !product || !product->metrics) return;

    long total_wait_ms;
    long total_turnaround_ms;
    int scheduled;
    int completed;
    int context_switches;
    int preemptions;

    pthread_mutex_lock(&scheduler->mutex);
    scheduler->stats.products_completed++;
    scheduler->stats.total_wait_time_ms += product->metrics->total_wait_time_ms;
    scheduler->stats.total_turnaround_time_ms += product->metrics->turnaround_time_ms;
    product->remaining_time = 0;

    total_wait_ms = scheduler->stats.total_wait_time_ms;
    total_turnaround_ms = scheduler->stats.total_turnaround_time_ms;
    scheduled = scheduler->stats.total_products_scheduled;
    completed = scheduler->stats.products_completed;
    context_switches = scheduler->stats.context_switches;
    preemptions = scheduler->stats.preemptions;
    pthread_mutex_unlock(&scheduler->mutex);

    double avg_wait = completed ? (double)total_wait_ms / completed : 0.0;
    double avg_turnaround = completed ? (double)total_turnaround_ms / completed : 0.0;

    metrics_scheduler_update(scheduled, completed, context_switches, preemptions,
                             avg_wait, avg_turnaround);
}

// =============================================
// FUNCIONES DE ESTADÍSTICAS
// =============================================

int scheduler_get_products_scheduled(const scheduler_t *scheduler) {
    return scheduler ? scheduler->stats.total_products_scheduled : 0;
}

int scheduler_get_products_completed(const scheduler_t *scheduler) {
    return scheduler ? scheduler->stats.products_completed : 0;
}

int scheduler_get_context_switches(const scheduler_t *scheduler) {
    return scheduler ? scheduler->stats.context_switches : 0;
}

int scheduler_get_preemptions(const scheduler_t *scheduler) {
    return scheduler ? scheduler->stats.preemptions : 0;
}

double scheduler_get_average_wait_time(const scheduler_t *scheduler) {
    if (!scheduler || scheduler->stats.total_products_scheduled == 0) return 0.0;
    
    return (double)scheduler->stats.total_wait_time_ms / 
           scheduler->stats.total_products_scheduled;
}

double scheduler_get_average_turnaround_time(const scheduler_t *scheduler) {
    if (!scheduler || scheduler->stats.products_completed == 0) return 0.0;
    
    return (double)scheduler->stats.total_turnaround_time_ms / 
           scheduler->stats.products_completed;
}

double scheduler_get_throughput(const scheduler_t *scheduler) {
    if (!scheduler) return 0.0;
    
    struct timespec current_time;
    get_current_time(&current_time);
    
    long total_time_ms = time_diff_ms(&scheduler->stats.start_time, &current_time);
    if (total_time_ms <= 0) return 0.0;
    
    return ((double)scheduler->stats.products_completed * 1000.0) / total_time_ms;
}

void scheduler_print_stats(const scheduler_t *scheduler) {
    if (!scheduler) return;
    
    printf("\n=== Estadísticas del Scheduler ===\n");
    printf("Algoritmo: %s\n", scheduler_get_algorithm_name(scheduler->config.algorithm));
    printf("Productos programados: %d\n", scheduler->stats.total_products_scheduled);
    printf("Productos completados: %d\n", scheduler->stats.products_completed);
    printf("Context switches: %d\n", scheduler->stats.context_switches);
    printf("Preempciones: %d\n", scheduler->stats.preemptions);
    printf("Tiempo promedio de espera: %.2f ms\n", scheduler_get_average_wait_time(scheduler));
    printf("Tiempo promedio de turnaround: %.2f ms\n", scheduler_get_average_turnaround_time(scheduler));
    printf("Throughput: %.3f productos/seg\n", scheduler_get_throughput(scheduler));
    printf("==================================\n");
}

void scheduler_reset_stats(scheduler_t *scheduler) {
    if (!scheduler) return;
    
    pthread_mutex_lock(&scheduler->mutex);
    memset(&scheduler->stats, 0, sizeof(scheduler->stats));
    get_current_time(&scheduler->stats.start_time);
    pthread_mutex_unlock(&scheduler->mutex);
    
    SCHEDULER_DEBUG("Estadísticas reiniciadas");
}

// =============================================
// FUNCIONES DE UTILIDAD
// =============================================

const char *scheduler_get_algorithm_name(scheduling_algorithm_t algorithm) {
    if (algorithm < 0 || algorithm > 1) return "UNKNOWN";
    return algorithm_names[algorithm];
}

const char *scheduler_get_state_name(scheduler_state_t state) {
    if (state < 0 || state > 3) return "UNKNOWN";
    return state_names[state];
}

int scheduler_validate_config(const scheduler_t *scheduler) {
    if (!scheduler) {
        SCHEDULER_ERROR("Scheduler NULL");
        return 0;
    }
    
    if (!scheduler->first_station) {
        SCHEDULER_ERROR("No hay primera estación configurada");
        return 0;
    }
    
    if (!scheduler->ready_queue) {
        SCHEDULER_ERROR("No hay ready_queue configurada");
        return 0;
    }
    
    SCHEDULER_DEBUG("Configuración válida");
    return 1;
}

void scheduler_print_info(const scheduler_t *scheduler) {
    if (!scheduler) return;
    
    printf("\n=== Información del Scheduler ===\n");
    printf("Nombre: %s\n", scheduler->name);
    printf("Algoritmo: %s\n", scheduler_get_algorithm_name(scheduler->config.algorithm));
    printf("Quantum: %d ms\n", scheduler->config.quantum_ms);
    printf("Preemption: %s\n", scheduler->config.preemption_enabled ? "Habilitado" : "Deshabilitado");
    printf("Estado: %s\n", scheduler_get_state_name(scheduler->state));
    printf("Hilo corriendo: %s\n", scheduler->thread_running ? "Sí" : "No");
    printf("Productos en cola: %d\n", scheduler_get_queue_size(scheduler));
    printf("=================================\n");
}

void scheduler_print_config(const scheduler_t *scheduler) {
    if (!scheduler) return;
    
    printf("\n=== Configuración del Scheduler ===\n");
    printf("Algoritmo: %s\n", scheduler_get_algorithm_name(scheduler->config.algorithm));
    printf("Quantum: %d ms\n", scheduler->config.quantum_ms);
    printf("Preemption: %s\n", scheduler->config.preemption_enabled ? "Sí" : "No");
    printf("Máx productos: %d\n", scheduler->config.max_products_in_system);
    printf("===================================\n");
}

void scheduler_debug_print_state(const scheduler_t *scheduler) {
    if (!scheduler) return;
    
    SCHEDULER_DEBUG("Estado: %s | Programados: %d | Completados: %d | Cola: %d",
                    scheduler_get_state_name(scheduler->state),
                    scheduler->stats.total_products_scheduled,
                    scheduler->stats.products_completed,
                    scheduler_get_queue_size(scheduler));
}

void scheduler_debug_print_ready_queue(const scheduler_t *scheduler) {
    if (!scheduler || !scheduler->ready_queue) return;
    
    pthread_mutex_lock(&scheduler->ready_queue->lock);
    
    printf("[SCHEDULER DEBUG] Ready Queue: ");
    node_t *current = scheduler->ready_queue->head;
    while (current) {
        printf("%d ", current->prod->id);
        current = current->next;
    }
    printf("\n");
    
    pthread_mutex_unlock(&scheduler->ready_queue->lock);
}

int scheduler_debug_check_integrity(const scheduler_t *scheduler) {
    if (!scheduler) return 0;
    
    int issues = 0;
    
    if (!scheduler->ready_queue) {
        SCHEDULER_ERROR("Ready queue es NULL");
        issues++;
    }
    
    if (!scheduler->first_station) {
        SCHEDULER_ERROR("Primera estación es NULL");
        issues++;
    }
    
    if (scheduler->thread_running && scheduler->state == SCHED_STOPPED) {
        SCHEDULER_ERROR("Estado inconsistente: thread running pero state=STOPPED");
        issues++;
    }
    
    if (issues == 0) {
        SCHEDULER_DEBUG("Integridad verificada: OK");
    } else {
        SCHEDULER_ERROR("Encontrados %d problemas de integridad", issues);
    }
    
    return issues == 0;
}