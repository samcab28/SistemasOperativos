SHELL := /bin/bash

WORKSPACE ?= .

.PHONY: build fmt lint test test-unit test-integration test-e2e test-all \
        run-master run-worker client submit ingest \
        load-test benchmark clean coverage help

# Default target
help:
	@echo "Available targets:"
	@echo "  build            - Build all workspace crates"
	@echo "  fmt              - Format code"
	@echo "  lint             - Run clippy"
	@echo "  test             - Run all tests (unit + integration)"
	@echo "  test-unit        - Run unit tests only"
	@echo "  test-integration - Run integration tests"
	@echo "  test-e2e         - Run end-to-end tests (requires running master/worker)"
	@echo "  test-all         - Run complete test suite with script"
	@echo "  load-test        - Run k6 load tests"
	@echo "  benchmark        - Run full benchmark suite"
	@echo "  coverage         - Generate code coverage report"
	@echo "  run-master       - Start master node"
	@echo "  run-worker       - Start worker node"
	@echo "  clean            - Clean build artifacts and test results"

build:
	cargo build --workspace

build-release:
	cargo build --release --workspace

fmt:
	cargo fmt --all

lint:
	cargo clippy --workspace --all-targets -- -D warnings

# Unit tests (in src/ files)
test-unit:
	cargo test --workspace --lib

# Integration tests (in tests/ directories)
test-integration:
	cargo test --workspace --test '*'

# E2E tests (requires running services)
test-e2e:
	@echo "Checking if master is running..."
	@if curl -s http://127.0.0.1:8080/api/v1/metrics > /dev/null 2>&1; then \
		echo "Running E2E tests..."; \
		cargo test -p common --test e2e_test -- --ignored --test-threads=1; \
	else \
		echo "Error: Master not running at http://127.0.0.1:8080"; \
		echo "Start with: make run-master"; \
		exit 1; \
	fi

# Run basic tests (no E2E)
test:
	cargo test --workspace

# Complete test suite with script
test-all:
	@chmod +x scripts/run_tests.sh
	@./scripts/run_tests.sh

# Load testing with k6
load-test:
	@chmod +x scripts/load_test.sh
	@./scripts/load_test.sh

# Benchmark suite
benchmark:
	@chmod +x scripts/benchmark.sh
	@./scripts/benchmark.sh

# Coverage report
coverage:
	@if command -v cargo-tarpaulin &> /dev/null; then \
		cargo tarpaulin --workspace --out Html --output-dir coverage; \
		echo "Coverage report: coverage/index.html"; \
	else \
		echo "cargo-tarpaulin not installed"; \
		echo "Install with: cargo install cargo-tarpaulin"; \
	fi

run-master:
	cargo run -p master

run-worker:
	cargo run -p worker -- \
		--worker-id worker-1 \
		--bind 127.0.0.1:9001 \
		--master-url http://127.0.0.1:8080 \
		--advertise-url http://127.0.0.1:9001 \
		--slots 2

run-worker-2:
	cargo run -p worker -- \
		--worker-id worker-2 \
		--bind 127.0.0.1:9002 \
		--master-url http://127.0.0.1:8080 \
		--advertise-url http://127.0.0.1:9002 \
		--slots 2

client:
	cargo run -p client -- --master-url http://127.0.0.1:8080 $$ARGS

submit:
	@if [ -z "$$TOPOLOGY" ]; then \
		echo "Usage: make submit TOPOLOGY=path/to/topology.json"; \
		exit 1; \
	fi
	cargo run -p client -- \
		--master-url http://127.0.0.1:8080 \
		submit-topology $$TOPOLOGY

ingest:
	@if [ -z "$$TOPOLOGY_ID" ] || [ -z "$$FILE" ]; then \
		echo "Usage: make ingest TOPOLOGY_ID=<id> FILE=path/to/events.jsonl"; \
		exit 1; \
	fi
	cargo run -p client -- \
		--master-url http://127.0.0.1:8080 \
		ingest $$TOPOLOGY_ID --file $$FILE

status:
	@if [ -z "$$TOPOLOGY_ID" ]; then \
		echo "Usage: make status TOPOLOGY_ID=<id>"; \
		exit 1; \
	fi
	cargo run -p client -- \
		--master-url http://127.0.0.1:8080 \
		status $$TOPOLOGY_ID

cancel:
	@if [ -z "$$TOPOLOGY_ID" ]; then \
		echo "Usage: make cancel TOPOLOGY_ID=<id>"; \
		exit 1; \
	fi
	cargo run -p client -- \
		--master-url http://127.0.0.1:8080 \
		cancel $$TOPOLOGY_ID

metrics:
	@curl -s http://127.0.0.1:8080/api/v1/metrics | jq '.'

clean:
	cargo clean
	rm -rf state/
	rm -rf results/
	rm -rf benchmarks/
	rm -rf coverage/
	rm -rf /tmp/stream-worker-tests/
	rm -rf /tmp/worker-test-*
	rm -rf /tmp/checkpoint-test-*

# Docker compose targets (if needed)
compose-up:
	docker compose up --build

compose-down:
	docker compose down

# Development workflow
dev: fmt lint test
	@echo "✓ Development checks passed"

# CI workflow
ci: fmt lint test-all
	@echo "✓ CI checks passed"