#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>

// Configuración condicional para threading (futuro)
#ifdef USE_THREADING
    #include <pthread.h>
    #define LOGGER_LOCK() pthread_mutex_lock(&g_logger_mutex)
    #define LOGGER_UNLOCK() pthread_mutex_unlock(&g_logger_mutex)
#else
    #define LOGGER_LOCK() // No-op para versión actual
    #define LOGGER_UNLOCK() // No-op para versión actual
#endif

// Niveles de logging
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_CRITICAL
} log_level_t;

// Destinos de logging
typedef enum {
    LOG_DEST_CONSOLE,
    LOG_DEST_FILE,
    LOG_DEST_BOTH
} log_destination_t;

// Configuración del logger
typedef struct {
    log_level_t min_level;           // Nivel mínimo a mostrar
    log_destination_t destination;   // Dónde escribir los logs
    FILE *log_file;                  // Archivo de log (si aplica)
    char log_filename[256];          // Nombre del archivo
    int show_timestamps;             // Mostrar timestamps
    int show_thread_id;              // Mostrar ID de hilo (futuro)
    int max_file_size;               // Rotación de archivos (futuro)
    
    // Estadísticas
    int messages_logged[5];          // Contador por nivel
    int total_messages;
    
    #ifdef USE_THREADING
        pthread_mutex_t file_mutex;  // Para escritura thread-safe
    #endif
} logger_config_t;

// Estructura para mensajes de log (para buffering futuro)
typedef struct {
    log_level_t level;
    struct timespec timestamp;
    char module[32];
    char message[512];
    int thread_id;
} log_message_t;

// =============================================
// FUNCIONES PRINCIPALES DE INICIALIZACIÓN
// =============================================

// Inicializar sistema de logging
int logger_init(log_level_t min_level, log_destination_t dest);

// Inicializar con archivo específico
int logger_init_with_file(log_level_t min_level, const char *filename);

// Configurar logger después de inicializar
void logger_set_level(log_level_t level);
void logger_set_destination(log_destination_t dest);
void logger_set_show_timestamps(int show);
void logger_set_show_thread_id(int show);

// Finalizar sistema de logging
void logger_cleanup(void);

// =============================================
// FUNCIONES PRINCIPALES DE LOGGING
// =============================================

// Función principal de logging (interna)
void logger_write(log_level_t level, const char *module, const char *format, ...);

// Macros principales para facilitar uso
#define LOG_DEBUG_MSG(module, ...) logger_write(LOG_DEBUG, module, __VA_ARGS__)
#define LOG_INFO_MSG(module, ...) logger_write(LOG_INFO, module, __VA_ARGS__)
#define LOG_WARNING_MSG(module, ...) logger_write(LOG_WARNING, module, __VA_ARGS__)
#define LOG_ERROR_MSG(module, ...) logger_write(LOG_ERROR, module, __VA_ARGS__)
#define LOG_CRITICAL_MSG(module, ...) logger_write(LOG_CRITICAL, module, __VA_ARGS__)

// Macros específicas para módulos del simulador
#define PRODUCT_DEBUG(...) LOG_DEBUG_MSG("PRODUCT", __VA_ARGS__)
#define PRODUCT_INFO(...) LOG_INFO_MSG("PRODUCT", __VA_ARGS__)
#define PRODUCT_ERROR(...) LOG_ERROR_MSG("PRODUCT", __VA_ARGS__)

#define FACTORY_DEBUG(...) LOG_DEBUG_MSG("FACTORY", __VA_ARGS__)
#define FACTORY_INFO(...) LOG_INFO_MSG("FACTORY", __VA_ARGS__)
#define FACTORY_ERROR(...) LOG_ERROR_MSG("FACTORY", __VA_ARGS__)

#define METRICS_DEBUG(...) LOG_DEBUG_MSG("METRICS", __VA_ARGS__)
#define METRICS_INFO(...) LOG_INFO_MSG("METRICS", __VA_ARGS__)
#define METRICS_ERROR(...) LOG_ERROR_MSG("METRICS", __VA_ARGS__)

#define STATION_DEBUG(...) LOG_DEBUG_MSG("STATION", __VA_ARGS__)
#define STATION_INFO(...) LOG_INFO_MSG("STATION", __VA_ARGS__)
#define STATION_ERROR(...) LOG_ERROR_MSG("STATION", __VA_ARGS__)

