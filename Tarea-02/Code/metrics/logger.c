#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>

// Variable global de configuración del logger
static logger_config_t g_logger_config;
static int g_logger_initialized = 0;

// Mutex global (solo se usa si USE_THREADING está definido)
#ifdef USE_THREADING
    pthread_mutex_t g_logger_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

// Colores ANSI para terminal (solo consola)
#define COLOR_RESET   "\033[0m"
#define COLOR_DEBUG   "\033[36m"  // Cyan
#define COLOR_INFO    "\033[32m"  // Green
#define COLOR_WARNING "\033[33m"  // Yellow
#define COLOR_ERROR   "\033[31m"  // Red
#define COLOR_CRITICAL "\033[35m" // Magenta

// Nombres de los niveles
static const char* level_names[] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "CRITICAL"
};

// Colores por nivel
static const char* level_colors[] = {
    COLOR_DEBUG,
    COLOR_INFO,
    COLOR_WARNING,
    COLOR_ERROR,
    COLOR_CRITICAL
};

// =============================================
// FUNCIONES PRINCIPALES DE INICIALIZACIÓN
// =============================================

int logger_init(log_level_t min_level, log_destination_t dest) {
    if (g_logger_initialized) {
        printf("[LOGGER] Ya está inicializado\n");
        return 1;
    }
    
    memset(&g_logger_config, 0, sizeof(logger_config_t));
    
    g_logger_config.min_level = min_level;
    g_logger_config.destination = dest;
    g_logger_config.show_timestamps = 1;
    g_logger_config.show_thread_id = 0;
    g_logger_config.max_file_size = 10 * 1024 * 1024; // 10MB
    g_logger_config.log_file = NULL;
    
    #ifdef USE_THREADING
        pthread_mutex_init(&g_logger_config.file_mutex, NULL);
    #endif
    
    g_logger_initialized = 1;
    printf("[LOGGER] Sistema inicializado (nivel: %s)\n", level_names[min_level]);
    
    return 1;
}

int logger_init_with_file(log_level_t min_level, const char *filename) {
    if (!logger_init(min_level, LOG_DEST_BOTH)) {
        return 0;
    }
    
    strncpy(g_logger_config.log_filename, filename, sizeof(g_logger_config.log_filename) - 1);
    g_logger_config.log_filename[sizeof(g_logger_config.log_filename) - 1] = '\0';
    
    g_logger_config.log_file = fopen(filename, "a");
    if (!g_logger_config.log_file) {
        printf("[LOGGER] Error al abrir archivo: %s\n", filename);
        g_logger_config.destination = LOG_DEST_CONSOLE;
        return 0;
    }
    
    printf("[LOGGER] Logging a archivo: %s\n", filename);
    return 1;
}

void logger_cleanup(void) {
    if (!g_logger_initialized) return;
    
    LOGGER_LOCK();
    
    if (g_logger_config.log_file) {
        fprintf(g_logger_config.log_file, "\n[LOGGER] Sistema finalizado\n");
        fclose(g_logger_config.log_file);
        g_logger_config.log_file = NULL;
    }
    
    #ifdef USE_THREADING
        pthread_mutex_destroy(&g_logger_config.file_mutex);
    #endif
    
    g_logger_initialized = 0;
    
    LOGGER_UNLOCK();
    
    printf("[LOGGER] Sistema finalizado\n");
}

// =============================================
// FUNCIONES DE CONFIGURACIÓN
// =============================================

void logger_set_level(log_level_t level) {
    if (!g_logger_initialized) return;
    
    LOGGER_LOCK();
    g_logger_config.min_level = level;
    LOGGER_UNLOCK();
    
    printf("[LOGGER] Nivel cambiado a: %s\n", level_names[level]);
}

void logger_set_destination(log_destination_t dest) {
    if (!g_logger_initialized) return;
    
    LOGGER_LOCK();
    g_logger_config.destination = dest;
    LOGGER_UNLOCK();
}

void logger_set_show_timestamps(int show) {
    if (!g_logger_initialized) return;
    g_logger_config.show_timestamps = show;
}

void logger_set_show_thread_id(int show) {
    if (!g_logger_initialized) return;
    g_logger_config.show_thread_id = show;
}

// =============================================
// FUNCIONES AUXILIARES
// =============================================

const char* logger_get_level_name(log_level_t level) {
    if (level < 0 || level >= 5) return "UNKNOWN";
    return level_names[level];
}

void logger_get_timestamp_str(char *buffer, size_t size) {
    struct timeval tv;
    struct tm *tm_info;
    
    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);
    
    snprintf(buffer, size, "%02d:%02d:%02d.%03d",
             tm_info->tm_hour,
             tm_info->tm_min,
             tm_info->tm_sec,
             (int)(tv.tv_usec / 1000));
}

int logger_is_level_enabled(log_level_t level) {
    if (!g_logger_initialized) return 0;
    return level >= g_logger_config.min_level;
}

