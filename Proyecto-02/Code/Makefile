SHELL := /bin/bash

WORKSPACE ?= .

.PHONY: build fmt lint test run-master run-worker client submit ingest compose-up compose-down

build:
	cargo build --workspace

fmt:
	cargo fmt --all

lint:
	cargo clippy --workspace --all-targets -- -D warnings

test:
	cargo test --workspace

run-master:
	cargo run -p master

run-worker:
	cargo run -p worker -- --worker-id worker-1 --bind 0.0.0.0:9001 --master-url http://master:8080

client:
	cargo run -p client -- --master-url http://127.0.0.1:8080 status $$TOPOLOGY_ID

compose-up:
	docker compose up --build

compose-down:
	docker compose down
