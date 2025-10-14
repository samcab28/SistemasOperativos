#include "station.h"
#include "scheduler.h"
#include "../metrics/logger.h"
#include "../metrics/metrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// Nombres de los tipos de estación
static const char *station_type_names[] = {
    "Corte",
    "Ensamblaje",
    "Empaque"
};

// Nombres de los estados
static const char *station_state_names[] = {
    "IDLE",
    "BUSY",
    "STOPPED"
};

// =============================================
// FUNCIONES AUXILIARES INTERNAS
// =============================================

// Obtener timestamp actual
static void get_current_time(struct timespec *ts) {
    clock_gettime(CLOCK_MONOTONIC, ts);
}

// Calcular diferencia de tiempo en ms
static long time_diff_ms(const struct timespec *start, const struct timespec *end) {
    long seconds = end->tv_sec - start->tv_sec;
    long nanoseconds = end->tv_nsec - start->tv_nsec;
    return (seconds * 1000) + (nanoseconds / 1000000);
}

// Generar número aleatorio en rango
static int random_range(int min, int max) {
    return min + (rand() % (max - min + 1));
}

// =============================================
// FUNCIONES DE CREACIÓN Y DESTRUCCIÓN
// =============================================

station_t *create_station(station_type_t type, const char *name, int processing_time_ms) {
    station_t *station = malloc(sizeof(station_t));
    if (!station) {
        STATION_ERROR("No se pudo crear estación");
        return NULL;
    }
    
    // Inicializar datos básicos
    station->id = type;
    station->type = type;
    strncpy(station->name, name, sizeof(station->name) - 1);
    station->name[sizeof(station->name) - 1] = '\0';
    
    // Inicializar threading
    station->thread_running = 0;
    
    // Inicializar sincronización
    pthread_mutex_init(&station->mutex, NULL);
    sem_init(&station->processing_semaphore, 0, 1); // Máximo 1 producto
    pthread_cond_init(&station->work_available, NULL);
    
    // Inicializar colas (se configuran externamente)
    station->input_queue = NULL;
    station->output_queue = NULL;
    
    // Configuración de procesamiento
    station->processing_time_ms = processing_time_ms;
    station->processing_variance_ms = processing_time_ms / 4; // 25% de variación
    
    // Estado inicial
    station->state = STATION_IDLE;
    station->current_product = NULL;
    
    // Chain of Responsibility
    station->next_station = NULL;
    station->scheduler = NULL;

    // Preempción
    station->preemption_requested = 0;
    station->preemption_target = NULL;
    
    // Inicializar estadísticas
    memset(&station->stats, 0, sizeof(station->stats));
    get_current_time(&station->stats.last_activity_time);
    
    STATION_INFO("Estación '%s' creada (tiempo procesamiento: %d ms)", name, processing_time_ms);
    
    return station;
}

void destroy_station(station_t *station) {
    if (!station) return;
    
    STATION_INFO("Destruyendo estación '%s'", station->name);
    
    // Detener hilo si está corriendo
    if (station->thread_running) {
        station_stop_thread(station);
    }
    
    // Destruir sincronización
    pthread_mutex_destroy(&station->mutex);
    sem_destroy(&station->processing_semaphore);
    pthread_cond_destroy(&station->work_available);
    
    free(station);
}

// =============================================
// FUNCIONES DE CONFIGURACIÓN
// =============================================

void station_set_input_queue(station_t *station, queue_t *queue) {
    if (!station) return;
    
    pthread_mutex_lock(&station->mutex);
    station->input_queue = queue;
    pthread_mutex_unlock(&station->mutex);
    
    STATION_DEBUG("Cola de entrada configurada para '%s'", station->name);
}

void station_set_output_queue(station_t *station, queue_t *queue) {
    if (!station) return;
    
    pthread_mutex_lock(&station->mutex);
    station->output_queue = queue;
    pthread_mutex_unlock(&station->mutex);
    
    STATION_DEBUG("Cola de salida configurada para '%s'", station->name);
}