// =============================================
// FUNCIÓN PRINCIPAL DE LOGGING
// =============================================

void logger_write(log_level_t level, const char *module, const char *format, ...) {
    if (!g_logger_initialized || !logger_is_level_enabled(level)) {
        return;
    }
    
    LOGGER_LOCK();
    
    // Preparar mensaje
    char message[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    // Preparar timestamp
    char timestamp[32] = "";
    if (g_logger_config.show_timestamps) {
        logger_get_timestamp_str(timestamp, sizeof(timestamp));
    }
    
    // Preparar thread ID (futuro)
    char thread_info[32] = "";
    #ifdef USE_THREADING
    if (g_logger_config.show_thread_id) {
        snprintf(thread_info, sizeof(thread_info), "[T:%ld] ", pthread_self());
    }
    #endif
    
    // Actualizar estadísticas
    g_logger_config.messages_logged[level]++;
    g_logger_config.total_messages++;
    
    // Escribir a consola
    if (g_logger_config.destination == LOG_DEST_CONSOLE || 
        g_logger_config.destination == LOG_DEST_BOTH) {
        
        if (g_logger_config.show_timestamps) {
            printf("[%s] ", timestamp);
        }
        
        printf("%s%s%-8s%s [%-10s] %s%s\n",
               level_colors[level],
               thread_info,
               level_names[level],
               COLOR_RESET,
               module,
               message,
               COLOR_RESET);
        
        fflush(stdout);
    }
    
    // Escribir a archivo
    if ((g_logger_config.destination == LOG_DEST_FILE || 
         g_logger_config.destination == LOG_DEST_BOTH) &&
        g_logger_config.log_file) {
        
        if (g_logger_config.show_timestamps) {
            fprintf(g_logger_config.log_file, "[%s] ", timestamp);
        }
        
        fprintf(g_logger_config.log_file, "%s%-8s [%-10s] %s\n",
                thread_info,
                level_names[level],
                module,
                message);
        
        fflush(g_logger_config.log_file);
    }
    
    LOGGER_UNLOCK();
}

// =============================================
// LOGGING CON UBICACIÓN (DEBUGGING)
// =============================================

void logger_write_with_location(log_level_t level, const char *module,
                               const char *file, const char *function, int line,
                               const char *format, ...) {
    if (!g_logger_initialized || !logger_is_level_enabled(level)) {
        return;
    }
    
    LOGGER_LOCK();
    
    // Preparar mensaje
    char message[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    // Preparar timestamp
    char timestamp[32] = "";
    if (g_logger_config.show_timestamps) {
        logger_get_timestamp_str(timestamp, sizeof(timestamp));
    }
    
    // Actualizar estadísticas
    g_logger_config.messages_logged[level]++;
    g_logger_config.total_messages++;
    
    // Escribir a consola con ubicación
    if (g_logger_config.destination == LOG_DEST_CONSOLE || 
        g_logger_config.destination == LOG_DEST_BOTH) {
        
        if (g_logger_config.show_timestamps) {
            printf("[%s] ", timestamp);
        }
        
        printf("%s%-8s%s [%-10s] %s\n",
               level_colors[level],
               level_names[level],
               COLOR_RESET,
               module,
               message);
        
        printf("         └─ at %s:%d in %s()\n", file, line, function);
        fflush(stdout);
    }
    
    // Escribir a archivo con ubicación
    if ((g_logger_config.destination == LOG_DEST_FILE || 
         g_logger_config.destination == LOG_DEST_BOTH) &&
        g_logger_config.log_file) {
        
        if (g_logger_config.show_timestamps) {
            fprintf(g_logger_config.log_file, "[%s] ", timestamp);
        }
        
        fprintf(g_logger_config.log_file, "%-8s [%-10s] %s (at %s:%d in %s())\n",
                level_names[level],
                module,
                message,
                file,
                line,
                function);
        
        fflush(g_logger_config.log_file);
    }
    
    LOGGER_UNLOCK();
}

// =============================================
// LOGGING ESPECIALIZADO
// =============================================

void logger_log_event(const char *event_name, int product_id, int station_id) {
    LOG_INFO_MSG("EVENT", "Evento: %s | Producto: %d | Estación: %d",
                 event_name, product_id, station_id);
}

void logger_log_state_change(int product_id, const char *old_state, const char *new_state) {
    LOG_INFO_MSG("STATE", "Producto %d: %s -> %s", product_id, old_state, new_state);
}

void logger_log_metric(const char *metric_name, double value, const char *unit) {
    LOG_INFO_MSG("METRIC", "%s: %.2f %s", metric_name, value, unit);
}

// =============================================
// FUNCIONES DE ESTADÍSTICAS
// =============================================

void logger_print_stats(void) {
    if (!g_logger_initialized) {
        printf("[ERROR] Logger no inicializado\n");
        return;
    }
    
    printf("\n=== ESTADÍSTICAS DEL LOGGER ===\n");
    printf("Total de mensajes: %d\n", g_logger_config.total_messages);
    printf("Mensajes por nivel:\n");
    for (int i = 0; i < 5; i++) {
        printf("  %-8s: %d\n", level_names[i], g_logger_config.messages_logged[i]);
    }
    printf("================================\n\n");
}

int logger_get_message_count(log_level_t level) {
    if (!g_logger_initialized || level < 0 || level >= 5) return 0;
    return g_logger_config.messages_logged[level];
}

int logger_get_total_messages(void) {
    if (!g_logger_initialized) return 0;
    return g_logger_config.total_messages;
}

void logger_reset_stats(void) {
    if (!g_logger_initialized) return;
    
    LOGGER_LOCK();
    memset(g_logger_config.messages_logged, 0, sizeof(g_logger_config.messages_logged));
    g_logger_config.total_messages = 0;
    LOGGER_UNLOCK();
    
    printf("[LOGGER] Estadísticas reiniciadas\n");
}

// =============================================
// GESTIÓN DE ARCHIVOS
// =============================================

int logger_change_file(const char *filename) {
    if (!g_logger_initialized) return 0;
    
    LOGGER_LOCK();
    
    // Cerrar archivo actual
    if (g_logger_config.log_file) {
        fclose(g_logger_config.log_file);
    }
    
    // Abrir nuevo archivo
    strncpy(g_logger_config.log_filename, filename, sizeof(g_logger_config.log_filename) - 1);
    g_logger_config.log_file = fopen(filename, "a");
    
    int success = (g_logger_config.log_file != NULL);
    
    LOGGER_UNLOCK();
    
    if (success) {
        printf("[LOGGER] Archivo cambiado a: %s\n", filename);
    } else {
        printf("[LOGGER] Error al cambiar archivo\n");
    }
    
    return success;
}

void logger_close_file(void) {
    if (!g_logger_initialized) return;
    
    LOGGER_LOCK();
    
    if (g_logger_config.log_file) {
        fclose(g_logger_config.log_file);
        g_logger_config.log_file = NULL;
        g_logger_config.destination = LOG_DEST_CONSOLE;
    }
    
    LOGGER_UNLOCK();
    
    printf("[LOGGER] Archivo cerrado\n");
}

int logger_rotate_file(void) {
    if (!g_logger_initialized || !g_logger_config.log_file) return 0;
    
    LOGGER_LOCK();
    
    // Generar nuevo nombre con timestamp
    char new_filename[300];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    snprintf(new_filename, sizeof(new_filename), "%s.%04d%02d%02d_%02d%02d%02d",
             g_logger_config.log_filename,
             tm_info->tm_year + 1900,
             tm_info->tm_mon + 1,
             tm_info->tm_mday,
             tm_info->tm_hour,
             tm_info->tm_min,
             tm_info->tm_sec);
    
    // Cerrar y renombrar archivo actual
    fclose(g_logger_config.log_file);
    rename(g_logger_config.log_filename, new_filename);
    
    // Abrir nuevo archivo
    g_logger_config.log_file = fopen(g_logger_config.log_filename, "a");
    
    int success = (g_logger_config.log_file != NULL);
    
    LOGGER_UNLOCK();
    
    if (success) {
        printf("[LOGGER] Archivo rotado a: %s\n", new_filename);
    }
    
    return success;
}

void logger_check_file_rotation(void) {
    // Implementación futura para rotación automática por tamaño
}

// =============================================
// FUNCIONES DE CONFIGURACIÓN
// =============================================

logger_config_t* logger_get_config(void) {
    if (!g_logger_initialized) return NULL;
    return &g_logger_config;
}

int logger_validate_config(void) {
    if (!g_logger_initialized) {
        printf("[LOGGER] No inicializado\n");
        return 0;
    }
    
    if (g_logger_config.destination == LOG_DEST_FILE && !g_logger_config.log_file) {
        printf("[LOGGER] Configuración inválida: archivo NULL\n");
        return 0;
    }
    
    printf("[LOGGER] Configuración válida\n");
    return 1;
}

void logger_print_config(void) {
    if (!g_logger_initialized) {
        printf("[ERROR] Logger no inicializado\n");
        return;
    }
    
    printf("\n=== CONFIGURACIÓN DEL LOGGER ===\n");
    printf("Nivel mínimo: %s\n", level_names[g_logger_config.min_level]);
    printf("Destino: ");
    switch (g_logger_config.destination) {
        case LOG_DEST_CONSOLE: printf("CONSOLA\n"); break;
        case LOG_DEST_FILE: printf("ARCHIVO\n"); break;
        case LOG_DEST_BOTH: printf("AMBOS\n"); break;
    }
    printf("Mostrar timestamps: %s\n", g_logger_config.show_timestamps ? "Sí" : "No");
    printf("Archivo: %s\n", g_logger_config.log_filename[0] ? g_logger_config.log_filename : "N/A");
    printf("================================\n\n");
}