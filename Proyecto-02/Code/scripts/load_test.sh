#!/bin/bash

set -e

echo "==================================="
echo "Running Load Tests with k6"
echo "==================================="

# Check if k6 is installed
if ! command -v k6 &> /dev/null; then
    echo "Error: k6 is not installed"
    echo "Install it from: https://k6.io/docs/getting-started/installation/"
    exit 1
fi

# Check if master is running
if ! curl -s http://127.0.0.1:8080/api/v1/metrics > /dev/null 2>&1; then
    echo "Error: Master is not running at http://127.0.0.1:8080"
    echo "Start it with: cargo run -p master"
    exit 1
fi

# Check if at least one worker is registered
METRICS=$(curl -s http://127.0.0.1:8080/api/v1/metrics)
WORKER_COUNT=$(echo "$METRICS" | jq '.workers | length')

if [ "$WORKER_COUNT" -eq 0 ]; then
    echo "Warning: No workers registered"
    echo "Start a worker with: cargo run -p worker -- --worker-id worker-1 --bind 127.0.0.1:9001 --master-url http://127.0.0.1:8080 --advertise-url http://127.0.0.1:9001"
    exit 1
fi

echo "Master is running with $WORKER_COUNT worker(s)"

# Create results directory
mkdir -p results

# Run basic load test
echo ""
echo "Running basic load test..."
k6 run --out json=results/load_test_results.json tests/load_test.js

# Run throughput test
echo ""
echo "Running throughput test..."
k6 run --out json=results/throughput_test_results.json tests/throughput_test.js

# Generate summary
echo ""
echo "==================================="
echo "Load Test Summary"
echo "==================================="
echo "Results saved in results/ directory"
echo ""
echo "View detailed results:"
    echo "  - results/load_test_results.json"
    echo "  - results/throughput_test_results.json"
    echo "  - results/throughput_summary.json"
    echo ""

# If jq is available, show quick summary
if command -v jq &> /dev/null && [ -f results/throughput_summary.json ]; then
    echo "Throughput Test Quick Stats:"
    cat results/throughput_summary.json | jq '{
      events_processed: .metrics.events_processed.values.count,
      avg_latency_ms: .metrics.batch_latency_ms.values.avg,
      p95_latency_ms: .metrics["batch_latency_ms"].values["p(95)"],
      throughput_eps: (.metrics.events_processed.values.count / 60)
    }'
fi

echo ""
echo "==================================="