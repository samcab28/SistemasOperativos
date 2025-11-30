use std::{
    collections::{HashMap, HashSet},
    net::SocketAddr,
    path::{Path as StdPath, PathBuf},
    sync::Arc,
    time::{Duration, Instant},
};

use axum::{
    extract::{Path, State},
    http::StatusCode,
    response::{IntoResponse, Response},
    routing::{get, post},
    Json, Router,
};

use chrono::Utc;
use parking_lot::{Mutex, RwLock};
use reqwest::Client;
use serde::{Deserialize, Serialize};
use serde_json::json;
use tokio::signal;
use tracing::{error, info};
use uuid::Uuid;

use common::{
    IngestRequest, TopologyDeployment, TopologyMetrics, TopologySpec, TopologyStatus,
    TopologyStatusKind, TopologySubmitResponse, TopologyTearDown, WorkerEventBatch,
    WorkerHeartbeat, WorkerMetrics, WorkerRegisterRequest, WorkerRegisterResponse,
};

// Shared, thread-safe state for the master process.
#[derive(Clone)]
struct AppState {
    inner: Arc<StateInner>,
}

struct StateInner {
    client: Client,
    workers: RwLock<HashMap<String, WorkerRecord>>,
    topologies: RwLock<HashMap<Uuid, TopologyRecord>>,
    // Cursor to keep round-robin worker selection deterministic across calls.
    rr_cursor: Mutex<usize>,
    state_path: PathBuf,
}

impl AppState {
    fn new() -> Self {
        let state_path = PathBuf::from("state/master_state.json");
        if let Some(dir) = state_path.parent() {
            let _ = std::fs::create_dir_all(dir);
        }
        // On boot, reload persisted topologies to avoid losing submitted work.
        let topologies = load_persisted_state(&state_path);
        Self {
            inner: Arc::new(StateInner {
                client: Client::new(),
                workers: RwLock::new(HashMap::new()),
                topologies: RwLock::new(topologies),
                rr_cursor: Mutex::new(0),
                state_path,
            }),
        }
    }

    fn select_worker(&self) -> Option<WorkerRecord> {
        // Pick a non-down worker in round-robin order.
        let workers = self.inner.workers.read();
        // Prefer workers under capacity (active topologies < slots) and lowest load ratio.
        let mut candidates: Vec<_> = workers
            .values()
            .filter(|w| !w.is_down)
            .map(|w| {
                let active = w.topologies.len();
                let load_ratio = (active as f64) / (w.slots as f64).max(1.0);
                (load_ratio, active, w.clone())
            })
            .collect();
        if candidates.is_empty() {
            return None;
        }
        candidates.sort_by(|a, b| a.0.partial_cmp(&b.0).unwrap_or(std::cmp::Ordering::Equal));
        // If all are full, fall back to round-robin to spread the pain.
        let under_capacity: Vec<_> = candidates
            .iter()
            .filter(|(_, active, w)| *active < w.slots)
            .cloned()
            .collect();
        if !under_capacity.is_empty() {
            return Some(under_capacity[0].2.clone());
        }
        let mut cursor = self.inner.rr_cursor.lock();
        let idx = *cursor % candidates.len();
        *cursor = (idx + 1) % candidates.len();
        Some(candidates[idx].2.clone())
    }

    fn get_worker(&self, worker_id: &str) -> Option<WorkerRecord> {
        self.inner.workers.read().get(worker_id).cloned()
    }

    async fn dispatch_topology(
        &self,
        worker: &WorkerRecord,
        deployment: &TopologyDeployment,
    ) -> Result<(), AppError> {
        // Forward the topology to a worker.
        let url = format!("{}/internal/topologies", worker.api_url);
        self.inner
            .client
            .post(url)
            .json(deployment)
            .send()
            .await?
            .error_for_status()?;
        Ok(())
    }

