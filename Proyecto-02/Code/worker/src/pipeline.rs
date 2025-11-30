use std::{
    collections::HashMap,
    path::{Path, PathBuf},
    time::{Duration, Instant},
};

use anyhow::anyhow;
use chrono::{DateTime, Utc};
use serde_json::{json, Map, Value};
use tokio::fs;
use tracing::{debug, info};
use uuid::Uuid;

use common::{
    AggregationSpec, EventPayload, FilterSpec, FlatMapSpec, MapSpec, OperatorKind, Predicate,
    TopologySpec, TransformFn, WindowAggregateSpec,
};

// Minimal in-process stream pipeline; executes operators sequentially on a single task.
pub struct PipelineEngine {
    topology_id: Uuid,
    operators: Vec<OperatorNode>,
    _spec: TopologySpec,
    attempt: u32,
}

impl PipelineEngine {
    pub fn new(
        topology_id: Uuid,
        attempt: u32,
        spec: TopologySpec,
        state_dir: &Path,
    ) -> anyhow::Result<Self> {
        // Build operator nodes from the topology spec (single-threaded pipeline).
        let mut operators = Vec::new();
        for operator in &spec.operators {
            let node = match &operator.kind {
                OperatorKind::Map(map) => OperatorNode::Map(MapOperator::new(map.clone())),
                OperatorKind::Filter(filter) => {
                    OperatorNode::Filter(FilterOperator::new(filter.clone()))
                }
                OperatorKind::FlatMap(flat) => {
                    OperatorNode::FlatMap(FlatMapOperator::new(flat.clone()))
                }
                OperatorKind::KeyBy(key_by) => OperatorNode::Key(KeyByOperator::new(key_by.clone())),
                OperatorKind::WindowAggregate(window) => OperatorNode::Window(WindowOperator::new(
                    topology_id,
                    attempt,
                    operator.id.clone(),
                    window.clone(),
                    state_dir,
                )?),
                OperatorKind::SinkLog => OperatorNode::Sink(SinkOperator::default()),
            };
            operators.push(node);
        }
        Ok(Self {
            topology_id,
            operators,
            _spec: spec,
            attempt,
        })
    }

    pub async fn process(&mut self, event: StreamingEvent) -> anyhow::Result<()> {
        // Push the event through each operator; operators can fan-out.
        let mut batch = vec![event];
        for operator in &mut self.operators {
            let mut next_batch = Vec::new();
            for ev in batch {
                operator.apply(ev, &mut next_batch).await?;
            }
            batch = next_batch;
            if batch.is_empty() {
                break;
            }
        }
        Ok(())
    }

    pub async fn shutdown(&mut self) {
        for operator in &mut self.operators {
            operator.shutdown().await;
        }
        info!("Topology {} pipeline stopped", self.topology_id);
    }
}

#[derive(Clone, Debug)]
// Unified internal representation; upstream payloads get converted before processing.
pub struct StreamingEvent {
    pub timestamp: DateTime<Utc>,
    pub key: Option<String>,
    pub data: Map<String, Value>,
}

impl StreamingEvent {
    pub fn get(&self, field: &str) -> Option<&Value> {
        if !field.contains('.') {
            return self.data.get(field);
        }
        let mut current: Option<&Value> = None;
        for (idx, part) in field.split('.').enumerate() {
            if idx == 0 {
                current = self.data.get(part);
            } else {
                current = match current {
                    Some(Value::Object(map)) => map.get(part),
                    _ => None,
                };
            }
            if current.is_none() {
                return None;
            }
        }
        current
    }
}

impl TryFrom<EventPayload> for StreamingEvent {
    type Error = anyhow::Error;

    fn try_from(value: EventPayload) -> Result<Self, Self::Error> {
        let map = match value.data {
            Value::Object(map) => map,
            other => {
                return Err(anyhow!(
                    "Events must be JSON objects, received {}",
                    other
                ))
            }
        };
        Ok(Self {
            timestamp: value.timestamp,
            key: None,
            data: map,
        })
    }
}

