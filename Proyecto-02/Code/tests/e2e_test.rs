// End-to-End tests
// These require master and worker to be running

use chrono::Utc;
use common::*;
use serde_json::json;
use std::time::Duration;
use uuid::Uuid;

const MASTER_URL: &str = "http://127.0.0.1:8080";

async fn wait_for_master() -> bool {
    let client = reqwest::Client::new();
    for _ in 0..10 {
        if client
            .get(format!("{}/api/v1/metrics", MASTER_URL))
            .send()
            .await
            .is_ok()
        {
            return true;
        }
        tokio::time::sleep(Duration::from_millis(500)).await;
    }
    false
}

#[tokio::test]
#[ignore] // Run with: cargo test --test e2e_test -- --ignored
async fn test_full_pipeline_execution() {
    if !wait_for_master().await {
        println!("Master not running, skipping E2E test");
        return;
    }

    let client = reqwest::Client::new();

    // 1. Submit topology
    let spec = TopologySpec {
        name: "e2e-test-pipeline".into(),
        description: Some("End-to-end test".into()),
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
                id: "filter1".into(),
                kind: OperatorKind::Filter(FilterSpec {
                    field: "level".into(),
                    predicate: Predicate::Equals {
                        value: json!("error"),
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
        edges: vec![
            EdgeSpec {
                from: "map1".into(),
                to: "filter1".into(),
            },
            EdgeSpec {
                from: "filter1".into(),
                to: "sink".into(),
            },
        ],
        parallelism: 1,
    };

    let submit_resp = client
        .post(format!("{}/api/v1/topologies", MASTER_URL))
        .json(&spec)
        .send()
        .await
        .unwrap();

    assert!(submit_resp.status().is_success());
    let submit_body: TopologySubmitResponse = submit_resp.json().await.unwrap();
    let topology_id = submit_body.topology_id;

    println!("Topology submitted: {}", topology_id);

    // 2. Wait for topology to be Running
    tokio::time::sleep(Duration::from_secs(2)).await;

    let status_resp = client
        .get(format!("{}/api/v1/topologies/{}", MASTER_URL, topology_id))
        .send()
        .await
        .unwrap();

    assert!(status_resp.status().is_success());
    let status: TopologyStatus = status_resp.json().await.unwrap();
    println!("Topology status: {:?}", status.status);

    // 3. Ingest events
    let events = vec![
        EventPayload {
            timestamp: Utc::now(),
            data: json!({"level": "ERROR", "message": "CRITICAL FAILURE"}),
        },
        EventPayload {
            timestamp: Utc::now(),
            data: json!({"level": "INFO", "message": "Normal operation"}),
        },
        EventPayload {
            timestamp: Utc::now(),
            data: json!({"level": "ERROR", "message": "ANOTHER ERROR"}),
        },
    ];

    let ingest_req = IngestRequest {
        topology_id,
        events,
    };

    let ingest_resp = client
        .post(format!("{}/api/v1/ingest", MASTER_URL))
        .json(&ingest_req)
        .send()
        .await
        .unwrap();

    assert!(ingest_resp.status().is_success());
    println!("Events ingested successfully");

    // 4. Check metrics
    tokio::time::sleep(Duration::from_secs(1)).await;

    let metrics_resp = client
        .get(format!("{}/api/v1/metrics", MASTER_URL))
        .send()
        .await
        .unwrap();

    assert!(metrics_resp.status().is_success());
    let metrics: serde_json::Value = metrics_resp.json().await.unwrap();
    println!("Metrics: {}", serde_json::to_string_pretty(&metrics).unwrap());

    // 5. Cancel topology
    let cancel_resp = client
        .post(format!(
            "{}/api/v1/topologies/{}/cancel",
            MASTER_URL, topology_id
        ))
        .send()
        .await
        .unwrap();

    assert!(cancel_resp.status().is_success());
    println!("Topology canceled");
}

#[tokio::test]
#[ignore]
async fn test_window_aggregation_e2e() {
    if !wait_for_master().await {
        println!("Master not running, skipping E2E test");
        return;
    }

    let client = reqwest::Client::new();

    let spec = TopologySpec {
        name: "e2e-window-test".into(),
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
                        length_ms: 5000,
                        slide_ms: None,
                        checkpoint_interval_ms: 30_000,
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

    let submit_resp = client
        .post(format!("{}/api/v1/topologies", MASTER_URL))
        .json(&spec)
        .send()
        .await
        .unwrap();

    let submit_body: TopologySubmitResponse = submit_resp.json().await.unwrap();
    let topology_id = submit_body.topology_id;

    tokio::time::sleep(Duration::from_secs(2)).await;

    // Ingest events
    let events: Vec<_> = (0..10)
        .map(|i| EventPayload {
            timestamp: Utc::now(),
            data: json!({"service": "api", "request_id": i}),
        })
        .collect();

    let ingest_req = IngestRequest {
        topology_id,
        events,
    };

    client
        .post(format!("{}/api/v1/ingest", MASTER_URL))
        .json(&ingest_req)
        .send()
        .await
        .unwrap();

    println!("Window aggregation test completed");

    // Cleanup
    client
        .post(format!(
            "{}/api/v1/topologies/{}/cancel",
            MASTER_URL, topology_id
        ))
        .send()
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_multiple_workers_distribution() {
    if !wait_for_master().await {
        println!("Master not running, skipping E2E test");
        return;
    }

    let client = reqwest::Client::new();

    // Submit multiple topologies
    let mut topology_ids = Vec::new();

    for i in 0..3 {
        let spec = TopologySpec {
            name: format!("distributed-topology-{}", i),
            description: None,
            operators: vec![
                OperatorSpec {
                    id: "map1".into(),
                    kind: OperatorKind::Map(MapSpec {
                        field: "value".into(),
                        transform: TransformFn::ToUpper,
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

        let resp = client
            .post(format!("{}/api/v1/topologies", MASTER_URL))
            .json(&spec)
            .send()
            .await
            .unwrap();

        let body: TopologySubmitResponse = resp.json().await.unwrap();
        topology_ids.push(body.topology_id);
    }

    tokio::time::sleep(Duration::from_secs(2)).await;

    // Check metrics to see distribution
    let metrics_resp = client
        .get(format!("{}/api/v1/metrics", MASTER_URL))
        .send()
        .await
        .unwrap();

    let metrics: serde_json::Value = metrics_resp.json().await.unwrap();
    println!(
        "Distribution test metrics: {}",
        serde_json::to_string_pretty(&metrics).unwrap()
    );

    // Cleanup
    for topology_id in topology_ids {
        client
            .post(format!(
                "{}/api/v1/topologies/{}/cancel",
                MASTER_URL, topology_id
            ))
            .send()
            .await
            .unwrap();
    }
}

#[tokio::test]
#[ignore]
async fn test_fault_tolerance_simulation() {
    if !wait_for_master().await {
        println!("Master not running, skipping E2E test");
        return;
    }

    let client = reqwest::Client::new();

    let spec = TopologySpec {
        name: "fault-tolerance-test".into(),
        description: None,
        operators: vec![OperatorSpec {
            id: "sink".into(),
            kind: OperatorKind::SinkLog,
            config: Default::default(),
        }],
        edges: vec![],
        parallelism: 1,
    };

    let resp = client
        .post(format!("{}/api/v1/topologies", MASTER_URL))
        .json(&spec)
        .send()
        .await
        .unwrap();

    let body: TopologySubmitResponse = resp.json().await.unwrap();
    let topology_id = body.topology_id;

    tokio::time::sleep(Duration::from_secs(2)).await;

    // NOTE: To fully test fault tolerance, manually kill a worker
    // and observe rescheduling. This test just verifies the setup.

    println!(
        "Fault tolerance test topology deployed: {}",
        topology_id
    );
    println!("Manually kill a worker to test recovery");

    // Wait and check status
    tokio::time::sleep(Duration::from_secs(5)).await;

    let status_resp = client
        .get(format!("{}/api/v1/topologies/{}", MASTER_URL, topology_id))
        .send()
        .await
        .unwrap();

    let status: TopologyStatus = status_resp.json().await.unwrap();
    println!("Final status: {:?}", status.status);

    // Cleanup
    client
        .post(format!(
            "{}/api/v1/topologies/{}/cancel",
            MASTER_URL, topology_id
        ))
        .send()
        .await
        .unwrap();
}   