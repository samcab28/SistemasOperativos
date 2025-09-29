#ifndef STATION_H
#define STATION_H

#include "product.h"
#include "queue.h"
#include <pthread.h>
#include <semaphore.h>

// Forward declaration
typedef struct station station_t;

// Estados de la estación
typedef enum {
    STATION_IDLE,
    STATION_BUSY,
    STATION_STOPPED
} station_state_t;

// Tipos de estación (para identificación)
typedef enum {
    STATION_TYPE_CUTTING = 0,      // Corte
    STATION_TYPE_ASSEMBLY = 1,     // Ensamblaje
    STATION_TYPE_PACKAGING = 2     // Empaque
} station_type_t;

// Estructura principal de la estación
struct station {
    // Identificación
    int id;
    station_type_t type;
    char name[50];
    
    // Threading
    pthread_t thread;
    int thread_running;
    
    // Sincronización
    pthread_mutex_t mutex;
    sem_t processing_semaphore;    // Máximo 1 producto a la vez
    pthread_cond_t work_available; // Señal de trabajo disponible
    
    // Colas de entrada y salida
    queue_t *input_queue;
    queue_t *output_queue;         // NULL si es última estación
    
    // Configuración de procesamiento
    int processing_time_ms;        // Tiempo base de procesamiento
    int processing_variance_ms;    // Variación aleatoria (+/-)
    
    // Estado actual
    station_state_t state;
    product_t *current_product;    // Producto en procesamiento
    struct timespec processing_start_time;
    
    // Chain of Responsibility
    station_t *next_station;
    
    // Estadísticas
    struct {
        int products_processed;
        int products_failed;
        long total_processing_time_ms;
        long total_idle_time_ms;
        struct timespec last_activity_time;
    } stats;
};

// =============================================
// FUNCIONES DE CREACIÓN Y DESTRUCCIÓN
// =============================================

// Crear estación con configuración básica
station_t *create_station(station_type_t type, const char *name, int processing_time_ms);

// Destruir estación y liberar recursos
void destroy_station(station_t *station);

// =============================================
// FUNCIONES DE CONFIGURACIÓN
// =============================================

// Configurar colas de entrada/salida
void station_set_input_queue(station_t *station, queue_t *queue);
void station_set_output_queue(station_t *station, queue_t *queue);

// Configurar siguiente estación (Chain of Responsibility)
void station_set_next(station_t *station, station_t *next);

// Configurar variación de tiempo de procesamiento
void station_set_processing_variance(station_t *station, int variance_ms);

// =============================================
// FUNCIONES DE CONTROL DEL HILO
// =============================================

// Iniciar el hilo de la estación
int station_start_thread(station_t *station);

// Detener el hilo de la estación (espera a que termine)
void station_stop_thread(station_t *station);

// Verificar si el hilo está corriendo
int station_is_running(const station_t *station);

// =============================================
// FUNCIÓN PRINCIPAL DEL HILO (Worker)
// =============================================

// Función del hilo de la estación (uso interno)
void *station_worker_thread(void *arg);

// =============================================
// FUNCIONES DE PROCESAMIENTO
// =============================================

// Procesar un producto (llamada interna por el hilo)
void station_process_product(station_t *station, product_t *product);

// Enviar producto a la siguiente estación
void station_send_to_next(station_t *station, product_t *product);

// Simular tiempo de procesamiento con variación
void station_simulate_processing(const station_t *station);

// =============================================
// FUNCIONES DE ESTADO Y ESTADÍSTICAS
// =============================================

// Obtener estado actual de la estación
station_state_t station_get_state(const station_t *station);

// Cambiar estado de la estación
void station_set_state(station_t *station, station_state_t state);

// Verificar si la estación está ocupada
int station_is_busy(const station_t *station);

// Obtener producto en procesamiento actual
product_t *station_get_current_product(const station_t *station);

// =============================================
// FUNCIONES DE ESTADÍSTICAS
// =============================================

// Obtener número de productos procesados
int station_get_products_processed(const station_t *station);

// Obtener tiempo total de procesamiento
long station_get_total_processing_time(const station_t *station);

// Obtener tiempo promedio de procesamiento
double station_get_average_processing_time(const station_t *station);

// Calcular utilización de la estación (%)
double station_get_utilization(const station_t *station);

// Imprimir estadísticas de la estación
void station_print_stats(const station_t *station);

// Resetear estadísticas
void station_reset_stats(station_t *station);

// =============================================
// FUNCIONES DE UTILIDAD
// =============================================

// Obtener nombre del tipo de estación
const char *station_get_type_name(station_type_t type);

// Obtener nombre del estado
const char *station_get_state_name(station_state_t state);

// Validar configuración de la estación
int station_validate_config(const station_t *station);

// Imprimir información de la estación
void station_print_info(const station_t *station);

// =============================================
// FUNCIONES PARA DEBUGGING
// =============================================

// Imprimir estado actual detallado
void station_debug_print_state(const station_t *station);

// Verificar estado de las colas
void station_debug_check_queues(const station_t *station);

#endif // STATION_H