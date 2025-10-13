#include "metrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

// Variable global del sistema de métricas
static metrics_system_t g_metrics;
static int g_metrics_initialized = 0;

// Mutex global (solo se usa si USE_THREADING está definido)
#ifdef USE_THREADING
    pthread_mutex_t g_metrics_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

// Nombres de las estaciones
static const char* station_names[METRICS_STATION_COUNT] = {"Corte", "Ensamblaje", "Empaque"};

// Nombres de los eventos (para debugging/logging)
static const char* event_names[] = {
    "PRODUCT_CREATED",
    "PRODUCT_QUEUED", 
    "PRODUCT_PROCESSING_START",
    "PRODUCT_PROCESSING_END",
    "PRODUCT_COMPLETED",
    "STATION_IDLE_START",
    "STATION_IDLE_END",
    "SCHEDULER_CONTEXT_SWITCH",
    "SCHEDULER_PREEMPTION"
};

// =============================================
// FUNCIONES AUXILIARES INTERNAS
// =============================================

static void init_station_metrics(station_metrics_t *station, int id) {
    station->station_id = id;
    strncpy(station->name, station_names[id], sizeof(station->name) - 1);
    station->name[sizeof(station->name) - 1] = '\0';
    
    station->products_processed = 0;
    station->products_waiting = 0;
    station->total_processing_time_ms = 0;
    station->completed_processing_time_ms = 0;
    station->total_idle_time_ms = 0;
    station->min_processing_time_ms = INT_MAX;
    station->max_processing_time_ms = 0;
    station->avg_processing_time_ms = 0.0;
    station->is_busy = 0;
    
    metrics_get_timestamp(&station->last_activity_time);
}

static void update_station_processing_stats(station_metrics_t *station,
                                           int slice_time_ms,
                                           int completed,
                                           int final_processing_time_ms) {
    station->total_processing_time_ms += slice_time_ms;

    if (!completed) {
        return;
    }

    station->products_processed++;
    station->completed_processing_time_ms += final_processing_time_ms;

    if (final_processing_time_ms < station->min_processing_time_ms) {
        station->min_processing_time_ms = final_processing_time_ms;
    }
    if (final_processing_time_ms > station->max_processing_time_ms) {
        station->max_processing_time_ms = final_processing_time_ms;
    }

    station->avg_processing_time_ms =
        station->products_processed > 0
            ? (double)station->completed_processing_time_ms / station->products_processed
            : 0.0;
}

// =============================================
// FUNCIONES PRINCIPALES DE LA API
// =============================================

int metrics_init(void) {
    if (g_metrics_initialized) {
        printf("[METRICS] Sistema ya inicializado\n");
        return 1;
    }
    
    // Inicializar estructura principal
    memset(&g_metrics, 0, sizeof(metrics_system_t));
    
    // Registrar tiempo de inicio
    metrics_get_timestamp(&g_metrics.system_start_time);
    
    // Inicializar métricas de estaciones
    for (int i = 0; i < METRICS_STATION_COUNT; i++) {
        init_station_metrics(&g_metrics.stations[i], i);
    }
    
    // Crear buffer de eventos (tamaño fijo por ahora)
    g_metrics.buffer_size = 1000;
    g_metrics.event_buffer = malloc(sizeof(metric_event_t) * g_metrics.buffer_size);
    if (!g_metrics.event_buffer) {
        printf("[ERROR] No se pudo crear buffer de eventos\n");
        return 0;
    }
    g_metrics.buffer_count = 0;
    
    #ifdef USE_THREADING
        // Inicializar sincronización (futuro)
        pthread_mutex_init(&g_metrics.buffer_mutex, NULL);
        sem_init(&g_metrics.event_semaphore, 0, 0);
    #endif
    
    g_metrics_initialized = 1;
    printf("[METRICS] Sistema de métricas inicializado\n");
    return 1;
}

