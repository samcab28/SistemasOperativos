# Compilador y flags
CC = clang
CFLAGS = -Wall -Wextra -pthread -g -DUSE_THREADING

# Directorios
SRC_CORE = core
SRC_PATTERNS = patterns
SRC_METRICS = metrics
SRC_UTILS = utils

# Archivos objeto - Sistema completo con threading
OBJ = main.o \
      core/product.o \
      core/queue.o \
      core/station.o \
      core/scheduler.o \
      patterns/factory.o \
      metrics/metrics.o \
      metrics/logger.o

# Target principal
all: simulador

# Compilar el ejecutable
simulador: $(OBJ)
	$(CC) $(CFLAGS) -o simulador $(OBJ)
	@echo "✓ Compilación exitosa con threading habilitado!"

# Compilar main.o
main.o: main.c core/product.h core/queue.h core/station.h core/scheduler.h \
        patterns/factory.h metrics/metrics.h metrics/logger.h
	$(CC) $(CFLAGS) -c main.c -o main.o

# Compilar core/product.o
core/product.o: core/product.c core/product.h
	$(CC) $(CFLAGS) -c core/product.c -o core/product.o

# Compilar core/queue.o
core/queue.o: core/queue.c core/queue.h core/product.h
	$(CC) $(CFLAGS) -c core/queue.c -o core/queue.o

# Compilar core/station.o
core/station.o: core/station.c core/station.h core/product.h core/queue.h \
                metrics/logger.h metrics/metrics.h
	$(CC) $(CFLAGS) -c core/station.c -o core/station.o

