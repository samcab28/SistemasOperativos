#ifndef METRICS_H
#define METRICS_H

#include "../core/product.h"
#include <stdio.h>
#include <time.h>

// Configuración condicional para threading (futuro)
#ifdef USE_THREADING
    #include <pthread.h>
    #include <semaphore.h>
    #define METRICS_LOCK() pthread_mutex_lock(&g_metrics_mutex)
    #define METRICS_UNLOCK() pthread_mutex_unlock(&g_metrics_mutex)
#else
    #define METRICS_LOCK() // No-op para versión actual
    #define METRICS_UNLOCK() // No-op para versión actual
#endif

// Cantidad de estaciones soportadas por el sistema de métricas
#define METRICS_STATION_COUNT 3

// Tipos de eventos del sistema
typedef enum {
    EVENT_PRODUCT_CREATED,
    EVENT_PRODUCT_QUEUED,
    EVENT_PRODUCT_PROCESSING_START,
    EVENT_PRODUCT_PROCESSING_END,
    EVENT_PRODUCT_COMPLETED,
    EVENT_STATION_IDLE_START,
    EVENT_STATION_IDLE_END,
    EVENT_SCHEDULER_CONTEXT_SWITCH,
    EVENT_SCHEDULER_PREEMPTION
} event_type_t;

// Estructura para un evento individual
typedef struct {
    event_type_t type;
    struct timespec timestamp;
    int product_id;
    int station_id;
    int additional_data;  // Para datos extra según el tipo de evento
} metric_event_t;

// Métricas por estación
typedef struct {
    int station_id;
    char name[50];
    
    // Contadores básicos
    int products_processed;
    int products_waiting;
    int total_processing_time_ms;
    int completed_processing_time_ms;
    int total_idle_time_ms;
    
    // Estadísticas de tiempo
    int min_processing_time_ms;
    int max_processing_time_ms;
    double avg_processing_time_ms;
    
    // Estado actual
    int is_busy;
    struct timespec last_activity_time;
} station_metrics_t;

// Métricas del scheduler (preparado para futuro)
typedef struct {
    int total_products_scheduled;
    int context_switches;
    int preemptions;
    
    double avg_wait_time_ms;
    double avg_turnaround_time_ms;
    double avg_response_time_ms;
    
    // Para Round Robin (futuro)
    int quantum_expirations;
    double cpu_utilization;
} scheduler_metrics_t;

// Sistema principal de métricas
typedef struct {
    // Tiempo de inicio del sistema
    struct timespec system_start_time;
    
    // Métricas globales
    int total_products_created;
    int total_products_completed;
    int total_products_failed;
    
    // Métricas por estación (3 estaciones)
    station_metrics_t stations[METRICS_STATION_COUNT];
    
    // Métricas del scheduler
    scheduler_metrics_t scheduler;
    
    // Buffer de eventos (simple para ahora, thread-safe para futuro)
    metric_event_t *event_buffer;
    int buffer_size;
    int buffer_count;
    
    #ifdef USE_THREADING
        pthread_mutex_t buffer_mutex;
        sem_t event_semaphore;
    #endif
    
} metrics_system_t;

// Resumen resumido de producto para reportes diferidos
typedef struct {
    int wait_time_ms;
    int process_time_ms;
    int preemptions;
} product_station_summary_t;

typedef struct {
    int product_id;
    int turnaround_time_ms;
    int total_wait_time_ms;
    product_station_summary_t stations[METRICS_STATION_COUNT];
} product_summary_t;

// Resumen consolidado para reportes diferidos
typedef struct {
    char algorithm[32];
    int num_products;
    int randomize_processing;
    int total_products_created;
    int total_products_completed;
    int total_products_failed;
    int total_runtime_ms;
    double throughput;
    double utilization;
    scheduler_metrics_t scheduler;
    station_metrics_t stations[METRICS_STATION_COUNT];
    int sample_product_count;
    product_summary_t sample_products[3];
} metrics_summary_t;

// =============================================
// FUNCIONES PRINCIPALES DE LA API
// =============================================

// Inicializar sistema de métricas
int metrics_init(void);

// Finalizar sistema de métricas
void metrics_cleanup(void);

// =============================================
// FUNCIONES PARA REGISTRAR EVENTOS
// =============================================

// Registrar evento general
void metrics_record_event(event_type_t type, int product_id, int station_id);

// Funciones específicas para productos
void metrics_product_created(int product_id);
void metrics_product_queued(int product_id);
void metrics_product_processing_start(int product_id, int station_id);
void metrics_product_processing_end(product_t *product, int station_id, int completed);
void metrics_product_completed(int product_id);

// Funciones específicas para estaciones
void metrics_station_start_processing(int station_id, int product_id);
void metrics_station_end_processing(int station_id, product_t *product, int completed);
void metrics_station_idle_start(int station_id);
void metrics_station_idle_end(int station_id);

// Funciones para scheduler (preparado para futuro)
void metrics_scheduler_context_switch(int old_product_id, int new_product_id);
void metrics_scheduler_preemption(int product_id);

// Actualizar métricas agregadas del scheduler
void metrics_scheduler_update(int total_scheduled,
                              int total_completed,
                              int context_switches,
                              int preemptions,
                              double avg_wait_ms,
                              double avg_turnaround_ms);

// =============================================
// FUNCIONES DE CONSULTA Y ESTADÍSTICAS
// =============================================

// Obtener métricas de una estación específica
station_metrics_t* metrics_get_station_stats(int station_id);

// Obtener métricas del scheduler
scheduler_metrics_t* metrics_get_scheduler_stats(void);

// Calcular estadísticas globales
double metrics_get_system_throughput(void);  // productos/segundo
double metrics_get_system_utilization(void); // porcentaje de uso
int metrics_get_total_runtime_ms(void);

// =============================================
// FUNCIONES DE SALIDA Y REPORTING
// =============================================

// Imprimir resumen completo
void metrics_print_summary(void);

// Imprimir métricas por estación
void metrics_print_station_stats(int station_id);

// Imprimir métricas del scheduler
void metrics_print_scheduler_stats(void);

// Imprimir métricas globales del sistema
void metrics_print_system_stats(void);

// Capturar y presentar resúmenes diferidos
void metrics_capture_summary(metrics_summary_t *summary);
void metrics_print_captured_summary(const metrics_summary_t *summary);
void metrics_write_captured_summary(const metrics_summary_t *summary, FILE *stream);

// =============================================
// FUNCIONES AUXILIARES
// =============================================

// Obtener timestamp actual
void metrics_get_timestamp(struct timespec *ts);

// Calcular diferencia de tiempo en milisegundos
long metrics_time_diff_ms(const struct timespec *start, const struct timespec *end);

// Resetear todas las métricas
void metrics_reset_all(void);

// Validar estado del sistema de métricas
int metrics_validate_system(void);

#endif // METRICS_H