void metrics_cleanup(void) {
    if (!g_metrics_initialized) return;
    
    METRICS_LOCK();
    
    if (g_metrics.event_buffer) {
        free(g_metrics.event_buffer);
        g_metrics.event_buffer = NULL;
    }
    
    #ifdef USE_THREADING
        pthread_mutex_destroy(&g_metrics.buffer_mutex);
        sem_destroy(&g_metrics.event_semaphore);
    #endif
    
    g_metrics_initialized = 0;
    
    METRICS_UNLOCK();
    
    printf("[METRICS] Sistema de métricas finalizado\n");
}

// =============================================
// FUNCIONES PARA REGISTRAR EVENTOS
// =============================================

void metrics_record_event(event_type_t type, int product_id, int station_id) {
    if (!g_metrics_initialized) return;
    
    METRICS_LOCK();
    
    // Agregar al buffer si hay espacio
    if (g_metrics.buffer_count < g_metrics.buffer_size) {
        metric_event_t *event = &g_metrics.event_buffer[g_metrics.buffer_count];
        event->type = type;
        event->product_id = product_id;
        event->station_id = station_id;
        event->additional_data = 0;
        metrics_get_timestamp(&event->timestamp);
        
        g_metrics.buffer_count++;
        
        printf("[METRICS] Evento: %s - Producto: %d, Estación: %d\n",
               event_names[type], product_id, station_id);
    }
    
    METRICS_UNLOCK();
}

void metrics_product_created(int product_id) {
    if (!g_metrics_initialized) return;
    
    METRICS_LOCK();
    g_metrics.total_products_created++;
    METRICS_UNLOCK();
    
    metrics_record_event(EVENT_PRODUCT_CREATED, product_id, -1);
}

void metrics_product_queued(int product_id) {
    metrics_record_event(EVENT_PRODUCT_QUEUED, product_id, -1);
}

void metrics_product_processing_start(int product_id, int station_id) {
    if (!g_metrics_initialized || station_id < 0 || station_id >= METRICS_STATION_COUNT) return;
    
    METRICS_LOCK();
    g_metrics.stations[station_id].is_busy = 1;
    metrics_get_timestamp(&g_metrics.stations[station_id].last_activity_time);
    METRICS_UNLOCK();
    
    metrics_record_event(EVENT_PRODUCT_PROCESSING_START, product_id, station_id);
}

void metrics_product_processing_end(product_t *product, int station_id, int completed) {
    if (!g_metrics_initialized || station_id < 0 || station_id >= METRICS_STATION_COUNT) return;

    int product_id = product ? product->id : -1;
    int total_time_ms = 0;
    if (completed && product && product->metrics) {
        total_time_ms = product->metrics->station_metrics[station_id].process_time_ms;
    }

    METRICS_LOCK();

    station_metrics_t *station = &g_metrics.stations[station_id];
    struct timespec end_time;
    metrics_get_timestamp(&end_time);

    long slice_time_ms = metrics_time_diff_ms(&station->last_activity_time, &end_time);
    update_station_processing_stats(station, (int)slice_time_ms, completed, total_time_ms);
    station->is_busy = 0;
    station->last_activity_time = end_time;

    METRICS_UNLOCK();

    metrics_record_event(EVENT_PRODUCT_PROCESSING_END, product_id, station_id);
}

void metrics_product_completed(int product_id) {
    if (!g_metrics_initialized) return;
    
    METRICS_LOCK();
    g_metrics.total_products_completed++;
    if (g_metrics.completion_order_count < METRICS_COMPLETION_ORDER_MAX) {
        g_metrics.completion_order[g_metrics.completion_order_count++] = product_id;
    }
    METRICS_UNLOCK();
    
    metrics_record_event(EVENT_PRODUCT_COMPLETED, product_id, -1);
}

void metrics_station_start_processing(int station_id, int product_id) {
    metrics_product_processing_start(product_id, station_id);
}

void metrics_station_end_processing(int station_id, product_t *product, int completed) {
    metrics_product_processing_end(product, station_id, completed);
}

