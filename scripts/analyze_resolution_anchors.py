#!/usr/bin/env python3
"""Validate and export the currently recovered AC6 resolution anchors.

This intentionally consumes re-agent/ghidra-bridge JSON instead of scraping a
terminal transcript.  Every emitted record is guarded by an exact fragment so
the report fails closed if a later Ghidra export changes the evidence.
"""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path


IMAGE_BASE = 0x82000000


def function_text(exports: Path, address: str) -> str:
    record = json.loads((exports / f"{address}.json").read_text())
    if record.get("address", "").lower() != address:
        raise ValueError(f"address mismatch in {address}.json")
    return record.get("decompiled", "")


def require_fragment(text: str, fragment: str, address: str) -> None:
    if fragment not in text:
        raise ValueError(f"expected fragment absent at 0x{address}: {fragment}")


def be_floats(image: bytes, address: int, count: int) -> list[float]:
    offset = address - IMAGE_BASE
    data = image[offset : offset + count * 4]
    if len(data) != count * 4:
        raise ValueError(f"0x{address:08x} is outside the memory image")
    return list(struct.unpack(f">{count}f", data))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--workspace", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    workspace = args.workspace.resolve()
    exports = workspace / "exports"
    image_path = workspace / "analysis-input" / "ACE6_X360.exe"
    output = args.output or workspace / "reports" / "resolution-anchors.json"

    viewport_specs = [
        ("820af548", 1280, 720, "full_resolution_viewport"),
        ("820af5d8", 640, 360, "half_resolution_viewport"),
        ("821382c0", 1280, 720, "renderer_default_viewport"),
    ]
    viewports = []
    for address, width, height, role in viewport_specs:
        text = function_text(exports, address)
        require_fragment(text, f"local_28 = 0x{width:x};", address)
        require_fragment(text, f"local_24 = 0x{height:x};", address)
        require_fragment(text, "Function_821DCFE8", address)
        viewports.append(
            {"address": f"0x{address}", "width": width, "height": height, "role": role}
        )

    allocator = function_text(exports, "820aebc8")
    allocation_fragments = [
        ("Function_821E0E88(0x280,0x168,0x18280186,2,0)", "color"),
        ("Function_821E0E88(0x280,0x168,0x1a220197,2,local_50)", "depth_or_auxiliary"),
    ]
    allocations = []
    for fragment, role in allocation_fragments:
        require_fragment(allocator, fragment, "820aebc8")
        allocations.append(
            {
                "address": "0x820aebc8",
                "width": 640,
                "height": 360,
                "role": role,
                "allocator": "0x821e0e88",
            }
        )

    stacked = function_text(exports, "82146c78")
    for fragment in (
        "Function_821E0E88(0x280,0x5a0,0x18280186,0,local_20)",
        "Function_821E0E88(0x280,0x5a0,0x1a220197,0,local_20)",
    ):
        require_fragment(stacked, fragment, "82146c78")

    effect = function_text(exports, "82249258")
    require_fragment(effect, ",0,0,0x500,0x2d0,3);", "82249258")
    require_fragment(effect, ",0,0,0x500,0x2d0,1);", "82249258")

    shader_setup = function_text(exports, "82105aa8")
    for fragment in (
        "Function_82334178(0xffffffff8205bf50,&local_40);",
        "local_40 = DAT_82069c08;",
        "local_3c = DAT_82069c0c;",
    ):
        require_fragment(shader_setup, fragment, "82105aa8")

    image = image_path.read_bytes()
    depth_range = be_floats(image, 0x820542B8, 2)
    if depth_range != [0.0, 1.0]:
        raise ValueError(f"unexpected viewport depth range: {depth_range}")
    shader_full_size = be_floats(image, 0x82069C08, 2)
    shader_small_size = [
        be_floats(image, 0x8206A10C, 1)[0],
        be_floats(image, 0x8206A108, 1)[0],
    ]
    if shader_full_size != [1280.0, 720.0] or shader_small_size != [208.0, 144.0]:
        raise ValueError("unexpected shader common-parameter dimensions")

    report = {
        "schema": "ace6-resolution-anchors-v1",
        "source": {
            "memory_image": str(image_path.relative_to(workspace)),
            "exports": "exports",
            "image_base": "0x82000000",
        },
        "viewport_depth_range": {"address": "0x820542b8", "min": 0.0, "max": 1.0},
        "viewports": viewports,
        "render_target_allocations": allocations,
        "stacked_surface_candidates": [
            {
                "address": "0x82146c78",
                "width": 640,
                "height": 1440,
                "roles": ["color", "depth_or_auxiliary"],
                "classification": "candidate_atlas_or_surface_stack",
            }
        ],
        "dynamic_effect_viewport": {
            "address": "0x82249258",
            "restores": [
                {"x": 0, "y": 0, "width": 1280, "height": 720, "selector": 3},
                {"x": 0, "y": 0, "width": 1280, "height": 720, "selector": 1},
            ],
        },
        "shader_common_parameter": {
            "address": "0x82105aa8",
            "parameter_name": "ACE_vCommonParam2",
            "setter": "0x82334178",
            "observed_dimension_pairs": [[208.0, 144.0], shader_full_size],
            "classification": "pass_dependent_dimensions",
        },
        "claim_boundary": (
            "Static anchors only; surface formats, ownership, projection, HUD, "
            "scissor and runtime parity remain unverified."
        ),
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
