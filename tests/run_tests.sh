#!/usr/bin/env bash
set -e

# Cleanup temporary test files on exit
trap 'rm -f tests/*.s tests/*.out' EXIT

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

echo "Running integration tests..."
for test_file in "$TEST_DIR"/test*.c; do
    name=$(basename "$test_file" .c)

    # Extract expected exit code
    expected=$(grep -E "// Expected:" "$test_file" | awk '{print $3}')
    if [ -z "$expected" ]; then
        echo -e "${RED}Skipped $name (no expected code comment)${NC}"
        continue
    fi

    # Compile C to assembly
    if ! "$SCC" "$test_file" -o "$TEST_DIR/$name.s"; then
        echo -e "${RED}FAIL: $name (compiler compilation error)${NC}"
        failed=$((failed + 1))
        continue
    fi

    # Assemble assembly to binary
    if ! clang "$TEST_DIR/$name.s" -o "$TEST_DIR/$name.out"; then
        echo -e "${RED}FAIL: $name (clang assembly error)${NC}"
        failed=$((failed + 1))
        continue
    fi

    # Run and check exit status
    set +e
    "$TEST_DIR/$name.out"
    actual=$?
    set -e

    if [ "$actual" -eq "$expected" ]; then
        echo -e "${GREEN}PASS: $name (returned $actual)${NC}"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAIL: $name (expected $expected, got $actual)${NC}"
        failed=$((failed + 1))
    fi

    # Cleanup temp files
    rm -f "$TEST_DIR/$name.s" "$TEST_DIR/$name.out"
done

echo "----------------------------------------"
echo "Tests completed: $passed passed, $failed failed."
if [ "$failed" -gt 0 ]; then
    exit 1
fi
