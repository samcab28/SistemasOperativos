#!/bin/bash

set -e

echo "==================================="
echo "Running Test Suite"
echo "==================================="

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 1. Unit Tests
echo -e "${YELLOW}[1/4] Running unit tests...${NC}"
cargo test --workspace --lib -- --test-threads=1
echo -e "${GREEN}✓ Unit tests passed${NC}\n"

# 2. Integration Tests
echo -e "${YELLOW}[2/4] Running integration tests...${NC}"
cargo test --workspace --test '*' -- --test-threads=1
echo -e "${GREEN}✓ Integration tests passed${NC}\n"

# 3. Check if master and worker are running for E2E
echo -e "${YELLOW}[3/4] Checking E2E prerequisites...${NC}"
if curl -s http://127.0.0.1:8080/api/v1/metrics > /dev/null 2>&1; then
    echo "Master is running, executing E2E tests..."
    cargo test --test e2e_test -- --ignored --test-threads=1
    echo -e "${GREEN}✓ E2E tests passed${NC}\n"
else
    echo -e "${YELLOW}⚠ Master not running, skipping E2E tests${NC}"
    echo "  To run E2E tests:"
    echo "  1. Start master: cargo run -p master"
    echo "  2. Start worker: cargo run -p worker -- --worker-id worker-1 --bind 127.0.0.1:9001 --master-url http://127.0.0.1:8080 --advertise-url http://127.0.0.1:9001"
    echo "  3. Run: cargo test --test e2e_test -- --ignored"
    echo ""
fi

# 4. Code Quality
echo -e "${YELLOW}[4/4] Running code quality checks...${NC}"
echo "Checking formatting..."
cargo fmt --all -- --check || (echo -e "${RED}✗ Code needs formatting. Run: make fmt${NC}" && exit 1)
echo "Running clippy..."
cargo clippy --workspace --all-targets -- -D warnings || (echo -e "${RED}✗ Clippy warnings found${NC}" && exit 1)
echo -e "${GREEN}✓ Code quality checks passed${NC}\n"

echo "==================================="
echo -e "${GREEN}All tests completed successfully!${NC}"
echo "==================================="

# Generate coverage report if possible
if command -v cargo-tarpaulin &> /dev/null; then
    echo -e "\n${YELLOW}Generating coverage report...${NC}"
    cargo tarpaulin --workspace --out Html --output-dir coverage
    echo -e "${GREEN}✓ Coverage report generated in coverage/index.html${NC}"
fi