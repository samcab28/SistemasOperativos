#ifndef PRODUCT_H
#define PRODUCT_H

#include <time.h>

// Estados del producto
typedef enum {
    STATE_CREATED,
    STATE_IN_QUEUE,
    STATE_PROCESSING,
    STATE_COMPLETED
} processing_state_t;

// Forward declaration para métricas
typedef struct product_metrics product_metrics_t;

// Estructura del producto
typedef struct product {
    int id;
    struct timespec arrival_time;
    int priority;
    int remaining_time;              // Para Round Robin
    processing_state_t state;
    product_metrics_t *metrics;
} product_t;

// Estructura para métricas del producto
struct product_metrics {
    int product_id;
    struct timespec creation_time;
    
    // Métricas por estación (3 estaciones: Corte, Ensamblaje, Empaque)
    struct {
        struct timespec entry_time;
        struct timespec exit_time;
        int wait_time_ms;
        int process_time_ms;
    } station_metrics[3];
    
    int total_wait_time_ms;
    int turnaround_time_ms;
};

// Funciones principales
void set_product_state(product_t *product, processing_state_t state);
processing_state_t get_product_state(const product_t *product);
void update_remaining_time(product_t *product, int time_used);

// Funciones para métricas
product_metrics_t *create_product_metrics(int product_id);
void free_product_metrics(product_metrics_t *metrics);
void record_station_entry(product_t *product, int station_id);
void record_station_exit(product_t *product, int station_id);

// Liberar producto
void free_product(product_t *product);

// Utilidades
void print_product_info(const product_t *product);
void print_product_metrics(const product_t *product);

#endif // PRODUCT_H