void station_set_next(station_t *station, station_t *next) {
    if (!station) return;
    
    pthread_mutex_lock(&station->mutex);
    station->next_station = next;
    pthread_mutex_unlock(&station->mutex);
    
    if (next) {
        STATION_DEBUG("'%s' -> '%s' (Chain configurado)", station->name, next->name);
    }
}

void station_set_processing_variance(station_t *station, int variance_ms) {
    if (!station) return;
    
    pthread_mutex_lock(&station->mutex);
    station->processing_variance_ms = variance_ms;
    pthread_mutex_unlock(&station->mutex);
    
    STATION_DEBUG("Varianza configurada: %d ms", variance_ms);
}

void station_set_scheduler(station_t *station, scheduler_t *scheduler) {
    if (!station) return;

    pthread_mutex_lock(&station->mutex);
    station->scheduler = scheduler;
    station_t *next = station->next_station;
    pthread_mutex_unlock(&station->mutex);

    if (next && next->scheduler != scheduler) {
        station_set_scheduler(next, scheduler);
    }
}

void station_request_preemption(station_t *station, product_t *product) {
    if (!station || !product) return;

    pthread_mutex_lock(&station->mutex);
    if (station->current_product == product) {
        station->preemption_requested = 1;
        station->preemption_target = product;
    }
    pthread_mutex_unlock(&station->mutex);
}

// =============================================
// FUNCIONES DE CONTROL DEL HILO
// =============================================

int station_start_thread(station_t *station) {
    if (!station) return 0;
    
    if (station->thread_running) {
        STATION_INFO("Hilo de '%s' ya está corriendo", station->name);
        return 1;
    }
    
    station->thread_running = 1;
    
    if (pthread_create(&station->thread, NULL, station_worker_thread, station) != 0) {
        STATION_ERROR("Fallo al crear hilo para '%s'", station->name);
        station->thread_running = 0;
        return 0;
    }
    
    STATION_INFO("Hilo iniciado para estación '%s'", station->name);
    return 1;
}

void station_stop_thread(station_t *station) {
    if (!station || !station->thread_running) return;
    
    STATION_INFO("Deteniendo hilo de '%s'...", station->name);
    
    // Señalar detención
    pthread_mutex_lock(&station->mutex);
    station->thread_running = 0;
    pthread_cond_signal(&station->work_available);
    pthread_mutex_unlock(&station->mutex);

    // Desbloquear posibles waits en la cola de entrada
    if (station->input_queue) {
        sem_post(&station->input_queue->items);
    }
    
    // Esperar a que termine
    pthread_join(station->thread, NULL);
    
    station_set_state(station, STATION_STOPPED);
    STATION_INFO("Hilo de '%s' detenido", station->name);
}

int station_is_running(const station_t *station) {
    return station ? station->thread_running : 0;
}

// =============================================
// FUNCIÓN PRINCIPAL DEL HILO (Worker)
// =============================================

void *station_worker_thread(void *arg) {
    station_t *station = (station_t *)arg;
    
    STATION_INFO("Worker thread iniciado para '%s'", station->name);
    
    while (station->thread_running) {
        // Esperar por producto en la cola de entrada
        if (!station->input_queue) {
            // Sin cola asignada: dormir un momento para no consumir CPU
            usleep(100000); // 100ms
            continue;
        }
        
        // Obtener producto (bloqueante)
        product_t *product = queue_pop(station->input_queue);
        
        if (!product) {
            // La cola pudo despertar por una señal de parada
            if (!station->thread_running) break;
            continue;
        }
        
        // Procesar producto
        STATION_INFO("'%s' recibió Producto %d", station->name, product->id);
        int completed = station_process_product(station, product);
        
        if (completed) {
            // Enviar a siguiente estación
            station_send_to_next(station, product);
        }
    }
    
    STATION_INFO("Worker thread finalizado para '%s'", station->name);
    return NULL;
}

