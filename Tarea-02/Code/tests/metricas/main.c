#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "patterns/factory.h"
#include "core/product.h"
#include "metrics/metrics.h"
#include "metrics/logger.h"

// Función auxiliar para simular procesamiento
void simulate_processing(int milliseconds) {
    usleep(milliseconds * 1000);
}

// Simular procesamiento de un producto en una estación
void simulate_station_processing(product_t *product, int station_id, int process_time_ms) {
    const char* station_names[] = {"Corte", "Ensamblaje", "Empaque"};
    
    STATION_INFO("Iniciando procesamiento en estación %s", station_names[station_id]);
    
    // Registrar entrada
    record_station_entry(product, station_id);
    metrics_station_start_processing(station_id, product->id);
    set_product_state(product, STATE_PROCESSING);
    
    STATION_DEBUG("Producto %d en procesamiento...", product->id);
    
    // Simular trabajo
    simulate_processing(process_time_ms);
    
    // Registrar salida
    record_station_exit(product, station_id);
    metrics_station_end_processing(station_id, product->id);
    
    STATION_INFO("Producto %d completado en %s", product->id, station_names[station_id]);
}

// Procesar producto por toda la línea de ensamblaje
void process_product_through_line(product_t *product) {
    PRODUCT_INFO("=== Procesando Producto %d ===", product->id);
    
    // Estación 1: Corte (2 segundos)
    simulate_station_processing(product, 0, 200);
    
    // Estación 2: Ensamblaje (3 segundos)
    simulate_station_processing(product, 1, 300);
    
    // Estación 3: Empaque (1 segundo)
    simulate_station_processing(product, 2, 100);
    
    // Marcar como completado
    set_product_state(product, STATE_COMPLETED);
    metrics_product_completed(product->id);
    
    PRODUCT_INFO("Producto %d COMPLETADO exitosamente", product->id);
}