    async fn dispatch_ingest(
        &self,
        worker: &WorkerRecord,
        batch: &WorkerEventBatch,
    ) -> Result<(), AppError> {
        // Forward event batches to the worker that owns the topology.
        let url = format!("{}/internal/ingest", worker.api_url);
        self.inner
            .client
            .post(url)
            .json(batch)
            .send()
            .await?
            .error_for_status()?;
        Ok(())
    }

    async fn dispatch_teardown(
        &self,
        worker: &WorkerRecord,
        teardown: &TopologyTearDown,
    ) -> Result<(), AppError> {
        let url = format!("{}/internal/teardown", worker.api_url);
        self.inner
            .client
            .post(url)
            .json(teardown)
            .send()
            .await?
            .error_for_status()?;
        Ok(())
    }

    fn persist_state(&self) -> Result<(), AppError> {
        // Serialize current topology map so restarts can resume from disk.
        let snapshot = PersistedState {
            topologies: self.inner.topologies.read().clone(),
        };
        let data = serde_json::to_vec_pretty(&snapshot)?;
        if let Some(dir) = self.inner.state_path.parent() {
            std::fs::create_dir_all(dir)?;
        }
        std::fs::write(&self.inner.state_path, data)?;
        Ok(())
    }
}

#[derive(Clone)]
struct WorkerRecord {
    worker_id: String,
    api_url: String,
    slots: usize,
    last_heartbeat: Instant,
    metrics: WorkerMetrics,
    topologies: HashSet<Uuid>,
    is_down: bool,
}

impl WorkerRecord {
    fn new(req: WorkerRegisterRequest) -> Self {
        Self {
            worker_id: req.worker_id,
            api_url: req.api_url,
            slots: req.slots.max(1),
            last_heartbeat: Instant::now(),
            metrics: WorkerMetrics::default(),
            topologies: HashSet::new(),
            is_down: false,
        }
    }
}

#[derive(Clone, Serialize, Deserialize)]
struct TopologyRecord {
    spec: TopologySpec,
    status: TopologyStatusKind,
    worker_id: Option<String>,
    metrics: TopologyMetrics,
    attempt: u32,
    last_error: Option<String>,
    last_dispatch_error: Option<String>,
}

impl TopologyRecord {
    fn new(spec: TopologySpec) -> Self {
        Self {
            spec,
            status: TopologyStatusKind::Accepted,
            worker_id: None,
            metrics: TopologyMetrics::default(),
            attempt: 0,
            last_error: None,
            last_dispatch_error: None,
        }
    }
}

#[derive(Debug)]
struct AppError(anyhow::Error);

impl<E> From<E> for AppError
where
    E: Into<anyhow::Error>,
{
    fn from(value: E) -> Self {
        Self(value.into())
    }
}

impl IntoResponse for AppError {
    fn into_response(self) -> Response {
        (
            StatusCode::INTERNAL_SERVER_ERROR,
            Json(json!({
                "error": self.0.to_string()
            })),
        )
            .into_response()
    }
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter("info")
        .compact()
        .init();

    let state = AppState::new();
    spawn_watchdog(state.clone()); // monitors worker heartbeats

    let app = Router::new()
        .route("/api/v1/workers/register", post(register_worker))
        .route(
            "/api/v1/workers/:worker_id/heartbeat",
            post(worker_heartbeat),
        )
        .route("/api/v1/topologies", post(submit_topology).get(list_topologies))
        .route("/api/v1/topologies/:id", get(get_topology))
        .route("/api/v1/topologies/:id/cancel", post(cancel_topology))
        .route("/api/v1/ingest", post(ingest))
        .route("/api/v1/metrics", get(metrics))
        .with_state(state);

    let addr = SocketAddr::from(([0, 0, 0, 0], 8080));
    info!("Master listening on {}", addr);
    axum::serve(
        tokio::net::TcpListener::bind(addr).await?,
        app.into_make_service(),
    )
    .with_graceful_shutdown(shutdown_signal())
    .await?;

