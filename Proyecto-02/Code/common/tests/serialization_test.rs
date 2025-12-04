use chrono::Utc;
use common::*;
use serde_json::json;
use uuid::Uuid;

#[test]
fn test_topology_spec_serialization_roundtrip() {
    let spec = TopologySpec {
        name: "test-topology".into(),
        description: Some("Test description".into()),
        operators: vec![
            OperatorSpec {
                id: "op1".into(),
                kind: OperatorKind::Map(MapSpec {
                    field: "message".into(),
                    transform: TransformFn::ToLower,
                }),
                config: Default::default(),
            },
        ],
        edges: vec![],
        parallelism: 4,
    };

    // Serialize to JSON
    let json = serde_json::to_value(&spec).unwrap();
    
    // Deserialize back
    let deserialized: TopologySpec = serde_json::from_value(json).unwrap();
    
    assert_eq!(spec.name, deserialized.name);
    assert_eq!(spec.parallelism, deserialized.parallelism);
    assert_eq!(spec.operators.len(), deserialized.operators.len());
}

#[test]
fn test_operator_kind_map_serialization() {
    let map_spec = MapSpec {
        field: "message".into(),
        transform: TransformFn::ToUpper,
    };
    
    let operator = OperatorSpec {
        id: "map1".into(),
        kind: OperatorKind::Map(map_spec),
        config: Default::default(),
    };

    let json = serde_json::to_string(&operator).unwrap();
    let deserialized: OperatorSpec = serde_json::from_str(&json).unwrap();
    
    assert_eq!(operator.id, deserialized.id);
    
    match deserialized.kind {
        OperatorKind::Map(spec) => {
            assert_eq!(spec.field, "message");
        }
        _ => panic!("Expected Map operator"),
    }
}

#[test]
fn test_operator_kind_filter_serialization() {
    let filter_spec = FilterSpec {
        field: "level".into(),
        predicate: Predicate::Equals {
            value: json!("ERROR"),
        },
    };
    
    let operator = OperatorSpec {
        id: "filter1".into(),
        kind: OperatorKind::Filter(filter_spec),
        config: Default::default(),
    };

    let json_str = serde_json::to_string(&operator).unwrap();
    let deserialized: OperatorSpec = serde_json::from_str(&json_str).unwrap();
    
    match deserialized.kind {
        OperatorKind::Filter(spec) => {
            assert_eq!(spec.field, "level");
            match spec.predicate {
                Predicate::Equals { value } => {
                    assert_eq!(value, json!("ERROR"));
                }
                _ => panic!("Expected Equals predicate"),
            }
        }
        _ => panic!("Expected Filter operator"),
    }
}

#[test]
fn test_window_aggregate_serialization() {
    let window_spec = WindowAggregateSpec {
        window: WindowSpec {
            length_ms: 5000,
            slide_ms: Some(1000),
            checkpoint_interval_ms: 30_000,
        },
        aggregator: AggregationSpec::Sum,
        key_field: "service".into(),
        value_field: Some("latency".into()),
    };
    
    let operator = OperatorSpec {
        id: "window1".into(),
        kind: OperatorKind::WindowAggregate(window_spec),
        config: Default::default(),
    };

    let json = serde_json::to_value(&operator).unwrap();
    let deserialized: OperatorSpec = serde_json::from_value(json).unwrap();
    
    match deserialized.kind {
        OperatorKind::WindowAggregate(spec) => {
            assert_eq!(spec.window.length_ms, 5000);
            assert_eq!(spec.window.slide_ms, Some(1000));
            assert_eq!(spec.key_field, "service");
            assert_eq!(spec.value_field, Some("latency".into()));
        }
        _ => panic!("Expected WindowAggregate operator"),
    }
}

