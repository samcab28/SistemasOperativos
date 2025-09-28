#include "product.h"
#include <stdlib.h>

product_t *create_product(int id) {
    product_t *p = malloc(sizeof(product_t));
    p->id = id;
    p->arrival_time = id; // simulado
    return p;
}

void free_product(product_t *p) {
    free(p);
}
