use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};
use serde_json::{Map, Value};
use std::collections::HashMap;
use uuid::Uuid;

pub type OperatorId = String;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TopologySpec {
    pub name: String,
    pub description: Option<String>,
    pub operators: Vec<OperatorSpec>,
    pub edges: Vec<EdgeSpec>,
    #[serde(default = "default_parallelism")]
    pub parallelism: usize,
}

fn default_parallelism() -> usize {
    1
}

impl TopologySpec {
    pub fn operator_map(&self) -> HashMap<&str, &OperatorSpec> {
        self.operators.iter().map(|op| (op.id.as_str(), op)).collect()
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OperatorSpec {
    pub id: OperatorId,
    pub kind: OperatorKind,
    #[serde(default)]
    pub config: Map<String, Value>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EdgeSpec {
    pub from: OperatorId,
    pub to: OperatorId,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum OperatorKind {
    Map(MapSpec),
    Filter(FilterSpec),
    FlatMap(FlatMapSpec),
    KeyBy(KeyBySpec),
    WindowAggregate(WindowAggregateSpec),
    SinkLog,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MapSpec {
    pub field: String,
    pub transform: TransformFn,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum TransformFn {
    ToLower,
    ToUpper,
    Trim,
    Prefix { value: String },
    Suffix { value: String },
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FilterSpec {
    pub field: String,
    pub predicate: Predicate,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum Predicate {
    Equals { value: Value },
    NotEquals { value: Value },
    GreaterThan { value: f64 },
    LessThan { value: f64 },
    Contains { value: String },
    Exists,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FlatMapSpec {
    pub field: String,
    pub separator: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KeyBySpec {
    pub field: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WindowAggregateSpec {
    pub window: WindowSpec,
    pub aggregator: AggregationSpec,
    pub key_field: String,
    pub value_field: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WindowSpec {
    pub length_ms: u64,
    pub slide_ms: Option<u64>,
    #[serde(default = "default_checkpoint_interval_ms")]
    pub checkpoint_interval_ms: u64,
}

fn default_checkpoint_interval_ms() -> u64 {
    30_000
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum AggregationSpec {
    Count,
    Sum,
    Average,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EventPayload {
    pub timestamp: DateTime<Utc>,
    pub data: Value,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IngestRequest {
    pub topology_id: Uuid,
    pub events: Vec<EventPayload>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkerRegisterRequest {
    pub worker_id: String,
    pub api_url: String,
    pub slots: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkerRegisterResponse {
    pub accepted: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkerHeartbeat {
    pub worker_id: String,
    pub metrics: WorkerMetrics,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct WorkerMetrics {
    pub cpu_pct: f64,
    pub mem_bytes: u64,
    pub active_topologies: usize,
    pub queue_depth: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TopologyDeployment {
    pub topology_id: Uuid,
    pub spec: TopologySpec,
    pub attempt: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TopologyTearDown {
    pub topology_id: Uuid,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum TopologyStatusKind {
    Accepted,
    Running,
    Failed,
    Completed,
    Canceled,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TopologyStatus {
    pub topology_id: Uuid,
    pub name: String,
    pub status: TopologyStatusKind,
    pub worker_id: Option<String>,
    pub metrics: TopologyMetrics,
    pub last_error: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct TopologyMetrics {
    pub events_ingested: u64,
    pub events_emitted: u64,
    pub last_checkpoint: Option<DateTime<Utc>>,
    pub windows_open: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TopologySubmitResponse {
    pub topology_id: Uuid,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkerEventBatch {
    pub topology_id: Uuid,
    pub events: Vec<EventPayload>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CheckpointManifest {
    pub topology_id: Uuid,
    pub operator_id: OperatorId,
    pub window_start: DateTime<Utc>,
    pub window_end: DateTime<Utc>,
    pub payload: Value,
}
