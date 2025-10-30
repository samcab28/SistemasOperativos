use std::collections::HashMap;
use std::sync::{Mutex, OnceLock};

use super::job_types::{Job, JobStatus};

#[derive(Default)]
pub struct InMemoryStorage {
    jobs: HashMap<String, Job>,
}

static STORAGE: OnceLock<Mutex<InMemoryStorage>> = OnceLock::new();

pub fn storage() -> &'static Mutex<InMemoryStorage> {
    STORAGE.get_or_init(|| Mutex::new(InMemoryStorage::default()))
}

impl InMemoryStorage {
    pub fn insert(&mut self, job: Job) { self.jobs.insert(job.id.clone(), job); }
    pub fn get(&self, id: &str) -> Option<Job> { self.jobs.get(id).cloned() }
    pub fn update(&mut self, job: Job) { self.jobs.insert(job.id.clone(), job); }
    pub fn list(&self) -> Vec<Job> { self.jobs.values().cloned().collect() }
    pub fn cancel(&mut self, id: &str) -> bool {
        if let Some(j) = self.jobs.get_mut(id) {
            j.status = JobStatus::Canceled; j.finished_at = Some(super::job_types::now_ms()); true
        } else { false }
    }
}