// =============================================
// FUNCIONES DE PROCESAMIENTO
// =============================================

static int station_compute_processing_time(const station_t *station) {
    if (!station) return 0;

    int variance = station->processing_variance_ms;
    int actual_time = station->processing_time_ms;

    if (variance > 0) {
        int offset = random_range(-variance, variance);
        actual_time += offset;
    }

    if (actual_time < 0) {
        actual_time = station->processing_time_ms / 2;
    }

    if (actual_time < 0) {
        actual_time = 0;
    }

    return actual_time;
}

int station_process_product(station_t *station, product_t *product) {
    if (!station || !product) {
        return 0;
    }
    
    sem_wait(&station->processing_semaphore);
    
    pthread_mutex_lock(&station->mutex);
    station->state = STATION_BUSY;
    station->current_product = product;
    station->preemption_requested = 0;
    station->preemption_target = NULL;
    get_current_time(&station->processing_start_time);
    product->current_station = station;
    pthread_mutex_unlock(&station->mutex);
    
    record_station_entry(product, station->id);
    metrics_station_start_processing(station->id, product->id);
    set_product_state(product, STATE_PROCESSING);

    if (station->scheduler) {
        scheduler_notify_execution_start(station->scheduler, product);
    }
    
    STATION_INFO("'%s' procesando Producto %d...", station->name, product->id);
    
    const int slice_ms = 50;
    int elapsed_ms = 0;
    int preempted = 0;
    int target_time_ms;

    if (product->metrics) {
        int *remaining_ptr = &product->metrics->station_metrics[station->id].remaining_time_ms;
        if (*remaining_ptr > 0) {
            target_time_ms = *remaining_ptr;
        } else {
            target_time_ms = station_compute_processing_time(station);
            *remaining_ptr = target_time_ms;
        }
    } else {
        target_time_ms = station_compute_processing_time(station);
    }

    while (elapsed_ms < target_time_ms) {
        int remaining = target_time_ms - elapsed_ms;
        int step = remaining < slice_ms ? remaining : slice_ms;
        if (step <= 0) {
            break;
        }

        // Procesar en rebanadas pequeñas para reaccionar a preempciones del scheduler
        usleep(step * 1000);
        elapsed_ms += step;

        pthread_mutex_lock(&station->mutex);
        int should_preempt = station->preemption_requested &&
                             station->preemption_target == product;
        int stop_running = !station->thread_running;
        int completed = elapsed_ms >= target_time_ms;

        if (completed) {
            station->preemption_requested = 0;
            station->preemption_target = NULL;
            pthread_mutex_unlock(&station->mutex);
            break;
        }

        if (should_preempt) {
            station->preemption_requested = 0;
            station->preemption_target = NULL;
        }
        pthread_mutex_unlock(&station->mutex);

        if (should_preempt || stop_running) {
            // Salir temprano para devolver el producto al scheduler
            preempted = 1;
            break;
        }
    }

    struct timespec end_time;
    get_current_time(&end_time);

    record_station_exit(product, station->id);
    metrics_station_end_processing(station->id, product, !preempted);

    long processing_time = time_diff_ms(&station->processing_start_time, &end_time);

    if (product->metrics) {
        int *remaining_ptr = &product->metrics->station_metrics[station->id].remaining_time_ms;
        if (*remaining_ptr > 0) {
            *remaining_ptr -= (int)processing_time;
            if (*remaining_ptr < 0) {
                *remaining_ptr = 0;
            }
        }
    }

    update_remaining_time(product, (int)processing_time);

    pthread_mutex_lock(&station->mutex);
    station->stats.total_processing_time_ms += processing_time;
    station->stats.last_activity_time = end_time;
    if (!preempted) {
        station->stats.products_processed++;
    }
    station->current_product = NULL;
    station->state = STATION_IDLE;
    pthread_mutex_unlock(&station->mutex);

    sem_post(&station->processing_semaphore);

    if (preempted) {
        STATION_INFO("'%s' preemptó Producto %d (tiempo parcial: %ld ms)",
                     station->name, product->id, processing_time);
        if (product->metrics) {
            product->metrics->station_metrics[station->id].preemptions++;
        }
        product->current_station = station;
        if (station->scheduler) {
            // Reentregar al scheduler para decidir el siguiente destino
            scheduler_requeue_preempted_product(station->scheduler, product);
            scheduler_notify_slice_end(station->scheduler, product, 1);
        } else if (station->input_queue) {
            set_product_state(product, STATE_IN_QUEUE);
            queue_push(station->input_queue, product);
        }
        return 0;
    }

    STATION_INFO("'%s' completó Producto %d (tiempo: %ld ms)",
                 station->name, product->id, processing_time);
    if (product->metrics) {
        product->metrics->station_metrics[station->id].remaining_time_ms = 0;
    }

    if (station->scheduler) {
        station_t *next_station = station->next_station;
        if (next_station) {
            // Continuar hacia la siguiente estación del pipeline
            product->current_station = next_station;
            scheduler_requeue_preempted_product(station->scheduler, product);
            scheduler_notify_slice_end(station->scheduler, product, 0);
        } else {
            product->current_station = NULL;
            scheduler_notify_slice_end(station->scheduler, product, 0);
        }
    }

    return 1;
}

