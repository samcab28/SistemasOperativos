use std::sync::OnceLock;
use std::time::Duration;
use std::sync::mpsc::{self, Receiver, Sender};

use crate::config::ServerConfig;
use crate::error::{ServerError, ServerResult};

use super::worker_pool::{Job, WorkerPool};
use super::worker_types::CpuWorkerConfig;

pub struct WorkerManager {
    cpu_pool: WorkerPool,
    cpu_timeout: Duration,
    io_pool: WorkerPool,
    io_timeout: Duration,
}

impl WorkerManager {
    pub fn new(config: &ServerConfig) -> Self {
        // Configure CPU pool parameters from ServerConfig
        let workers = config.workers.get_workers("cpu");
        let depth = config.queues.get_depth("cpu");
        let timeout = config.timeouts.cpu_timeout;

        let cpu_cfg = CpuWorkerConfig { workers, queue_depth: depth, timeout };

        let cpu_pool = WorkerPool::new("cpu", cpu_cfg.workers, cpu_cfg.queue_depth);

        // IO pool mirrors CPU settings by default but uses IO timeout and separate sizing
        let io_workers = config.workers.get_workers("io");
        let io_depth = config.queues.get_depth("io");
        let io_timeout = config.timeouts.io_timeout;
        let io_pool = WorkerPool::new("io", io_workers, io_depth);

        Self { cpu_pool, cpu_timeout: cpu_cfg.timeout, io_pool, io_timeout }
    }

    pub fn cpu_timeout(&self) -> Duration { self.cpu_timeout }

    /// Submit a CPU task that returns a value via oneshot channel.
    pub fn submit_cpu<F, R>(&self, f: F) -> ServerResult<R>
    where
        F: FnOnce() -> R + Send + 'static,
        R: Send + 'static,
    {
        let (tx, rx): (Sender<R>, Receiver<R>) = mpsc::channel();

        let job: Job = Box::new(move || {
            let result = f();
            // Ignore send errors if receiver dropped
            let _ = tx.send(result);
        });

        self.cpu_pool.submit(job)?;

        match rx.recv_timeout(self.cpu_timeout) {
            Ok(value) => Ok(value),
            Err(mpsc::RecvTimeoutError::Timeout) => Err(ServerError::Timeout),
            Err(_) => Err(ServerError::internal("worker channel closed")),
        }
    }

    /// Submit an IO task that returns a value via oneshot channel.
    pub fn submit_io<F, R>(&self, f: F) -> ServerResult<R>
    where
        F: FnOnce() -> R + Send + 'static,
        R: Send + 'static,
    {
        let (tx, rx): (Sender<R>, Receiver<R>) = mpsc::channel();

        let job: Job = Box::new(move || {
            let result = f();
            let _ = tx.send(result);
        });

        self.io_pool.submit(job)?;

        match rx.recv_timeout(self.io_timeout) {
            Ok(value) => Ok(value),
            Err(mpsc::RecvTimeoutError::Timeout) => Err(ServerError::Timeout),
            Err(_) => Err(ServerError::internal("worker channel closed")),
        }
    }
}

static GLOBAL_MANAGER: OnceLock<WorkerManager> = OnceLock::new();

pub fn init_global_worker_manager(config: &ServerConfig) {
    let _ = GLOBAL_MANAGER.set(WorkerManager::new(config));
}

pub fn worker_manager() -> &'static WorkerManager {
    GLOBAL_MANAGER.get_or_init(|| WorkerManager::new(&ServerConfig::default()))
}
