// Integration tests for worker operators
// These tests verify operator functionality through the PipelineEngine

use chrono::Utc;
use common::*;
use serde_json::json;
use std::path::PathBuf;
use uuid::Uuid;

fn temp_state_dir() -> PathBuf {
    std::env::temp_dir().join(format!("worker-ops-test-{}", Uuid::new_v4()))
}

fn cleanup_dir(dir: &PathBuf) {
    let _ = std::fs::remove_dir_all(dir);
}

#[tokio::test]
async fn test_map_to_lower_operator() {
    let state_dir = temp_state_dir();
    let topology_id = Uuid::new_v4();

    let spec = TopologySpec {
        name: "map-test".into(),
        description: None,
        operators: vec![
            OperatorSpec {
                id: "map1".into(),
                kind: OperatorKind::Map(MapSpec {
                    field: "message".into(),
                    transform: TransformFn::ToLower,
                }),
                config: Default::default(),
            },
            OperatorSpec {
                id: "sink".into(),
                kind: OperatorKind::SinkLog,
                config: Default::default(),
            },
        ],
        edges: vec![],
        parallelism: 1,
    };

    // This will fail if pipeline module isn't accessible
    // Make sure worker/src/lib.rs exports: pub mod pipeline;
    let mut engine = worker::pipeline::PipelineEngine::new(topology_id, 0, spec, &state_dir)
        .expect("Failed to create engine");

    let payload = EventPayload {
        timestamp: Utc::now(),
        data: json!({"message": "HELLO WORLD"}),
    };

    let event = worker::pipeline::StreamingEvent::try_from(payload)
        .expect("Failed to create event");

    let result = engine.process(event).await;
    assert!(result.is_ok());

    cleanup_dir(&state_dir);
}

#[tokio::test]
async fn test_filter_operator() {
    let state_dir = temp_state_dir();
    let topology_id = Uuid::new_v4();

    let spec = TopologySpec {
        name: "filter-test".into(),
        description: None,
        operators: vec![
            OperatorSpec {
                id: "filter1".into(),
                kind: OperatorKind::Filter(FilterSpec {
                    field: "level".into(),
                    predicate: Predicate::Equals {
                        value: json!("ERROR"),
                    },
                }),
                config: Default::default(),
            },
            OperatorSpec {
                id: "sink".into(),
                kind: OperatorKind::SinkLog,
                config: Default::default(),
            },
        ],
        edges: vec![],
        parallelism: 1,
    };

    let mut engine = worker::pipeline::PipelineEngine::new(topology_id, 0, spec, &state_dir)
        .expect("Failed to create engine");

    // Event that should pass
    let payload = EventPayload {
        timestamp: Utc::now(),
        data: json!({"level": "ERROR", "message": "Test"}),
    };

    let event = worker::pipeline::StreamingEvent::try_from(payload)
        .expect("Failed to create event");

    let result = engine.process(event).await;
    assert!(result.is_ok());

    cleanup_dir(&state_dir);
}

#[tokio::test]
async fn test_window_count_operator() {
    let state_dir = temp_state_dir();
    let topology_id = Uuid::new_v4();

    let spec = TopologySpec {
        name: "window-test".into(),
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
                        length_ms: 10_000,
                        slide_ms: None,
                        checkpoint_interval_ms: 60_000,
                    },
                    aggregator: AggregationSpec::Count,
                    key_field: "service".into(),
                    value_field: None,
                }),
                config: Default::default(),
            },
            OperatorSpec {
                id: "sink".into(),
                kind: OperatorKind::SinkLog,
                config: Default::default(),
            },
        ],
        edges: vec![],
        parallelism: 1,
    };

    let mut engine = worker::pipeline::PipelineEngine::new(topology_id, 0, spec, &state_dir)
        .expect("Failed to create engine");

    // Process multiple events
    for i in 0..5 {
        let payload = EventPayload {
            timestamp: Utc::now(),
            data: json!({"service": "api", "value": i}),
        };

        let event = worker::pipeline::StreamingEvent::try_from(payload)
            .expect("Failed to create event");

        let result = engine.process(event).await;
        assert!(result.is_ok());
    }

    cleanup_dir(&state_dir);
}

#[tokio::test]
async fn test_chained_operators() {
    let state_dir = temp_state_dir();
    let topology_id = Uuid::new_v4();

    let spec = TopologySpec {
        name: "chain-test".into(),
        description: None,
        operators: vec![
            OperatorSpec {
                id: "map1".into(),
                kind: OperatorKind::Map(MapSpec {
                    field: "message".into(),
                    transform: TransformFn::ToUpper,
                }),
                config: Default::default(),
            },
            OperatorSpec {
                id: "filter1".into(),
                kind: OperatorKind::Filter(FilterSpec {
                    field: "level".into(),
                    predicate: Predicate::Contains {
                        value: "error".into(),
                    },
                }),
                config: Default::default(),
            },
            OperatorSpec {
                id: "sink".into(),
                kind: OperatorKind::SinkLog,
                config: Default::default(),
            },
        ],
        edges: vec![],
        parallelism: 1,
    };

    let mut engine = worker::pipeline::PipelineEngine::new(topology_id, 0, spec, &state_dir)
        .expect("Failed to create engine");

    let payload = EventPayload {
        timestamp: Utc::now(),
        data: json!({"level": "error", "message": "test"}),
    };

    let event = worker::pipeline::StreamingEvent::try_from(payload)
        .expect("Failed to create event");

    let result = engine.process(event).await;
    assert!(result.is_ok());

    cleanup_dir(&state_dir);
}