# Compilar core/scheduler.o
core/scheduler.o: core/scheduler.c core/scheduler.h core/product.h core/queue.h \
                  core/station.h metrics/logger.h metrics/metrics.h
	$(CC) $(CFLAGS) -c core/scheduler.c -o core/scheduler.o

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
	rm -f *.o core/*.o patterns/*.o metrics/*.o utils/*.o simulador simulador.log results.log
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

# Ejecutar con número específico de productos
run-with-args: simulador
	./simulador $(ARGS)

# Ejemplo: make run-products PRODUCTS=20
run-products: simulador
	./simulador $(PRODUCTS)

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

# Verificar archivos requeridos
check-files:
	@echo "Verificando archivos requeridos..."
	@test -f core/product.c || (echo "✗ Falta core/product.c" && exit 1)
	@test -f core/queue.c || (echo "✗ Falta core/queue.c" && exit 1)
	@test -f core/station.c || (echo "✗ Falta core/station.c" && exit 1)
	@test -f core/scheduler.c || (echo "✗ Falta core/scheduler.c" && exit 1)
	@test -f patterns/factory.c || (echo "✗ Falta patterns/factory.c" && exit 1)
	@test -f metrics/metrics.c || (echo "✗ Falta metrics/metrics.c" && exit 1)
	@test -f metrics/logger.c || (echo "✗ Falta metrics/logger.c" && exit 1)
	@test -f main.c || (echo "✗ Falta main.c" && exit 1)
	@echo "✓ Todos los archivos requeridos presentes"

# Compilar con información de debugging extendida
debug: CFLAGS += -DDEBUG -O0
debug: clean simulador
	@echo "✓ Compilado en modo DEBUG"

# Compilar con optimización (sin threading debug)
release: CFLAGS += -O2 -DNDEBUG
release: clean simulador
	@echo "✓ Compilado en modo RELEASE"

# Compilar sin threading (para comparación)
no-threading: CFLAGS = -Wall -Wextra -pthread -g
no-threading: clean simulador
	@echo "✓ Compilado SIN threading (versión secuencial)"

# Verificar memory leaks con valgrind
valgrind: simulador
	@which valgrind > /dev/null || (echo "✗ valgrind no está instalado" && exit 1)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./simulador

# Detectar race conditions con helgrind
helgrind: simulador
	@which valgrind > /dev/null || (echo "✗ valgrind no está instalado" && exit 1)
	valgrind --tool=helgrind ./simulador

# Detectar race conditions con thread sanitizer (requiere recompilación)
tsan: CFLAGS += -fsanitize=thread
tsan: clean simulador
	@echo "✓ Compilado con Thread Sanitizer"
	./simulador

# Stress test con muchos productos
stress-test: simulador
	@echo "Ejecutando stress test con 50 productos..."
	./simulador 50

# Test básico con pocos productos
quick-test: simulador
	@echo "Ejecutando test rápido con 3 productos..."
	./simulador 3

# Comparar algoritmos con diferentes cantidades de productos
compare: simulador
	@echo "=== Comparación con 5 productos ==="
	./simulador 5
	@sleep 2
	@echo "\n=== Comparación con 10 productos ==="
	./simulador 10
	@sleep 2
	@echo "\n=== Comparación con 20 productos ==="
	./simulador 20

# Mostrar tamaño del ejecutable
size: simulador
	@echo "Tamaño del ejecutable:"
	@ls -lh simulador
	@echo "\nDesglose por sección:"
	@size simulador

# Generar documentación (si tienes doxygen)
docs:
	@which doxygen > /dev/null || (echo "✗ doxygen no está instalado" && exit 1)
	doxygen Doxyfile

# Mostrar ayuda extendida
help:
	@echo "=========================================="
	@echo "Makefile para Simulador de Línea de Ensamblaje"
	@echo "Sistema Completo con Threading y Scheduling"
	@echo "=========================================="
	@echo ""
	@echo "Targets de compilación:"
	@echo "  make              - Compila el proyecto con threading"
	@echo "  make clean        - Elimina archivos compilados y logs"
	@echo "  make clean-logs   - Elimina solo archivos de log"
	@echo "  make rebuild      - Limpia y recompila"
	@echo "  make debug        - Compila en modo debug (con -DDEBUG)"
	@echo "  make release      - Compila optimizado para producción"
	@echo "  make no-threading - Compila sin threading (secuencial)"
	@echo ""
	@echo "Targets de ejecución:"
	@echo "  make run                  - Compila y ejecuta (10 productos)"
	@echo "  make run-products PRODUCTS=N - Ejecuta con N productos"
	@echo "  make run-log              - Ejecuta y muestra el log"
	@echo "  make quick-test           - Test rápido (3 productos)"
	@echo "  make stress-test          - Test de estrés (50 productos)"
	@echo "  make compare              - Compara con 5, 10 y 20 productos"
	@echo ""
	@echo "Targets de verificación:"
	@echo "  make check-dirs   - Verifica estructura de directorios"
	@echo "  make check-files  - Verifica archivos requeridos"
	@echo "  make valgrind     - Detecta memory leaks"
	@echo "  make helgrind     - Detecta race conditions (valgrind)"
	@echo "  make tsan         - Detecta race conditions (thread sanitizer)"
	@echo ""
	@echo "Targets de información:"
	@echo "  make stats        - Estadísticas del código"
	@echo "  make size         - Tamaño del ejecutable"
	@echo "  make help         - Muestra esta ayuda"
	@echo ""
	@echo "Ejemplos de uso:"
	@echo "  make && make run              # Compilar y ejecutar"
	@echo "  make run-products PRODUCTS=15 # Ejecutar con 15 productos"
	@echo "  make debug && make valgrind   # Debug + detección de leaks"
	@echo "  make helgrind                 # Detectar race conditions"
	@echo "  make compare                  # Comparar algoritmos"
	@echo ""
	@echo "Estructura requerida:"
	@echo "  core/product.c, core/product.h"
	@echo "  core/queue.c, core/queue.h"
	@echo "  core/station.c, core/station.h"
	@echo "  core/scheduler.c, core/scheduler.h"
	@echo "  patterns/factory.c, patterns/factory.h"
	@echo "  metrics/metrics.c, metrics/metrics.h"
	@echo "  metrics/logger.c, metrics/logger.h"
	@echo "  main.c"
	@echo ""

# Verificación completa antes de entregar
pre-submit: clean check-dirs check-files
	@echo "Compilando..."
	@make all
	@echo "Ejecutando tests básicos..."
	@make quick-test
	@echo "Verificando memory leaks..."
	@make valgrind 2>/dev/null || echo "Valgrind no disponible, saltando..."
	@echo "\n✓ Proyecto listo para entrega"
	@echo "Archivos a incluir:"
	@echo "  - Código fuente (core/, patterns/, metrics/, main.c)"
	@echo "  - Makefile"
	@echo "  - README.md"
	@echo "  - simulador (ejecutable)"
	@echo "  - simulador.log (ejemplo de ejecución)"

.PHONY: all clean clean-logs rebuild run run-with-args run-products run-log \
        stats check-dirs check-files debug release no-threading valgrind \
        helgrind tsan stress-test quick-test compare size docs help pre-submit