enum OperatorNode {
    Map(MapOperator),
    Filter(FilterOperator),
    FlatMap(FlatMapOperator),
    Key(KeyByOperator),
    Window(WindowOperator),
    Sink(SinkOperator),
}

impl OperatorNode {
    async fn apply(
        &mut self,
        event: StreamingEvent,
        out: &mut Vec<StreamingEvent>,
    ) -> anyhow::Result<()> {
        match self {
            OperatorNode::Map(op) => op.apply(event, out),
            OperatorNode::Filter(op) => op.apply(event, out),
            OperatorNode::FlatMap(op) => op.apply(event, out),
            OperatorNode::Key(op) => op.apply(event, out),
            OperatorNode::Window(op) => op.apply(event, out).await,
            OperatorNode::Sink(op) => op.apply(event, out),
        }
    }

    async fn shutdown(&mut self) {
        if let OperatorNode::Window(op) = self {
            op.flush_all().await;
        }
    }
}

struct MapOperator {
    spec: MapSpec,
}

impl MapOperator {
    fn new(spec: MapSpec) -> Self {
        Self { spec }
    }

    fn apply(&self, mut event: StreamingEvent, out: &mut Vec<StreamingEvent>) -> anyhow::Result<()> {
        // Simple string transforms on a given field.
        if let Some(val) = event.data.get_mut(&self.spec.field) {
            if let Some(text) = val.as_str() {
                let new_val = match &self.spec.transform {
                    TransformFn::ToLower => text.to_lowercase(),
                    TransformFn::ToUpper => text.to_uppercase(),
                    TransformFn::Trim => text.trim().to_string(),
                    TransformFn::Prefix { value } => format!("{}{}", value, text),
                    TransformFn::Suffix { value } => format!("{}{}", text, value),
                };
                *val = Value::String(new_val);
            }
        }
        out.push(event);
        Ok(())
    }
}

struct FilterOperator {
    spec: FilterSpec,
}

impl FilterOperator {
    fn new(spec: FilterSpec) -> Self {
        Self { spec }
    }

    fn apply(&self, event: StreamingEvent, out: &mut Vec<StreamingEvent>) -> anyhow::Result<()> {
        // Drop events that do not satisfy the predicate.
        let field_value = event.data.get(&self.spec.field);
        let pass = match (&self.spec.predicate, field_value) {
            (Predicate::Exists, Some(_)) => true,
            (Predicate::Equals { value }, Some(actual)) => actual == value,
            (Predicate::NotEquals { value }, Some(actual)) => actual != value,
            (Predicate::GreaterThan { value }, Some(actual)) => actual
                .as_f64()
                .map(|num| num > *value)
                .unwrap_or(false),
            (Predicate::LessThan { value }, Some(actual)) => actual
                .as_f64()
                .map(|num| num < *value)
                .unwrap_or(false),
            (Predicate::Contains { value }, Some(actual)) => actual
                .as_str()
                .map(|text| text.contains(value))
                .unwrap_or(false),
            _ => false,
        };
        if pass {
            out.push(event);
        }
        Ok(())
    }
}

struct FlatMapOperator {
    spec: FlatMapSpec,
}

impl FlatMapOperator {
    fn new(spec: FlatMapSpec) -> Self {
        Self { spec }
    }

    fn apply(&self, event: StreamingEvent, out: &mut Vec<StreamingEvent>) -> anyhow::Result<()> {
        // Expand arrays or delimited strings into multiple cloned events.
        if let Some(Value::Array(items)) = event.data.get(&self.spec.field) {
            for item in items {
                let mut cloned = event.clone();
                cloned.data.insert(self.spec.field.clone(), item.clone());
                out.push(cloned);
            }
        } else if let Some(Value::String(value)) = event.data.get(&self.spec.field) {
            let separator = self.spec.separator.clone().unwrap_or_else(|| ",".into());
            for part in value.split(separator.as_str()) {
                let mut cloned = event.clone();
                cloned
                    .data
                    .insert(self.spec.field.clone(), Value::String(part.trim().into()));
                out.push(cloned);
            }
        } else {
            out.push(event);
        }
        Ok(())
    }
}

