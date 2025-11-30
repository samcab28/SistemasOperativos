use std::{
    collections::HashMap,
    net::SocketAddr,
    path::PathBuf,
    sync::{
        atomic::{AtomicUsize, Ordering},
        atomic::{AtomicU64},
        Arc,
    },
    time::Duration,
};

use axum::{
    extract::State,
    http::StatusCode,
    response::{IntoResponse, Response},
    routing::post,
    Json, Router,
};
use clap::Parser;
use parking_lot::RwLock;
use reqwest::Client;
use sysinfo::System;
use tokio::signal;
use tracing::{error, info};
use uuid::Uuid;

use common::{
    TopologyDeployment, TopologyTearDown, WorkerEventBatch, WorkerHeartbeat, WorkerMetrics,
    WorkerRegisterRequest,
};

use pipeline::{PipelineEngine, StreamingEvent};

mod pipeline;

#[derive(Parser, Debug)]
#[command(author, version, about = "Mini Flink-like worker node")]
struct Args {
    #[arg(long, default_value = "http://127.0.0.1:8080")]
    master_url: String,
    #[arg(long, default_value = "0.0.0.0:9001")]
    bind: SocketAddr,
    #[arg(long)]
    worker_id: String,
    #[arg(long, default_value_t = 2)]
    slots: usize,
    #[arg(long, default_value = "./state")]
    state_dir: PathBuf,
    #[arg(
        long,
        help = "URL accesible para que el master envíe tareas (por defecto http://<bind>)"
    )]
    advertise_url: Option<String>,
}

#[derive(Clone)]
// Shared state consumed by HTTP handlers and background tasks.
struct WorkerState {
    worker_id: String,
    master_url: String,
    client: Client,
    slots: usize,
    state_dir: PathBuf,
    topologies: Arc<RwLock<HashMap<Uuid, TopologyRuntime>>>,
    bind: SocketAddr,
    advertise_url: Option<String>,
    system: Arc<parking_lot::Mutex<System>>,
    throughput_state: Arc<parking_lot::Mutex<ThroughputSnapshot>>,
}

#[derive(Clone)]
// Handle to a running topology: channel to push events plus small metadata.
struct TopologyRuntime {
    spec: common::TopologySpec,
    tx: tokio::sync::mpsc::Sender<StreamingEvent>,
    pending: Arc<AtomicUsize>,
    processed: Arc<AtomicU64>,
    attempt: u32,
}

struct ThroughputSnapshot {
    total: u64,
    instant: std::time::Instant,
}

impl ThroughputSnapshot {
    fn new() -> Self {
        Self {
            total: 0,
            instant: std::time::Instant::now(),
        }
    }
}

impl Default for ThroughputSnapshot {
    fn default() -> Self {
        Self::new()
    }
}

impl WorkerState {
    fn new(args: &Args) -> Self {
        Self {
            worker_id: args.worker_id.clone(),
            master_url: args.master_url.clone(),
            client: Client::new(),
            slots: args.slots,
            state_dir: args.state_dir.clone(),
            topologies: Arc::new(RwLock::new(HashMap::new())),
            bind: args.bind,
            advertise_url: args.advertise_url.clone(),
            system: Arc::new(parking_lot::Mutex::new(System::new_all())),
            throughput_state: Arc::new(parking_lot::Mutex::new(ThroughputSnapshot::default())),
        }
    }

    fn ensure_dirs(&self) -> anyhow::Result<()> {
        // Prepare local state dir for checkpoints.
        std::fs::create_dir_all(&self.state_dir)?;
        Ok(())
    }

    fn attach_runtime(
        &self,
        topology_id: Uuid,
        spec: common::TopologySpec,
        attempt: u32,
    ) -> anyhow::Result<()> {
        // Create channels and spawn the pipeline task for this topology.
        let (tx, rx) = tokio::sync::mpsc::channel(2048);
        let pending = Arc::new(AtomicUsize::new(0));
        let processed = Arc::new(AtomicU64::new(0));
        let engine = PipelineEngine::new(topology_id, attempt, spec.clone(), &self.state_dir)?;
        tokio::spawn(run_pipeline(
            engine,
            rx,
            pending.clone(),
            processed.clone(),
        ));
        self.topologies.write().insert(
            topology_id,
            TopologyRuntime {
                spec,
                tx,
                pending,
                processed,
                attempt,
            },
        );
        Ok(())
    }

    async fn enqueue_events(
        &self,
        topology_id: Uuid,
        events: Vec<common::EventPayload>,
    ) -> Result<(), WorkerError> {
        // Push incoming events into the pipeline channel.
        let runtime = self
            .topologies
            .read()
            .get(&topology_id)
            .cloned()
            .ok_or_else(|| WorkerError(anyhow::anyhow!("Topology {} not found", topology_id)))?;

        let tx = runtime.tx.clone();
        for event in events {
            runtime.pending.fetch_add(1, Ordering::SeqCst);
            let streaming_event = StreamingEvent::try_from(event)?;
            tx.send(streaming_event).await.map_err(|_| {
                WorkerError(anyhow::anyhow!(
                    "Topology {} channel closed",
                    topology_id
                ))
            })?;
        }
        Ok(())
    }