void station_send_to_next(station_t *station, product_t *product) {
    if (!station || !product) return;

    if (station->scheduler && station->next_station) {
        // El scheduler se encargó de reencolar el producto para la siguiente estación
        return;
    }
    
    pthread_mutex_lock(&station->mutex);
    
    // Si hay cola de salida, enviar ahí
    if (station->output_queue) {
        queue_push(station->output_queue, product);
        STATION_DEBUG("Producto %d enviado a cola de salida", product->id);

        // Si no existe siguiente estación, este es el producto finalizado
        if (!station->next_station && product->metrics) {
            struct timespec *exit_time = &product->metrics->station_metrics[station->id].exit_time;
            product->metrics->completion_time = *exit_time;
            product->metrics->turnaround_time_ms = (int)time_diff_ms(&product->metrics->creation_time,
                                                                     exit_time);
            set_product_state(product, STATE_COMPLETED);
            metrics_product_completed(product->id);

            if (station->scheduler) {
                scheduler_record_product_completion(station->scheduler, product);
            }
        }
    }
    // Si hay siguiente estación (Chain), enviar a su cola de entrada
    else if (station->next_station && station->next_station->input_queue) {
        queue_push(station->next_station->input_queue, product);
        STATION_DEBUG("Producto %d enviado a '%s'", 
                     product->id, station->next_station->name);
    }
    // Si es la última estación
    else {
        if (product->metrics) {
            struct timespec *exit_time = &product->metrics->station_metrics[station->id].exit_time;
            product->metrics->completion_time = *exit_time;
            product->metrics->turnaround_time_ms = (int)time_diff_ms(&product->metrics->creation_time,
                                                                     exit_time);
        }
        set_product_state(product, STATE_COMPLETED);
        metrics_product_completed(product->id);
        if (station->scheduler) {
            scheduler_record_product_completion(station->scheduler, product);
        }
        STATION_INFO("Producto %d COMPLETADO (última estación)", product->id);
    }
    
    pthread_mutex_unlock(&station->mutex);
}

// =============================================
// FUNCIONES DE ESTADO Y ESTADÍSTICAS
// =============================================

station_state_t station_get_state(const station_t *station) {
    if (!station) return STATION_STOPPED;
    return station->state;
}

void station_set_state(station_t *station, station_state_t state) {
    if (!station) return;
    
    pthread_mutex_lock(&station->mutex);
    station->state = state;
    pthread_mutex_unlock(&station->mutex);
}

int station_is_busy(const station_t *station) {
    if (!station) return 0;
    return station->state == STATION_BUSY;
}