    Ok(())
}

fn spawn_watchdog(state: AppState) {
    // Periodically mark workers as down if heartbeats stop arriving.
    tokio::spawn(async move {
        let mut ticker = tokio::time::interval(Duration::from_secs(3));
        loop {
            ticker.tick().await;
            let expired: Vec<String> = {
                let workers = state.inner.workers.read();
                workers
                    .values()
                    .filter(|w| !w.is_down && w.last_heartbeat.elapsed() > Duration::from_secs(6))
                    .map(|w| w.worker_id.clone())
                    .collect()
            };
            for worker_id in expired {
                mark_worker_down(&state, &worker_id).await;
            }
        }
    });
}

async fn mark_worker_down(state: &AppState, worker_id: &str) {
    info!("Marking worker {} as down", worker_id);
    let affected = {
        let mut workers = state.inner.workers.write();
        if let Some(worker) = workers.get_mut(worker_id) {
            worker.is_down = true;
            worker.topologies.iter().copied().collect::<Vec<_>>()
        } else {
            Vec::new()
        }
    };

    for topo_id in affected {
        if let Err(err) = reschedule_topology(state, topo_id).await {
            error!("Failed to reschedule topology {}: {}", topo_id, err.0);
        }
    }
}

async fn reschedule_topology(state: &AppState, topology_id: Uuid) -> Result<(), AppError> {
    // Reset status and hand the topology to another available worker.
    {
        let mut topologies = state.inner.topologies.write();
        let entry = topologies
            .get_mut(&topology_id)
            .ok_or_else(|| anyhow::anyhow!("Topology {} not found", topology_id))?;
        if matches!(entry.status, TopologyStatusKind::Canceled | TopologyStatusKind::Completed) {
            return Ok(());
        }
        entry.status = TopologyStatusKind::Accepted;
        entry.worker_id = None;
        entry.last_error = Some("Re-scheduling after worker failure".into());
        entry.attempt += 1;
    }
    state.persist_state()?;

    if let Some(worker) = state.select_worker() {
        deploy_topology_to_worker(state, worker, topology_id).await?;
    }
    Ok(())
}

async fn register_worker(
    State(state): State<AppState>,
    Json(req): Json<WorkerRegisterRequest>,
) -> Result<Json<WorkerRegisterResponse>, AppError> {
    info!("Worker {} registered at {}", req.worker_id, req.api_url);
    let mut workers = state.inner.workers.write();
    workers.insert(req.worker_id.clone(), WorkerRecord::new(req));
    Ok(Json(WorkerRegisterResponse { accepted: true }))
}

async fn worker_heartbeat(
    State(state): State<AppState>,
    Path(worker_id): Path<String>,
    Json(heartbeat): Json<WorkerHeartbeat>,
) -> Result<StatusCode, AppError> {
    let mut workers = state.inner.workers.write();
    if let Some(worker) = workers.get_mut(&worker_id) {
        worker.last_heartbeat = Instant::now();
        worker.metrics = heartbeat.metrics;
        worker.is_down = false;
    }
    Ok(StatusCode::NO_CONTENT)
}

async fn submit_topology(
    State(state): State<AppState>,
    Json(spec): Json<TopologySpec>,
) -> Result<Json<TopologySubmitResponse>, AppError> {
    // Accept topology, persist it in memory, and immediately assign to a worker.
    let topology_id = Uuid::new_v4();
    {
        let mut topologies = state.inner.topologies.write();
        topologies.insert(topology_id, TopologyRecord::new(spec.clone()));
    }

    assign_topology(&state, topology_id).await?;

    Ok(Json(TopologySubmitResponse { topology_id }))
}