#[test]
fn test_transform_fn_all_variants_serialization() {
    let transforms = vec![
        TransformFn::ToLower,
        TransformFn::ToUpper,
        TransformFn::Trim,
        TransformFn::Prefix { value: "PRE_".into() },
        TransformFn::Suffix { value: "_POST".into() },
    ];

    for transform in transforms {
        let json = serde_json::to_string(&transform).unwrap();
        let deserialized: TransformFn = serde_json::from_str(&json).unwrap();
        
        // Verify it deserializes correctly
        match (&transform, &deserialized) {
            (TransformFn::ToLower, TransformFn::ToLower) => {},
            (TransformFn::ToUpper, TransformFn::ToUpper) => {},
            (TransformFn::Trim, TransformFn::Trim) => {},
            (TransformFn::Prefix { value: v1 }, TransformFn::Prefix { value: v2 }) => {
                assert_eq!(v1, v2);
            },
            (TransformFn::Suffix { value: v1 }, TransformFn::Suffix { value: v2 }) => {
                assert_eq!(v1, v2);
            },
            _ => panic!("Transform mismatch"),
        }
    }
}

#[test]
fn test_predicate_all_variants_serialization() {
    let predicates = vec![
        Predicate::Equals { value: json!("test") },
        Predicate::NotEquals { value: json!(42) },
        Predicate::GreaterThan { value: 10.5 },
        Predicate::LessThan { value: 100.0 },
        Predicate::Contains { value: "error".into() },
        Predicate::Exists,
    ];

    for predicate in predicates {
        let json = serde_json::to_string(&predicate).unwrap();
        let deserialized: Predicate = serde_json::from_str(&json).unwrap();
        
        // Basic smoke test - just verify it deserializes
        match (&predicate, &deserialized) {
            (Predicate::Exists, Predicate::Exists) => {},
            (Predicate::Equals { value: _ }, Predicate::Equals { value: _ }) => {},
            (Predicate::NotEquals { value: _ }, Predicate::NotEquals { value: _ }) => {},
            (Predicate::GreaterThan { value: _ }, Predicate::GreaterThan { value: _ }) => {},
            (Predicate::LessThan { value: _ }, Predicate::LessThan { value: _ }) => {},
            (Predicate::Contains { value: _ }, Predicate::Contains { value: _ }) => {},
            _ => panic!("Predicate mismatch"),
        }
    }
}

#[test]
fn test_event_payload_serialization() {
    let timestamp = Utc::now();
    let payload = EventPayload {
        timestamp,
        data: json!({
            "service": "api",
            "message": "Test event",
            "level": "INFO",
            "request_id": 12345
        }),
    };

    let json = serde_json::to_string(&payload).unwrap();
    let deserialized: EventPayload = serde_json::from_str(&json).unwrap();
    
    assert_eq!(payload.timestamp, deserialized.timestamp);
    assert_eq!(payload.data["service"], deserialized.data["service"]);
    assert_eq!(payload.data["message"], deserialized.data["message"]);
}