void metrics_station_idle_start(int station_id) {
    if (!g_metrics_initialized || station_id < 0 || station_id >= METRICS_STATION_COUNT) return;
    
    METRICS_LOCK();
    g_metrics.stations[station_id].is_busy = 0;
    metrics_get_timestamp(&g_metrics.stations[station_id].last_activity_time);
    METRICS_UNLOCK();
    
    metrics_record_event(EVENT_STATION_IDLE_START, -1, station_id);
}

void metrics_station_idle_end(int station_id) {
    metrics_record_event(EVENT_STATION_IDLE_END, -1, station_id);
}

// Funciones del scheduler (preparadas para futuro)
void metrics_scheduler_context_switch(int old_product_id, int new_product_id) {
    if (!g_metrics_initialized) return;
    
    METRICS_LOCK();
    g_metrics.scheduler.context_switches++;
    METRICS_UNLOCK();
    
    printf("[METRICS] Context switch: %d -> %d\n", old_product_id, new_product_id);
}

void metrics_scheduler_preemption(int product_id) {
    if (!g_metrics_initialized) return;
    
    METRICS_LOCK();
    g_metrics.scheduler.preemptions++;
    METRICS_UNLOCK();
    
    metrics_record_event(EVENT_SCHEDULER_PREEMPTION, product_id, -1);
}

void metrics_scheduler_update(int total_scheduled,
                              int total_completed,
                              int context_switches,
                              int preemptions,
                              double avg_wait_ms,
                              double avg_turnaround_ms) {
    if (!g_metrics_initialized) return;

    METRICS_LOCK();
    g_metrics.scheduler.total_products_scheduled = total_scheduled;
    g_metrics.scheduler.context_switches = context_switches;
    g_metrics.scheduler.preemptions = preemptions;
    g_metrics.scheduler.avg_wait_time_ms = avg_wait_ms;
    g_metrics.scheduler.avg_turnaround_time_ms = avg_turnaround_ms;
    g_metrics.total_products_completed = total_completed;
    METRICS_UNLOCK();
}

// =============================================
// FUNCIONES DE CONSULTA Y ESTADÍSTICAS
// =============================================

station_metrics_t* metrics_get_station_stats(int station_id) {
    if (!g_metrics_initialized || station_id < 0 || station_id >= METRICS_STATION_COUNT) {
        return NULL;
    }
    return &g_metrics.stations[station_id];
}

scheduler_metrics_t* metrics_get_scheduler_stats(void) {
    if (!g_metrics_initialized) return NULL;
    return &g_metrics.scheduler;
}

double metrics_get_system_throughput(void) {
    if (!g_metrics_initialized) return 0.0;
    
    int runtime_ms = metrics_get_total_runtime_ms();
    if (runtime_ms <= 0) return 0.0;
    
    return (double)g_metrics.total_products_completed * 1000.0 / runtime_ms;
}

double metrics_get_system_utilization(void) {
    if (!g_metrics_initialized) return 0.0;
    
    int total_processing_time = 0;
    for (int i = 0; i < METRICS_STATION_COUNT; i++) {
        total_processing_time += g_metrics.stations[i].total_processing_time_ms;
    }
    
    int runtime_ms = metrics_get_total_runtime_ms();
    if (runtime_ms <= 0) return 0.0;
    
    return (double)total_processing_time * 100.0 / (runtime_ms * METRICS_STATION_COUNT);
}

int metrics_get_total_runtime_ms(void) {
    if (!g_metrics_initialized) return 0;
    
    struct timespec current_time;
    metrics_get_timestamp(&current_time);
    
    return (int)metrics_time_diff_ms(&g_metrics.system_start_time, &current_time);
}

// =============================================
// FUNCIONES DE SALIDA Y REPORTING
// =============================================

