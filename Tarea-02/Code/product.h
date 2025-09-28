#ifndef PRODUCT_H
#define PRODUCT_H

typedef struct {
    int id;
    int arrival_time;
} product_t;

product_t *create_product(int id);
void free_product(product_t *p);

#endif
