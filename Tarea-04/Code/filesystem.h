/**
 * filesystem.h
 * Definiciones y estructuras principales del sistema de archivos simulado
 */

#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdint.h>
#include <stdbool.h>

/* Constantes del sistema */
#define BLOCK_SIZE 512                    /* Tamaño de cada bloque en bytes */
#define MAX_FILES 100                     /* Número máximo de archivos */
#define MAX_BLOCKS 2048                   /* Total de bloques (1MB / 512B) */
#define MAX_FILENAME 256                  /* Longitud máxima del nombre */
#define MAX_FILE_SIZE (MAX_BLOCKS * BLOCK_SIZE) /* 1MB máximo por archivo */

/* Códigos de error */
typedef enum {
    FS_SUCCESS = 0,
    FS_ERROR_FILE_EXISTS,
    FS_ERROR_FILE_NOT_FOUND,
    FS_ERROR_NO_SPACE,
    FS_ERROR_INVALID_SIZE,
    FS_ERROR_MAX_FILES,
    FS_ERROR_INVALID_OFFSET,
    FS_ERROR_READ_FAILURE,
    FS_ERROR_WRITE_FAILURE,
    FS_ERROR_INVALID_PARAMS
} fs_error_t;

/* Estructura que representa un archivo */
typedef struct {
    char name[MAX_FILENAME];              /* Nombre del archivo */
    uint32_t size;                        /* Tamaño en bytes */
    uint32_t block_count;                 /* Número de bloques asignados */
    uint32_t blocks[MAX_BLOCKS];          /* Índices de bloques asignados */
    bool in_use;                          /* Indica si el slot está ocupado */
} file_entry_t;

/* Tabla de archivos (directorio) */
typedef struct {
    file_entry_t files[MAX_FILES];        /* Array de archivos */
    uint32_t file_count;                  /* Contador de archivos activos */
} file_table_t;

/* Sistema de bloques */
typedef struct {
    uint8_t data[MAX_BLOCKS][BLOCK_SIZE]; /* Bloques de datos */
    bool allocated[MAX_BLOCKS];           /* Mapa de bloques libres/ocupados */
    uint32_t free_blocks;                 /* Contador de bloques libres */
} block_system_t;

/* Sistema de archivos completo */
typedef struct {
    file_table_t file_table;              /* Tabla de archivos */
    block_system_t block_system;          /* Sistema de bloques */
} filesystem_t;

/* Funciones principales del API */
fs_error_t fs_init(filesystem_t *fs);
fs_error_t fs_create(filesystem_t *fs, const char *filename, uint32_t size);
fs_error_t fs_write(filesystem_t *fs, const char *filename, uint32_t offset, 
                    const char *data, uint32_t data_size);
fs_error_t fs_read(filesystem_t *fs, const char *filename, uint32_t offset, 
                   uint32_t size, char *buffer);
fs_error_t fs_delete(filesystem_t *fs, const char *filename);
void fs_list(const filesystem_t *fs);
const char* fs_error_string(fs_error_t error);

#endif /* FILESYSTEM_H */