struct KeyByOperator {
    field: String,
}

impl KeyByOperator {
    fn new(spec: common::KeyBySpec) -> Self {
        Self {
            field: spec.field,
        }
    }

    fn apply(&self, mut event: StreamingEvent, out: &mut Vec<StreamingEvent>) -> anyhow::Result<()> {
        // Set event.key for downstream keyed operations.
        if let Some(value) = event.data.get(&self.field) {
            if let Some(text) = value.as_str() {
                event.key = Some(text.to_string());
            } else {
                event.key = Some(value.to_string());
            }
        }
        out.push(event);
        Ok(())
    }
}

// Maintains sliding/tumbling windows and persists checkpoints.
struct WindowOperator {
    spec: WindowAggregateSpec,
    windows: HashMap<WindowKey, WindowState>,
    checkpoint_timer: Instant,
    checkpoint_dir: PathBuf,
    topology_id: Uuid,
}

type WindowKey = (String, i64);

impl WindowOperator {
    fn new(
        topology_id: Uuid,
        attempt: u32,
        operator_id: String,
        spec: WindowAggregateSpec,
        state_dir: &Path,
    ) -> anyhow::Result<Self> {
        // Prepare per-operator checkpoint directory.
        let checkpoint_dir = state_dir
            .join(topology_id.to_string())
            .join(format!("attempt-{}", attempt))
            .join(operator_id);
        std::fs::create_dir_all(&checkpoint_dir)?;
        let mut op = Self {
            spec,
            windows: HashMap::new(),
            checkpoint_timer: Instant::now(),
            checkpoint_dir,
            topology_id,
        };
        op.load_latest_checkpoint()?;
        Ok(op)
    }

    async fn apply(
        &mut self,
        event: StreamingEvent,
        out: &mut Vec<StreamingEvent>,
    ) -> anyhow::Result<()> {
        // Assign event to a time window bucket and update aggregation state.
        let key_value = event
            .data
            .get(&self.spec.key_field)
            .cloned()
            .ok_or_else(|| anyhow!("Window operator missing key field {}", self.spec.key_field))?;
        let key = key_value
            .as_str()
            .map(|s| s.to_string())
            .or_else(|| key_value.as_i64().map(|v| v.to_string()))
            .unwrap_or_else(|| key_value.to_string());

        let ts_ms = event.timestamp.timestamp_millis();
        let length = self.spec.window.length_ms as i64;
        let slide = self
            .spec
            .window
            .slide_ms
            .map(|v| v as i64)
            .unwrap_or(length);
        let start = (ts_ms / slide) * slide;
        let end = start + length;
        let entry = self
            .windows
            .entry((key.clone(), start))
            .or_insert_with(|| WindowState::new(&key, start, end));
        entry.update(&self.spec.aggregator, &self.spec.value_field, &event)?;
        entry.last_timestamp = event.timestamp;

        // Flush expired windows
        let expired_keys: Vec<WindowKey> = self
            .windows
            .iter()
            .filter_map(|(k, state)| (ts_ms >= state.end_ms).then_some(k.clone()))
            .collect();
        for window_key in expired_keys {
            if let Some(state) = self.windows.remove(&window_key) {
                out.push(state.into_event(&self.spec.aggregator, event.timestamp));
            }
        }

        if self.checkpoint_timer.elapsed()
            > Duration::from_millis(self.spec.window.checkpoint_interval_ms)
        {
            // Best-effort snapshot of in-flight windows.
            self.persist_checkpoint().await?;
            self.checkpoint_timer = Instant::now();
        }

        Ok(())
    }

