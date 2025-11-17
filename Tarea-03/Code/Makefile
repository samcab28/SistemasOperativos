# Configuración del compilador
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude
LDFLAGS =

# Directorios
SRC_DIR = src
BUILD_DIR = build
INC_DIR = include

# Archivos
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
TARGET = memsim

# Regla principal
all: $(BUILD_DIR) $(TARGET)

# Crear directorio build si no existe
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Enlazar el ejecutable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Compilar archivos .c a .o
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Limpiar archivos generados
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Ejecutar con parámetros de ejemplo
run: $(TARGET)
	./$(TARGET) first 1024 entrada.txt
	@echo "\n[Programa terminado correctamente]"

# Ejecutar con diferentes estrategias
run-first: $(TARGET)
	./$(TARGET) first 1024 entrada.txt
	@echo "\n[Programa terminado correctamente]"

run-best: $(TARGET)
	./$(TARGET) best 1024 entrada.txt
	@echo "\n[Programa terminado correctamente]"

run-worst: $(TARGET)
	./$(TARGET) worst 1024 entrada.txt
	@echo "\n[Programa terminado correctamente]"

# Verificar que no hay memory leaks con valgrind (si está instalado)
valgrind: $(TARGET)
	valgrind --leak-check=full ./$(TARGET) first 1024 entrada.txt

# Comparar las tres estrategias lado a lado
test-all: $(TARGET)
	@echo "=== FIRST FIT ==="
	@./$(TARGET) first 1024 entrada.txt
	@echo "\n=== BEST FIT ==="
	@./$(TARGET) best 1024 entrada.txt
	@echo "\n=== WORST FIT ==="
	@./$(TARGET) worst 1024 entrada.txt
	@echo "\n[Todas las pruebas completadas]"

.PHONY: all clean run run-first run-best run-worst valgrind test-all