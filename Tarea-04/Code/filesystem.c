/**
 * filesystem.c
 * Implementación del sistema de archivos
 */

#include "filesystem.h"
#include "block_manager.h"
#include "file_operations.h"
#include <string.h>
#include <stdio.h>

const char* fs_error_string(fs_error_t error) {
    switch (error) {
        case FS_SUCCESS:            return "Operación exitosa";
        case FS_ERROR_FILE_EXISTS:  return "El archivo ya existe";
        case FS_ERROR_FILE_NOT_FOUND: return "Archivo no encontrado";
        case FS_ERROR_NO_SPACE:     return "No hay espacio suficiente";
        case FS_ERROR_INVALID_SIZE: return "Tamaño inválido";
        case FS_ERROR_MAX_FILES:    return "Número máximo de archivos alcanzado";
        case FS_ERROR_INVALID_OFFSET: return "Offset inválido";
        case FS_ERROR_READ_FAILURE: return "Error al leer";
        case FS_ERROR_WRITE_FAILURE: return "Error al escribir";
        case FS_ERROR_INVALID_PARAMS: return "Parámetros inválidos";
        default:                    return "Error desconocido";
    }
}

fs_error_t fs_init(filesystem_t *fs) {
    if (!fs) {
        return FS_ERROR_INVALID_PARAMS;
    }

    file_table_init(&fs->file_table);
    block_init(&fs->block_system);
    
    return FS_SUCCESS;
}

fs_error_t fs_create(filesystem_t *fs, const char *filename, uint32_t size) {
    if (!fs || !file_validate_name(filename) || !file_validate_size(size)) {
        return FS_ERROR_INVALID_PARAMS;
    }

    /* Calcular bloques necesarios */
    uint32_t blocks_needed = block_calculate_needed(size);
    uint32_t blocks[MAX_BLOCKS];

    /* Asignar bloques */
    fs_error_t result = block_allocate(&fs->block_system, blocks_needed, blocks);
    if (result != FS_SUCCESS) {
        return result;
    }

    /* Crear entrada en la tabla de archivos */
    result = file_create_entry(&fs->file_table, filename, size, blocks, blocks_needed);
    if (result != FS_SUCCESS) {
        /* Liberar bloques si falla la creación del archivo */
        block_free(&fs->block_system, blocks, blocks_needed);
        return result;
    }

    return FS_SUCCESS;
}

fs_error_t fs_write(filesystem_t *fs, const char *filename, uint32_t offset, 
                    const char *data, uint32_t data_size) {
    
    if (!fs || !file_validate_name(filename) || !data) {
        return FS_ERROR_INVALID_PARAMS;
    }

    /* Buscar archivo */
    file_entry_t *file = file_find(&fs->file_table, filename);
    if (!file) {
        return FS_ERROR_FILE_NOT_FOUND;
    }

    /* Validar límites */
    if (offset >= file->size || offset + data_size > file->size) {
        return FS_ERROR_INVALID_OFFSET;
    }

    /* Escribir datos en los bloques */
    return block_write(&fs->block_system, file->blocks, file->block_count, 
                      offset, data, data_size);
}

fs_error_t fs_read(filesystem_t *fs, const char *filename, uint32_t offset, 
                   uint32_t size, char *buffer) {
    
    if (!fs || !file_validate_name(filename) || !buffer) {
        return FS_ERROR_INVALID_PARAMS;
    }

    /* Buscar archivo */
    file_entry_t *file = file_find(&fs->file_table, filename);
    if (!file) {
        return FS_ERROR_FILE_NOT_FOUND;
    }

    /* Validar límites */
    if (offset >= file->size) {
        return FS_ERROR_INVALID_OFFSET;
    }

    /* Ajustar tamaño si excede el archivo */
    if (offset + size > file->size) {
        size = file->size - offset;
    }

    /* Leer datos de los bloques */
    return block_read(&fs->block_system, file->blocks, file->block_count, 
                     offset, size, buffer);
}

fs_error_t fs_delete(filesystem_t *fs, const char *filename) {
    if (!fs || !file_validate_name(filename)) {
        return FS_ERROR_INVALID_PARAMS;
    }

    /* Buscar archivo */
    file_entry_t *file = file_find(&fs->file_table, filename);
    if (!file) {
        return FS_ERROR_FILE_NOT_FOUND;
    }

    /* Liberar bloques */
    block_free(&fs->block_system, file->blocks, file->block_count);

    /* Eliminar entrada */
    file_remove_entry(&fs->file_table, file);

    return FS_SUCCESS;
}

void fs_list(const filesystem_t *fs) {
    if (!fs) {
        return;
    }

    if (fs->file_table.file_count == 0) {
        printf("(no hay archivos)\n");
        return;
    }

    printf("Archivos en el sistema:\n");
    printf("%-30s %10s\n", "Nombre", "Tamaño");
    printf("----------------------------------------\n");

    for (uint32_t i = 0; i < MAX_FILES; i++) {
        if (fs->file_table.files[i].in_use) {
            printf("%-30s %10u bytes\n", 
                   fs->file_table.files[i].name, 
                   fs->file_table.files[i].size);
        }
    }
}