async fn assign_topology(state: &AppState, topology_id: Uuid) -> Result<(), AppError> {
    let worker = state
        .select_worker()
        .ok_or_else(|| anyhow::anyhow!("No workers registered or available"))?;
    if let Err(err) = deploy_topology_to_worker(state, worker, topology_id).await {
        // Try another worker if possible.
        if let Some(alt_worker) = state.select_worker() {
            deploy_topology_to_worker(state, alt_worker, topology_id).await?;
            return Ok(());
        }
        return Err(err);
    }
    Ok(())
}

async fn deploy_topology_to_worker(
    state: &AppState,
    worker: WorkerRecord,
    topology_id: Uuid,
) -> Result<(), AppError> {
    let (spec, attempt) = {
        let topologies = state.inner.topologies.read();
        let entry = topologies
            .get(&topology_id)
            .ok_or_else(|| anyhow::anyhow!("Topology {} not found", topology_id))?;
        (entry.spec.clone(), entry.attempt)
    };

    let deployment = TopologyDeployment {
        topology_id,
        spec,
        attempt,
    };

    state
        .dispatch_topology(&worker, &deployment)
        .await
        .map_err(|e| {
            let mut topologies = state.inner.topologies.write();
            if let Some(entry) = topologies.get_mut(&topology_id) {
                entry.last_dispatch_error = Some(e.0.to_string());
                entry.attempt += 1;
                entry.status = TopologyStatusKind::Accepted;
                entry.worker_id = None;
            }
            e
        })?;

    {
        let mut workers = state.inner.workers.write();
        if let Some(entry) = workers.get_mut(&worker.worker_id) {
            entry.topologies.insert(topology_id);
        }
    }

    let mut topologies = state.inner.topologies.write();
    if let Some(entry) = topologies.get_mut(&topology_id) {
        entry.status = TopologyStatusKind::Running;
        entry.worker_id = Some(worker.worker_id.clone());
        entry.last_dispatch_error = None;
    }
    drop(topologies);
    state.persist_state()?;
    Ok(())
}

async fn get_topology(
    State(state): State<AppState>,
    Path(id): Path<Uuid>,
) -> Result<Json<TopologyStatus>, AppError> {
    let topologies = state.inner.topologies.read();
    let record = topologies
        .get(&id)
        .ok_or_else(|| anyhow::anyhow!("Topology {} not found", id))?;

    let status = TopologyStatus {
        topology_id: id,
        name: record.spec.name.clone(),
        status: record.status.clone(),
        worker_id: record.worker_id.clone(),
        metrics: record.metrics.clone(),
        last_error: record.last_error.clone(),
        attempt: record.attempt,
    };
    Ok(Json(status))
}

async fn ingest(
    State(state): State<AppState>,
    Json(req): Json<IngestRequest>,
) -> Result<StatusCode, AppError> {
    // Look up the worker for this topology and forward the events.
    let topology = {
        let topologies = state.inner.topologies.read();
        topologies
            .get(&req.topology_id)
            .cloned()
            .ok_or_else(|| anyhow::anyhow!("Topology {} not found", req.topology_id))?
    };

    let worker_id = topology
        .worker_id
        .ok_or_else(|| anyhow::anyhow!("Topology is not assigned to a worker"))?;
    let worker = state
        .get_worker(&worker_id)
        .ok_or_else(|| anyhow::anyhow!("Worker {} unavailable", worker_id))?;

    let batch = WorkerEventBatch {
        topology_id: req.topology_id,
        events: req.events.clone(),
    };

    state.dispatch_ingest(&worker, &batch).await?;

    {
        let mut topologies = state.inner.topologies.write();
        if let Some(entry) = topologies.get_mut(&req.topology_id) {
            entry.metrics.events_ingested += batch.events.len() as u64;
            entry.metrics.events_emitted = entry.metrics.events_ingested;
            entry.metrics.last_checkpoint = Some(Utc::now());
        }
    }
    state.persist_state()?;

    Ok(StatusCode::ACCEPTED)
}

