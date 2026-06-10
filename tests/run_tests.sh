#!/usr/bin/env bash
set -e

# Build compiler
echo "Building compiler..."
make

# Directory paths
TEST_DIR="tests"
SCC="./ncc"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

passed=0
failed=0

echo "Running integration tests in-memory..."
for test_file in "$TEST_DIR"/test*.nc; do
    name=$(basename "$test_file" .nc)

    # Extract expected exit code
    expected=$(grep -E "// Expected:" "$test_file" | awk '{print $3}')
    if [ -z "$expected" ]; then
        echo -e "${RED}Skipped $name (no expected code comment)${NC}"
        continue
    fi

    # Run in memory JIT and check exit status
    set +e
    "$SCC" -r "$test_file" > /dev/null 2>&1
    actual=$?
    set -e

    if [ "$actual" -eq "$expected" ]; then
        echo -e "${GREEN}PASS: $name (returned $actual)${NC}"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAIL: $name (expected $expected, got $actual)${NC}"
        failed=$((failed + 1))
    fi
done

echo "----------------------------------------"
echo "Tests completed: $passed passed, $failed failed."
if [ "$failed" -gt 0 ]; then
    exit 1
fi