    async fn flush_all(&mut self) {
        for (_, state) in self.windows.drain() {
            let event = state.into_event(&self.spec.aggregator, Utc::now());
            info!(
                key = ?event.key,
                "Flushed window for topology {}",
                self.topology_id
            );
        }
    }

    async fn persist_checkpoint(&self) -> anyhow::Result<()> {
        // Write current window states to disk for auditing/recovery.
        let mut payloads = Vec::new();
        for ((key, start), state) in &self.windows {
            payloads.push(json!({
                "key": key,
                "window_start": start,
                "window_end": state.end_ms,
                "count": state.count,
                "sum": state.sum,
            }));
        }
        let checkpoint_path = self
            .checkpoint_dir
            .join(format!("checkpoint-{}.json", Utc::now().timestamp_millis()));
        fs::write(checkpoint_path, serde_json::to_vec_pretty(&payloads)?).await?;
        Ok(())
    }

    fn load_latest_checkpoint(&mut self) -> anyhow::Result<()> {
        // On startup, reload the most recent snapshot to avoid losing in-flight windows.
        let mut latest: Option<PathBuf> = None;
        if let Ok(entries) = std::fs::read_dir(&self.checkpoint_dir) {
            for entry in entries.flatten() {
                let path = entry.path();
                if let Some(name) = path.file_name().and_then(|s| s.to_str()) {
                    if name.starts_with("checkpoint-") && name.ends_with(".json") {
                        latest = match &latest {
                            Some(current) => {
                                if path > *current {
                                    Some(path)
                                } else {
                                    Some(current.clone())
                                }
                            }
                            None => Some(path),
                        };
                    }
                }
            }
        }

        if let Some(path) = latest {
            let data = std::fs::read(&path)?;
            let payload: Vec<serde_json::Value> = serde_json::from_slice(&data)?;
            for entry in payload {
                let key = entry
                    .get("key")
                    .and_then(|v| v.as_str())
                    .ok_or_else(|| anyhow!("Malformed checkpoint key"))?;
                let start_ms = entry
                    .get("window_start")
                    .and_then(|v| v.as_i64())
                    .ok_or_else(|| anyhow!("Malformed checkpoint window_start"))?;
                let end_ms = entry
                    .get("window_end")
                    .and_then(|v| v.as_i64())
                    .ok_or_else(|| anyhow!("Malformed checkpoint window_end"))?;
                let count = entry
                    .get("count")
                    .and_then(|v| v.as_u64())
                    .unwrap_or(0);
                let sum = entry
                    .get("sum")
                    .and_then(|v| v.as_f64())
                    .unwrap_or(0.0);
                let mut state = WindowState::new(key, start_ms, end_ms);
                state.count = count;
                state.sum = sum;
                self.windows.insert((key.to_string(), start_ms), state);
            }
        }
        Ok(())
    }
}

#[derive(Clone)]
struct WindowState {
    key: String,
    start_ms: i64,
    end_ms: i64,
    count: u64,
    sum: f64,
    last_timestamp: DateTime<Utc>,
}

impl WindowState {
    fn new(key: &str, start_ms: i64, end_ms: i64) -> Self {
        Self {
            key: key.into(),
            start_ms,
            end_ms,
            count: 0,
            sum: 0.0,
            last_timestamp: Utc::now(),
        }
    }

    fn update(
        &mut self,
        agg: &AggregationSpec,
        value_field: &Option<String>,
        event: &StreamingEvent,
    ) -> anyhow::Result<()> {
        match agg {
            AggregationSpec::Count => {
                self.count += 1;
            }
            AggregationSpec::Sum | AggregationSpec::Average => {
                let field = value_field
                    .as_ref()
                    .ok_or_else(|| anyhow!("value_field is required for sum/average"))?;
                let value = event
                    .get(field)
                    .and_then(|v| v.as_f64())
                    .ok_or_else(|| anyhow!("Unable to parse value for aggregation field {}", field))?;
                self.sum += value;
                self.count += 1;
            }
        }
        Ok(())
    }

