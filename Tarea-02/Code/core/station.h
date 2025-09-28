#ifndef STATION_H
#define STATION_H

#include "product.h"

typedef struct station station_t;

struct station {
    char name[50];
    int process_time;
    station_t *next;
};

station_t *create_station(const char *name, int process_time);
void set_next_station(station_t *s, station_t *next);
void process_product(station_t *s, product_t *p);
void free_station(station_t *s);

#endif
