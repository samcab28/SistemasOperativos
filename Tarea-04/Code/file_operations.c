/**
 * file_operations.c
 * Implementación de operaciones sobre archivos
 */

#include "file_operations.h"
#include <string.h>
#include <stdio.h>

void file_table_init(file_table_t *ft) {
    memset(ft->files, 0, sizeof(ft->files));
    ft->file_count = 0;
}

bool file_validate_name(const char *filename) {
    if (!filename || strlen(filename) == 0) {
        return false;
    }
    
    size_t len = strlen(filename);
    if (len >= MAX_FILENAME) {
        return false;
    }
    
    return true;
}

bool file_validate_size(uint32_t size) {
    return size > 0 && size <= MAX_FILE_SIZE;
}

file_entry_t* file_find(file_table_t *ft, const char *filename) {
    if (!file_validate_name(filename)) {
        return NULL;
    }

    for (uint32_t i = 0; i < MAX_FILES; i++) {
        if (ft->files[i].in_use && 
            strcmp(ft->files[i].name, filename) == 0) {
            return &ft->files[i];
        }
    }
    
    return NULL;
}

file_entry_t* file_find_free_slot(file_table_t *ft) {
    if (ft->file_count >= MAX_FILES) {
        return NULL;
    }

    for (uint32_t i = 0; i < MAX_FILES; i++) {
        if (!ft->files[i].in_use) {
            return &ft->files[i];
        }
    }
    
    return NULL;
}

fs_error_t file_create_entry(file_table_t *ft, const char *filename, 
                             uint32_t size, const uint32_t *blocks, 
                             uint32_t block_count) {
    
    if (!file_validate_name(filename) || !file_validate_size(size)) {
        return FS_ERROR_INVALID_PARAMS;
    }

    /* Verificar si el archivo ya existe */
    if (file_find(ft, filename) != NULL) {
        return FS_ERROR_FILE_EXISTS;
    }

    /* Buscar slot libre */
    file_entry_t *file = file_find_free_slot(ft);
    if (!file) {
        return FS_ERROR_MAX_FILES;
    }

    /* Inicializar entrada */
    strncpy(file->name, filename, MAX_FILENAME - 1);
    file->name[MAX_FILENAME - 1] = '\0';
    file->size = size;
    file->block_count = block_count;
    memcpy(file->blocks, blocks, block_count * sizeof(uint32_t));
    file->in_use = true;
    
    ft->file_count++;
    
    return FS_SUCCESS;
}

void file_remove_entry(file_table_t *ft, file_entry_t *file) {
    if (file && file->in_use) {
        memset(file, 0, sizeof(file_entry_t));
        file->in_use = false;
        ft->file_count--;
    }
}