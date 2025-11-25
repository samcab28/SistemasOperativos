/**
 * main.c
 * Programa principal con interfaz de línea de comandos
 */

#include "filesystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT 1024
#define MAX_DATA 4096

/* Prototipos de funciones auxiliares */
static void print_help(void);
static void handle_create(filesystem_t *fs, char *args);
static void handle_write(filesystem_t *fs, char *args);
static void handle_read(filesystem_t *fs, char *args);
static void handle_delete(filesystem_t *fs, char *args);
static void print_error(fs_error_t error);

int main(void) {
    filesystem_t fs;
    char input[MAX_INPUT];
    char command[64];

    /* Inicializar sistema de archivos */
    fs_error_t result = fs_init(&fs);
    if (result != FS_SUCCESS) {
        fprintf(stderr, "Error al inicializar el sistema: %s\n", 
                fs_error_string(result));
        return EXIT_FAILURE;
    }

    printf("=== Sistema de Archivos Simple ===\n");
    printf("Escribe 'help' para ver los comandos disponibles\n\n");

    /* Loop principal */
    while (1) {
        printf("> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        /* Eliminar salto de línea */
        input[strcspn(input, "\n")] = 0;

        /* Obtener comando */
        if (sscanf(input, "%63s", command) != 1) {
            continue;
        }

        /* Procesar comandos */
        if (strcmp(command, "CREATE") == 0) {
            handle_create(&fs, input + strlen(command));
        } 
        else if (strcmp(command, "WRITE") == 0) {
            handle_write(&fs, input + strlen(command));
        } 
        else if (strcmp(command, "READ") == 0) {
            handle_read(&fs, input + strlen(command));
        } 
        else if (strcmp(command, "DELETE") == 0) {
            handle_delete(&fs, input + strlen(command));
        } 
        else if (strcmp(command, "LIST") == 0) {
            fs_list(&fs);
        } 
        else if (strcmp(command, "help") == 0 || strcmp(command, "HELP") == 0) {
            print_help();
        } 
        else if (strcmp(command, "exit") == 0 || strcmp(command, "EXIT") == 0) {
            printf("Saliendo del sistema...\n");
            break;
        } 
        else {
            printf("Comando no reconocido. Escribe 'help' para ayuda.\n");
        }
    }

    return EXIT_SUCCESS;
}

static void print_help(void) {
    printf("\nComandos disponibles:\n");
    printf("  CREATE <nombre> <tamaño>           - Crea un archivo\n");
    printf("  WRITE <nombre> <offset> <datos>    - Escribe datos en un archivo\n");
    printf("  READ <nombre> <offset> <tamaño>    - Lee datos de un archivo\n");
    printf("  DELETE <nombre>                    - Elimina un archivo\n");
    printf("  LIST                               - Lista todos los archivos\n");
    printf("  help                               - Muestra esta ayuda\n");
    printf("  exit                               - Sale del programa\n\n");
    printf("Ejemplo:\n");
    printf("  > CREATE archivo1.txt 1000\n");
    printf("  > WRITE archivo1.txt 0 \"Hola, mundo\"\n");
    printf("  > READ archivo1.txt 0 11\n");
    printf("  > DELETE archivo1.txt\n\n");
}

static void handle_create(filesystem_t *fs, char *args) {
    char filename[MAX_FILENAME];
    uint32_t size;

    if (sscanf(args, "%255s %u", filename, &size) != 2) {
        printf("Error: Formato incorrecto. Uso: CREATE <nombre> <tamaño>\n");
        return;
    }

    fs_error_t result = fs_create(fs, filename, size);
    if (result == FS_SUCCESS) {
        printf("Archivo '%s' creado exitosamente (%u bytes)\n", filename, size);
    } else {
        print_error(result);
    }
}

static void handle_write(filesystem_t *fs, char *args) {
    char filename[MAX_FILENAME];
    uint32_t offset;
    //char data[MAX_DATA];
    char *data_start;

    /* Parsear nombre y offset */
    if (sscanf(args, "%255s %u", filename, &offset) != 2) {
        printf("Error: Formato incorrecto. Uso: WRITE <nombre> <offset> <datos>\n");
        return;
    }

    /* Buscar inicio de los datos (después del offset) */
    data_start = args;
    for (int i = 0; i < 2 && data_start; i++) {
        data_start = strchr(data_start, ' ');
        if (data_start) data_start++;
    }

    if (!data_start || strlen(data_start) == 0) {
        printf("Error: No se especificaron datos para escribir\n");
        return;
    }

    /* Eliminar comillas si existen */
    if (data_start[0] == '"') {
        data_start++;
        size_t len = strlen(data_start);
        if (len > 0 && data_start[len - 1] == '"') {
            data_start[len - 1] = '\0';
        }
    }

    fs_error_t result = fs_write(fs, filename, offset, data_start, strlen(data_start));
    if (result == FS_SUCCESS) {
        printf("Datos escritos exitosamente en '%s'\n", filename);
    } else {
        print_error(result);
    }
}

static void handle_read(filesystem_t *fs, char *args) {
    char filename[MAX_FILENAME];
    uint32_t offset, size;
    char *buffer;

    if (sscanf(args, "%255s %u %u", filename, &offset, &size) != 3) {
        printf("Error: Formato incorrecto. Uso: READ <nombre> <offset> <tamaño>\n");
        return;
    }

    buffer = malloc(size + 1);
    if (!buffer) {
        printf("Error: No se pudo asignar memoria\n");
        return;
    }

    memset(buffer, 0, size + 1);

    fs_error_t result = fs_read(fs, filename, offset, size, buffer);
    if (result == FS_SUCCESS) {
        printf("Salida: \"%s\"\n", buffer);
    } else {
        print_error(result);
    }

    free(buffer);
}

static void handle_delete(filesystem_t *fs, char *args) {
    char filename[MAX_FILENAME];

    if (sscanf(args, "%255s", filename) != 1) {
        printf("Error: Formato incorrecto. Uso: DELETE <nombre>\n");
        return;
    }

    fs_error_t result = fs_delete(fs, filename);
    if (result == FS_SUCCESS) {
        printf("Archivo '%s' eliminado exitosamente\n", filename);
    } else {
        print_error(result);
    }
}

static void print_error(fs_error_t error) {
    printf("Error: %s\n", fs_error_string(error));
}