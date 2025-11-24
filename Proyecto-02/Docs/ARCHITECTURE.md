# Arquitectura - Motor de Streaming (Ruta B)

## Componentes

| Componente | Responsabilidad principal |
| ---------- | ------------------------- |
| `master`   | API HTTP (`axum`) para registrar workers, recibir topologías y manejar ingesta. Mantiene el catálogo de topologías, planifica (round-robin) y coordina reintentos ante fallos. |
| `worker`   | Nodo ejecutor. Expone endpoints internos para desplegar topologías y recibir lotes de eventos. Cada topología se ejecuta en un `PipelineEngine` (Tokio) con operadores encadenados, ventanas temporales y checkpointing a disco. Reporta heartbeats al master. |
| `client`   | CLI (`clap`) para interacción humana: submit/topologías, status, ingest. Serializa mensajes usando las estructuras del crate `common`. |
| `common`   | Modelos compartidos (esquema de topologías, operadores, ventanas, mensajes de registro/heartbeat/ingest). Evita duplicación entre binarios. |

## Flujo de Vida de una Topología

1. **Submit**: el cliente envía un JSON (`TopologySpec`) al `master` (`POST /api/v1/topologies`). El master persiste el estado inicial (Accepted) y elige un worker vía round-robin consciente de carga (slots registrados).
2. **Deployment**: el master envía `TopologyDeployment` al worker destino (`POST /internal/topologies`). El worker crea los canales y tareas asíncronas necesarias para procesar eventos y confirma con `201`.
3. **Ingesta**: `POST /api/v1/ingest` acepta lotes (`IngestRequest`). El master reenvía los eventos al worker dueño (`POST /internal/ingest`) y actualiza métricas básicas.
4. **Ejecución**: el `PipelineEngine` procesa los eventos en serie (o con paralelismo configurable en el futuro) aplicando operadores `map`, `filter`, `flat_map`, `key_by` y `window_aggregate`. El último operador suele ser `sink_log`, que registra los resultados.
5. **Checkpoint**: operadores de ventana generan snapshots periódicos (`state/<topology>/<operator>/checkpoint-*.json`) con las métricas de ventanas activas. Sirve como evidencia de estado y podría usarse para recovery ampliado.
6. **Observabilidad**: heartbeats cada 2s (`WorkerHeartbeat`) incluyen número de topologías activas y backlog. El master expone `GET /api/v1/topologies/{id}` con estado + métricas.
7. **Fault-Tolerance**: si el master deja de recibir heartbeats por >6s, marca el worker `DOWN`, libera sus topologías y las replanifica (nuevo intento + nuevo worker). El worker receptor obtiene el `TopologySpec` completo y puede reconstruir estado desde checkpoints futuros.

## Operadores Soportados

Todos los operadores trabajan sobre eventos JSON (objetos). El timestamp se toma de cada evento (`timestamp` o `ts`, RFC3339).

| Operador | Descripción | Configuración principal |
| -------- | ----------- | ----------------------- |
| `map`        | Aplica transformaciones simples (`to_lower`, `to_upper`, `trim`, `prefix`, `suffix`) sobre un campo string. | `field`, `transform`. |
| `filter`     | Filtra eventos según predicado (`equals`, `not_equals`, `greater_than`, `less_than`, `contains`, `exists`). | `field`, `predicate`. |
| `flat_map`   | Expande un campo array o string (split) en múltiples eventos clonados. | `field`, `separator`. |
| `key_by`     | Define la clave de partición para operadores posteriores. | `field`. |
| `window_aggregate` | Ventanas temporales (tumbling, `slide_ms` opcional) con agregaciones `count`, `sum`, `average`. Requiere `key_field` y opcional `value_field`. Incluye checkpointing periódico. | `window.length_ms`, `window.slide_ms`, `checkpoint_interval_ms`, `aggregator`, `value_field`. |
| `sink_log`   | Fin de la tubería; emite resultados via `tracing::debug!`. Ideal para demos o piping a otro sistema. |

## Coordinación y Protocolos

### API Master

- `POST /api/v1/workers/register` → registro inicial (`WorkerRegisterRequest`).
- `POST /api/v1/workers/{id}/heartbeat` → métricas periódicas (`WorkerHeartbeat`).
- `POST /api/v1/topologies` → recibe `TopologySpec` y responde con `TopologySubmitResponse`.
- `GET /api/v1/topologies/{id}` → `TopologyStatus`.
- `POST /api/v1/ingest` → lotes `IngestRequest`.

### API Worker Interna

- `POST /internal/topologies` → despliegue (`TopologyDeployment`).
- `POST /internal/ingest` → lotes `WorkerEventBatch`.

### Persistencia

- El `master` mantiene su estado en memoria (para la versión académica). Puede persistirse en sqlite fácilmente serializando `TopologyRecord`.
- Los `workers` guardan checkpoints JSON por operador en `state/<topology>/<operator>/checkpoint-*.json`.

## Planificación y Tolerancia a Fallos

- **Round-Robin con conciencia de slots**: se selecciona el siguiente worker registrado; el campo `slots` sirve para decidir la carga (futuras mejoras podrían incorporar métrica real).
- **Heartbeats**: 2s. A los 6s sin respuesta, el master marca `DOWN` y reintenta las topologías asignadas (`attempt++`). La reasignación reusa la misma definición de topología.
- **Reintentos**: mínimo 1 reintento (el master vuelve a `Accepted` y despliega de nuevo). El worker es idempotente porque cada despliegue crea nuevas estructuras y la ingesta es coordinada por el master.
- **Checkpointing**: best-effort; se persisten todas las ventanas en memoria sin pausar la ejecución. Los archivos sirven como snapshot para auditoría.

## Observabilidad

- `tracing` en master/worker con niveles `info/debug`.
- Métricas mínimas: eventos ingeridos/emitted por topología (master), backlog y #topologías activas por worker.
- TODO: exponer `/metrics` Prometheus y mejorar métricas de CPU/memoria (hooks listos en `WorkerMetrics`).

## Tests

- Unit tests sugeridos (pendiente de implementar en esta iteración) para operadores (`MapOperator`, `FilterOperator`, `WindowOperator`).
- Integración: levantar master + worker (ver `scripts/demo.sh`) y ejecutar pipeline ejemplo.
- End-to-end: `docker-compose up` + CLI `submit`/`ingest`.

## Mejoras Futuras

- Persistencia del master (sqlite) + recuperación tras reinicio.
- Canales entre workers para ejecutar operadores en nodos distintos (actualmente una topología corre en un único worker, aunque múltiples topologías pueden distribuirse).
- Backpressure real y métricas de recursos del sistema (`sysinfo`).
- REST `GET /api/v1/metrics` y `POST /api/v1/topologies/{id}/cancel`.

## Actualizado (implementado)
- Master persiste topologias en state/master_state.json y ahora expone GET /api/v1/topologies y POST /api/v1/topologies/{id}/cancel.
- Nuevo GET /api/v1/metrics con snapshot de workers (backlog y CPU/mem) y topologias.
- El worker restaura ventanas desde el checkpoint mas reciente al desplegar una topologia.
- Heartbeats envian CPU/mem reales usando sysinfo.

