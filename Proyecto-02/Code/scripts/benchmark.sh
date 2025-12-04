#!/bin/bash

set -e

echo "==================================="
echo "Benchmark Suite"
echo "==================================="

# Create benchmark results directory
mkdir -p benchmarks

# Function to get system info
get_system_info() {
    echo "=== System Information ===" > benchmarks/system_info.txt
    echo "Date: $(date)" >> benchmarks/system_info.txt
    echo "Hostname: $(hostname)" >> benchmarks/system_info.txt
    echo "OS: $(uname -a)" >> benchmarks/system_info.txt
    echo "CPU: $(lscpu | grep 'Model name' | cut -d':' -f2 | xargs)" >> benchmarks/system_info.txt
    echo "RAM: $(free -h | grep Mem | awk '{print $2}')" >> benchmarks/system_info.txt
    echo "Rust version: $(rustc --version)" >> benchmarks/system_info.txt
    echo "" >> benchmarks/system_info.txt
}

# Start fresh environment
echo "Preparing environment..."
get_system_info

# Kill any existing processes
pkill -f "target/.*master" || true
pkill -f "target/.*worker" || true
sleep 2

# Build in release mode
echo "Building in release mode..."
cargo build --release --workspace

# Start master
echo "Starting master..."
./target/release/master > benchmarks/master.log 2>&1 &
MASTER_PID=$!
sleep 3

# Verify master is running
if ! curl -s http://127.0.0.1:8080/api/v1/metrics > /dev/null 2>&1; then
    echo "Error: Master failed to start"
    kill $MASTER_PID 2>/dev/null || true
    exit 1
fi

# Start 2 workers
echo "Starting workers..."
./target/release/worker --worker-id worker-1 --bind 127.0.0.1:9001 \
    --master-url http://127.0.0.1:8080 \
    --advertise-url http://127.0.0.1:9001 > benchmarks/worker-1.log 2>&1 &
WORKER1_PID=$!

./target/release/worker --worker-id worker-2 --bind 127.0.0.1:9002 \
    --master-url http://127.0.0.1:8080 \
    --advertise-url http://127.0.0.1:9002 > benchmarks/worker-2.log 2>&1 &
WORKER2_PID=$!

sleep 3

# Verify workers registered
WORKER_COUNT=$(curl -s http://127.0.0.1:8080/api/v1/metrics | jq '.workers | length')
if [ "$WORKER_COUNT" -lt 2 ]; then
    echo "Error: Not all workers registered (found: $WORKER_COUNT)"
    kill $MASTER_PID $WORKER1_PID $WORKER2_PID 2>/dev/null || true
    exit 1
fi

echo "Environment ready: Master + 2 Workers"

# Cleanup function
cleanup() {
    echo "Cleaning up..."
    kill $MASTER_PID $WORKER1_PID $WORKER2_PID 2>/dev/null || true
    sleep 2
}

trap cleanup EXIT

# Run benchmarks
echo ""
echo "=== Benchmark 1: Single Topology Performance ==="
k6 run --vus 1 --duration 30s tests/throughput_test.js \
    | tee benchmarks/single_topology.txt

sleep 5

echo ""
echo "=== Benchmark 2: Multiple Topologies (Load Distribution) ==="
k6 run --vus 5 --duration 30s tests/load_test.js \
    | tee benchmarks/multi_topology.txt

sleep 5

echo ""
echo "=== Benchmark 3: High Throughput (Stress Test) ==="
k6 run tests/throughput_test.js \
    | tee benchmarks/high_throughput.txt

# Get final system metrics
echo ""
echo "=== Final System Metrics ==="
curl -s http://127.0.0.1:8080/api/v1/metrics | jq '.' | tee benchmarks/final_metrics.json

# Generate report
cat > benchmarks/REPORT.md <<EOF
# Benchmark Report

Generated: $(date)

## System Configuration
$(cat benchmarks/system_info.txt)

## Test Environment
- Master: 1 instance
- Workers: 2 instances
- Rust: Release mode

## Benchmark Results

### 1. Single Topology Performance (30s, 1 VU)
See: benchmarks/single_topology.txt

### 2. Multiple Topologies Load Distribution (30s, 5 VUs)
See: benchmarks/multi_topology.txt

### 3. High Throughput Stress Test (60s, 5 VUs, batches of 100)
See: benchmarks/high_throughput.txt

## Key Metrics
$(cat benchmarks/final_metrics.json | jq '{
  workers: [.workers[] | {
    id: .worker_id,
    cpu_pct: .metrics.cpu_pct,
    mem_gb: (.metrics.mem_bytes / 1024 / 1024 / 1024),
    active_topologies: .metrics.active_topologies,
    throughput_eps: .metrics.throughput_eps
  }],
  topologies: [.topologies[] | {
    name: .name,
    status: .status,
    events_ingested: .metrics.events_ingested,
    events_emitted: .metrics.events_emitted
  }]
}')

## Logs
- Master: benchmarks/master.log
- Worker 1: benchmarks/worker-1.log
- Worker 2: benchmarks/worker-2.log
EOF

echo ""
echo "==================================="
echo "Benchmarks Complete!"
echo "==================================="
echo "Report: benchmarks/REPORT.md"
echo "Results: benchmarks/"
echo ""
cat benchmarks/REPORT.md