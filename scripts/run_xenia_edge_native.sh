#!/usr/bin/env bash
# Launch the pinned native Xenia Edge AppImage with a persistent profile root.
# Xenia Edge remains an oracle only; this launcher does not alter AC6 runtime
# code, generated C++, Ghidra, Xenia/ReXGlue checkouts, or game assets.
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
edge_release="60ff861"
edge_appimage="$repository_root/.tools/xenia-edge-$edge_release/xenia_edge_linux.AppImage"
edge_sha256="c2cac2a029ce0d44a71c4e919fd71c702654079023b63fd669472ba3cd78b828"
default_profile_root="$repository_root/.tools/xenia-edge-profile"
profile_root="${XENIA_EDGE_PROFILE_ROOT:-$default_profile_root}"

if [[ ! -x "$edge_appimage" ]]; then
  echo "Xenia Edge AppImage is unavailable: $edge_appimage" >&2
  exit 2
fi
printf '%s  %s\n' "$edge_sha256" "$edge_appimage" | sha256sum -c - >/dev/null

# AppImage FUSE is not available in the current headless environment.  The
# runtime extracts itself transiently, while all emulator state stays here.
mkdir -p "$profile_root/content" "$profile_root/cache_host"

args=(
  --appimage-extract-and-run
  "--storage_root=$profile_root"
  "--content_root=$profile_root/content"
  "--cache_root=$profile_root/cache_host"
)

# Optional persistent auto-login.  Leave unset to use Edge's profile UI once;
# the resulting account and config are retained in profile_root thereafter.
if [[ -n "${XENIA_EDGE_PROFILE_XUID:-}" ]]; then
  if [[ ! "${XENIA_EDGE_PROFILE_XUID}" =~ ^[0-9A-Fa-f]{16}$ ]]; then
    echo "XENIA_EDGE_PROFILE_XUID must be exactly 16 hexadecimal digits" >&2
    exit 2
  fi
  args+=("--logged_profile_slot_0_xuid=${XENIA_EDGE_PROFILE_XUID}")
fi

exec "$edge_appimage" "${args[@]}" "$@"
