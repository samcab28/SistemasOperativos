#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "product.h"
#include "queue.h"
#include "station.h"
#include <pthread.h>
#include <semaphore.h>

// Forward declarations
typedef struct scheduler scheduler_t;
typedef struct scheduling_strategy scheduling_strategy_t;

// Tipos de algoritmos de scheduling
typedef enum {
    SCHED_FCFS,          // First Come First Serve
    SCHED_ROUND_ROBIN    // Round Robin con quantum
} scheduling_algorithm_t;

// Estados del scheduler
typedef enum {
    SCHED_IDLE,
    SCHED_RUNNING,
    SCHED_PAUSED,
    SCHED_STOPPED
} scheduler_state_t;

// Estructura para configuración del scheduler
typedef struct {
    scheduling_algorithm_t algorithm;
    int quantum_ms;              // Para Round Robin
    int preemption_enabled;      // Habilitar preemption
    int max_products_in_system;  // Límite de productos
} scheduler_config_t;

// Estructura principal del scheduler
struct scheduler {
    // Identificación
    int id;
    char name[50];
    
    // Threading
    pthread_t thread;
    int thread_running;
    
    // Sincronización
    pthread_mutex_t mutex;
    pthread_cond_t products_available;
    sem_t scheduling_semaphore;
    
    // Configuración
    scheduler_config_t config;
    
    // Estrategia de scheduling (Strategy Pattern)
    scheduling_strategy_t *strategy;
    
    // Colas del scheduler
    queue_t *ready_queue;        // Productos listos para procesar
    queue_t *waiting_queue;      // Productos esperando (futuro)
    
    // Producto actualmente siendo despachado
    product_t *current_product;
    
    // Primera estación de la línea
    station_t *first_station;
    
    // Estado del scheduler
    scheduler_state_t state;
    
    // Estadísticas
    struct {
        int total_products_scheduled;
        int products_completed;
        int context_switches;
        int preemptions;
        long total_wait_time_ms;
        long total_turnaround_time_ms;
        struct timespec start_time;
        product_t *last_dispatched_product;
    } stats;
    
    // Control de quantum (Round Robin)
    struct {
        struct timespec quantum_start_time;
        int quantum_expired;
        struct timespec quantum_deadline;
        int waiting_for_event;
        int event_pending;
        int event_type;
        pthread_cond_t cond;
    } quantum_control;
};

// =============================================
// FUNCIONES DE CREACIÓN Y DESTRUCCIÓN
// =============================================

// Crear scheduler con configuración básica
scheduler_t *create_scheduler(scheduling_algorithm_t algorithm, int quantum_ms);

// Destruir scheduler y liberar recursos
void destroy_scheduler(scheduler_t *scheduler);

// =============================================
// FUNCIONES DE CONFIGURACIÓN
// =============================================

// Configurar primera estación de la línea
void scheduler_set_first_station(scheduler_t *scheduler, station_t *station);

// Configurar cola de productos listos
void scheduler_set_ready_queue(scheduler_t *scheduler, queue_t *queue);

// Cambiar algoritmo de scheduling
void scheduler_set_algorithm(scheduler_t *scheduler, scheduling_algorithm_t algorithm);

// Configurar quantum (para Round Robin)
void scheduler_set_quantum(scheduler_t *scheduler, int quantum_ms);

// Habilitar/deshabilitar preemption
void scheduler_set_preemption(scheduler_t *scheduler, int enabled);

// Configurar límite de productos en el sistema
void scheduler_set_max_products(scheduler_t *scheduler, int max_products);

// =============================================
// FUNCIONES DE CONTROL DEL HILO
// =============================================

// Iniciar el hilo del scheduler
int scheduler_start_thread(scheduler_t *scheduler);

// Detener el hilo del scheduler
void scheduler_stop_thread(scheduler_t *scheduler);

// Pausar el scheduler
void scheduler_pause(scheduler_t *scheduler);

// Reanudar el scheduler
void scheduler_resume(scheduler_t *scheduler);

// Verificar si está corriendo
int scheduler_is_running(const scheduler_t *scheduler);

// =============================================
// FUNCIÓN PRINCIPAL DEL HILO (Worker)
// =============================================

// Función del hilo del scheduler (uso interno)
void *scheduler_worker_thread(void *arg);

// =============================================
// FUNCIONES DE SCHEDULING
// =============================================

// Agregar producto a la cola de listos
void scheduler_add_product(scheduler_t *scheduler, product_t *product);

// Agregar múltiples productos (batch)
void scheduler_add_batch(scheduler_t *scheduler, product_t **products, int count);

// Reencolar producto preemptado sin contar como nuevo scheduling
void scheduler_requeue_preempted_product(scheduler_t *scheduler, product_t *product);

// Notificar al scheduler que el producto terminó su porción (preemptado o completado)
void scheduler_notify_slice_end(scheduler_t *scheduler, product_t *product, int was_preempted);

