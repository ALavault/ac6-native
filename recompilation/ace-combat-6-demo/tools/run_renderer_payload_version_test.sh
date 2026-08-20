#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build=${TMPDIR:-/tmp}/ac6-renderer-payload-version-test-$$
trap 'rm -rf "$build"' EXIT
mkdir -p "$build"
${CXX:-c++} -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
  -I"$root/include" \
  "$root/tests/renderer_payload_version_tests.cpp" \
  -o "$build/renderer_payload_version_tests"
"$build/renderer_payload_version_tests"
