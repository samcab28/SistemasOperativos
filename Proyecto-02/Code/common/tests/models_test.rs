use chrono::Utc;
use common::*;
use serde_json::json;

#[test]
fn test_topology_spec_default_parallelism() {
    let spec: TopologySpec = serde_json::from_value(json!({
        "name": "test-topology",
        "operators": [],
        "edges": []
    }))
    .unwrap();
    
    assert_eq!(spec.parallelism, 1);
    assert_eq!(spec.name, "test-topology");
}

#[test]
fn test_topology_spec_custom_parallelism() {
    let spec: TopologySpec = serde_json::from_value(json!({
        "name": "parallel-topology",
        "operators": [],
        "edges": [],
        "parallelism": 4
    }))
    .unwrap();
    
    assert_eq!(spec.parallelism, 4);
}

#[test]
fn test_operator_map_creation() {
    let spec = TopologySpec {
        name: "test".into(),
        description: Some("Test topology".into()),
        operators: vec![
            OperatorSpec {
                id: "op1".into(),
                kind: OperatorKind::Map(MapSpec {
                    field: "message".into(),
                    transform: TransformFn::ToLower,
                }),
                config: Default::default(),
            },
            OperatorSpec {
                id: "op2".into(),
                kind: OperatorKind::Filter(FilterSpec {
                    field: "level".into(),
                    predicate: Predicate::Equals {
                        value: json!("ERROR"),
                    },
                }),
                config: Default::default(),
            },
        ],
        edges: vec![EdgeSpec {
            from: "op1".into(),
            to: "op2".into(),
        }],
        parallelism: 1,
    };

    let map = spec.operator_map();
    assert_eq!(map.len(), 2);
    assert!(map.contains_key("op1"));
    assert!(map.contains_key("op2"));
}

#[test]
fn test_transform_fn_serialization() {
    let transform = TransformFn::ToLower;
    let json = serde_json::to_value(&transform).unwrap();
    assert_eq!(json["kind"], "to_lower");

    let prefix = TransformFn::Prefix {
        value: "PREFIX_".into(),
    };
    let json = serde_json::to_value(&prefix).unwrap();
    assert_eq!(json["kind"], "prefix");
    assert_eq!(json["value"], "PREFIX_");
}

#[test]
fn test_predicate_serialization() {
    let equals = Predicate::Equals {
        value: json!("test"),
    };
    let json = serde_json::to_value(&equals).unwrap();
    assert_eq!(json["kind"], "equals");

    let greater = Predicate::GreaterThan { value: 10.5 };
    let json = serde_json::to_value(&greater).unwrap();
    assert_eq!(json["kind"], "greater_than");
    assert_eq!(json["value"], 10.5);
}

#[test]
fn test_window_spec_default_checkpoint() {
    let spec: WindowSpec = serde_json::from_value(json!({
        "length_ms": 5000,
        "slide_ms": 1000
    }))
    .unwrap();

    assert_eq!(spec.length_ms, 5000);
    assert_eq!(spec.slide_ms, Some(1000));
    assert_eq!(spec.checkpoint_interval_ms, 30_000);
}

#[test]
fn test_aggregation_spec_variants() {
    let count = AggregationSpec::Count;
    let sum = AggregationSpec::Sum;
    let avg = AggregationSpec::Average;

    let count_json = serde_json::to_value(&count).unwrap();
    let sum_json = serde_json::to_value(&sum).unwrap();
    let avg_json = serde_json::to_value(&avg).unwrap();

    assert_eq!(count_json["type"], "count");
    assert_eq!(sum_json["type"], "sum");
    assert_eq!(avg_json["type"], "average");
}

#[test]
fn test_event_payload_creation() {
    let payload = EventPayload {
        timestamp: Utc::now(),
        data: json!({
            "service": "api",
            "message": "Request processed",
            "level": "INFO"
        }),
    };

    assert!(payload.data.is_object());
    assert_eq!(payload.data["service"], "api");
}

#[test]
fn test_worker_register_request() {
    let req = WorkerRegisterRequest {
        worker_id: "worker-1".into(),
        api_url: "http://localhost:9001".into(),
        slots: 4,
    };

    let json = serde_json::to_value(&req).unwrap();
    assert_eq!(json["worker_id"], "worker-1");
    assert_eq!(json["slots"], 4);
}

#[test]
fn test_topology_status_kind_variants() {
    let statuses = vec![
        TopologyStatusKind::Accepted,
        TopologyStatusKind::Running,
        TopologyStatusKind::Failed,
        TopologyStatusKind::Completed,
        TopologyStatusKind::Canceled,
    ];

    for status in statuses {
        let json = serde_json::to_value(&status).unwrap();
        assert!(json.is_string());
    }
}

#[test]
fn test_worker_metrics_default() {
    let metrics = WorkerMetrics::default();
    assert_eq!(metrics.cpu_pct, 0.0);
    assert_eq!(metrics.mem_bytes, 0);
    assert_eq!(metrics.active_topologies, 0);
    assert_eq!(metrics.queue_depth, 0);
    assert_eq!(metrics.throughput_eps, 0.0);
}

#[test]
fn test_topology_metrics_default() {
    let metrics = TopologyMetrics::default();
    assert_eq!(metrics.events_ingested, 0);
    assert_eq!(metrics.events_emitted, 0);
    assert_eq!(metrics.windows_open, 0);
    assert!(metrics.last_checkpoint.is_none());
}

#[test]
fn test_ingest_request_with_multiple_events() {
    let topology_id = uuid::Uuid::new_v4();
    let req = IngestRequest {
        topology_id,
        events: vec![
            EventPayload {
                timestamp: Utc::now(),
                data: json!({"msg": "event1"}),
            },
            EventPayload {
                timestamp: Utc::now(),
                data: json!({"msg": "event2"}),
            },
        ],
    };

    assert_eq!(req.events.len(), 2);
    assert_eq!(req.topology_id, topology_id);
}