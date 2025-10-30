use std::collections::VecDeque;
use std::sync::{Condvar, Mutex};

pub struct TaskQueue<T> {
    inner: Mutex<Inner<T>>,
    cv: Condvar,
}

struct Inner<T> {
    queue: VecDeque<T>,
    capacity: usize,
    closed: bool,
}

impl<T> TaskQueue<T> {
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            inner: Mutex::new(Inner {
                queue: VecDeque::with_capacity(capacity),
                capacity,
                closed: false,
            }),
            cv: Condvar::new(),
        }
    }

    /// Try to push without blocking. Returns false if full or closed.
    pub fn try_push(&self, item: T) -> bool {
        let mut inner = self.inner.lock().unwrap();
        if inner.closed || inner.queue.len() >= inner.capacity {
            return false;
        }
        inner.queue.push_back(item);
        self.cv.notify_one();
        true
    }

    /// Pop blocking; returns None when closed and empty.
    pub fn pop(&self) -> Option<T> {
        let mut inner = self.inner.lock().unwrap();
        loop {
            if let Some(item) = inner.queue.pop_front() {
                return Some(item);
            }
            if inner.closed {
                return None;
            }
            inner = self.cv.wait(inner).unwrap();
        }
    }

    pub fn close(&self) {
        let mut inner = self.inner.lock().unwrap();
        inner.closed = true;
        self.cv.notify_all();
    }

    /// Current queue length
    pub fn len(&self) -> usize {
        let inner = self.inner.lock().unwrap();
        inner.queue.len()
    }

    /// Queue capacity
    pub fn capacity(&self) -> usize {
        let inner = self.inner.lock().unwrap();
        inner.capacity
    }
}
