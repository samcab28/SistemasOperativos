#include "factory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Crear el factory
product_factory_t *create_product_factory(void) {
    product_factory_t *factory = malloc(sizeof(product_factory_t));
    if (!factory) {
        printf("[ERROR] No se pudo crear el factory\n");
        return NULL;
    }
    
    factory->next_id = 0;
    factory->total_created = 0;
    
    printf("[FACTORY] Factory creado\n");
    return factory;
}

// Crear un producto (Factory Method Pattern)
product_t *factory_create_product(product_factory_t *factory) {
    if (!factory) {
        printf("[ERROR] Factory es NULL\n");
        return NULL;
    }
    
    // Asignar memoria para el producto
    product_t *product = malloc(sizeof(product_t));
    if (!product) {
        printf("[ERROR] No se pudo crear producto\n");
        return NULL;
    }
    
    // Inicializar el producto
    product->id = factory->next_id++;
    product->priority = 0;
    product->remaining_time = 10000;  // 10 segundos por defecto
    product->state = STATE_CREATED;
    product->current_station = NULL;
    
    // Crear métricas
    product->metrics = create_product_metrics(product->id);
    
    // Registrar tiempo de llegada
    clock_gettime(CLOCK_MONOTONIC, &product->arrival_time);
    
    // Actualizar contador del factory
    factory->total_created++;
    
    printf("[FACTORY] Producto %d creado\n", product->id);
    
    return product;
}

// Crear múltiples productos (batch)
product_t **factory_create_batch(product_factory_t *factory, int count) {
    if (!factory || count <= 0) {
        printf("[ERROR] Parámetros inválidos para crear batch\n");
        return NULL;
    }
    
    product_t **products = malloc(sizeof(product_t*) * count);
    if (!products) {
        printf("[ERROR] No se pudo asignar memoria para batch\n");
        return NULL;
    }
    
    printf("[FACTORY] Creando batch de %d productos...\n", count);
    
    for (int i = 0; i < count; i++) {
        products[i] = factory_create_product(factory);
        if (!products[i]) {
            // Si falla, liberar los ya creados
            printf("[ERROR] Fallo al crear producto %d del batch\n", i);
            for (int j = 0; j < i; j++) {
                free_product(products[j]);
            }
            free(products);
            return NULL;
        }
    }
    
    printf("[FACTORY] Batch de %d productos creado exitosamente\n", count);
    return products;
}

// Destruir factory
void destroy_product_factory(product_factory_t *factory) {
    if (factory) {
        printf("[FACTORY] Destruyendo factory (productos creados: %d)\n", 
               factory->total_created);
        free(factory);
    }
}

// Imprimir estadísticas del factory
void print_factory_stats(const product_factory_t *factory) {
    if (!factory) {
        printf("[ERROR] Factory es NULL\n");
        return;
    }
    
    printf("\n=== ESTADÍSTICAS DEL FACTORY ===\n");
    printf("Total de productos creados: %d\n", factory->total_created);
    printf("Próximo ID disponible: %d\n", factory->next_id);
    printf("================================\n\n");
}