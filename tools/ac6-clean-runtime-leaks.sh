#!/usr/bin/env bash
# Remove guest-memory shm files left behind by dead ac6recomp runs.
#
# rexglue reserves guest memory as a /dev/shm/xenia_memory_<tick> mapping of
# 4,831,838,207 bytes and only unlinks it in Memory::~Memory, i.e. on a clean
# shutdown. Every AC6 runtime probe ends by timeout (124) or abort (134), so
# every probe leaks its reservation. /dev/shm is tmpfs, so the resident pages
# are host RAM: cycle 323 found 28 orphans holding 9.8 GiB after the cycle
# 304-318 campaign, on a box whose whole /dev/shm is 61 GiB.
#
# A file is removed only when no live process maps it. That check is what makes
# this safe to run while a probe is in flight, and it is not optional: deleting
# the live run's mapping would corrupt the measurement it is producing.
#
# Usage: ac6-clean-runtime-leaks.sh [--dry-run]
set -uo pipefail

DRY_RUN=0
[ "${1:-}" = "--dry-run" ] && DRY_RUN=1

shopt -s nullglob
candidates=(/dev/shm/xenia_memory_* /dev/shm/xenia_code_cache_*)
shopt -u nullglob

if [ ${#candidates[@]} -eq 0 ]; then
  echo "ac6-clean-runtime-leaks: no guest memory files present"
  exit 0
fi

removed=0
kept=0
freed_kib=0

for path in "${candidates[@]}"; do
  name=$(basename "$path")
  # grep over every live process's maps; a hit means the file is still in use.
  if grep -l -- "$name" /proc/[0-9]*/maps >/dev/null 2>&1; then
    echo "  keep   $name (mapped by a live process)"
    kept=$((kept + 1))
    continue
  fi
  size_kib=$(du -k "$path" 2>/dev/null | cut -f1)
  size_kib=${size_kib:-0}
  if [ "$DRY_RUN" = "1" ]; then
    echo "  would remove $name (${size_kib} KiB resident)"
  else
    rm -f -- "$path" && echo "  remove $name (${size_kib} KiB resident)"
  fi
  removed=$((removed + 1))
  freed_kib=$((freed_kib + size_kib))
done

printf 'ac6-clean-runtime-leaks: %d orphaned, %d in use, %d MiB %s\n' \
  "$removed" "$kept" "$((freed_kib / 1024))" \
  "$([ "$DRY_RUN" = "1" ] && echo 'reclaimable' || echo 'reclaimed')"
