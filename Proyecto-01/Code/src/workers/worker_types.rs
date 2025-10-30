use std::time::Duration;

#[derive(Clone, Debug)]
pub struct CpuWorkerConfig {
    pub workers: usize,
    pub queue_depth: usize,
    pub timeout: Duration,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum WorkPriority {
    Low,
    Normal,
    High,
}

impl WorkPriority {
    pub fn from_str(s: &str) -> Self {
        match s.to_lowercase().as_str() {
            "high" => WorkPriority::High,
            "low" => WorkPriority::Low,
            _ => WorkPriority::Normal,
        }
    }
}
