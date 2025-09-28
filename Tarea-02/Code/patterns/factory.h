#ifndef FACTORY_H
#define FACTORY_H

#include "../core/product.h"

// Factory para crear productos
typedef struct product_factory {
    int next_id;
    int total_created;
} product_factory_t;

// Crear el factory
product_factory_t *create_product_factory(void);

// Crear un producto básico
product_t *factory_create_product(product_factory_t *factory);

// Crear múltiples productos
product_t **factory_create_batch(product_factory_t *factory, int count);

// Destruir factory
void destroy_product_factory(product_factory_t *factory);

// Imprimir estadísticas del factory
void print_factory_stats(const product_factory_t *factory);

#endif // FACTORY_H