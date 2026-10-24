#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
TEST_RUNNER="$BUILD_DIR/self_test_runner"

mkdir -p "$BUILD_DIR"

echo "Compiling..."
gcc -std=c11 -Wall -Wextra -pedantic -Werror -o "$TEST_RUNNER" "$PROJECT_ROOT/tests/self_test.c"

echo "Running tests..."
"$TEST_RUNNER" "$@"
