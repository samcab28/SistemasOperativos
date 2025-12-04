#!/bin/bash

set -e

echo "==================================="
echo "Distributed Streaming Engine Demo"
echo "==================================="

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Cleaning up...${NC}"
    kill $MASTER_PID $WORKER1_PID $WORKER2_PID 2>/dev/null || true
    sleep 2
    echo -e "${GREEN}✓ Cleanup complete${NC}"
}

trap cleanup EXIT INT TERM

# Build project
echo -e "${BLUE}Building project...${NC}"
cargo build --release --workspace
echo -e "${GREEN}✓ Build complete${NC}\n"

# Start master
echo -e "${BLUE}Starting master...${NC}"
./target/release/master > /tmp/master.log 2>&1 &
MASTER_PID=$!
sleep 3

if ! curl -s http://127.0.0.1:8080/api/v1/metrics > /dev/null 2>&1; then
    echo "Error: Master failed to start"
    exit 1
fi
echo -e "${GREEN}✓ Master started (PID: $MASTER_PID)${NC}\n"

# Start workers
echo -e "${BLUE}Starting worker-1...${NC}"
./target/release/worker \
    --worker-id worker-1 \
    --bind 127.0.0.1:9001 \
    --master-url http://127.0.0.1:8080 \
    --advertise-url http://127.0.0.1:9001 \
    > /tmp/worker-1.log 2>&1 &
WORKER1_PID=$!
sleep 2
echo -e "${GREEN}✓ Worker-1 started (PID: $WORKER1_PID)${NC}\n"

echo -e "${BLUE}Starting worker-2...${NC}"
./target/release/worker \
    --worker-id worker-2 \
    --bind 127.0.0.1:9002 \
    --master-url http://127.0.0.1:8080 \
    --advertise-url http://127.0.0.1:9002 \
    > /tmp/worker-2.log 2>&1 &
WORKER2_PID=$!
sleep 2
echo -e "${GREEN}✓ Worker-2 started (PID: $WORKER2_PID)${NC}\n"

# Verify system
echo -e "${BLUE}System Status:${NC}"
curl -s http://127.0.0.1:8080/api/v1/metrics | jq '{
  workers: [.workers[] | {id: .worker_id, slots: .metrics.active_topologies}],
  topologies: .topologies | length
}'
echo ""

# Demo 1: Simple Map + Filter Pipeline
echo -e "${YELLOW}=== Demo 1: Simple Map + Filter Pipeline ===${NC}"
cat > /tmp/demo_topology.json <<EOF
{
  "name": "demo-map-filter",
  "operators": [
    {
      "id": "map1",
      "kind": {
        "type": "map",
        "field": "message",
        "transform": {"kind": "to_lower"}
      },
      "config": {}
    },
    {
      "id": "filter1",
      "kind": {
        "type": "filter",
        "field": "level",
        "predicate": {"kind": "equals", "value": "error"}
      },
      "config": {}
    },
    {
      "id": "sink",
      "kind": {"type": "sink_log"},
      "config": {}
    }
  ],
  "edges": [
    {"from": "map1", "to": "filter1"},
    {"from": "filter1", "to": "sink"}
  ],
  "parallelism": 1
}
EOF

TOPOLOGY1=$(./target/release/client \
    --master-url http://127.0.0.1:8080 \
    submit-topology /tmp/demo_topology.json | grep -oP 'topology_id": "\K[^"]+')

echo -e "${GREEN}✓ Topology submitted: $TOPOLOGY1${NC}"
sleep 2

# Ingest events for Demo 1
cat > /tmp/demo_events.jsonl <<EOF
{"timestamp":"2024-01-01T10:00:00Z","data":{"level":"ERROR","message":"CRITICAL FAILURE","service":"api"}}
{"timestamp":"2024-01-01T10:00:01Z","data":{"level":"INFO","message":"Normal operation","service":"api"}}
{"timestamp":"2024-01-01T10:00:02Z","data":{"level":"ERROR","message":"ANOTHER ERROR","service":"web"}}
{"timestamp":"2024-01-01T10:00:03Z","data":{"level":"WARN","message":"Warning message","service":"api"}}
{"timestamp":"2024-01-01T10:00:04Z","data":{"level":"ERROR","message":"FAILED TO CONNECT","service":"db"}}
EOF

./target/release/client \
    --master-url http://127.0.0.1:8080 \
    ingest $TOPOLOGY1 --file /tmp/demo_events.jsonl

