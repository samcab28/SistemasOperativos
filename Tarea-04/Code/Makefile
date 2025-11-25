# Makefile para el Sistema de Archivos Simple

# Compilador y flags
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -O2 -g
LDFLAGS =

# Directorios
SRC_DIR = .
OBJ_DIR = obj
BIN_DIR = bin

# Nombre del ejecutable
TARGET = $(BIN_DIR)/filesystem

# Archivos fuente
SOURCES = main.c filesystem.c block_manager.c file_operations.c
OBJECTS = $(addprefix $(OBJ_DIR)/, $(SOURCES:.c=.o))
HEADERS = filesystem.h block_manager.h file_operations.h

# Regla por defecto
.PHONY: all
all: directories $(TARGET)

# Crear directorios necesarios
.PHONY: directories
directories:
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(BIN_DIR)

# Enlazar el ejecutable
$(TARGET): $(OBJECTS)
	@echo "Enlazando $(TARGET)..."
	@$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "Compilación completada exitosamente!"

# Compilar archivos objeto
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@echo "Compilando $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Ejecutar el programa
.PHONY: run
run: all
	@echo "Ejecutando el sistema de archivos..."
	@$(TARGET)

# Ejecutar pruebas automáticas
.PHONY: test
test: all
	@echo "Ejecutando pruebas..."
	@bash test_script.sh

# Limpiar archivos generados
.PHONY: clean
clean:
	@echo "Limpiando archivos generados..."
	@rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Limpieza completada"

# Limpiar y recompilar
.PHONY: rebuild
rebuild: clean all

# Mostrar información de compilación
.PHONY: info
info:
	@echo "Sistema de Archivos Simple"
	@echo "=========================="
	@echo "Compilador: $(CC)"
	@echo "Flags: $(CFLAGS)"
	@echo "Fuentes: $(SOURCES)"
	@echo "Ejecutable: $(TARGET)"

# Verificar estilo de código
.PHONY: check
check:
	@echo "Verificando estilo de código..."
	@$(CC) $(CFLAGS) -fsyntax-only $(SOURCES)
	@echo "Verificación completada"

# Ayuda
.PHONY: help
help:
	@echo "Makefile del Sistema de Archivos Simple"
	@echo ""
	@echo "Objetivos disponibles:"
	@echo "  all      - Compila el proyecto (por defecto)"
	@echo "  run      - Compila y ejecuta el programa"
	@echo "  test     - Ejecuta las pruebas automáticas"
	@echo "  clean    - Elimina archivos generados"
	@echo "  rebuild  - Limpia y recompila"
	@echo "  check    - Verifica sintaxis del código"
	@echo "  info     - Muestra información de compilación"
	@echo "  help     - Muestra esta ayuda"