void metrics_print_summary(void) {
    if (!g_metrics_initialized) {
        printf("[ERROR] Sistema de métricas no inicializado\n");
        return;
    }
    
    printf("\n");
    printf("=========================================\n");
    printf("       RESUMEN DE MÉTRICAS GLOBALES     \n");
    printf("=========================================\n");
    
    printf("Tiempo total de ejecución: %d ms\n", metrics_get_total_runtime_ms());
    printf("Productos creados: %d\n", g_metrics.total_products_created);
    printf("Productos completados: %d\n", g_metrics.total_products_completed);
    printf("Productos fallidos: %d\n", g_metrics.total_products_failed);
    printf("Throughput del sistema: %.2f productos/segundo\n", metrics_get_system_throughput());
    printf("Utilización del sistema: %.1f%%\n", metrics_get_system_utilization());
    
    printf("\nMétricas por estación:\n");
    for (int i = 0; i < METRICS_STATION_COUNT; i++) {
        metrics_print_station_stats(i);
    }
    
    printf("=========================================\n\n");
}

void metrics_print_station_stats(int station_id) {
    station_metrics_t *station = metrics_get_station_stats(station_id);
    if (!station) {
        printf("[ERROR] Estación %d inválida\n", station_id);
        return;
    }
    
    printf("\n--- Estación %d (%s) ---\n", station_id, station->name);
    printf("  Productos procesados: %d\n", station->products_processed);
    printf("  Tiempo total de procesamiento: %d ms\n", station->total_processing_time_ms);
    printf("  Tiempo promedio: %.1f ms\n", station->avg_processing_time_ms);
    printf("  Tiempo mínimo: %d ms\n", station->min_processing_time_ms == INT_MAX ? 0 : station->min_processing_time_ms);
    printf("  Tiempo máximo: %d ms\n", station->max_processing_time_ms);
    printf("  Estado actual: %s\n", station->is_busy ? "OCUPADA" : "LIBRE");
}

void metrics_print_scheduler_stats(void) {
    scheduler_metrics_t *sched = metrics_get_scheduler_stats();
    if (!sched) return;
    
    printf("\n--- Métricas del Scheduler ---\n");
    printf("  Productos programados: %d\n", sched->total_products_scheduled);
    printf("  Cambios de contexto: %d\n", sched->context_switches);
    printf("  Preempciones: %d\n", sched->preemptions);
    printf("  Tiempo promedio de espera: %.1f ms\n", sched->avg_wait_time_ms);
    printf("  Tiempo promedio de turnaround: %.1f ms\n", sched->avg_turnaround_time_ms);
}

void metrics_print_system_stats(void) {
    metrics_print_summary();
    metrics_print_scheduler_stats();
}

void metrics_capture_summary(metrics_summary_t *summary) {
    if (!summary) return;

    memset(summary, 0, sizeof(*summary));

    if (!g_metrics_initialized) {
        return;
    }

    METRICS_LOCK();

    summary->total_products_created = g_metrics.total_products_created;
    summary->total_products_completed = g_metrics.total_products_completed;
    summary->total_products_failed = g_metrics.total_products_failed;
    summary->scheduler = g_metrics.scheduler;
    summary->completion_order_count = g_metrics.completion_order_count;
    int copy_count = summary->completion_order_count;
    if (copy_count > METRICS_COMPLETION_ORDER_MAX) {
        copy_count = METRICS_COMPLETION_ORDER_MAX;
    }
    for (int i = 0; i < copy_count; ++i) {
        summary->completion_order[i] = g_metrics.completion_order[i];
    }

    for (int i = 0; i < METRICS_STATION_COUNT; ++i) {
        summary->stations[i] = g_metrics.stations[i];
    }

    struct timespec current_time;
    metrics_get_timestamp(&current_time);
    summary->total_runtime_ms = (int)metrics_time_diff_ms(&g_metrics.system_start_time, &current_time);
    if (summary->total_runtime_ms < 0) {
        summary->total_runtime_ms = 0;
    }

    int total_processing_time = 0;
    for (int i = 0; i < METRICS_STATION_COUNT; ++i) {
        total_processing_time += summary->stations[i].total_processing_time_ms;
    }

    if (summary->total_runtime_ms > 0) {
        summary->throughput = (double)summary->total_products_completed * 1000.0 /
                               summary->total_runtime_ms;
        summary->utilization = (double)total_processing_time * 100.0 /
                               (summary->total_runtime_ms * METRICS_STATION_COUNT);
    } else {
        summary->throughput = 0.0;
        summary->utilization = 0.0;
    }

    METRICS_UNLOCK();
}

