#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${TMPDIR:-/tmp}/ac6-xenos-cpu-interrupt-contract"
mkdir -p "$build_dir"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
  -I"$root/include" \
  "$root/tests/xenos_cpu_interrupt_contract_tests.cpp" \
  -o "$build_dir/xenos_cpu_interrupt_contract_tests"
"$build_dir/xenos_cpu_interrupt_contract_tests"
echo "xenos CPU interrupt contract: PASS"
