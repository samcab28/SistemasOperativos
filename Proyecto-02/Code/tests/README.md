# Documentación de la Suite de Pruebas

Este directorio contiene la suite completa de pruebas para el motor de streaming distribuido.

## Estructura de Pruebas

```
tests/
├── README.md                 # Este archivo
├── e2e_test.rs              # Pruebas de integración end-to-end
├── load_test.js             # Script de pruebas de carga con k6
└── throughput_test.js       # Prueba de estrés de alto rendimiento con k6
```

## Ejecutar Pruebas

### Inicio Rápido

```bash
# Ejecutar todas las pruebas básicas
make test

# Ejecutar suite completa de pruebas (incluye E2E si los servicios están corriendo)
make test-all

# Ejecutar pruebas de carga
make load-test

# Ejecutar suite de benchmarks
make benchmark
```

### Pruebas Unitarias

Las pruebas unitarias se encuentran en el directorio `src/` o `tests/` de cada crate.

```bash
# Ejecutar unitarias pruebas de un paquete específico
cargo test -p common
cargo test -p worker
cargo test -p master
```

### Pruebas de Integración

Las pruebas de integración verifican las interacciones entre componentes.

```bash
# Ejecutar todas las pruebas de integración
make test-integration

# Ejecutar una prueba de integración específica
cargo test --test operators_test
cargo test --test checkpoint_test
```

### Pruebas End-to-End

Las pruebas E2E requieren que los servicios master y worker estén corriendo.

**Configuración:**
```bash
# Terminal 1: Iniciar master
make run-master

# Terminal 2: Iniciar worker
make run-worker

# Terminal 3: Ejecutar pruebas E2E
make test-e2e
```

## Pruebas de Carga con k6

### Prerequisitos

Instalar k6:
```bash
# macOS
brew install k6

# Ubuntu/Debian
sudo gpg -k
sudo gpg --no-default-keyring --keyring /usr/share/keyrings/k6-archive-keyring.gpg --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys C5AD17C747E3415A3642D57D77C6C491D6AC1D69
echo "deb [signed-by=/usr/share/keyrings/k6-archive-keyring.gpg] https://dl.k6.io/deb stable main" | sudo tee /etc/apt/sources.list.d/k6.list
sudo apt-get update
sudo apt-get install k6

# Windows
choco install k6
```

### Ejecutar Pruebas de Carga

```bash
# Prueba de carga básica (incremento gradual)
k6 run tests/load_test.js

# Prueba de alto rendimiento (60s, objetivo de 5k+ eventos/s)
k6 run tests/throughput_test.js

# Parámetros personalizados
k6 run --vus 10 --duration 60s tests/load_test.js
```

### Interpretar Resultados

k6 proporciona métricas incluyendo:
- **http_req_duration**: Latencia de solicitudes (p95, p99)
- **http_req_failed**: Tasa de solicitudes fallidas
- **topology_submit_success**: Tasa de éxito en envío de topologías
- **ingest_success**: Tasa de éxito en ingesta de eventos
- **events_processed**: Total de eventos procesados
- **throughput_eps**: Eventos por segundo

## Benchmarking

### Suite Completa de Benchmarks

```bash
make benchmark
```

Esto hará:
1. Construir binarios de release
2. Iniciar master + 2 workers
3. Ejecutar tres escenarios de benchmark:
   - Rendimiento de topología única (30s)
   - Distribución de carga con múltiples topologías (30s)
   - Prueba de estrés de alto rendimiento (60s)
4. Generar reporte en `benchmarks/REPORT.md`

### Escenarios de Benchmark

#### 1. Rendimiento de Topología Única
- **Objetivo**: Medir rendimiento de un solo pipeline
- **Configuración**: 1 topología, 1 VU, 30s
- **Métricas**: Eventos/s, distribución de latencia

#### 2. Distribución de Carga
- **Objetivo**: Probar programación multi-topología
- **Configuración**: Múltiples topologías, 5 VUs, 30s
- **Métricas**: Distribución de workers, balanceo

#### 3. Prueba de Estrés
- **Objetivo**: Capacidad máxima de rendimiento
- **Configuración**: 1 topología, 5 VUs, lotes de 100 eventos
- **Métricas**: Rendimiento pico, latencia P95/P99

### Rendimiento Esperado

En un sistema moderno (4 núcleos, 8GB RAM):
- **Rendimiento**: 5,000-10,000 eventos/s
- **Latencia P95**: < 500ms para ingesta
- **Latencia P99**: < 1000ms para ingesta

## Organización de Pruebas

### common/tests/
- `models_test.rs`: Serialización/deserialización de modelos de datos

### worker/tests/
- `operators_test.rs`: Lógica de operadores individuales
- `checkpoint_test.rs`: Persistencia y recuperación de checkpoints

### master/tests/
- `integration_test.rs`: Endpoints de API del master y programación

### tests/ (raíz del workspace)
- `e2e_test.rs`: Pruebas de integración del sistema completo
- `*.js`: Scripts de pruebas de carga con k6

## Escribir Nuevas Pruebas

### Ejemplo de Prueba Unitaria

```rust
#[test]
fn test_nueva_funcionalidad() {
    let entrada = crear_entrada_de_prueba();
    let resultado = mi_funcion(entrada);
    assert_eq!(resultado, salida_esperada);
}
```

### Ejemplo de Prueba de Integración

```rust
#[tokio::test]
async fn test_interaccion_componentes() {
    let componente_a = ComponenteA::new();
    let componente_b = ComponenteB::new();
    
    let resultado = componente_a.interactuar_con(componente_b).await;
    assert!(resultado.is_ok());
}
```

### Ejemplo de Prueba E2E

```rust
#[tokio::test]
#[ignore]  // Solo ejecutar cuando se solicite explícitamente
async fn test_flujo_end_to_end() {
    let client = reqwest::Client::new();
    
    // Enviar topología
    let topologia = crear_topologia_de_prueba();
    let response = client.post(url).json(&topologia).send().await.unwrap();
    let topology_id = response.json::<TopologySubmitResponse>().await.unwrap().topology_id;
    
    // Ingerir eventos
    // ...
    
    // Verificar resultados
    // ...
}
```

## Integración Continua

Para pipelines de CI/CD:

```bash
# Ejecutar todas las verificaciones
make ci

# O paso a paso
make fmt
make lint
make test
```

## Solución de Problemas

### Pruebas Colgadas
- Verificar si los puertos 8080 (master) y 9001-9002 (workers) están disponibles
- Matar procesos existentes: `pkill -f "target/.*master"`

### Pruebas E2E Fallando
- Asegurar que master y worker estén corriendo
- Revisar logs en el directorio `state/`
- Verificar conectividad de red: `curl http://127.0.0.1:8080/api/v1/metrics`

### Pruebas k6 Fallando
- Instalar k6 correctamente
- Verificar que el master sea accesible
- Verificar que al menos un worker esté registrado

### Cobertura Baja
- Agregar pruebas para módulos sin cobertura
- Enfocarse en rutas críticas (operadores, programación, tolerancia a fallos)

## Recursos

- [Guía de Testing de Rust](https://doc.rust-lang.org/book/ch11-00-testing.html)
- [Documentación de k6](https://k6.io/docs/)
- [cargo-tarpaulin](https://github.com/xd009642/tarpaulin)