echo -e "${GREEN}✓ Events ingested${NC}"
sleep 2

# Show status
echo -e "\n${BLUE}Topology Status:${NC}"
./target/release/client \
    --master-url http://127.0.0.1:8080 \
    status $TOPOLOGY1
echo ""

# Demo 2: Window Aggregation
echo -e "${YELLOW}=== Demo 2: Window Aggregation Pipeline ===${NC}"
cat > /tmp/window_topology.json <<EOF
{
  "name": "demo-window-count",
  "operators": [
    {
      "id": "keyby",
      "kind": {
        "type": "key_by",
        "field": "service"
      },
      "config": {}
    },
    {
      "id": "window",
      "kind": {
        "type": "window_aggregate",
        "window": {
          "length_ms": 60000,
          "checkpoint_interval_ms": 30000
        },
        "aggregator": {"type": "count"},
        "key_field": "service"
      },
      "config": {}
    },
    {
      "id": "sink",
      "kind": {"type": "sink_log"},
      "config": {}
    }
  ],
  "edges": [],
  "parallelism": 1
}
EOF

TOPOLOGY2=$(./target/release/client \
    --master-url http://127.0.0.1:8080 \
    submit-topology /tmp/window_topology.json | grep -oP 'topology_id": "\K[^"]+')

echo -e "${GREEN}✓ Topology submitted: $TOPOLOGY2${NC}"
sleep 2

# Generate events for window aggregation
cat > /tmp/window_events.jsonl <<EOF
{"timestamp":"2024-01-01T10:00:00Z","data":{"service":"api","request_id":1}}
{"timestamp":"2024-01-01T10:00:01Z","data":{"service":"web","request_id":2}}
{"timestamp":"2024-01-01T10:00:02Z","data":{"service":"api","request_id":3}}
{"timestamp":"2024-01-01T10:00:03Z","data":{"service":"api","request_id":4}}
{"timestamp":"2024-01-01T10:00:04Z","data":{"service":"db","request_id":5}}
{"timestamp":"2024-01-01T10:00:05Z","data":{"service":"web","request_id":6}}
{"timestamp":"2024-01-01T10:00:06Z","data":{"service":"api","request_id":7}}
{"timestamp":"2024-01-01T10:00:07Z","data":{"service":"db","request_id":8}}
EOF

./target/release/client \
    --master-url http://127.0.0.1:8080 \
    ingest $TOPOLOGY2 --file /tmp/window_events.jsonl

echo -e "${GREEN}✓ Events ingested for window aggregation${NC}"
sleep 2

# Show system metrics
echo -e "\n${BLUE}System Metrics:${NC}"
curl -s http://127.0.0.1:8080/api/v1/metrics | jq '{
  workers: [.workers[] | {
    id: .worker_id,
    cpu: .metrics.cpu_pct,
    active_topologies: .metrics.active_topologies,
    queue_depth: .metrics.queue_depth
  }],
  topologies: [.topologies[] | {
    name: .name,
    status: .status,
    events_ingested: .metrics.events_ingested,
    events_emitted: .metrics.events_emitted
  }]
}'

# Demo 3: Fault Tolerance
echo -e "\n${YELLOW}=== Demo 3: Fault Tolerance (Simulated Worker Failure) ===${NC}"
echo -e "${BLUE}Current system state:${NC}"
curl -s http://127.0.0.1:8080/api/v1/metrics | jq '.workers[] | {id: .worker_id, is_down: .is_down, topologies: .topologies}'

echo -e "\n${YELLOW}Killing worker-1 to simulate failure...${NC}"
kill $WORKER1_PID
sleep 7  # Wait for heartbeat timeout

echo -e "\n${BLUE}System state after worker failure:${NC}"
curl -s http://127.0.0.1:8080/api/v1/metrics | jq '.workers[] | {id: .worker_id, is_down: .is_down, topologies: .topologies}'

echo -e "\n${GREEN}Notice: Topologies should be rescheduled to worker-2${NC}"

# Show final metrics
echo -e "\n${BLUE}Final Metrics:${NC}"
curl -s http://127.0.0.1:8080/api/v1/metrics | jq '.'

# Logs location
echo -e "\n${YELLOW}Logs available at:${NC}"
echo "  - Master: /tmp/master.log"
echo "  - Worker 1: /tmp/worker-1.log"
echo "  - Worker 2: /tmp/worker-2.log"

echo -e "\n${GREEN}Demo complete! Press Ctrl+C to exit.${NC}"
sleep 10