#define SCHEDULER_DEBUG(...) LOG_DEBUG_MSG("SCHEDULER", __VA_ARGS__)
#define SCHEDULER_INFO(...) LOG_INFO_MSG("SCHEDULER", __VA_ARGS__)
#define SCHEDULER_ERROR(...) LOG_ERROR_MSG("SCHEDULER", __VA_ARGS__)

#define SYSTEM_DEBUG(...) LOG_DEBUG_MSG("SYSTEM", __VA_ARGS__)
#define SYSTEM_INFO(...) LOG_INFO_MSG("SYSTEM", __VA_ARGS__)
#define SYSTEM_ERROR(...) LOG_ERROR_MSG("SYSTEM", __VA_ARGS__)

// =============================================
// FUNCIONES DE UTILIDAD
// =============================================

// Obtener nombre del nivel de log
const char* logger_get_level_name(log_level_t level);

// Obtener timestamp formateado
void logger_get_timestamp_str(char *buffer, size_t size);

// Verificar si un nivel está habilitado
int logger_is_level_enabled(log_level_t level);

// =============================================
// FUNCIONES DE ESTADÍSTICAS Y REPORTING
// =============================================

// Imprimir estadísticas del logger
void logger_print_stats(void);

// Obtener contador de mensajes por nivel
int logger_get_message_count(log_level_t level);

// Obtener total de mensajes
int logger_get_total_messages(void);

// Resetear contadores
void logger_reset_stats(void);

// =============================================
// FUNCIONES DE GESTIÓN DE ARCHIVOS
// =============================================

// Cambiar archivo de log
int logger_change_file(const char *filename);

// Cerrar archivo actual
void logger_close_file(void);

// Rotar archivo de log (crear nuevo archivo)
int logger_rotate_file(void);

// Verificar tamaño del archivo y rotar si es necesario
void logger_check_file_rotation(void);

// =============================================
// FUNCIONES ESPECIALES PARA DEBUGGING
// =============================================

// Log con información de función y línea
#define LOG_DEBUG_FUNC(module, ...) \
    logger_write_with_location(LOG_DEBUG, module, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

#define LOG_ERROR_FUNC(module, ...) \
    logger_write_with_location(LOG_ERROR, module, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

// Función interna para logging con ubicación
void logger_write_with_location(log_level_t level, const char *module, 
                               const char *file, const char *function, int line,
                               const char *format, ...);

// Logging de eventos del sistema (integración con métricas)
void logger_log_event(const char *event_name, int product_id, int station_id);

// Logging de cambios de estado
void logger_log_state_change(int product_id, const char *old_state, const char *new_state);

// Logging de métricas importantes
void logger_log_metric(const char *metric_name, double value, const char *unit);

// =============================================
// FUNCIONES DE CONFIGURACIÓN AVANZADA
// =============================================

// Obtener configuración actual
logger_config_t* logger_get_config(void);

// Validar configuración del logger
int logger_validate_config(void);

// Imprimir configuración actual
void logger_print_config(void);

// Configurar formato personalizado (futuro)
void logger_set_format(const char *format);

// =============================================
// FUNCIONES PARA THREADING (PREPARADAS)
// =============================================

#ifdef USE_THREADING
// Buffer asíncrono para logging sin bloqueo
int logger_init_async_buffer(int buffer_size);
void logger_flush_async_buffer(void);
void logger_stop_async_logging(void);
#endif

// =============================================
// MACROS DE CONVENIENCIA
// =============================================

// Macros para logging rápido (sin especificar módulo)
#define DLOG(...) LOG_DEBUG_MSG("MAIN", __VA_ARGS__)
#define ILOG(...) LOG_INFO_MSG("MAIN", __VA_ARGS__)
#define WLOG(...) LOG_WARNING_MSG("MAIN", __VA_ARGS__)
#define ELOG(...) LOG_ERROR_MSG("MAIN", __VA_ARGS__)
#define CLOG(...) LOG_CRITICAL_MSG("MAIN", __VA_ARGS__)

// Macros condicionales para debugging (solo en modo debug)
#ifdef DEBUG
    #define DEBUG_LOG(...) LOG_DEBUG_MSG("DEBUG", __VA_ARGS__)
    #define DEBUG_PRINT(x) printf("DEBUG: %s = %d\n", #x, x)
#else
    #define DEBUG_LOG(...) // No-op en release
    #define DEBUG_PRINT(x) // No-op en release
#endif

#endif // LOGGER_H