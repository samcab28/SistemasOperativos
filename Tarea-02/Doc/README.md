<div align="center">

# Proyecto 02 – Simulador de Línea de Ensamblaje

**IC6600 – Principios de Sistemas Operativos**

**Estudiantes**  
Anthony Barrantes Jiménez · Samir Cabrera Tabash

</div>

---

## Tabla de contenido

1. [Descripción general](#descripción-general)
2. [Arquitectura de la solución](#arquitectura-de-la-solución)
3. [Flujo de procesamiento](#flujo-de-procesamiento)
4. [Algoritmos de _scheduling_](#algoritmos-de-scheduling)
5. [Métricas y reportes generados](#métricas-y-reportes-generados)
6. [Requisitos y compilación](#requisitos-y-compilación)
7. [Uso del simulador](#uso-del-simulador)
8. [Ejemplos de ejecución](#ejemplos-de-ejecución)
9. [Archivos de salida y logs](#archivos-de-salida-y-logs)
10. [Estructura del repositorio](#estructura-del-repositorio)

---

## Descripción general

Este proyecto implementa un **simulador concurrente de una línea de ensamblaje** de tres estaciones (Corte, Ensamblaje y Empaque). Cada estación se ejecuta en su propio hilo y recibe productos a través de colas thread-safe. Un **scheduler** independiente coordina qué producto se procesa en cada instante utilizando uno de dos algoritmos de planificación:

- **First-Come, First-Served (FCFS)**
- **Round Robin (RR)** con _quantum_ configurable

Durante la simulación, el sistema recopila métricas detalladas sobre tiempos de espera, tiempos de procesamiento, preempciones y orden de finalización de los productos. Al finalizar cada corrida se genera un informe completo tanto en consola como en archivos (`results.log` y `simulador.log`).

## Arquitectura de la solución

La aplicación se organiza en módulos independientes que siguen patrones de diseño para aislar responsabilidades:

| Módulo      | Ubicación                                                  | Responsabilidad principal                                                                                  |
| ----------- | ---------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------- |
| `core/`     | `product.c/h`, `station.c/h`, `scheduler.c/h`, `queue.c/h` | Entidades base: productos, estaciones (hilos), scheduler y colas thread-safe.                              |
| `patterns/` | `factory.c/h`                                              | _Factory_ que crea lotes de productos con estados iniciales consistentes.                                  |
| `metrics/`  | `metrics.c/h`, `logger.c/h`                                | Subsistema de métricas globales y logger configurable a archivo y consola.                                 |
| `main.c`    | raíz                                                       | Punto de entrada. Procesa CLI, construye el pipeline, ejecuta simulaciones y genera reportes comparativos. |

Componentes adicionales:

- **Colas (`queue_t`)**: implementadas con mutex y semáforos para garantizar acceso seguro entre hilos.
- **Estaciones**: cada estación es un hilo que consume productos, respeta tiempos de procesamiento (deterministas o con varianza) y notifica al scheduler al completar o ser preemptada.
- **Scheduler**: hilo que decide qué producto despachar; mantiene estados y contadores de métricas específicos del algoritmo seleccionado.

## Flujo de procesamiento

1. **Inicialización**: se levantan el logger y el sistema de métricas.
2. **Configuración**: se crean las colas, estaciones y el scheduler según los parámetros ingresados.
3. **Generación de productos**: el factory produce un lote con métricas individuales y tiempo de llegada.
4. **Despacho inicial**: el scheduler recibe todos los productos en su cola de listos y arranca la simulación.
5. **Procesamiento concurrente**:
   - Cada estación consume productos desde su cola de entrada.
   - RR puede solicitar preempciones según el quantum.
   - Los productos recorren las tres estaciones antes de considerarse completados.
6. **Monitoreo**: métricas centrales registran cada evento (entrada, salida, preempciones, etc.).
7. **Finalización**: al terminar los productos se detienen los hilos, se liberan recursos y se generan los reportes.

## Algoritmos de _scheduling_

### First-Come, First-Served (FCFS)

- **Estrategia**: orden FIFO clásico; no hay preempciones.
- **Comportamiento**: un producto permanece en cada estación hasta completarla antes de pasar a la siguiente.
- **Casos de uso**: línea estable con cargas homogéneas o cuando se desea el pipeline más simple posible.

### Round Robin (RR)

- **Estrategia**: los productos reciben un _quantum_ por estación. Al expirar, la estación notifica al scheduler para reencolar el producto, permitiendo que otros avancen.
- **Paralelismo**: diseñado para mantener el pipeline activo en todas las estaciones, incluso si un producto es preemptado en una etapa previa.
- **Configuración**: `--quantum=<ms>` (default `500`).
- **Métricas clave**: número de preempciones por producto y por estación, cantidad total de _context switches_.

### Modo determinista vs aleatorio

- **Determinista** (`--deterministic`, valor por defecto): cada estación usa el tiempo exacto configurado.
- **Aleatorio** (`--random` o `--randomize`): introduce una variación ±25 % sobre cada tiempo base, ajustable en `station_set_processing_variance`.

## Métricas y reportes generados

El subsistema de métricas recopila información granular:

- **Por producto**:
  - Tiempo de llegada y de finalización (relativo al inicio de la simulación).
  - Tiempo total (`turnaround`) y tiempo total de espera.
  - Por estación: tiempo de entrada, tiempo de salida, tiempo en cola, tiempo procesado y preempciones.
- **Por estación**:
  - Productos completados, tiempo de procesamiento acumulado y promedio, mínimos/máximos, uso acumulado.
- **Scheduler**:
  - Productos planificados, cambios de contexto, preempciones, promedios de espera/turnaround.
- **Global**:
  - Tiempo total de simulación, throughput promedio, utilización agregada de las estaciones.
- **Orden de finalización**: secuencia exacta en que los productos terminaron todo el pipeline.

El resumen se imprime por consola y se escribe en `results.log` (modo append). Adicionalmente, `simulador.log` guarda el historial cronológico de eventos y mensajes del subsistema `logger`.

## Requisitos y compilación

### Dependencias recomendadas (Debian/Ubuntu)

```bash
sudo apt update
sudo apt install build-essential clang make
```

> El proyecto se compila con `clang` por defecto, pero `gcc` también es compatible. Se requiere soporte POSIX para pthreads (habitualmente incluido en `libpthread`).

### Compilación

1. Posicionarse en la carpeta `Code/`.
2. Ejecutar `make` para construir el binario `simulador` con soporte de threading (`-DUSE_THREADING`).
3. Opcional: `make clean` elimina artefactos previos.

El script `execute.sh` automatiza `make clean && make` y, si la compilación es exitosa, ejecuta el simulador inmediato.

## Uso del simulador

```
./simulador [opciones]
```

### Parámetros principales

| Opción                     | Descripción                                                                                              | Valor por defecto |
| -------------------------- | -------------------------------------------------------------------------------------------------------- | ----------------- |
| `--products=<n>`           | Cantidad de productos a generar (1–100). También puede indicarse simplemente `<n>` como argumento final. | `10`              |
| `--times=t1,t2,t3`         | Tiempos (ms) para Corte, Ensamblaje y Empaque.                                                           | `2000,2500,4000`  |
| `--quantum=<ms>`           | Quantum para RR (se ignora en FCFS).                                                                     | `500`             |
| `--algorithm=lista`        | Coma-separada: `fcfs`, `rr`, `both`, `none`.                                                             | `both`            |
| `--random` / `--randomize` | Habilita variación aleatoria ±25 % en cada estación.                                                     | Deshabilitado     |
| `--deterministic`          | Fuerza modo determinista.                                                                                | Habilitado        |

Notas:

- Si se especifica `--algorithm=none`, debe acompañarse de algún otro valor válido en la lista (por ejemplo, `--algorithm=none,rr`).
- `--products` y el argumento numérico sencillo son equivalentes; si se usan ambos, el último valor prevalece.
- Valores inválidos o fuera de rango producen mensajes descriptivos y terminan la ejecución.

## Ejemplos de ejecución

### 1. Corrida determinista con ambos algoritmos

```bash
./simulador 20 --times=1800,2200,3200 --quantum=750
```

### 2. Solo Round Robin con 15 productos y variación aleatoria

```bash
./simulador --algorithm=rr --products=15 --quantum=1200 --random
```

### 3. FCFS con tiempos personalizados

```bash
./simulador --algorithm=fcfs --times=1000,2000,3000 --deterministic --products=5
```

Tras cada ejecución se pueden revisar:

- Consola: resumen detallado.
- `results.log`: historiales acumulados de todas las simulaciones.
- `simulador.log`: log cronológico (útil para depuración).

## Archivos de salida y logs

| Archivo         | Propósito                                                           |
| --------------- | ------------------------------------------------------------------- |
| `simulador`     | Binario generado por `make`.                                        |
| `simulador.log` | Log detallado del subsistema `logger` (niveles INFO/DEBUG).         |
| `results.log`   | Resumen formateado de cada corrida (modo append).                   |
| `metrics/`      | Fuente del subsistema de métricas (no se modifica automáticamente). |

Los informes de métricas incluyen los primeros tres productos destacados con toda la información solicitada (tiempos, esperas, preempciones y marcas de entrada/salida por estación).

## Estructura del repositorio

```
Code/
├── Makefile
├── execute.sh
├── main.c
├── core/
│   ├── product.c/h
│   ├── queue.c/h
│   ├── scheduler.c/h
│   └── station.c/h
├── metrics/
│   ├── logger.c/h
│   └── metrics.c/h
├── patterns/
│   └── factory.c/h

Doc/
└── README.md (este documento)

Enunciado/
└── ... (material de referencia)
```
