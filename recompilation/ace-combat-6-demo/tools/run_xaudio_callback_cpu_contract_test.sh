#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
out=$(mktemp)
trap 'rm -f "$out"' EXIT
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
  -I"$root/include" "$root/tests/xaudio_callback_cpu_contract_tests.cpp" \
  -o "$out"
"$out"
