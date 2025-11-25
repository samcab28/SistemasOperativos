/**
 * block_manager.h
 * Módulo de gestión de bloques de memoria
 */

#ifndef BLOCK_MANAGER_H
#define BLOCK_MANAGER_H

#include "filesystem.h"

/* Inicializa el sistema de bloques */
void block_init(block_system_t *bs);

/* Asigna bloques contiguos si es posible, o dispersos */
fs_error_t block_allocate(block_system_t *bs, uint32_t count, uint32_t *blocks);

/* Libera bloques previamente asignados */
void block_free(block_system_t *bs, const uint32_t *blocks, uint32_t count);

/* Escribe datos en bloques específicos */
fs_error_t block_write(block_system_t *bs, const uint32_t *blocks, 
                       uint32_t block_count, uint32_t offset, 
                       const char *data, uint32_t data_size);

/* Lee datos de bloques específicos */
fs_error_t block_read(const block_system_t *bs, const uint32_t *blocks, 
                      uint32_t block_count, uint32_t offset, 
                      uint32_t size, char *buffer);

/* Calcula cuántos bloques se necesitan para un tamaño dado */
uint32_t block_calculate_needed(uint32_t size);

#endif /* BLOCK_MANAGER_H */