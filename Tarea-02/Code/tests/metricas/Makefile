# Compilador y flags
CC = clang
CFLAGS = -Wall -Wextra -pthread -g

# Directorios
SRC_CORE = core
SRC_PATTERNS = patterns
SRC_METRICS = metrics
SRC_UTILS = utils

# Archivos objeto para Fase 1 - Sistema Completo
OBJ = main.o \
      core/product.o \
      patterns/factory.o \
      metrics/metrics.o \
      metrics/logger.o

# Target principal
all: simulador

# Compilar el ejecutable
simulador: $(OBJ)
	$(CC) $(CFLAGS) -o simulador $(OBJ)
	@echo "✓ Compilación exitosa!"

# Compilar main.o
main.o: main.c core/product.h patterns/factory.h metrics/metrics.h metrics/logger.h
	$(CC) $(CFLAGS) -c main.c -o main.o

# Compilar core/product.o
core/product.o: core/product.c core/product.h
	$(CC) $(CFLAGS) -c core/product.c -o core/product.o

# Compilar patterns/factory.o
patterns/factory.o: patterns/factory.c patterns/factory.h core/product.h
	$(CC) $(CFLAGS) -c patterns/factory.c -o patterns/factory.o

# Compilar metrics/metrics.o
metrics/metrics.o: metrics/metrics.c metrics/metrics.h core/product.h
	$(CC) $(CFLAGS) -c metrics/metrics.c -o metrics/metrics.o

# Compilar metrics/logger.o
metrics/logger.o: metrics/logger.c metrics/logger.h
	$(CC) $(CFLAGS) -c metrics/logger.c -o metrics/logger.o

# Limpiar archivos compilados
clean:
	rm -f *.o core/*.o patterns/*.o metrics/*.o utils/*.o simulador simulador.log
	@echo "✓ Limpieza completada"

# Limpiar solo archivos de log
clean-logs:
	rm -f *.log
	@echo "✓ Logs eliminados"

# Limpiar y recompilar
rebuild: clean all

# Ejecutar el programa
run: simulador
	./simulador

# Ejecutar y mostrar el log después
run-log: run
	@echo "\n=== CONTENIDO DEL LOG ==="
	@cat simulador.log

# Ver estadísticas de compilación
stats:
	@echo "=== ESTADÍSTICAS DEL PROYECTO ==="
	@echo "Archivos C:"
	@find . -name "*.c" | wc -l
	@echo "Archivos H:"
	@find . -name "*.h" | wc -l
	@echo "Líneas de código:"
	@find . -name "*.c" -o -name "*.h" | xargs wc -l | tail -1

# Verificar estructura de directorios
check-dirs:
	@echo "Verificando estructura de directorios..."
	@test -d core || (echo "✗ Falta directorio core/" && exit 1)
	@test -d patterns || (echo "✗ Falta directorio patterns/" && exit 1)
	@test -d metrics || (echo "✗ Falta directorio metrics/" && exit 1)
	@echo "✓ Estructura de directorios correcta"

# Compilar con información de debugging extendida
debug: CFLAGS += -DDEBUG -O0
debug: clean simulador
	@echo "✓ Compilado en modo DEBUG"

# Compilar con optimización
release: CFLAGS += -O2 -DNDEBUG
release: clean simulador
	@echo "✓ Compilado en modo RELEASE"

# Verificar memory leaks con valgrind (si está instalado)
valgrind: simulador
	@which valgrind > /dev/null || (echo "✗ valgrind no está instalado" && exit 1)
	valgrind --leak-check=full --show-leak-kinds=all ./simulador

# Mostrar ayuda
help:
	@echo "=========================================="
	@echo "Makefile para Simulador de Línea de Ensamblaje"
	@echo "Fase 1: Factory + Metrics + Logger"
	@echo "=========================================="
	@echo ""
	@echo "Targets disponibles:"
	@echo "  make              - Compila el proyecto"
	@echo "  make clean        - Elimina archivos compilados"
	@echo "  make clean-logs   - Elimina archivos de log"
	@echo "  make rebuild      - Limpia y recompila"
	@echo "  make run          - Compila y ejecuta el programa"
	@echo "  make run-log      - Ejecuta y muestra el log"
	@echo "  make debug        - Compila en modo debug"
	@echo "  make release      - Compila optimizado"
	@echo "  make stats        - Muestra estadísticas del código"
	@echo "  make check-dirs   - Verifica estructura de directorios"
	@echo "  make valgrind     - Ejecuta con valgrind (detección de leaks)"
	@echo "  make help         - Muestra esta ayuda"
	@echo ""
	@echo "Estructura requerida:"
	@echo "  core/product.c, core/product.h"
	@echo "  patterns/factory.c, patterns/factory.h"
	@echo "  metrics/metrics.c, metrics/metrics.h"
	@echo "  metrics/logger.c, metrics/logger.h"
	@echo "  main.c"
	@echo ""

.PHONY: all clean clean-logs rebuild run run-log stats check-dirs debug release valgrind help