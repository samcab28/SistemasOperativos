#include "station.h"
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
            usleep(100000); // 100ms
            continue;
        }
        
        // Obtener producto (bloqueante)
        product_t *product = queue_pop(station->input_queue);
        
        if (!product) {
            if (!station->thread_running) break;
            continue;
        }
        
        // Procesar producto
        STATION_INFO("'%s' recibió Producto %d", station->name, product->id);
        station_process_product(station, product);
        
        // Enviar a siguiente estación
        station_send_to_next(station, product);
    }
    
    STATION_INFO("Worker thread finalizado para '%s'", station->name);
    return NULL;
}

// =============================================
// FUNCIONES DE PROCESAMIENTO
// =============================================

void station_process_product(station_t *station, product_t *product) {
    if (!station || !product) return;
    
    // Adquirir semáforo (solo 1 producto a la vez)
    sem_wait(&station->processing_semaphore);
    
    pthread_mutex_lock(&station->mutex);
    station->state = STATION_BUSY;
    station->current_product = product;
    get_current_time(&station->processing_start_time);
    pthread_mutex_unlock(&station->mutex);
    
    // Registrar inicio en métricas
    record_station_entry(product, station->id);
    metrics_station_start_processing(station->id, product->id);
    set_product_state(product, STATE_PROCESSING);
    
    STATION_INFO("'%s' procesando Producto %d...", station->name, product->id);
    
    // Simular procesamiento
    station_simulate_processing(station);
    
    // Registrar fin en métricas
    record_station_exit(product, station->id);
    metrics_station_end_processing(station->id, product->id);
    
    // Actualizar estadísticas
    pthread_mutex_lock(&station->mutex);
    struct timespec end_time;
    get_current_time(&end_time);
    long processing_time = time_diff_ms(&station->processing_start_time, &end_time);
    
    station->stats.products_processed++;
    station->stats.total_processing_time_ms += processing_time;
    station->current_product = NULL;
    station->state = STATION_IDLE;
    pthread_mutex_unlock(&station->mutex);
    
    // Liberar semáforo
    sem_post(&station->processing_semaphore);
    
    STATION_INFO("'%s' completó Producto %d (tiempo: %ld ms)", 
                 station->name, product->id, processing_time);
}

void station_send_to_next(station_t *station, product_t *product) {
    if (!station || !product) return;
    
    pthread_mutex_lock(&station->mutex);
    
    // Si hay cola de salida, enviar ahí
    if (station->output_queue) {
        queue_push(station->output_queue, product);
        STATION_DEBUG("Producto %d enviado a cola de salida", product->id);
    }
    // Si hay siguiente estación (Chain), enviar a su cola de entrada
    else if (station->next_station && station->next_station->input_queue) {
        queue_push(station->next_station->input_queue, product);
        STATION_DEBUG("Producto %d enviado a '%s'", 
                     product->id, station->next_station->name);
    }
    // Si es la última estación
    else {
        set_product_state(product, STATE_COMPLETED);
        metrics_product_completed(product->id);
        STATION_INFO("Producto %d COMPLETADO (última estación)", product->id);
    }
    
    pthread_mutex_unlock(&station->mutex);
}

void station_simulate_processing(const station_t *station) {
    if (!station) return;
    
    // Calcular tiempo con variación aleatoria
    int variance = random_range(-station->processing_variance_ms, 
                                station->processing_variance_ms);
    int actual_time = station->processing_time_ms + variance;
    
    if (actual_time < 0) actual_time = station->processing_time_ms / 2;
    
    // Simular procesamiento
    usleep(actual_time * 1000); // Convertir ms a microsegundos
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