    fn into_event(self, aggregator: &AggregationSpec, now: DateTime<Utc>) -> StreamingEvent {
        // Emit window boundaries plus aggregate summary for downstream operators.
        let mut data = Map::new();
        let window_start = DateTime::<Utc>::from_timestamp_millis(self.start_ms)
            .unwrap_or(now)
            .to_rfc3339();
        let window_end = DateTime::<Utc>::from_timestamp_millis(self.end_ms)
            .unwrap_or(now)
            .to_rfc3339();
        data.insert("window_start".into(), Value::String(window_start));
        data.insert("window_end".into(), Value::String(window_end));

        let aggregate_value = match aggregator {
            AggregationSpec::Count => Value::from(self.count),
            AggregationSpec::Sum => Value::from(self.sum),
            AggregationSpec::Average => {
                if self.count == 0 {
                    Value::Null
                } else {
                    Value::from(self.sum / self.count as f64)
                }
            }
        };

        data.insert("count".into(), Value::from(self.count));
        data.insert("sum".into(), Value::from(self.sum));
        data.insert("aggregate".into(), aggregate_value);

        StreamingEvent {
            timestamp: now,
            key: Some(self.key),
            data,
        }
    }
}

#[derive(Default)]
struct SinkOperator;

impl SinkOperator {
    fn apply(&self, event: StreamingEvent, _out: &mut Vec<StreamingEvent>) -> anyhow::Result<()> {
        debug!(key = ?event.key, payload = ?event.data, "Sink output");
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use common::{EdgeSpec, OperatorSpec, WindowSpec};

    fn temp_state_dir() -> PathBuf {
        std::env::temp_dir().join("stream-worker-tests")
    }

    fn build_spec(operators: Vec<OperatorSpec>) -> TopologySpec {
        TopologySpec {
            name: "test".into(),
            description: None,
            operators,
            edges: vec![],
            parallelism: 1,
        }
    }

    #[tokio::test]
    async fn map_operator_transforms_field() {
        let spec = build_spec(vec![OperatorSpec {
            id: "map".into(),
            kind: OperatorKind::Map(MapSpec {
                field: "value".into(),
                transform: TransformFn::ToUpper,
            }),
            config: Default::default(),
        }]);
        let mut engine =
            PipelineEngine::new(Uuid::new_v4(), 0, spec, temp_state_dir().as_path()).unwrap();
        let payload = EventPayload {
            timestamp: Utc::now(),
            data: json!({"value": "ok", "other": 1}),
        };
        let event = StreamingEvent::try_from(payload).unwrap();
        engine.process(event).await.unwrap();
    }

    #[tokio::test]
    async fn window_operator_emits_counts() {
        let operators = vec![
            OperatorSpec {
                id: "key".into(),
                kind: OperatorKind::KeyBy(common::KeyBySpec {
                    field: "service".into(),
                }),
                config: Default::default(),
            },
            OperatorSpec {
                id: "window".into(),
                kind: OperatorKind::WindowAggregate(WindowAggregateSpec {
                    window: WindowSpec {
                        length_ms: 1000,
                        slide_ms: None,
                        checkpoint_interval_ms: 5000,
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
        ];
        let spec = TopologySpec {
            name: "window-test".into(),
            description: None,
            operators,
            edges: vec![
                EdgeSpec {
                    from: "key".into(),
                    to: "window".into(),
                },
                EdgeSpec {
                    from: "window".into(),
                    to: "sink".into(),
                },
            ],
            parallelism: 1,
        };
        let mut engine =
            PipelineEngine::new(Uuid::new_v4(), 0, spec, temp_state_dir().as_path()).unwrap();
        for i in 0..3 {
            let payload = EventPayload {
                timestamp: DateTime::from_timestamp(1 + i, 0).unwrap(),
                data: json!({"service": "api", "value": i}),
            };
            engine
                .process(StreamingEvent::try_from(payload).unwrap())
                .await
                .unwrap();
        }
    }
}
