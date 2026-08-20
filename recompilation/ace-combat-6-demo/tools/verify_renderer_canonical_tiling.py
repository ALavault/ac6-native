#!/usr/bin/env python3
"""Fail-closed source audit for the PAL renderer canonical tiled writeback."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", nargs="?", type=Path,
                        default=Path(__file__).resolve().parents[1])
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve()

    paths = {
        "tiling_header": root / "include/ac6demo/xenos_tiling.hpp",
        "canonical_header": root / "include/ac6demo/renderer_canonical_tiling.hpp",
        "tiling_source": root / "src/xenos_tiling.cpp",
        "join": root / "src/xenos_guest_present_join.hpp",
        "tiling_tests": root / "tests/ac6-demo-xenos-tiling-tests.cpp",
    }
    for path in paths.values():
        require(path.is_file(), f"missing {path.relative_to(root)}")

    text = {name: path.read_text() for name, path in paths.items()}
    require("kReachedResolveTiledPaddingBytes" in text["tiling_header"],
            "padding extent is not explicit")
    require("void tile_reached_rgba8" in text["tiling_header"],
            "inverse tiling declaration is missing")
    require("void tile_reached_rgba8" in text["tiling_source"],
            "inverse tiling implementation is missing")
    require("require_reached_buffer_sizes" in text["tiling_source"],
            "tile/untile size guard is missing")
    require("canonicalize_reached_tiled_writeback" in text["canonical_header"],
            "canonical writeback helper is missing")
    require("untile_reached_rgba8(gpu_tiled, linear);" in text["canonical_header"],
            "GPU tiled payload is not canonicalized to linear pixels")
    require("tile_reached_rgba8(linear, guest_tiled);" in text["canonical_header"],
            "canonical helper does not retile into the guest buffer")
    require("Vulkan tiled resolve differs from its qualified linear pixels" in
            text["canonical_header"], "pre-writeback digest guard is missing")
    require("canonicalize_reached_tiled_writeback(" in text["join"],
            "guest-present join bypasses canonical helper")
    require("std::copy_n(resolve.tiled_bytes" not in text["join"],
            "legacy direct GPU tiled-byte copy remains")
    require("make_nonzero_pattern" in text["tiling_tests"],
            "non-black tile/untile test is missing")
    require("preserved_padding" in text["tiling_tests"],
            "low-level padding preservation test is missing")
    require("guest_before_rejection" in text["tiling_tests"],
            "transactional failure test is missing")
    require("assert(guest_tiled == guest_before_rejection)" in
            text["tiling_tests"],
            "failed canonicalization may mutate the guest buffer")
    require("kReachedResolveTiledPaddingBytes == 0x14000U" in
            text["tiling_tests"], "reached padding extent is not pinned")

    result = {
        "schema": "ac6-demo-renderer-canonical-tiling-verification/v1",
        "status": "PASS",
        "files": {
            str(path.relative_to(root)): sha256(path)
            for path in paths.values()
        },
        "linear_bytes": 0x384000,
        "tiled_extent_bytes": 0x398000,
        "preserved_padding_bytes": 0x14000,
        "transactional_before_guest_store": True,
        "runtime_pal_run_claimed": False,
    }
    print(json.dumps(result, indent=2, sort_keys=True) if args.json
          else "renderer_canonical_tiling=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
