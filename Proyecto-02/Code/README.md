# Motor de Streaming - Ruta B (Rust)

Implementación simplificada de un motor de procesamiento distribuido de eventos inspirado en Flink. El proyecto incluye un `master` coordinador, `workers` que ejecutan las topologías y un `client` CLI para enviar topologías/ingesta/consultas.

## Requisitos

- Rust 1.78+ y `cargo`.
- Docker (opcional) para levantar el entorno de demo.

## Estructura

```
code/
├── Cargo.toml
├── master/        # API HTTP + planificador
├── worker/        # Ejecutor de operadores streaming
├── client/        # CLI para submit/status/ingest
├── common/        # Modelos compartidos (topologías, mensajes, estados)
├── docs/          # Arquitectura y ejemplos
├── scripts/       # Utilidades (demo, tests)
└── docker-compose.yml
```

## Uso local

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

```
make compose-up
```

Levanta `master` + `worker`. Usa el CLI desde el host para enviar topologías o un contenedor adicional.

## Scripts

- `make fmt` / `make lint` / `make test`
- `scripts/demo.sh`: flujo completo (levanta master/worker locales, envía topología ejemplo e inyecta eventos).

## Documentación

Consulta `docs/ARCHITECTURE.md` para el diseño detallado, mensajes y protocolos, y `docs/examples/` para topologías y datasets mínimos. Cada módulo incluye comentarios y logs estructurados vía `tracing`.