static void metrics_write_summary_stream(const metrics_summary_t *summary, FILE *stream) {
    if (!summary || !stream) {
        return;
    }

    const char *algorithm_name = summary->algorithm[0] ? summary->algorithm : "(desconocido)";

    fprintf(stream, "\n=========================================\n");
    fprintf(stream, " RESUMEN DE ALGORITMO: %s\n", algorithm_name);
    fprintf(stream, "=========================================\n");
    fprintf(stream, "Productos configurados: %d\n", summary->num_products);
    fprintf(stream, "Modo de procesamiento: %s\n", summary->randomize_processing ? "ALEATORIO" : "DETERMINISTA");
    fprintf(stream, "Productos creados: %d\n", summary->total_products_created);
    fprintf(stream, "Productos completados: %d\n", summary->total_products_completed);
    fprintf(stream, "Productos fallidos: %d\n", summary->total_products_failed);
    fprintf(stream, "Tiempo total de ejecución: %d ms\n", summary->total_runtime_ms);
    fprintf(stream, "Throughput del sistema: %.2f productos/segundo\n", summary->throughput);
    fprintf(stream, "Utilización promedio de estaciones: %.1f%%\n", summary->utilization);

    fprintf(stream, "\n--- Métricas del Scheduler ---\n");
    fprintf(stream, "Productos programados: %d\n", summary->scheduler.total_products_scheduled);
    fprintf(stream, "Cambios de contexto: %d\n", summary->scheduler.context_switches);
    fprintf(stream, "Preempciones: %d\n", summary->scheduler.preemptions);
    fprintf(stream, "Tiempo promedio de espera: %.2f ms\n", summary->scheduler.avg_wait_time_ms);
    fprintf(stream, "Tiempo promedio de turnaround: %.2f ms\n", summary->scheduler.avg_turnaround_time_ms);

    fprintf(stream, "\n--- Métricas por Estación ---\n");
    for (int i = 0; i < METRICS_STATION_COUNT; ++i) {
        const station_metrics_t *station = &summary->stations[i];
        int min_time = (station->min_processing_time_ms == INT_MAX) ? 0 : station->min_processing_time_ms;
        fprintf(stream, "Estación %d (%s)\n", station->station_id, station->name);
        fprintf(stream, "  Productos procesados: %d\n", station->products_processed);
        fprintf(stream, "  Tiempo total de procesamiento: %d ms\n", station->total_processing_time_ms);
        fprintf(stream, "  Tiempo promedio: %.2f ms\n", station->avg_processing_time_ms);
        fprintf(stream, "  Tiempo mínimo: %d ms\n", min_time);
        fprintf(stream, "  Tiempo máximo: %d ms\n", station->max_processing_time_ms);
    }

    if (summary->completion_order_count > 0) {
        int order_count = summary->completion_order_count;
        if (order_count > METRICS_COMPLETION_ORDER_MAX) {
            order_count = METRICS_COMPLETION_ORDER_MAX;
        }
        fprintf(stream, "\n--- Orden final de procesamiento ---\n");
        for (int i = 0; i < order_count; ++i) {
            fprintf(stream, "%s%d", (i == 0) ? "" : " -> ", summary->completion_order[i]);
        }
        fprintf(stream, "\n");
    }

    if (summary->sample_product_count > 0) {
        fprintf(stream, "\n--- Productos destacados (primeros %d) ---\n", summary->sample_product_count);
        for (int i = 0; i < summary->sample_product_count; ++i) {
            const product_summary_t *product = &summary->sample_products[i];
            fprintf(stream, "Producto %d\n", product->product_id);
            fprintf(stream, "  Llegada: %d ms\n", product->arrival_time_ms);
            fprintf(stream, "  Turnaround: %d ms\n", product->turnaround_time_ms);
            fprintf(stream, "  Espera total: %d ms\n", product->total_wait_time_ms);
            fprintf(stream, "  Finalización: %d ms\n", product->completion_time_ms);
            for (int station_id = 0; station_id < METRICS_STATION_COUNT; ++station_id) {
                const product_station_summary_t *ps = &product->stations[station_id];
                if (ps->process_time_ms == 0 && ps->wait_time_ms == 0 && ps->preemptions == 0) {
                    continue;
                }
                const station_metrics_t *station = &summary->stations[station_id];
                const char *station_name = station->name[0] ? station->name : "-";
                fprintf(stream,
                        "    Estación %d (%s): %d ms procesado, %d ms espera, %d preempciones\n",
                        station_id, station_name, ps->process_time_ms, ps->wait_time_ms, ps->preemptions);
                fprintf(stream,
                        "      Entrada: %d ms | Salida: %d ms\n",
                        ps->entry_time_ms, ps->exit_time_ms);
            }
        }
    }
    fprintf(stream, "=========================================\n\n");
    fflush(stream);
}

