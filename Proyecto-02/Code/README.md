# Motor de Streaming - Ruta B (Rust)

Implementación simplificada de un motor de procesamiento distribuido de eventos inspirado en Flink. El proyecto incluye un `master` coordinador, `workers` que ejecutan las topologías y un `client` CLI para enviar topologías/ingesta/consultas.

## Requisitos

- Rust 1.78+ y `cargo`.
- Make (para usar los comandos del Makefile).
- Docker (opcional) para levantar el entorno de demo.
- k6 (opcional) para pruebas de carga.
- cargo-tarpaulin (opcional) para reportes de cobertura.

## Estructura

```
code/
├── Cargo.toml
├── Makefile           # Comandos de desarrollo y CI
├── master/            # API HTTP + planificador
├── worker/            # Ejecutor de operadores streaming
├── client/            # CLI para submit/status/ingest
├── common/            # Modelos compartidos (topologías, mensajes, estados)
├── docs/              # Arquitectura y ejemplos
├── scripts/           # Utilidades (demo, tests)
└── docker-compose.yml
```

## Comandos Make

Ejecuta `make help` para ver todos los comandos disponibles.

### Compilación

```bash
# Compilar todo el workspace
make build

# Compilar versión release
make build-release
```

### Desarrollo

```bash
# Formatear código
make fmt

# Ejecutar linter (clippy)
make lint

# Workflow completo de desarrollo (fmt + lint + test)
make dev
```

### Testing

```bash
# Tests básicos (unit + integration)
make test

# Solo tests unitarios
make test-unit

# Solo tests de integración
make test-integration

# Tests end-to-end (requiere master corriendo)
make test-e2e

# Suite completa de tests
make test-all

# Pruebas de carga con k6
make load-test

# Suite de benchmarks
make benchmark

# Generar reporte de cobertura
make coverage
```

### Limpieza

```bash
# Limpiar artefactos de build y directorios temporales
make clean
```

### CI

```bash
# Workflow de CI (fmt + lint + test-all)
make ci
```

## Uso local

### Opción 1: Usando Make

1. **Compilar todo**

   ```bash
   make build
   ```

2. **Levantar el master**

   ```bash
   make run-master
   ```

3. **Levantar worker(s)** (en otra terminal)

   ```bash
   make run-worker
   # O para levantar un segundo worker:
   make run-worker-2
   ```

4. **Enviar una topología**

   ```bash
   make submit TOPOLOGY=docs/examples/log_topology.json
   ```

5. **Consultar estado**

   ```bash
   make status TOPOLOGY_ID=<id>
   ```

6. **Inyectar eventos**

   ```bash
   make ingest TOPOLOGY_ID=<id> FILE=docs/examples/logs.jsonl
   ```

7. **Ver métricas**

   ```bash
   make metrics
   ```

8. **Cancelar topología**
   ```bash
   make cancel TOPOLOGY_ID=<id>
   ```

### Opción 2: Usando cargo directamente

1. **Compilar todo**

   ```bash
   cargo build --workspace
   ```

2. **Levantar el master**

   ```bash
   cargo run -p master
   ```

3. **Levantar al menos un worker**

   ```bash
   cargo run -p worker -- \
     --worker-id worker-1 \
     --bind 127.0.0.1:9001 \
     --master-url http://127.0.0.1:8080 \
     --slots 2 \
     --advertise-url http://127.0.0.1:9001
   ```

   > Si `bind` usa `0.0.0.0` o una IP privada, especifica `--advertise-url` con la URL accesible para el master.

4. **Enviar una topología**

   ```bash
   cargo run -p client -- \
     --master-url http://127.0.0.1:8080 \
     submit-topology docs/examples/log_topology.json
   ```

5. **Consultar estado**

   ```bash
   cargo run -p client -- --master-url http://127.0.0.1:8080 status <TOPOLOGY_ID>
   ```

6. **Inyectar eventos (JSONL con timestamp RFC3339)**

   ```bash
   cargo run -p client -- --master-url http://127.0.0.1:8080 ingest <TOPOLOGY_ID> \
     --file docs/examples/logs.jsonl
   ```

7. **Cancelar o listar topologías**

   ```bash
   # listar todas
   curl http://127.0.0.1:8080/api/v1/topologies
   # cancelar una
   curl -X POST http://127.0.0.1:8080/api/v1/topologies/<TOPOLOGY_ID>/cancel
   ```

8. **Métricas**
   ```bash
   curl http://127.0.0.1:8080/api/v1/metrics
   ```

## Docker Compose

```bash
# Usando Make
make compose-up
make compose-down

# O directamente
docker compose up --build
docker compose down
```

Levanta `master` + `worker`. Usa el CLI desde el host para enviar topologías o un contenedor adicional.

## Scripts

- `make fmt` / `make lint` / `make test`
- `scripts/demo.sh`: flujo completo (levanta master/worker locales, envía topología ejemplo e inyecta eventos).
- `scripts/run_tests.sh`: suite completa de tests.
- `scripts/load_test.sh`: pruebas de carga con k6.
- `scripts/benchmark.sh`: suite de benchmarks.

## Documentación

Consulta `docs/ARCHITECTURE.md` para el diseño detallado, mensajes y protocolos, y `docs/examples/` para topologías y datasets mínimos. Cada módulo incluye comentarios y logs estructurados vía `tracing`.

## Benchmarking

Consulta tests/benchmark/Report.md para detalles sobre los resultados de las pruebas de benchmarking realizadas en el sistema.
