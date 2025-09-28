#include <stdio.h>
#include <stdlib.h>
#include "product.h"
#include "station.h"
#include "scheduler.h"
#include "metrics.h"

int main() {
    printf("=== Simulador de Línea de Ensamblaje ===\n");

    // Crear productos con Factory
    product_t *products[10];
    for (int i = 0; i < 10; i++) {
        products[i] = create_product(i);
    }

    // Crear estaciones con Chain of Responsibility
    station_t *corte = create_station("Corte", 2);
    station_t *ensamblaje = create_station("Ensamblaje", 3);
    station_t *empaque = create_station("Empaque", 1);

    set_next_station(corte, ensamblaje);
    set_next_station(ensamblaje, empaque);

    // Selección de algoritmo de scheduling
    sched_type_t policy = MY_SCHED_FCFS;
    scheduler_t *sched = create_scheduler(policy, 2); // quantum=2 para RR

    // Simulación mínima
    for (int i = 0; i < 10; i++) {
        add_to_scheduler(sched, products[i]);
    }

    run_scheduler(sched, corte);

    // Liberar memoria
    free_scheduler(sched);
    free_station(corte);
    free_station(ensamblaje);
    free_station(empaque);
    for (int i = 0; i < 10; i++) {
        free_product(products[i]);
    }

    return 0;
}