void metrics_print_captured_summary(const metrics_summary_t *summary) {
    metrics_write_summary_stream(summary, stdout);
}

void metrics_write_captured_summary(const metrics_summary_t *summary, FILE *stream) {
    metrics_write_summary_stream(summary, stream);
}

// =============================================
// FUNCIONES AUXILIARES
// =============================================

void metrics_get_timestamp(struct timespec *ts) {
    clock_gettime(CLOCK_MONOTONIC, ts);
}

long metrics_time_diff_ms(const struct timespec *start, const struct timespec *end) {
    long seconds = end->tv_sec - start->tv_sec;
    long nanoseconds = end->tv_nsec - start->tv_nsec;
    return (seconds * 1000) + (nanoseconds / 1000000);
}

void metrics_get_system_start_time(struct timespec *out_start) {
    if (!out_start) {
        return;
    }

    if (!g_metrics_initialized) {
        out_start->tv_sec = 0;
        out_start->tv_nsec = 0;
        return;
    }

    METRICS_LOCK();
    *out_start = g_metrics.system_start_time;
    METRICS_UNLOCK();
}

void metrics_reset_all(void) {
    if (!g_metrics_initialized) return;
    
    METRICS_LOCK();
    
    // Reiniciar contadores globales
    g_metrics.total_products_created = 0;
    g_metrics.total_products_completed = 0;
    g_metrics.total_products_failed = 0;
    
    // Reiniciar métricas de estaciones
    for (int i = 0; i < METRICS_STATION_COUNT; i++) {
        init_station_metrics(&g_metrics.stations[i], i);
    }
    
    // Reiniciar métricas del scheduler
    memset(&g_metrics.scheduler, 0, sizeof(scheduler_metrics_t));
    
    // Reiniciar orden de finalización
    memset(g_metrics.completion_order, 0, sizeof(g_metrics.completion_order));
    g_metrics.completion_order_count = 0;

    // Vaciar buffer de eventos
    g_metrics.buffer_count = 0;
    
    // Actualizar tiempo de inicio
    metrics_get_timestamp(&g_metrics.system_start_time);
    
    METRICS_UNLOCK();
    
    printf("[METRICS] Sistema reiniciado\n");
}

int metrics_validate_system(void) {
    if (!g_metrics_initialized) {
        printf("[METRICS] Sistema no inicializado\n");
        return 0;
    }
    
    if (!g_metrics.event_buffer) {
        printf("[METRICS] Buffer de eventos inválido\n");
        return 0;
    }
    
    printf("[METRICS] Sistema válido\n");
    return 1;
}