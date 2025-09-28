# Compilador y flags
CC = clang
CFLAGS = -Wall -Wextra -pthread -g

# Directorios
SRC_CORE = core
SRC_PATTERNS = patterns
SRC_METRICS = metrics
SRC_UTILS = utils

# Archivos objeto para Fase 1
OBJ = main.o \
      core/product.o \
      patterns/factory.o

# Target principal
all: simulador

# Compilar el ejecutable
simulador: $(OBJ)
	$(CC) $(CFLAGS) -o simulador $(OBJ)
	@echo "✓ Compilación exitosa!"

# Compilar main.o
main.o: main.c core/product.h patterns/factory.h
	$(CC) $(CFLAGS) -c main.c -o main.o

# Compilar core/product.o
core/product.o: core/product.c core/product.h
	$(CC) $(CFLAGS) -c core/product.c -o core/product.o

# Compilar patterns/factory.o
patterns/factory.o: patterns/factory.c patterns/factory.h core/product.h
	$(CC) $(CFLAGS) -c patterns/factory.c -o patterns/factory.o

# Limpiar archivos compilados
clean:
	rm -f *.o core/*.o patterns/*.o metrics/*.o utils/*.o simulador
	@echo "✓ Limpieza completada"

# Limpiar y recompilar
rebuild: clean all

# Ejecutar el programa
run: simulador
	./simulador

# Mostrar ayuda
help:
	@echo "Makefile para Simulador de Línea de Ensamblaje - Fase 1"
	@echo ""
	@echo "Targets disponibles:"
	@echo "  make          - Compila el proyecto"
	@echo "  make clean    - Elimina archivos compilados"
	@echo "  make rebuild  - Limpia y recompila"
	@echo "  make run      - Compila y ejecuta el programa"
	@echo "  make help     - Muestra esta ayuda"

.PHONY: all clean rebuild run help