int main() {
    printf("\n");
    printf("========================================\n");
    printf("  SIMULADOR DE LÍNEA DE ENSAMBLAJE\n");
    printf("  Fase 1: Sistema Completo de Métricas\n");
    printf("========================================\n\n");
    
    // ========================================
    // PASO 1: Inicializar subsistemas
    // ========================================
    SYSTEM_INFO("Inicializando subsistemas...");
    
    // Inicializar logger (nivel DEBUG, consola y archivo)
    if (!logger_init_with_file(LOG_DEBUG, "simulador.log")) {
        SYSTEM_ERROR("Fallo al inicializar logger con archivo");
        logger_init(LOG_DEBUG, LOG_DEST_CONSOLE);
    }
    
    logger_set_show_timestamps(1);
    logger_print_config();
    
    // Inicializar sistema de métricas
    if (!metrics_init()) {
        SYSTEM_ERROR("Fallo al inicializar sistema de métricas");
        logger_cleanup();
        return 1;
    }
    
    SYSTEM_INFO("Subsistemas inicializados correctamente");
    
    // ========================================
    // PASO 2: Crear Factory
    // ========================================
    SYSTEM_INFO("Creando factory de productos...");
    
    product_factory_t *factory = create_product_factory();
    if (!factory) {
        SYSTEM_ERROR("Fallo al crear factory");
        metrics_cleanup();
        logger_cleanup();
        return 1;
    }
    
    FACTORY_INFO("Factory creado exitosamente");
    
    // ========================================
    // PASO 3: Crear productos individuales
    // ========================================
    SYSTEM_INFO("Creando productos individuales...");
    
    product_t *p1 = factory_create_product(factory);
    product_t *p2 = factory_create_product(factory);
    product_t *p3 = factory_create_product(factory);
    
    if (!p1 || !p2 || !p3) {
        FACTORY_ERROR("Fallo al crear productos individuales");
        destroy_product_factory(factory);
        metrics_cleanup();
        logger_cleanup();
        return 1;
    }
    
    // Registrar en métricas
    metrics_product_created(p1->id);
    metrics_product_created(p2->id);
    metrics_product_created(p3->id);
    
    PRODUCT_INFO("3 productos individuales creados");
    
    // ========================================
    // PASO 4: Crear batch de productos
    // ========================================
    SYSTEM_INFO("Creando batch de productos...");
    
    int batch_size = 7;
    product_t **batch = factory_create_batch(factory, batch_size);
    
    if (!batch) {
        FACTORY_ERROR("Fallo al crear batch de productos");
        free_product(p1);
        free_product(p2);
        free_product(p3);
        destroy_product_factory(factory);
        metrics_cleanup();
        logger_cleanup();
        return 1;
    }
    
    // Registrar batch en métricas
    for (int i = 0; i < batch_size; i++) {
        metrics_product_created(batch[i]->id);
    }
    
    FACTORY_INFO("Batch de %d productos creado", batch_size);
    print_factory_stats(factory);
    
    // ========================================
    // PASO 5: Procesar productos individuales
    // ========================================
    SYSTEM_INFO("Procesando productos individuales por la línea...");
    
    process_product_through_line(p1);
    process_product_through_line(p2);
    process_product_through_line(p3);
    
    SYSTEM_INFO("Productos individuales procesados");
    
    // ========================================
    // PASO 6: Procesar batch de productos
    // ========================================
    SYSTEM_INFO("Procesando batch por la línea...");
    
    for (int i = 0; i < batch_size; i++) {
        process_product_through_line(batch[i]);
    }
    
    SYSTEM_INFO("Batch procesado completamente");
    
    // ========================================
    // PASO 7: Mostrar métricas de productos
    // ========================================
    SYSTEM_INFO("Mostrando métricas individuales de productos...");
    
    printf("\n--- Métricas de Productos Individuales ---\n");
    print_product_metrics(p1);
    print_product_metrics(p2);
    print_product_metrics(p3);
    
    printf("\n--- Métricas de Productos del Batch ---\n");
    for (int i = 0; i < batch_size && i < 3; i++) {
        print_product_metrics(batch[i]);
    }
    SYSTEM_INFO("(Mostrando solo primeros 3 del batch)");
    
    // ========================================
    // PASO 8: Mostrar métricas por estación
    // ========================================
    SYSTEM_INFO("Mostrando métricas por estación...");
    
    printf("\n");
    metrics_print_station_stats(0);
    metrics_print_station_stats(1);
    metrics_print_station_stats(2);
    
    // ========================================
    // PASO 9: Mostrar resumen global
    // ========================================
    SYSTEM_INFO("Generando resumen global del sistema...");
    
    metrics_print_summary();
    
    // ========================================
    // PASO 10: Mostrar estadísticas del logger
    // ========================================
    SYSTEM_INFO("Estadísticas del sistema de logging...");
    
    logger_print_stats();
    
    // ========================================
    // PASO 11: Pruebas de diferentes niveles
    // ========================================
    SYSTEM_INFO("Probando diferentes niveles de logging...");
    
    DLOG("Este es un mensaje DEBUG");
    ILOG("Este es un mensaje INFO");
    WLOG("Este es un mensaje WARNING");
    ELOG("Este es un mensaje ERROR");
    CLOG("Este es un mensaje CRITICAL");
    
    // ========================================
    // PASO 12: Prueba de logging con ubicación
    // ========================================
    SYSTEM_INFO("Probando logging con ubicación de código...");
    
    LOG_DEBUG_FUNC("TEST", "Debug con ubicación de código");
    LOG_ERROR_FUNC("TEST", "Error con ubicación de código");
    
    // ========================================
    // PASO 13: Prueba de eventos y métricas especializados
    // ========================================
    SYSTEM_INFO("Probando logging especializado...");
    
    logger_log_event("PRODUCT_CREATED", 999, -1);
    logger_log_state_change(999, "CREATED", "COMPLETED");
    logger_log_metric("Throughput", metrics_get_system_throughput(), "prod/seg");
    logger_log_metric("Utilización", metrics_get_system_utilization(), "%");
    
    // ========================================
    // PASO 14: Cambiar nivel de logging
    // ========================================
    SYSTEM_INFO("Cambiando nivel de logging a WARNING...");
    
    logger_set_level(LOG_WARNING);
    
    DLOG("Este DEBUG no se mostrará");
    ILOG("Este INFO no se mostrará");
    WLOG("Este WARNING sí se mostrará");
    
    // Restaurar nivel
    logger_set_level(LOG_DEBUG);
    SYSTEM_INFO("Nivel restaurado a DEBUG");
    
    // ========================================
    // PASO 15: Limpieza de memoria
    // ========================================
    SYSTEM_INFO("Iniciando limpieza de memoria...");
    
    // Liberar productos individuales
    free_product(p1);
    free_product(p2);
    free_product(p3);
    PRODUCT_DEBUG("3 productos individuales liberados");
    
    // Liberar batch
    for (int i = 0; i < batch_size; i++) {
        free_product(batch[i]);
    }
    free(batch);
    PRODUCT_DEBUG("Batch de %d productos liberado", batch_size);
    
    // Destruir factory
    destroy_product_factory(factory);
    FACTORY_DEBUG("Factory destruido");
    
    // ========================================
    // PASO 16: Finalizar subsistemas
    // ========================================
    SYSTEM_INFO("Finalizando subsistemas...");
    
    // Mostrar estadísticas finales antes de cerrar
    printf("\n=== ESTADÍSTICAS FINALES ===\n");
    logger_print_stats();
    
    // Finalizar métricas
    metrics_cleanup();
    SYSTEM_INFO("Sistema de métricas finalizado");
    
    // Finalizar logger (esto debe ser lo último)
    SYSTEM_INFO("Sistema de logging finalizado");
    logger_cleanup();
    
    // ========================================
    // FIN
    // ========================================
    printf("\n========================================\n");
    printf("  Prueba completada exitosamente!\n");
    printf("  Revisa el archivo 'simulador.log'\n");
    printf("========================================\n\n");
    
    return 0;
}