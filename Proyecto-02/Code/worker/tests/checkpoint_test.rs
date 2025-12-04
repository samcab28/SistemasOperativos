// Checkpoint and recovery tests for worker

use chrono::Utc;
use common::*;
use serde_json::json;
use std::path::PathBuf;
use uuid::Uuid;

fn temp_state_dir() -> PathBuf {
    std::env::temp_dir().join(format!("checkpoint-test-{}", Uuid::new_v4()))
}

fn cleanup_dir(dir: &PathBuf) {
    let _ = std::fs::remove_dir_all(dir);
}

#[tokio::test]
async fn test_checkpoint_directory_created() {
    let topology_id = Uuid::new_v4();
    let state_dir = temp_state_dir();

    let spec = TopologySpec {
        name: "checkpoint-test".into(),
        description: None,
        operators: vec![
            OperatorSpec {
                id: "keyby".into(),
                kind: OperatorKind::KeyBy(KeyBySpec {
                    field: "key".into(),
                }),
                config: Default::default(),
            },
            OperatorSpec {
                id: "window".into(),
                kind: OperatorKind::WindowAggregate(WindowAggregateSpec {
                    window: WindowSpec {
                        length_ms: 10_000,
                        slide_ms: None,
                        checkpoint_interval_ms: 100, // Fast checkpoint
                    },
                    aggregator: AggregationSpec::Count,
                    key_field: "key".into(),
                    value_field: None,
                }),
                config: Default::default(),
            },
        ],
        edges: vec![],
        parallelism: 1,
    };

    let mut engine = worker::pipeline::PipelineEngine::new(topology_id, 0, spec, &state_dir)
        .expect("Failed to create engine");

    // Send events
    for i in 0..5 {
        let payload = EventPayload {
            timestamp: Utc::now(),
            data: json!({"key": "test", "value": i}),
        };

        let event = worker::pipeline::StreamingEvent::try_from(payload)
            .expect("Failed to create event");

        engine.process(event).await.expect("Process failed");
    }

    // Wait for checkpoint
    tokio::time::sleep(tokio::time::Duration::from_millis(200)).await;

    // Verify checkpoint directory exists
    let checkpoint_dir = state_dir
        .join(topology_id.to_string())
        .join("attempt-0")
        .join("window");

    assert!(
        checkpoint_dir.exists(),
        "Checkpoint directory should exist at {:?}",
        checkpoint_dir
    );

    cleanup_dir(&state_dir);
}

#[tokio::test]
async fn test_checkpoint_recovery() {
    let topology_id = Uuid::new_v4();
    let state_dir = temp_state_dir();

    let spec = TopologySpec {
        name: "recovery-test".into(),
        description: None,
        operators: vec![
            OperatorSpec {
                id: "keyby".into(),
                kind: OperatorKind::KeyBy(KeyBySpec {
                    field: "service".into(),
                }),
                config: Default::default(),
            },
            OperatorSpec {
                id: "window".into(),
                kind: OperatorKind::WindowAggregate(WindowAggregateSpec {
                    window: WindowSpec {
                        length_ms: 60_000,
                        slide_ms: None,
                        checkpoint_interval_ms: 100,
                    },
                    aggregator: AggregationSpec::Count,
                    key_field: "service".into(),
                    value_field: None,
                }),
                config: Default::default(),
            },
        ],
        edges: vec![],
        parallelism: 1,
    };

    // First engine - create checkpoint
    {
        let mut engine = worker::pipeline::PipelineEngine::new(topology_id, 0, spec.clone(), &state_dir)
            .expect("Failed to create engine");

        for _ in 0..3 {
            let payload = EventPayload {
                timestamp: Utc::now(),
                data: json!({"service": "api"}),
            };

            let event = worker::pipeline::StreamingEvent::try_from(payload)
                .expect("Failed to create event");

            engine.process(event).await.expect("Process failed");
        }

        tokio::time::sleep(tokio::time::Duration::from_millis(200)).await;
        engine.shutdown().await;
    }

    // Second engine - should load checkpoint without error
    let result = worker::pipeline::PipelineEngine::new(topology_id, 0, spec, &state_dir);
    assert!(result.is_ok(), "Engine should recover from checkpoint");

    cleanup_dir(&state_dir);
}