#[test]
fn test_ingest_request_serialization() {
    let topology_id = Uuid::new_v4();
    let request = IngestRequest {
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

    let json = serde_json::to_string(&request).unwrap();
    let deserialized: IngestRequest = serde_json::from_str(&json).unwrap();
    
    assert_eq!(request.topology_id, deserialized.topology_id);
    assert_eq!(request.events.len(), deserialized.events.len());
}

#[test]
fn test_worker_register_request_serialization() {
    let request = WorkerRegisterRequest {
        worker_id: "worker-test-1".into(),
        api_url: "http://localhost:9001".into(),
        slots: 4,
    };

    let json = serde_json::to_string(&request).unwrap();
    let deserialized: WorkerRegisterRequest = serde_json::from_str(&json).unwrap();
    
    assert_eq!(request.worker_id, deserialized.worker_id);
    assert_eq!(request.api_url, deserialized.api_url);
    assert_eq!(request.slots, deserialized.slots);
}

#[test]
fn test_worker_heartbeat_serialization() {
    let heartbeat = WorkerHeartbeat {
        worker_id: "worker-1".into(),
        metrics: WorkerMetrics {
            cpu_pct: 45.5,
            mem_bytes: 1024 * 1024 * 512, // 512 MB
            active_topologies: 3,
            queue_depth: 150,
            throughput_eps: 1234.56,
        },
    };

    let json = serde_json::to_string(&heartbeat).unwrap();
    let deserialized: WorkerHeartbeat = serde_json::from_str(&json).unwrap();
    
    assert_eq!(heartbeat.worker_id, deserialized.worker_id);
    assert_eq!(heartbeat.metrics.cpu_pct, deserialized.metrics.cpu_pct);
    assert_eq!(heartbeat.metrics.mem_bytes, deserialized.metrics.mem_bytes);
    assert_eq!(heartbeat.metrics.active_topologies, deserialized.metrics.active_topologies);
}

#[test]
fn test_topology_deployment_serialization() {
    let topology_id = Uuid::new_v4();
    let deployment = TopologyDeployment {
        topology_id,
        spec: TopologySpec {
            name: "deploy-test".into(),
            description: None,
            operators: vec![],
            edges: vec![],
            parallelism: 1,
        },
        attempt: 2,
    };

    let json = serde_json::to_string(&deployment).unwrap();
    let deserialized: TopologyDeployment = serde_json::from_str(&json).unwrap();
    
    assert_eq!(deployment.topology_id, deserialized.topology_id);
    assert_eq!(deployment.spec.name, deserialized.spec.name);
    assert_eq!(deployment.attempt, deserialized.attempt);
}

#[test]
fn test_topology_status_serialization() {
    let status = TopologyStatus {
        topology_id: Uuid::new_v4(),
        name: "test-topology".into(),
        status: TopologyStatusKind::Running,
        worker_id: Some("worker-1".into()),
        metrics: TopologyMetrics {
            events_ingested: 1000,
            events_emitted: 950,
            last_checkpoint: Some(Utc::now()),
            windows_open: 5,
        },
        last_error: None,
        attempt: 1,
    };

    let json = serde_json::to_string(&status).unwrap();
    let deserialized: TopologyStatus = serde_json::from_str(&json).unwrap();
    
    assert_eq!(status.topology_id, deserialized.topology_id);
    assert_eq!(status.name, deserialized.name);
    assert_eq!(status.metrics.events_ingested, deserialized.metrics.events_ingested);
}

#[test]
fn test_topology_status_kind_json_format() {
    let statuses = vec![
        (TopologyStatusKind::Accepted, "\"Accepted\""),
        (TopologyStatusKind::Running, "\"Running\""),
        (TopologyStatusKind::Failed, "\"Failed\""),
        (TopologyStatusKind::Completed, "\"Completed\""),
        (TopologyStatusKind::Canceled, "\"Canceled\""),
    ];

    for (status, expected_json) in statuses {
        let json = serde_json::to_string(&status).unwrap();
        assert_eq!(json, expected_json);
        
        let deserialized: TopologyStatusKind = serde_json::from_str(&json).unwrap();
        // Compare as strings since we can't derive PartialEq on the enum
        assert_eq!(
            serde_json::to_string(&status).unwrap(),
            serde_json::to_string(&deserialized).unwrap()
        );
    }
}

#[test]
fn test_checkpoint_manifest_serialization() {
    let manifest = CheckpointManifest {
        topology_id: Uuid::new_v4(),
        operator_id: "window-1".into(),
        window_start: Utc::now(),
        window_end: Utc::now(),
        payload: json!({
            "key": "service-api",
            "count": 100,
            "sum": 1234.56
        }),
    };

    let json = serde_json::to_string(&manifest).unwrap();
    let deserialized: CheckpointManifest = serde_json::from_str(&json).unwrap();
    
    assert_eq!(manifest.topology_id, deserialized.topology_id);
    assert_eq!(manifest.operator_id, deserialized.operator_id);
    assert_eq!(manifest.payload["key"], deserialized.payload["key"]);
}

#[test]
fn test_worker_event_batch_serialization() {
    let batch = WorkerEventBatch {
        topology_id: Uuid::new_v4(),
        events: vec![
            EventPayload {
                timestamp: Utc::now(),
                data: json!({"value": 1}),
            },
            EventPayload {
                timestamp: Utc::now(),
                data: json!({"value": 2}),
            },
        ],
    };

    let json = serde_json::to_string(&batch).unwrap();
    let deserialized: WorkerEventBatch = serde_json::from_str(&json).unwrap();
    
    assert_eq!(batch.topology_id, deserialized.topology_id);
    assert_eq!(batch.events.len(), deserialized.events.len());
}

#[test]
fn test_edge_spec_serialization() {
    let edge = EdgeSpec {
        from: "op1".into(),
        to: "op2".into(),
    };

    let json = serde_json::to_string(&edge).unwrap();
    let deserialized: EdgeSpec = serde_json::from_str(&json).unwrap();
    
    assert_eq!(edge.from, deserialized.from);
    assert_eq!(edge.to, deserialized.to);
}

#[test]
fn test_complex_topology_serialization() {
    let topology = TopologySpec {
        name: "complex-pipeline".into(),
        description: Some("A complex streaming pipeline".into()),
        operators: vec![
            OperatorSpec {
                id: "source".into(),
                kind: OperatorKind::Map(MapSpec {
                    field: "message".into(),
                    transform: TransformFn::ToLower,
                }),
                config: Default::default(),
            },
            OperatorSpec {
                id: "filter".into(),
                kind: OperatorKind::Filter(FilterSpec {
                    field: "level".into(),
                    predicate: Predicate::Contains {
                        value: "error".into(),
                    },
                }),
                config: Default::default(),
            },
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
                        slide_ms: Some(10_000),
                        checkpoint_interval_ms: 30_000,
                    },
                    aggregator: AggregationSpec::Average,
                    key_field: "service".into(),
                    value_field: Some("latency_ms".into()),
                }),
                config: Default::default(),
            },
            OperatorSpec {
                id: "sink".into(),
                kind: OperatorKind::SinkLog,
                config: Default::default(),
            },
        ],
        edges: vec![
            EdgeSpec {
                from: "source".into(),
                to: "filter".into(),
            },
            EdgeSpec {
                from: "filter".into(),
                to: "keyby".into(),
            },
            EdgeSpec {
                from: "keyby".into(),
                to: "window".into(),
            },
            EdgeSpec {
                from: "window".into(),
                to: "sink".into(),
            },
        ],
        parallelism: 4,
    };

    // Serialize and deserialize
    let json_str = serde_json::to_string_pretty(&topology).unwrap();
    let deserialized: TopologySpec = serde_json::from_str(&json_str).unwrap();
    
    assert_eq!(topology.name, deserialized.name);
    assert_eq!(topology.operators.len(), deserialized.operators.len());
    assert_eq!(topology.edges.len(), deserialized.edges.len());
    assert_eq!(topology.parallelism, deserialized.parallelism);
    
    // Verify operator map works
    let op_map = deserialized.operator_map();
    assert_eq!(op_map.len(), 5);
    assert!(op_map.contains_key("source"));
    assert!(op_map.contains_key("sink"));
}

#[test]
fn test_json_with_extra_fields_ignored() {
    // Test that extra unknown fields in JSON are ignored gracefully
    let json_with_extras = json!({
        "name": "test",
        "operators": [],
        "edges": [],
        "parallelism": 1,
        "unknown_field": "should be ignored",
        "another_extra": 123
    });

    let result = serde_json::from_value::<TopologySpec>(json_with_extras);
    assert!(result.is_ok());
    let spec = result.unwrap();
    assert_eq!(spec.name, "test");
}

#[test]
fn test_minimal_valid_topology() {
    // Test the absolute minimum valid topology
    let json = json!({
        "name": "minimal",
        "operators": [],
        "edges": []
    });

    let result = serde_json::from_value::<TopologySpec>(json);
    assert!(result.is_ok());
    let spec = result.unwrap();
    assert_eq!(spec.name, "minimal");
    assert_eq!(spec.parallelism, 1); // Default
}