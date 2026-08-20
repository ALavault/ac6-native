#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build=${TMPDIR:-/tmp}/ac6-canonical-tiling-test-$$
trap 'rm -rf "$build"' EXIT
mkdir -p "$build"
${CXX:-c++} -std=c++20 -UNDEBUG -Wall -Wextra -Wpedantic -Wconversion \
  -Wshadow -Wdouble-promotion -I"$root/include" \
  "$root/src/xenos_tiling.cpp" "$root/src/hash.cpp" \
  "$root/tests/ac6-demo-xenos-tiling-tests.cpp" \
  -o "$build/ac6-demo-xenos-tiling-tests"
"$build/ac6-demo-xenos-tiling-tests"
