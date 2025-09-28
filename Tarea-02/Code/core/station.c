#include "station.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

station_t *create_station(const char *name, int process_time) {
    station_t *s = malloc(sizeof(station_t));
    strncpy(s->name, name, 50);
    s->process_time = process_time;
    s->next = NULL;
    return s;
}

void set_next_station(station_t *s, station_t *next) {
    s->next = next;
}

void process_product(station_t *s, product_t *p) {
    printf("Producto %d procesado en %s...\n", p->id, s->name);
    sleep(s->process_time); // Simulación del tiempo de trabajo

    if (s->next) {
        process_product(s->next, p);
    }
}

void free_station(station_t *s) {
    free(s);
}