    async fn register_with_master(&self) -> anyhow::Result<()> {
        let req = WorkerRegisterRequest {
            worker_id: self.worker_id.clone(),
            api_url: self
                .advertise_url
                .clone()
                .unwrap_or_else(|| format!("http://{}", self.bind)),
            slots: self.slots,
        };
        self.client
            .post(format!("{}/api/v1/workers/register", self.master_url))
            .json(&req)
            .send()
            .await?
            .error_for_status()?;
        Ok(())
    }

    fn gather_metrics(&self) -> WorkerMetrics {
        // Basic backlog, plus host CPU/memory via sysinfo.
        let topologies = self.topologies.read();
        let active = topologies.len();
        let queue_depth = topologies
            .values()
            .map(|rt| rt.pending.load(Ordering::Relaxed))
            .sum();
        let processed_total: u64 = topologies
            .values()
            .map(|rt| rt.processed.load(Ordering::Relaxed))
            .sum();
        let throughput_eps = {
            let mut snap = self.throughput_state.lock();
            let now = std::time::Instant::now();
            let delta_events = processed_total.saturating_sub(snap.total);
            let delta_secs = (now.duration_since(snap.instant).as_secs_f64()).max(1e-3);
            snap.total = processed_total;
            snap.instant = now;
            delta_events as f64 / delta_secs
        };
        let mut sys = self.system.lock();
        sys.refresh_cpu_all();
        sys.refresh_memory();
        let cpu_pct = sys
            .cpus()
            .iter()
            .map(|c| c.cpu_usage() as f64)
            .sum::<f64>()
            / sys.cpus().len().max(1) as f64;
        let mem_bytes = (sys.total_memory() - sys.available_memory()) * 1024;
        WorkerMetrics {
            cpu_pct,
            mem_bytes,
            active_topologies: active,
            queue_depth,
            throughput_eps,
        }
    }

    fn teardown_topology(&self, topology_id: Uuid) {
        self.topologies.write().remove(&topology_id);
    }
}

async fn run_pipeline(
    mut engine: PipelineEngine,
    mut rx: tokio::sync::mpsc::Receiver<StreamingEvent>,
    pending: Arc<AtomicUsize>,
    processed: Arc<AtomicU64>,
) {
    // Core loop: receive events from channel and process through operators.
    while let Some(event) = rx.recv().await {
        if let Err(err) = engine.process(event).await {
            error!("Pipeline error: {}", err);
        }
        pending.fetch_sub(1, Ordering::SeqCst);
        processed.fetch_add(1, Ordering::SeqCst);
    }
    engine.shutdown().await;
}

#[derive(Debug)]
struct WorkerError(anyhow::Error);

impl<E> From<E> for WorkerError
where
    E: Into<anyhow::Error>,
{
    fn from(value: E) -> Self {
        Self(value.into())
    }
}

impl IntoResponse for WorkerError {
    fn into_response(self) -> Response {
        (
            StatusCode::INTERNAL_SERVER_ERROR,
            format!("{{\"error\":\"{}\"}}", self.0),
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

    let args = Args::parse();
    let worker_state = WorkerState::new(&args);
    worker_state.ensure_dirs()?;
    worker_state.register_with_master().await?;

    spawn_heartbeat(worker_state.clone());

    let router = Router::new()
        .route("/internal/topologies", post(deploy_topology))
        .route("/internal/ingest", post(ingest_events))
        .route("/internal/teardown", post(teardown_topology))
        .with_state(worker_state.clone());

    info!("Worker listening on {}", args.bind);
    axum::serve(
        tokio::net::TcpListener::bind(args.bind).await?,
        router.into_make_service(),
    )
    .with_graceful_shutdown(worker_shutdown_signal())
    .await?;

    Ok(())
}

fn spawn_heartbeat(state: WorkerState) {
    // Send heartbeats to master every few seconds.
    tokio::spawn(async move {
        let mut interval = tokio::time::interval(Duration::from_secs(2));
        loop {
            interval.tick().await;
            let metrics = state.gather_metrics();
            let req = WorkerHeartbeat {
                worker_id: state.worker_id.clone(),
                metrics,
            };
            if let Err(err) = state
                .client
                .post(format!(
                    "{}/api/v1/workers/{}/heartbeat",
                    state.master_url, state.worker_id
                ))
                .json(&req)
                .send()
                .await
                .and_then(|res| res.error_for_status())
            {
                error!("Failed to send heartbeat: {}", err);
            }
        }
    });
}

async fn deploy_topology(
    State(state): State<WorkerState>,
    Json(deployment): Json<TopologyDeployment>,
) -> Result<StatusCode, WorkerError> {
    info!(
        "Deploying topology {} ({})",
        deployment.topology_id, deployment.spec.name
    );
    state.attach_runtime(deployment.topology_id, deployment.spec, deployment.attempt)?;
    Ok(StatusCode::CREATED)
}

async fn ingest_events(
    State(state): State<WorkerState>,
    Json(batch): Json<WorkerEventBatch>,
) -> Result<StatusCode, WorkerError> {
    state
        .enqueue_events(batch.topology_id, batch.events)
        .await?;
    Ok(StatusCode::ACCEPTED)
}

async fn teardown_topology(
    State(state): State<WorkerState>,
    Json(msg): Json<TopologyTearDown>,
) -> Result<StatusCode, WorkerError> {
    info!("Tearing down topology {}", msg.topology_id);
    state.teardown_topology(msg.topology_id);
    Ok(StatusCode::NO_CONTENT)
}

async fn worker_shutdown_signal() {
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
}
