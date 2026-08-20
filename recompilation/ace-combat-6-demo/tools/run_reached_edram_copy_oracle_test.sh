#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build=${TMPDIR:-/tmp}/ac6-edram-copy-oracle-test-$$
trap 'rm -rf "$build"' EXIT
mkdir -p "$build"
${CXX:-c++} -std=c++20 -UNDEBUG \
  -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wdouble-promotion \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I"$root/include" \
  "$root/tests/reached_edram_copy_oracle_tests.cpp" \
  "$root/src/hash.cpp" "$root/src/xenos_tiling.cpp" \
  -lcrypto -o "$build/reached_edram_copy_oracle_tests"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  "$build/reached_edram_copy_oracle_tests"
