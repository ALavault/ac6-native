#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out=${TMPDIR:-/tmp}/ac6-xenon-affinity-contract-test
c++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
  -I"$root/include" "$root/tests/xenon_affinity_contract_tests.cpp" -o "$out"
"$out"
