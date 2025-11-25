/**
 * file_operations.h
 * Operaciones sobre archivos individuales
 */

#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

#include "filesystem.h"

/* Inicializa la tabla de archivos */
void file_table_init(file_table_t *ft);

/* Busca un archivo por nombre */
file_entry_t* file_find(file_table_t *ft, const char *filename);

/* Encuentra un slot libre en la tabla de archivos */
file_entry_t* file_find_free_slot(file_table_t *ft);

/* Crea una entrada de archivo en la tabla */
fs_error_t file_create_entry(file_table_t *ft, const char *filename, 
                             uint32_t size, const uint32_t *blocks, 
                             uint32_t block_count);

/* Elimina una entrada de archivo */
void file_remove_entry(file_table_t *ft, file_entry_t *file);

/* Valida los parámetros de entrada */
bool file_validate_name(const char *filename);
bool file_validate_size(uint32_t size);

#endif /* FILE_OPERATIONS_H */