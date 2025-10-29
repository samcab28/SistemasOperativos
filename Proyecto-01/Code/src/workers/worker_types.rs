use std::time::Duration;

#[derive(Clone, Debug)]
pub struct CpuWorkerConfig {
    pub workers: usize,
    pub queue_depth: usize,
    pub timeout: Duration,
}