// Seleccionar siguiente producto según algoritmo
product_t *scheduler_select_next_product(scheduler_t *scheduler);

// Despachar producto a la primera estación
void scheduler_dispatch_product(scheduler_t *scheduler, product_t *product);

// Realizar context switch
void scheduler_context_switch(scheduler_t *scheduler, product_t *old_product, 
                             product_t *new_product);

// =============================================
// FUNCIONES ESPECÍFICAS DE FCFS
// =============================================

// Seleccionar siguiente producto (FCFS)
product_t *scheduler_fcfs_select_next(scheduler_t *scheduler);

// Procesar con FCFS
void scheduler_fcfs_process(scheduler_t *scheduler);

// =============================================
// FUNCIONES ESPECÍFICAS DE ROUND ROBIN
// =============================================

// Seleccionar siguiente producto (Round Robin)
product_t *scheduler_rr_select_next(scheduler_t *scheduler);

// Iniciar quantum para producto
void scheduler_rr_start_quantum(scheduler_t *scheduler, product_t *product);

// Verificar si el quantum expiró
int scheduler_rr_is_quantum_expired(scheduler_t *scheduler);

// Manejar expiración de quantum (preemption)
void scheduler_rr_handle_quantum_expiration(scheduler_t *scheduler);

// Procesar con Round Robin
void scheduler_rr_process(scheduler_t *scheduler);

// =============================================
// FUNCIONES DE ESTADO Y CONTROL
// =============================================

// Obtener estado actual
scheduler_state_t scheduler_get_state(const scheduler_t *scheduler);

// Cambiar estado
void scheduler_set_state(scheduler_t *scheduler, scheduler_state_t state);

// Verificar si hay productos en cola
int scheduler_has_products(const scheduler_t *scheduler);

// Obtener número de productos en cola
int scheduler_get_queue_size(const scheduler_t *scheduler);

// Esperar a que todos los productos sean procesados
void scheduler_wait_completion(scheduler_t *scheduler);

// Registrar la finalización de un producto
void scheduler_record_product_completion(scheduler_t *scheduler, product_t *product);

// =============================================
// FUNCIONES DE ESTADÍSTICAS
// =============================================

// Obtener productos programados
int scheduler_get_products_scheduled(const scheduler_t *scheduler);

// Obtener productos completados
int scheduler_get_products_completed(const scheduler_t *scheduler);

// Obtener número de context switches
int scheduler_get_context_switches(const scheduler_t *scheduler);

// Obtener número de preempciones
int scheduler_get_preemptions(const scheduler_t *scheduler);

// Calcular tiempo promedio de espera
double scheduler_get_average_wait_time(const scheduler_t *scheduler);

// Calcular tiempo promedio de turnaround
double scheduler_get_average_turnaround_time(const scheduler_t *scheduler);

// Calcular throughput (productos/segundo)
double scheduler_get_throughput(const scheduler_t *scheduler);

// Imprimir estadísticas del scheduler
void scheduler_print_stats(const scheduler_t *scheduler);

// Resetear estadísticas
void scheduler_reset_stats(scheduler_t *scheduler);

// =============================================
// FUNCIONES DE UTILIDAD
// =============================================

// Obtener nombre del algoritmo
const char *scheduler_get_algorithm_name(scheduling_algorithm_t algorithm);

// Obtener nombre del estado
const char *scheduler_get_state_name(scheduler_state_t state);

// Validar configuración del scheduler
int scheduler_validate_config(const scheduler_t *scheduler);

// Imprimir información del scheduler
void scheduler_print_info(const scheduler_t *scheduler);

// Imprimir configuración actual
void scheduler_print_config(const scheduler_t *scheduler);

// =============================================
// FUNCIONES PARA DEBUGGING
// =============================================

// Imprimir estado actual detallado
void scheduler_debug_print_state(const scheduler_t *scheduler);

// Imprimir contenido de la cola de listos
void scheduler_debug_print_ready_queue(const scheduler_t *scheduler);

// Verificar integridad del scheduler
int scheduler_debug_check_integrity(const scheduler_t *scheduler);

// =============================================
// CALLBACKS Y HOOKS (OPCIONALES)
// =============================================

// Tipo de función callback para eventos del scheduler
typedef void (*scheduler_event_callback_t)(scheduler_t *scheduler, 
                                          product_t *product, 
                                          void *user_data);

// Registrar callback para cuando se programa un producto
void scheduler_register_schedule_callback(scheduler_t *scheduler,
                                         scheduler_event_callback_t callback,
                                         void *user_data);

// Registrar callback para context switches
void scheduler_register_context_switch_callback(scheduler_t *scheduler,
                                               scheduler_event_callback_t callback,
                                               void *user_data);

#endif // SCHEDULER_H