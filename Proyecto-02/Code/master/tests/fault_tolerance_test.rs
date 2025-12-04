// Scheduler and load balancing tests for the master

use std::collections::HashMap;
use std::time::{Duration, Instant};

#[test]
fn test_round_robin_worker_selection() {
    let workers = vec!["worker-1", "worker-2", "worker-3"];
    let mut cursor = 0;
    let mut selections = Vec::new();

    for _ in 0..9 {
        let selected = workers[cursor % workers.len()];
        selections.push(selected);
        cursor += 1;
    }

    assert_eq!(selections[0], "worker-1");
    assert_eq!(selections[3], "worker-1");
    assert_eq!(selections[6], "worker-1");
}

#[test]
fn test_load_based_worker_selection() {
    struct MockWorker {
        id: &'static str,
        active: usize,
        slots: usize,
    }

    let workers = vec![
        MockWorker { id: "worker-1", active: 2, slots: 4 },
        MockWorker { id: "worker-2", active: 3, slots: 4 },
        MockWorker { id: "worker-3", active: 0, slots: 2 },
    ];

    let mut candidates: Vec<_> = workers
        .iter()
        .map(|w| ((w.active as f64) / (w.slots as f64).max(1.0), w.id))
        .collect();

    candidates.sort_by(|a, b| a.0.partial_cmp(&b.0).unwrap());

    assert_eq!(candidates[0].1, "worker-3"); // 0% load
    assert_eq!(candidates[1].1, "worker-1"); // 50% load
}

#[test]
fn test_worker_capacity_filtering() {
    struct Worker {
        id: &'static str,
        active: usize,
        slots: usize,
    }

    let workers = vec![
        Worker { id: "worker-1", active: 4, slots: 4 }, // Full
        Worker { id: "worker-2", active: 3, slots: 4 },
        Worker { id: "worker-3", active: 1, slots: 4 },
    ];

    let available: Vec<_> = workers.iter().filter(|w| w.active < w.slots).collect();

    assert_eq!(available.len(), 2);
    assert_eq!(available[0].id, "worker-2");
}

#[test]
fn test_heartbeat_timeout_detection() {
    struct Worker {
        id: &'static str,
        last_heartbeat: Instant,
    }

    let timeout = Duration::from_secs(6);
    let now = Instant::now();

    let workers = vec![
        Worker { id: "worker-1", last_heartbeat: now - Duration::from_secs(3) },
        Worker { id: "worker-2", last_heartbeat: now - Duration::from_secs(10) },
    ];

    let expired: Vec<_> = workers
        .iter()
        .filter(|w| w.last_heartbeat.elapsed() > timeout)
        .map(|w| w.id)
        .collect();

    assert_eq!(expired.len(), 1);
    assert_eq!(expired[0], "worker-2");
}

#[test]
fn test_topology_worker_assignment() {
    let mut assignments: HashMap<uuid::Uuid, String> = HashMap::new();
    
    let topo1 = uuid::Uuid::new_v4();
    let topo2 = uuid::Uuid::new_v4();

    assignments.insert(topo1, "worker-1".into());
    assignments.insert(topo2, "worker-2".into());

    assert_eq!(assignments.get(&topo1), Some(&"worker-1".to_string()));
    
    let mut counts: HashMap<String, usize> = HashMap::new();
    for worker in assignments.values() {
        *counts.entry(worker.clone()).or_insert(0) += 1;
    }
    
    assert_eq!(counts.get("worker-1"), Some(&1));
}