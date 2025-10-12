#include "product.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Función auxiliar para calcular diferencia de tiempo en milisegundos
static long timespec_diff_ms(const struct timespec *start, const struct timespec *end) {
    long seconds = end->tv_sec - start->tv_sec;
    long nanoseconds = end->tv_nsec - start->tv_nsec;
    return (seconds * 1000) + (nanoseconds / 1000000);
}

// Función para obtener el nombre del estado
static const char* get_state_name(processing_state_t state) {
    switch (state) {
        case STATE_CREATED: return "CREATED";
        case STATE_IN_QUEUE: return "IN_QUEUE";
        case STATE_PROCESSING: return "PROCESSING";
        case STATE_COMPLETED: return "COMPLETED";
        default: return "UNKNOWN";
    }
}

// Cambiar estado del producto
void set_product_state(product_t *product, processing_state_t state) {
    if (product == NULL) return;
    
    product->state = state;
    printf("[PRODUCT %d] Estado: %s\n", product->id, get_state_name(state));
}

// Obtener estado del producto
processing_state_t get_product_state(const product_t *product) {
    return product ? product->state : STATE_CREATED;
}

// Actualizar tiempo restante (para Round Robin)
void update_remaining_time(product_t *product, int time_used) {
    if (product == NULL || time_used < 0) return;
    
    product->remaining_time -= time_used;
    if (product->remaining_time < 0) {
        product->remaining_time = 0;
    }
}

// Crear métricas del producto
product_metrics_t *create_product_metrics(int product_id) {
    product_metrics_t *metrics = malloc(sizeof(product_metrics_t));
    if (!metrics) {
        printf("[ERROR] No se pudo crear métricas para producto %d\n", product_id);
        return NULL;
    }
    
    memset(metrics, 0, sizeof(product_metrics_t));
    metrics->product_id = product_id;
    clock_gettime(CLOCK_MONOTONIC, &metrics->creation_time);
    metrics->last_event_time = metrics->creation_time;
    
    return metrics;
}

// Liberar métricas
void free_product_metrics(product_metrics_t *metrics) {
    if (metrics) {
        free(metrics);
    }
}

// Registrar entrada a estación
void record_station_entry(product_t *product, int station_id) {
    if (!product || !product->metrics || station_id < 0 || station_id >= 3) {
        return;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &product->metrics->station_metrics[station_id].entry_time);
    
    // Calcular tiempo de espera hasta iniciar procesamiento en esta estación
    long wait_time = timespec_diff_ms(&product->metrics->last_event_time,
                                      &product->metrics->station_metrics[station_id].entry_time);
    if (wait_time < 0) wait_time = 0;
    product->metrics->station_metrics[station_id].wait_time_ms = (int)wait_time;
    product->metrics->total_wait_time_ms += (int)wait_time;
    
    printf("[PRODUCT %d] Entrada a estación %d\n", product->id, station_id);
}

// Registrar salida de estación
void record_station_exit(product_t *product, int station_id) {
    if (!product || !product->metrics || station_id < 0 || station_id >= 3) {
        return;
    }
    
    struct timespec *entry_time = &product->metrics->station_metrics[station_id].entry_time;
    struct timespec *exit_time = &product->metrics->station_metrics[station_id].exit_time;
    
    clock_gettime(CLOCK_MONOTONIC, exit_time);
    
    // Calcular tiempo de procesamiento
    product->metrics->station_metrics[station_id].process_time_ms += 
        timespec_diff_ms(entry_time, exit_time);
    product->metrics->last_event_time = *exit_time;
    
    printf("[PRODUCT %d] Salida de estación %d (tiempo: %d ms)\n", 
           product->id, station_id, 
           product->metrics->station_metrics[station_id].process_time_ms);
}

// Liberar producto
void free_product(product_t *product) {
    if (product) {
        if (product->metrics) {
            free_product_metrics(product->metrics);
        }
        free(product);
    }
}

// Imprimir información del producto
void print_product_info(const product_t *product) {
    if (!product) {
        printf("[ERROR] Producto NULL\n");
        return;
    }
    
    printf("\n=== PRODUCTO %d ===\n", product->id);
    printf("Estado: %s\n", get_state_name(product->state));
    printf("Prioridad: %d\n", product->priority);
    printf("Tiempo restante: %d ms\n", product->remaining_time);
    printf("==================\n\n");
}

// Imprimir métricas del producto
void print_product_metrics(const product_t *product) {
    if (!product || !product->metrics) {
        printf("[ERROR] Producto o métricas NULL\n");
        return;
    }
    
    product_metrics_t *m = product->metrics;
    
    printf("\n=== MÉTRICAS PRODUCTO %d ===\n", product->id);
    printf("Turnaround: %d ms\n", m->turnaround_time_ms);
    printf("Espera total: %d ms\n", m->total_wait_time_ms);
    
    const char* stations[] = {"Corte", "Ensamblaje", "Empaque"};
    printf("\nPor estación:\n");
    for (int i = 0; i < 3; i++) {
        if (m->station_metrics[i].process_time_ms > 0) {
            printf("  %s: %d ms\n", 
                   stations[i],
                   m->station_metrics[i].process_time_ms);
            printf("    Espera: %d ms\n", m->station_metrics[i].wait_time_ms);
            printf("    Preemptions: %d\n", m->station_metrics[i].preemptions);
        }
    }
    printf("==========================\n\n");
}