#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build=${TMPDIR:-/tmp}/ac6-reached-copy-differential-$$
trap 'rm -rf "$build"' EXIT
mkdir -p "$build"
common=(
  -std=c++20 -UNDEBUG -Wall -Wextra -Wpedantic -Wconversion -Wshadow
  -I"$root/include"
  "$root/tests/reached_copy_differential_tests.cpp"
  "$root/src/xenos_tiling.cpp"
  "$root/src/hash.cpp"
)
${CXX:-c++} "${common[@]}" -O2 -o "$build/test" -lcrypto
"$build/test"
${CXX:-c++} "${common[@]}" -O1 -g -fsanitize=address \
  -fno-omit-frame-pointer -o "$build/test-asan" -lcrypto
ASAN_OPTIONS=detect_leaks=1 "$build/test-asan"
${CXX:-c++} "${common[@]}" -O1 -g -fsanitize=undefined \
  -fno-omit-frame-pointer -o "$build/test-ubsan" -lcrypto
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 "$build/test-ubsan"
