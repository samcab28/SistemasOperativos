#include <stdio.h>
#include <stdlib.h>
#include "patterns/factory.h"
#include "core/product.h"

int main() {
    printf("========================================\n");
    printf("  SIMULADOR DE LÍNEA DE ENSAMBLAJE\n");
    printf("  Fase 1: Test del Factory Pattern\n");
    printf("========================================\n\n");
    
    // 1. Crear el factory
    printf(">>> Paso 1: Creando Factory\n");
    product_factory_t *factory = create_product_factory();
    if (!factory) {
        printf("[ERROR] No se pudo crear el factory\n");
        return 1;
    }
    printf("\n");
    
    // 2. Crear productos individuales
    printf(">>> Paso 2: Creando productos individuales\n");
    product_t *p1 = factory_create_product(factory);
    product_t *p2 = factory_create_product(factory);
    product_t *p3 = factory_create_product(factory);
    printf("\n");
    
    // 3. Mostrar información de los productos
    printf(">>> Paso 3: Información de productos individuales\n");
    print_product_info(p1);
    print_product_info(p2);
    print_product_info(p3);
    
    // 4. Crear un batch de productos
    printf(">>> Paso 4: Creando batch de 7 productos\n");
    int batch_size = 7;
    product_t **batch = factory_create_batch(factory, batch_size);
    if (!batch) {
        printf("[ERROR] No se pudo crear el batch\n");
        free_product(p1);
        free_product(p2);
        free_product(p3);
        destroy_product_factory(factory);
        return 1;
    }
    printf("\n");
    
    // 5. Mostrar resumen de todos los productos
    printf(">>> Paso 5: Resumen de productos del batch\n");
    for (int i = 0; i < batch_size; i++) {
        printf("Producto %d - ID: %d, Estado: ", 
               i + 1, batch[i]->id);
        switch (batch[i]->state) {
            case STATE_CREATED: printf("CREATED\n"); break;
            case STATE_IN_QUEUE: printf("IN_QUEUE\n"); break;
            case STATE_PROCESSING: printf("PROCESSING\n"); break;
            case STATE_COMPLETED: printf("COMPLETED\n"); break;
        }
    }
    printf("\n");
    
    // 6. Probar cambios de estado
    printf(">>> Paso 6: Probando cambios de estado\n");
    set_product_state(p1, STATE_IN_QUEUE);
    set_product_state(p2, STATE_PROCESSING);
    set_product_state(p3, STATE_COMPLETED);
    printf("\n");
    
    // 7. Mostrar estadísticas del factory
    printf(">>> Paso 7: Estadísticas del Factory\n");
    print_factory_stats(factory);
    
    // 8. Simular métricas de una estación
    printf(">>> Paso 8: Simulando procesamiento en estación 0 (Corte)\n");
    record_station_entry(batch[0], 0);
    printf("Procesando...\n");
    // Simular trabajo (en un caso real, aquí iría el procesamiento)
    for (volatile int i = 0; i < 10000000; i++);
    record_station_exit(batch[0], 0);
    print_product_metrics(batch[0]);
    
    // 9. Limpieza
    printf(">>> Paso 9: Liberando memoria\n");
    
    // Liberar productos individuales
    free_product(p1);
    free_product(p2);
    free_product(p3);
    
    // Liberar batch
    for (int i = 0; i < batch_size; i++) {
        free_product(batch[i]);
    }
    free(batch);
    
    // Destruir factory
    destroy_product_factory(factory);
    
    printf("\n========================================\n");
    printf("  Test completado exitosamente!\n");
    printf("========================================\n");
    
    return 0;
}