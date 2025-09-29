#include <stdio.h>
#include <stdlib.h>
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

// Función para ejecutar simulación con un algoritmo específico
void run_simulation(scheduling_algorithm_t algorithm, int quantum_ms, int num_products) {
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
    
    // Estación 1: Corte (2 segundos)
    station_t *cutting = create_station(STATION_TYPE_CUTTING, "Corte", 2000);
    station_set_input_queue(cutting, queue_cutting);
    station_set_output_queue(cutting, queue_assembly);
    
    // Estación 2: Ensamblaje (3 segundos)
    station_t *assembly = create_station(STATION_TYPE_ASSEMBLY, "Ensamblaje", 3000);
    station_set_input_queue(assembly, queue_assembly);
    station_set_output_queue(assembly, queue_packaging);
    
    // Estación 3: Empaque (1 segundo)
    station_t *packaging = create_station(STATION_TYPE_PACKAGING, "Empaque", 1000);
    station_set_input_queue(packaging, queue_packaging);
    station_set_output_queue(packaging, queue_output);
    
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
    
    // Estadísticas del scheduler
    scheduler_print_stats(scheduler);
    
    // Estadísticas por estación
    printf("\n--- Estadísticas por Estación ---\n");
    station_print_stats(cutting);
    station_print_stats(assembly);
    station_print_stats(packaging);
    
    // Métricas globales
    printf("\n--- Métricas Globales del Sistema ---\n");
    metrics_print_summary();
    
    // Mostrar algunos productos como ejemplo
    printf("\n--- Métricas de Productos (primeros 3) ---\n");
    for (int i = 0; i < 3 && i < num_products; i++) {
        print_product_metrics(products[i]);
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
    int run_both = 1; // Por defecto ejecutar ambos algoritmos
    
    if (argc > 1) {
        num_products = atoi(argv[1]);
        if (num_products < 1 || num_products > 100) {
            printf("Número de productos debe estar entre 1 y 100\n");
            num_products = 10;
        }
    }
    
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
    if (run_both) {
        // Prueba 1: FCFS
        SYSTEM_INFO("Iniciando simulación con FCFS...");
        run_simulation(SCHED_FCFS, 0, num_products);
        
        // Pausa entre simulaciones
        printf("\nEsperando 3 segundos antes de la siguiente simulación...\n");
        sleep(3);
        
        // Resetear métricas
        metrics_reset_all();
        logger_reset_stats();
        
        // Prueba 2: Round Robin
        SYSTEM_INFO("Iniciando simulación con Round Robin...");
        run_simulation(SCHED_ROUND_ROBIN, 2000, num_products);
        
        // ========================================
        // Comparación de algoritmos
        // ========================================
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
    } else {
        // Ejecutar solo uno (por defecto FCFS)
        run_simulation(SCHED_FCFS, 0, num_products);
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