async fn list_topologies(State(state): State<AppState>) -> Result<Json<Vec<TopologyStatus>>, AppError> {
    let topologies = state.inner.topologies.read();
    let mut statuses = Vec::new();
    for (id, record) in topologies.iter() {
        statuses.push(TopologyStatus {
            topology_id: *id,
            name: record.spec.name.clone(),
            status: record.status.clone(),
            worker_id: record.worker_id.clone(),
            metrics: record.metrics.clone(),
            last_error: record.last_error.clone(),
            attempt: record.attempt,
        });
    }
    Ok(Json(statuses))
}

async fn cancel_topology(
    State(state): State<AppState>,
    Path(id): Path<Uuid>,
) -> Result<StatusCode, AppError> {
    let teardown_msg = TopologyTearDown { topology_id: id };
    let worker = {
        let mut topologies = state.inner.topologies.write();
        let entry = topologies
            .get_mut(&id)
            .ok_or_else(|| anyhow::anyhow!("Topology {} not found", id))?;
        entry.status = TopologyStatusKind::Canceled;
        entry.last_error = Some("Canceled by user".into());
        let worker_id = entry.worker_id.clone();
        worker_id
    };

    if let Some(worker_id) = worker.clone() {
        if let Some(worker) = state.get_worker(&worker_id) {
            let _ = state.dispatch_teardown(&worker, &teardown_msg).await;
        }
    }

    {
        let mut workers = state.inner.workers.write();
        if let Some(worker_id) = worker {
            if let Some(w) = workers.get_mut(&worker_id) {
                w.topologies.remove(&id);
            }
        }
    }

    state.persist_state()?;
    Ok(StatusCode::NO_CONTENT)
}

async fn metrics(State(state): State<AppState>) -> Result<Json<MasterMetrics>, AppError> {
    let worker_metrics: Vec<_> = state
        .inner
        .workers
        .read()
        .values()
        .map(|w| WorkerSnapshot {
            worker_id: w.worker_id.clone(),
            is_down: w.is_down,
            metrics: w.metrics.clone(),
            topologies: w.topologies.iter().copied().collect(),
        })
        .collect();
    let topo_status: Vec<_> = state
        .inner
        .topologies
        .read()
        .iter()
        .map(|(id, record)| TopologyStatus {
            topology_id: *id,
            name: record.spec.name.clone(),
            status: record.status.clone(),
            worker_id: record.worker_id.clone(),
            metrics: record.metrics.clone(),
            last_error: record.last_error.clone(),
            attempt: record.attempt,
        })
        .collect();

    Ok(Json(MasterMetrics {
        workers: worker_metrics,
        topologies: topo_status,
    }))
}

async fn shutdown_signal() {
    let ctrl_c = async {
        signal::ctrl_c()
            .await
            .expect("failed to install Ctrl+C handler");
    };

    #[cfg(unix)]
    let terminate = async {
        signal::unix::signal(signal::unix::SignalKind::terminate())
            .expect("failed to install signal handler")
            .recv()
            .await;
    };

    #[cfg(not(unix))]
    let terminate = std::future::pending::<()>();

    tokio::select! {
        _ = ctrl_c => {},
        _ = terminate => {},
    }
    info!("Shutdown signal received");
}

#[derive(Serialize, Deserialize)]
struct PersistedState {
    topologies: HashMap<Uuid, TopologyRecord>,
}

fn load_persisted_state(path: &StdPath) -> HashMap<Uuid, TopologyRecord> {
    if let Ok(data) = std::fs::read(path) {
        if let Ok(state) = serde_json::from_slice::<PersistedState>(&data) {
            return state.topologies;
        }
    }
    HashMap::new()
}

#[derive(Serialize)]
struct MasterMetrics {
    workers: Vec<WorkerSnapshot>,
    topologies: Vec<TopologyStatus>,
}

#[derive(Serialize)]
struct WorkerSnapshot {
    worker_id: String,
    is_down: bool,
    metrics: WorkerMetrics,
    topologies: Vec<Uuid>,
}
