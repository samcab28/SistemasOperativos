use crate::error::{ServerError, ServerResult};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc};
use std::thread::{self, JoinHandle};

use super::task_queue::TaskQueue;

pub type Job = Box<dyn FnOnce() + Send + 'static>;

pub struct WorkerPool {
    name: String,
    queue: Arc<TaskQueue<Job>>,
    workers: Vec<JoinHandle<()>>,
    shutdown: Arc<AtomicBool>,
}

impl WorkerPool {
    pub fn new(name: impl Into<String>, size: usize, capacity: usize) -> Self {
        let name = name.into();
        let queue: Arc<TaskQueue<Job>> = Arc::new(TaskQueue::<Job>::with_capacity(capacity));
        let shutdown = Arc::new(AtomicBool::new(false));

        let mut workers = Vec::with_capacity(size);
        for i in 0..size {
            let queue = Arc::clone(&queue);
            let shutdown = Arc::clone(&shutdown);
            let _worker_name = format!("{}-{}", &name, i);
            let handle = thread::spawn(move || {
                while !shutdown.load(Ordering::SeqCst) {
                    match queue.pop() {
                        Some(job) => {
                            // Execute the job; any panic only kills this thread
                            job();
                        }
                        None => {
                            // Queue closed; exit
                            break;
                        }
                    }
                }
            });
            workers.push(handle);
        }

        Self {
            name,
            queue,
            workers,
            shutdown,
        }
    }

    pub fn submit(&self, job: Job) -> ServerResult<()> {
        if self.shutdown.load(Ordering::SeqCst) {
            return Err(ServerError::ResourceExhausted(format!(
                "pool {} shutting down",
                self.name
            )));
        }
        if !self.queue.try_push(job) {
            return Err(ServerError::ResourceExhausted(format!(
                "pool {} queue full",
                self.name
            )));
        }
        Ok(())
    }

    pub fn shutdown(&mut self) {
        self.shutdown.store(true, Ordering::SeqCst);
        self.queue.close();
        // Join all workers
        for handle in self.workers.drain(..) {
            let _ = handle.join();
        }
    }
}

impl Drop for WorkerPool {
    fn drop(&mut self) {
        self.shutdown();
    }
}
