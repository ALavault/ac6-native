#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
  echo "usage: $0 XENIA_CHECKOUT_COPY" >&2
  exit 2
fi

checkout=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
base_commit=$(tr -d '[:space:]' < "$script_dir/BASE_COMMIT")

if [ ! -d "$checkout/.git" ]; then
  echo "not a git checkout: $checkout" >&2
  exit 2
fi

actual=$(git -C "$checkout" rev-parse HEAD)
if [ "$actual" != "$base_commit" ]; then
  echo "wrong Xenia base: expected $base_commit, got $actual" >&2
  exit 1
fi

if [ -n "$(git -C "$checkout" status --porcelain)" ]; then
  echo "refusing dirty Xenia checkout" >&2
  exit 1
fi

if git -C "$checkout" apply --check "$script_dir/0001-agent-bridge-present-bounded-input-hook.patch"; then
  git -C "$checkout" apply "$script_dir/0001-agent-bridge-present-bounded-input-hook.patch"
else
  echo "patch does not apply cleanly (already applied or source drift)" >&2
  exit 1
fi

git -C "$checkout" diff --check
echo "applied Xenia emu-agent bridge patch at $base_commit"