product_t *station_get_current_product(const station_t *station) {
    if (!station) return NULL;
    return station->current_product;
}

int station_get_products_processed(const station_t *station) {
    if (!station) return 0;
    return station->stats.products_processed;
}

long station_get_total_processing_time(const station_t *station) {
    if (!station) return 0;
    return station->stats.total_processing_time_ms;
}

double station_get_average_processing_time(const station_t *station) {
    if (!station || station->stats.products_processed == 0) return 0.0;
    
    return (double)station->stats.total_processing_time_ms / 
           station->stats.products_processed;
}

double station_get_utilization(const station_t *station) {
    if (!station) return 0.0;
    
    struct timespec current_time;
    get_current_time(&current_time);
    
    long total_time = time_diff_ms(&station->stats.last_activity_time, &current_time);
    if (total_time <= 0) return 0.0;
    
    return ((double)station->stats.total_processing_time_ms / total_time) * 100.0;
}

void station_print_stats(const station_t *station) {
    if (!station) return;
    
    printf("\n=== Estadísticas Estación: %s ===\n", station->name);
    printf("Productos procesados: %d\n", station->stats.products_processed);
    printf("Tiempo total procesamiento: %ld ms\n", station->stats.total_processing_time_ms);
    printf("Tiempo promedio: %.2f ms\n", station_get_average_processing_time(station));
    printf("Utilización: %.1f%%\n", station_get_utilization(station));
    printf("Estado actual: %s\n", station_get_state_name(station->state));
    printf("================================\n");
}

void station_reset_stats(station_t *station) {
    if (!station) return;
    
    pthread_mutex_lock(&station->mutex);
    memset(&station->stats, 0, sizeof(station->stats));
    get_current_time(&station->stats.last_activity_time);
    pthread_mutex_unlock(&station->mutex);
    
    STATION_DEBUG("Estadísticas reiniciadas para '%s'", station->name);
}

// =============================================
// FUNCIONES DE UTILIDAD
// =============================================

const char *station_get_type_name(station_type_t type) {
    if (type < 0 || type > 2) return "UNKNOWN";
    return station_type_names[type];
}

const char *station_get_state_name(station_state_t state) {
    if (state < 0 || state > 2) return "UNKNOWN";
    return station_state_names[state];
}

int station_validate_config(const station_t *station) {
    if (!station) {
        STATION_ERROR("Estación NULL");
        return 0;
    }
    
    if (!station->input_queue) {
        STATION_ERROR("'%s' no tiene cola de entrada", station->name);
        return 0;
    }
    
    STATION_DEBUG("Configuración de '%s' válida", station->name);
    return 1;
}

void station_print_info(const station_t *station) {
    if (!station) return;
    
    printf("\n=== Información Estación ===\n");
    printf("Nombre: %s\n", station->name);
    printf("Tipo: %s\n", station_get_type_name(station->type));
    printf("Tiempo procesamiento: %d ms (±%d ms)\n", 
           station->processing_time_ms, station->processing_variance_ms);
    printf("Estado: %s\n", station_get_state_name(station->state));
    printf("Hilo corriendo: %s\n", station->thread_running ? "Sí" : "No");
    printf("Cola entrada: %s\n", station->input_queue ? "Configurada" : "No configurada");
    printf("Cola salida: %s\n", station->output_queue ? "Configurada" : "No configurada");
    printf("============================\n");
}

void station_debug_print_state(const station_t *station) {
    if (!station) return;
    
    STATION_DEBUG("Estado de '%s': %s | Productos: %d | Running: %d",
                  station->name,
                  station_get_state_name(station->state),
                  station->stats.products_processed,
                  station->thread_running);
}

void station_debug_check_queues(const station_t *station) {
    if (!station) return;
    
    STATION_DEBUG("Colas de '%s': entrada=%p, salida=%p",
                  station->name,
                  (void*)station->input_queue,
                  (void*)station->output_queue);
}