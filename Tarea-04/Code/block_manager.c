/**
 * block_manager.c
 * Implementación de la gestión de bloques
 */

#include "block_manager.h"
#include <string.h>
#include <stdio.h>

void block_init(block_system_t *bs) {
    memset(bs->data, 0, sizeof(bs->data));
    memset(bs->allocated, 0, sizeof(bs->allocated));
    bs->free_blocks = MAX_BLOCKS;
}

uint32_t block_calculate_needed(uint32_t size) {
    return (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
}

fs_error_t block_allocate(block_system_t *bs, uint32_t count, uint32_t *blocks) {
    if (count > bs->free_blocks) {
        return FS_ERROR_NO_SPACE;
    }

    uint32_t allocated = 0;
    
    /* Buscar y asignar bloques libres */
    for (uint32_t i = 0; i < MAX_BLOCKS && allocated < count; i++) {
        if (!bs->allocated[i]) {
            bs->allocated[i] = true;
            blocks[allocated++] = i;
            bs->free_blocks--;
        }
    }

    if (allocated != count) {
        /* Revertir asignación si falla */
        block_free(bs, blocks, allocated);
        return FS_ERROR_NO_SPACE;
    }

    return FS_SUCCESS;
}

void block_free(block_system_t *bs, const uint32_t *blocks, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (blocks[i] < MAX_BLOCKS && bs->allocated[blocks[i]]) {
            bs->allocated[blocks[i]] = false;
            memset(bs->data[blocks[i]], 0, BLOCK_SIZE);
            bs->free_blocks++;
        }
    }
}

fs_error_t block_write(block_system_t *bs, const uint32_t *blocks, 
                       uint32_t block_count, uint32_t offset, 
                       const char *data, uint32_t data_size) {
    
    if (!data || data_size == 0) {
        return FS_SUCCESS;
    }

    uint32_t start_block = offset / BLOCK_SIZE;
    uint32_t block_offset = offset % BLOCK_SIZE;
    uint32_t bytes_written = 0;

    for (uint32_t i = start_block; i < block_count && bytes_written < data_size; i++) {
        uint32_t block_idx = blocks[i];
        
        if (block_idx >= MAX_BLOCKS) {
            return FS_ERROR_WRITE_FAILURE;
        }

        uint32_t write_size = BLOCK_SIZE - block_offset;
        if (write_size > data_size - bytes_written) {
            write_size = data_size - bytes_written;
        }

        memcpy(bs->data[block_idx] + block_offset, data + bytes_written, write_size);
        bytes_written += write_size;
        block_offset = 0; /* Los siguientes bloques empiezan desde 0 */
    }

    return FS_SUCCESS;
}

fs_error_t block_read(const block_system_t *bs, const uint32_t *blocks, 
                      uint32_t block_count, uint32_t offset, 
                      uint32_t size, char *buffer) {
    
    if (!buffer || size == 0) {
        return FS_SUCCESS;
    }

    uint32_t start_block = offset / BLOCK_SIZE;
    uint32_t block_offset = offset % BLOCK_SIZE;
    uint32_t bytes_read = 0;

    for (uint32_t i = start_block; i < block_count && bytes_read < size; i++) {
        uint32_t block_idx = blocks[i];
        
        if (block_idx >= MAX_BLOCKS) {
            return FS_ERROR_READ_FAILURE;
        }

        uint32_t read_size = BLOCK_SIZE - block_offset;
        if (read_size > size - bytes_read) {
            read_size = size - bytes_read;
        }

        memcpy(buffer + bytes_read, bs->data[block_idx] + block_offset, read_size);
        bytes_read += read_size;
        block_offset = 0;
    }

    return FS_SUCCESS;
}