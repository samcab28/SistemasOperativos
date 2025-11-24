#!/usr/bin/env bash
set -euo pipefail

MASTER_URL=${MASTER_URL:-http://127.0.0.1:8080}
TOPOLOGY_FILE=${TOPOLOGY_FILE:-docs/examples/log_topology.json}
EVENTS_FILE=${EVENTS_FILE:-docs/examples/logs.jsonl}

echo "[demo] Building workspace..."
cargo build --workspace

echo "[demo] Starting master and worker in background..."
{ cargo run -p master & echo $! > /tmp/master.pid; }
sleep 1
{ cargo run -p worker -- --worker-id worker-demo --bind 127.0.0.1:9001 --advertise-url http://127.0.0.1:9001 --master-url "$MASTER_URL" & echo $! > /tmp/worker.pid; }
sleep 3

echo "[demo] Submitting topology..."
TOPOLOGY_ID=$(cargo run -p client -- --master-url "$MASTER_URL" submit-topology "$TOPOLOGY_FILE" | awk '{print $3}')
echo "[demo] Topology id: $TOPOLOGY_ID"

echo "[demo] Injecting events..."
cargo run -p client -- --master-url "$MASTER_URL" ingest "$TOPOLOGY_ID" --file "$EVENTS_FILE"

echo "[demo] Querying status..."
cargo run -p client -- --master-url "$MASTER_URL" status "$TOPOLOGY_ID"

echo "[demo] Stopping processes..."
kill "$(cat /tmp/master.pid)" "$(cat /tmp/worker.pid)"
rm -f /tmp/master.pid /tmp/worker.pid
