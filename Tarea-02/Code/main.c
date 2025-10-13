#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "patterns/factory.h"
#include "core/product.h"
#include "core/station.h"
#include "core/scheduler.h"
#include "core/queue.h"
#include "metrics/metrics.h"
#include "metrics/logger.h"

// Función auxiliar para comparar algoritmos
void print_comparison_header(const char *algorithm) {
    printf("\n");
    printf("================================================\n");
    printf(" PRUEBA CON ALGORITMO: %s\n", algorithm);
    printf("================================================\n\n");
}

#define DEFAULT_TIME_CUTTING   2000
#define DEFAULT_TIME_ASSEMBLY  2500
#define DEFAULT_TIME_PACKAGING 4000
#define DEFAULT_RR_QUANTUM     500

#define NUM_STATIONS 3

static int parse_station_times(const char *value, int output[NUM_STATIONS]) {
    if (!value || !output) {
        return 0;
    }

    char buffer[128];
    strncpy(buffer, value, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char *cursor = buffer;
    for (int i = 0; i < NUM_STATIONS; ++i) {
        while (*cursor == ' ') {
            cursor++;
        }

        if (*cursor == '\0') {
            return 0;
        }

        char *endptr = NULL;
        long parsed = strtol(cursor, &endptr, 10);
        if (endptr == cursor || parsed <= 0) {
            return 0;
        }

        output[i] = (int)parsed;

        while (*endptr == ' ') {
            endptr++;
        }

        if (i < NUM_STATIONS - 1) {
            if (*endptr != ',') {
                return 0;
            }
            cursor = endptr + 1;
        } else if (*endptr != '\0') {
            return 0;
        }
    }

    return 1;
}

// Función para ejecutar simulación con un algoritmo específico
void run_simulation(scheduling_algorithm_t algorithm, int quantum_ms, int num_products,
                    const int station_times[NUM_STATIONS], int randomize_processing,
                    metrics_summary_t *out_summary) {
    // Imprimir encabezado
    const char *alg_name = (algorithm == SCHED_FCFS) ? "FCFS" : "ROUND ROBIN";
    print_comparison_header(alg_name);

    // ========================================
    // PASO 1: Crear colas de comunicación (IPC)
    // ========================================
    SYSTEM_INFO("Creando colas de comunicación entre estaciones...");
    
    queue_t *queue_cutting = malloc(sizeof(queue_t));
    queue_t *queue_assembly = malloc(sizeof(queue_t));
    queue_t *queue_packaging = malloc(sizeof(queue_t));
    queue_t *queue_output = malloc(sizeof(queue_t));
    
    queue_init(queue_cutting);
    queue_init(queue_assembly);
    queue_init(queue_packaging);
    queue_init(queue_output);
    
    SYSTEM_INFO("Colas creadas exitosamente");

    // ========================================
    // PASO 2: Crear estaciones como hilos
    // ========================================
    SYSTEM_INFO("Creando estaciones de trabajo...");
    SYSTEM_INFO("Tiempos configurados - Corte: %d ms, Ensamblaje: %d ms, Empaque: %d ms (%s)",
                station_times[0], station_times[1], station_times[2],
                randomize_processing ? "modo aleatorio" : "modo determinista");
    
    // Estación 1: Corte
    station_t *cutting = create_station(STATION_TYPE_CUTTING, "Corte", station_times[0]);
    station_set_input_queue(cutting, queue_cutting);
    station_set_output_queue(cutting, queue_assembly);
    
    // Estación 2: Ensamblaje
    station_t *assembly = create_station(STATION_TYPE_ASSEMBLY, "Ensamblaje", station_times[1]);
    station_set_input_queue(assembly, queue_assembly);
    station_set_output_queue(assembly, queue_packaging);
    
    // Estación 3: Empaque
    station_t *packaging = create_station(STATION_TYPE_PACKAGING, "Empaque", station_times[2]);
    station_set_input_queue(packaging, queue_packaging);
    station_set_output_queue(packaging, queue_output);

    // Configurar variaciones
    if (randomize_processing) {
        station_set_processing_variance(cutting, station_times[0] / 4);
        station_set_processing_variance(assembly, station_times[1] / 4);
        station_set_processing_variance(packaging, station_times[2] / 4);
    } else {
        station_set_processing_variance(cutting, 0);
        station_set_processing_variance(assembly, 0);
        station_set_processing_variance(packaging, 0);
    }
    
    // Configurar Chain of Responsibility
    station_set_next(cutting, assembly);
    station_set_next(assembly, packaging);
    
    SYSTEM_INFO("Estaciones configuradas");

    // ========================================
    // PASO 3: Iniciar hilos de estaciones
    // ========================================
    SYSTEM_INFO("Iniciando hilos de estaciones...");
    
    if (!station_start_thread(cutting)) {
        SYSTEM_ERROR("Fallo al iniciar hilo de Corte");
        return;
    }
    if (!station_start_thread(assembly)) {
        SYSTEM_ERROR("Fallo al iniciar hilo de Ensamblaje");
        return;
    }
    if (!station_start_thread(packaging)) {
        SYSTEM_ERROR("Fallo al iniciar hilo de Empaque");
        return;
    }
    
    SYSTEM_INFO("Todos los hilos de estaciones iniciados");

    // ========================================
    // PASO 4: Crear scheduler
    // ========================================
    SYSTEM_INFO("Creando scheduler...");
    
    scheduler_t *scheduler = create_scheduler(algorithm, quantum_ms);
    if (!scheduler) {
        SYSTEM_ERROR("Fallo al crear scheduler");
        return;
    }
    
    scheduler_set_first_station(scheduler, cutting);
    scheduler_print_config(scheduler);

    // ========================================
    // PASO 5: Crear productos con factory
    // ========================================
    SYSTEM_INFO("Creando %d productos...", num_products);
    
    product_factory_t *factory = create_product_factory();
    if (!factory) {
        SYSTEM_ERROR("Fallo al crear factory");
        return;
    }
    
    product_t **products = factory_create_batch(factory, num_products);
    if (!products) {
        SYSTEM_ERROR("Fallo al crear batch de productos");
        return;
    }
    
    FACTORY_INFO("Batch de %d productos creado", num_products);
    print_factory_stats(factory);

    // ========================================
    // PASO 6: Agregar productos al scheduler
    // ========================================
    SYSTEM_INFO("Agregando productos al scheduler...");
    
    for (int i = 0; i < num_products; i++) {
        scheduler_add_product(scheduler, products[i]);
        metrics_product_created(products[i]->id);
    }
    
    SCHEDULER_INFO("%d productos agregados a la cola de listos", num_products);

    // ========================================
    // PASO 7: Iniciar scheduler
    // ========================================
    SYSTEM_INFO("Iniciando scheduler...");
    
    if (!scheduler_start_thread(scheduler)) {
        SYSTEM_ERROR("Fallo al iniciar scheduler");
        return;
    }
    
    scheduler_print_info(scheduler);

    // ========================================
    // PASO 8: Monitorear progreso
    // ========================================
    SYSTEM_INFO("Procesamiento en curso...");
    
    int last_completed = 0;
    while (1) {
        sleep(2); // Verificar cada 2 segundos
        
        // Contar productos completados en cola de salida
        pthread_mutex_lock(&queue_output->lock);
        int completed = 0;
        node_t *current = queue_output->head;
        while (current) {
            completed++;
            current = current->next;
        }
        pthread_mutex_unlock(&queue_output->lock);
        
        // Mostrar progreso si cambió
        if (completed != last_completed) {
            SYSTEM_INFO("Progreso: %d/%d productos completados", completed, num_products);
            last_completed = completed;
        }
        
        // Verificar si todos los productos terminaron
        if (completed >= num_products) {
            SYSTEM_INFO("Todos los productos completados!");
            break;
        }
        
        // Timeout de seguridad (30 segundos por producto)
        static int timeout_counter = 0;
        timeout_counter++;
        if (timeout_counter > num_products * 15) {
            SYSTEM_ERROR("Timeout alcanzado, deteniendo simulación");
            break;
        }
    }

    // ========================================
    // PASO 9: Detener hilos
    // ========================================
    SYSTEM_INFO("Deteniendo hilos del sistema...");
    
    // Detener scheduler primero
    scheduler_stop_thread(scheduler);
    
    // Detener estaciones
    station_stop_thread(cutting);
    station_stop_thread(assembly);
    station_stop_thread(packaging);
    
    SYSTEM_INFO("Todos los hilos detenidos");

    // ========================================
    // PASO 10: Mostrar estadísticas
    // ========================================
    printf("\n");
    printf("=========================================\n");
    printf(" ESTADÍSTICAS DE LA SIMULACIÓN\n");
    printf("=========================================\n");
    
    if (out_summary) {
        metrics_capture_summary(out_summary);
        strncpy(out_summary->algorithm, alg_name, sizeof(out_summary->algorithm) - 1);
        out_summary->algorithm[sizeof(out_summary->algorithm) - 1] = '\0';
        out_summary->num_products = num_products;
        out_summary->randomize_processing = randomize_processing;
        int sample_count = num_products < 3 ? num_products : 3;
        out_summary->sample_product_count = sample_count;
        struct timespec system_start;
        metrics_get_system_start_time(&system_start);
        for (int i = 0; i < sample_count; ++i) {
            const product_t *product = products[i];
            product_summary_t *snapshot = &out_summary->sample_products[i];
            snapshot->product_id = product ? product->id : -1;
            if (!product || !product->metrics) {
                continue;
            }

            const product_metrics_t *metrics = product->metrics;
            snapshot->arrival_time_ms = (int)metrics_time_diff_ms(&system_start, &metrics->creation_time);
            if (snapshot->arrival_time_ms < 0) {
                snapshot->arrival_time_ms = 0;
            }
            if (metrics->completion_time.tv_sec != 0 || metrics->completion_time.tv_nsec != 0) {
                snapshot->completion_time_ms =
                    (int)metrics_time_diff_ms(&system_start, &metrics->completion_time);
            } else {
                snapshot->completion_time_ms = snapshot->arrival_time_ms + metrics->turnaround_time_ms;
            }
            if (snapshot->completion_time_ms < 0) {
                snapshot->completion_time_ms = snapshot->arrival_time_ms + metrics->turnaround_time_ms;
            }
            snapshot->turnaround_time_ms = metrics->turnaround_time_ms;
            snapshot->total_wait_time_ms = metrics->total_wait_time_ms;
            for (int station_id = 0; station_id < METRICS_STATION_COUNT; ++station_id) {
                snapshot->stations[station_id].process_time_ms =
                    metrics->station_metrics[station_id].process_time_ms;
                snapshot->stations[station_id].wait_time_ms =
                    metrics->station_metrics[station_id].wait_time_ms;
                snapshot->stations[station_id].preemptions =
                    metrics->station_metrics[station_id].preemptions;
                const struct timespec *entry_ts = &metrics->station_metrics[station_id].entry_time;
                const struct timespec *exit_ts = &metrics->station_metrics[station_id].exit_time;
                if (entry_ts->tv_sec != 0 || entry_ts->tv_nsec != 0) {
                    snapshot->stations[station_id].entry_time_ms =
                        (int)metrics_time_diff_ms(&system_start, entry_ts);
                    if (snapshot->stations[station_id].entry_time_ms < 0) {
                        snapshot->stations[station_id].entry_time_ms = 0;
                    }
                }
                if (exit_ts->tv_sec != 0 || exit_ts->tv_nsec != 0) {
                    snapshot->stations[station_id].exit_time_ms =
                        (int)metrics_time_diff_ms(&system_start, exit_ts);
                    if (snapshot->stations[station_id].exit_time_ms < 0) {
                        snapshot->stations[station_id].exit_time_ms = 0;
                    }
                }
            }
        }
    } else {
        scheduler_print_stats(scheduler);
        printf("\n--- Estadísticas por Estación ---\n");
        station_print_stats(cutting);
        station_print_stats(assembly);
        station_print_stats(packaging);
        printf("\n--- Métricas Globales del Sistema ---\n");
        metrics_print_summary();
        printf("\n--- Métricas de Productos (primeros 3) ---\n");
        for (int i = 0; i < 3 && i < num_products; i++) {
            print_product_metrics(products[i]);
        }
    }

    // ========================================
    // PASO 11: Limpieza de memoria
    // ========================================
    SYSTEM_INFO("Liberando recursos...");
    
    // Liberar productos (están en queue_output)
    pthread_mutex_lock(&queue_output->lock);
    node_t *node = queue_output->head;
    while (node) {
        free_product(node->prod);
        node_t *temp = node;
        node = node->next;
        free(temp);
    }
    queue_output->head = queue_output->tail = NULL;
    pthread_mutex_unlock(&queue_output->lock);
    
    free(products); // Solo el array, los productos ya se liberaron
    
    // Destruir estaciones
    destroy_station(cutting);
    destroy_station(assembly);
    destroy_station(packaging);
    
    // Destruir colas
    queue_destroy(queue_cutting);
    queue_destroy(queue_assembly);
    queue_destroy(queue_packaging);
    queue_destroy(queue_output);
    free(queue_cutting);
    free(queue_assembly);
    free(queue_packaging);
    free(queue_output);
    
    // Destruir scheduler
    destroy_scheduler(scheduler);
    
    // Destruir factory
    destroy_product_factory(factory);
    
    SYSTEM_INFO("Recursos liberados");
    
    printf("\n");
    printf("================================================\n");
    printf(" SIMULACIÓN COMPLETADA: %s\n", alg_name);
    printf("================================================\n\n");
}

int main(int argc, char *argv[]) {
    printf("\n");
    printf("========================================\n");
    printf(" SIMULADOR DE LÍNEA DE ENSAMBLAJE\n");
    printf(" Sistema Completo con Concurrencia\n");
    printf("========================================\n\n");
    
    // Procesar argumentos de línea de comandos
    int num_products = 10;
    int randomize_processing = 0;
    int run_fcfs = 1;
    int run_rr = 1;
    int rr_quantum_ms = DEFAULT_RR_QUANTUM;
    int station_times[NUM_STATIONS] = {
        DEFAULT_TIME_CUTTING,
        DEFAULT_TIME_ASSEMBLY,
        DEFAULT_TIME_PACKAGING
    };

    metrics_summary_t summary_fcfs;
    metrics_summary_t summary_rr;
    int summary_fcfs_valid = 0;
    int summary_rr_valid = 0;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];

        if (strcmp(arg, "--random") == 0 || strcmp(arg, "--randomize") == 0) {
            randomize_processing = 1;
            continue;
        }

        if (strcmp(arg, "--deterministic") == 0) {
            randomize_processing = 0;
            continue;
        }

        if (strncmp(arg, "--algorithm=", 12) == 0) {
            char buffer[64];
            strncpy(buffer, arg + 12, sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0';

            run_fcfs = 0;
            run_rr = 0;

            char *token = strtok(buffer, ",");
            while (token) {
                while (*token == ' ') token++;
                size_t token_len = strlen(token);
                while (token_len > 0 && token[token_len - 1] == ' ') {
                    token[token_len - 1] = '\0';
                    token_len--;
                }

                if (token_len == 0) {
                    printf("Algoritmo vacío en --algorithm\n");
                    return 1;
                }

                for (char *p = token; *p; ++p) {
                    *p = (char)tolower((unsigned char)*p);
                }

                if (strcmp(token, "fcfs") == 0) {
                    run_fcfs = 1;
                } else if (strcmp(token, "rr") == 0 || strcmp(token, "roundrobin") == 0 || strcmp(token, "round-robin") == 0) {
                    run_rr = 1;
                } else if (strcmp(token, "both") == 0) {
                    run_fcfs = 1;
                    run_rr = 1;
                } else if (strcmp(token, "none") == 0) {
                    // Ignorar 'none' explícito, siempre que otro token habilite algo
                } else {
                    printf("Algoritmo desconocido en --algorithm: %s\n", token);
                    return 1;
                }

                token = strtok(NULL, ",");
            }

            if (!run_fcfs && !run_rr) {
                printf("Debe seleccionar al menos un algoritmo en --algorithm (fcfs, rr, ambos)\n");
                return 1;
            }
            continue;
        }

        if (strncmp(arg, "--times=", 8) == 0) {
            int parsed[NUM_STATIONS];
            if (!parse_station_times(arg + 8, parsed)) {
                printf("Formato inválido para --times. Use --times=t1,t2,t3\n");
                return 1;
            }
            for (int j = 0; j < NUM_STATIONS; ++j) {
                station_times[j] = parsed[j];
            }
            continue;
        }

        if (strncmp(arg, "--quantum=", 10) == 0) {
            char *endptr = NULL;
            long parsed_quantum = strtol(arg + 10, &endptr, 10);
            if (*endptr != '\0' || parsed_quantum <= 0) {
                printf("Formato inválido para --quantum. Use --quantum=valor_ms (>0)\n");
                return 1;
            }
            rr_quantum_ms = (int)parsed_quantum;
            continue;
        }

        if (strncmp(arg, "--products=", 11) == 0) {
            char *endptr = NULL;
            long parsed_products = strtol(arg + 11, &endptr, 10);
            if (*endptr != '\0' || parsed_products < 1 || parsed_products > 100) {
                printf("Formato inválido para --products. Use --products=valor (1-100)\n");
                return 1;
            }
            num_products = (int)parsed_products;
            continue;
        }

        char *endptr = NULL;
        long value = strtol(arg, &endptr, 10);
        if (*endptr == '\0') {
            if (value < 1 || value > 100) {
                printf("Número de productos debe estar entre 1 y 100\n");
                return 1;
            }
            num_products = (int)value;
            continue;
        }

        printf("Argumento desconocido: %s\n", arg);
        return 1;
    }
    
    if (!run_fcfs && !run_rr) {
        SYSTEM_INFO("No se seleccionó algoritmo; se ejecutarán FCFS y Round Robin por defecto");
        run_fcfs = 1;
        run_rr = 1;
    }

    SYSTEM_INFO("Configuración solicitada: productos=%d, tiempos=%d/%d/%d ms, modo=%s",
                num_products,
                station_times[0], station_times[1], station_times[2],
                randomize_processing ? "aleatorio" : "determinista");
    SYSTEM_INFO("Quantum Round Robin: %d ms", rr_quantum_ms);

    char algorithm_summary[32] = "";
    if (run_fcfs) {
        strncat(algorithm_summary, "FCFS", sizeof(algorithm_summary) - strlen(algorithm_summary) - 1);
    }
    if (run_rr) {
        if (strlen(algorithm_summary) > 0) {
            strncat(algorithm_summary, ", ", sizeof(algorithm_summary) - strlen(algorithm_summary) - 1);
        }
        strncat(algorithm_summary, "Round Robin", sizeof(algorithm_summary) - strlen(algorithm_summary) - 1);
    }
    SYSTEM_INFO("Algoritmos seleccionados: %s", algorithm_summary);

    // ========================================
    // Inicializar subsistemas
    // ========================================
    SYSTEM_INFO("Inicializando subsistemas...");
    
    // Inicializar logger
    if (!logger_init_with_file(LOG_INFO, "simulador.log")) {
        SYSTEM_ERROR("Fallo al abrir archivo de log, usando solo consola");
        logger_init(LOG_INFO, LOG_DEST_CONSOLE);
    }
    logger_set_show_timestamps(1);
    SYSTEM_INFO("Logger inicializado");
    
    // Inicializar métricas
    if (!metrics_init()) {
        SYSTEM_ERROR("Fallo al inicializar métricas");
        logger_cleanup();
        return 1;
    }
    SYSTEM_INFO("Sistema de métricas inicializado");
    
    // Resetear métricas antes de cada prueba
    metrics_reset_all();
    
    // ========================================
    // Ejecutar simulaciones
    // ========================================
    int executed_fcfs = 0;
    int executed_rr = 0;

    if (run_fcfs) {
        SYSTEM_INFO("Iniciando simulación con FCFS...");
        run_simulation(SCHED_FCFS, 0, num_products, station_times, randomize_processing,
                       &summary_fcfs);
        executed_fcfs = 1;
        summary_fcfs_valid = 1;
    }

    if (run_rr) {
        if (executed_fcfs) {
            printf("\nEsperando 3 segundos antes de la siguiente simulación...\n");
            sleep(3);
            metrics_reset_all();
            logger_reset_stats();
        }
        SYSTEM_INFO("Iniciando simulación con Round Robin...");
        run_simulation(SCHED_ROUND_ROBIN, rr_quantum_ms, num_products, station_times,
                       randomize_processing, &summary_rr);
        executed_rr = 1;
        summary_rr_valid = 1;
    }

    FILE *results_file = fopen("results.log", "a");
    if (!results_file) {
        SYSTEM_ERROR("No se pudo abrir 'results.log' para escritura. Resultados solo en consola.");
    }

    if (summary_fcfs_valid) {
        metrics_print_captured_summary(&summary_fcfs);
        metrics_write_captured_summary(&summary_fcfs, results_file);
    }

    if (summary_rr_valid) {
        metrics_print_captured_summary(&summary_rr);
        metrics_write_captured_summary(&summary_rr, results_file);
    }

    if (summary_fcfs_valid && summary_rr_valid) {
        printf("\n");
        printf("================================================\n");
        printf(" COMPARACIÓN DE ALGORITMOS\n");
        printf("================================================\n");
        printf("\nPara comparar resultados detallados, revise:\n");
        printf(" - Las estadísticas mostradas arriba\n");
        printf(" - El archivo 'simulador.log'\n");
        printf(" - Los tiempos de turnaround y espera\n");
        printf("\nObservaciones esperadas:\n");
        printf(" FCFS:\n");
        printf(" - Sin context switches\n");
        printf(" - Sin preempciones\n");
        printf(" - Orden estricto de llegada\n");
        printf(" - Puede tener mayor variación en tiempos de espera\n");
        printf("\n");
        printf(" Round Robin:\n");
        printf(" - Múltiples context switches\n");
        printf(" - Preempciones cuando expira quantum\n");
        printf(" - Más equitativo en tiempo de CPU\n");
        printf(" - Overhead adicional por cambios de contexto\n");
        printf("\n");

        if (results_file) {
            fprintf(results_file, "\n================================================\n");
            fprintf(results_file, " COMPARACIÓN DE ALGORITMOS\n");
            fprintf(results_file, "================================================\n");
            fprintf(results_file, "\nPara comparar resultados detallados, revise:\n");
            fprintf(results_file, " - Las estadísticas mostradas arriba\n");
            fprintf(results_file, " - El archivo 'simulador.log'\n");
            fprintf(results_file, " - Los tiempos de turnaround y espera\n");
            fprintf(results_file, "\nObservaciones esperadas:\n");
            fprintf(results_file, " FCFS:\n");
            fprintf(results_file, " - Sin context switches\n");
            fprintf(results_file, " - Sin preempciones\n");
            fprintf(results_file, " - Orden estricto de llegada\n");
            fprintf(results_file, " - Puede tener mayor variación en tiempos de espera\n");
            fprintf(results_file, "\n");
            fprintf(results_file, " Round Robin:\n");
            fprintf(results_file, " - Múltiples context switches\n");
            fprintf(results_file, " - Preempciones cuando expira quantum\n");
            fprintf(results_file, " - Más equitativo en tiempo de CPU\n");
            fprintf(results_file, " - Overhead adicional por cambios de contexto\n");
            fprintf(results_file, "\n");
            fflush(results_file);
        }
    }

    if (results_file) {
        fclose(results_file);
    }
    
    // ========================================
    // Finalizar subsistemas
    // ========================================
    SYSTEM_INFO("Finalizando subsistemas...");
    
    // Mostrar estadísticas finales del logger
    logger_print_stats();
    
    // Finalizar métricas
    metrics_cleanup();
    
    // Finalizar logger (debe ser último)
    logger_cleanup();
    
    // ========================================
    // Fin del programa
    // ========================================
    printf("\n");
    printf("========================================\n");
    printf(" SIMULACIÓN COMPLETA\n");
    printf(" Revise 'simulador.log' para detalles\n");
    printf("========================================\n\n");
    
    return 0;
}