#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise SystemExit(f"missing {label}: {needle}")


def forbid(text: str, needle: str, label: str) -> None:
    if needle in text:
        raise SystemExit(f"stale {label}: {needle}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    header = (args.root / "include/ac6demo/vulkan_shared_memory.hpp").read_text()
    source = (args.root / "src/vulkan_shared_memory.cpp").read_text()

    require(header, "bool populate(", "shared refresh result")
    require(header, "bool populate_constants(", "constant refresh result")
    require(header, "refresh_epoch()", "monotonic refresh epoch")
    require(source, "shared_version_.needs_upload(digest)", "shared digest gate")
    require(source, "shared_version_.mark_uploaded(digest)", "shared commit")
    require(source, "shader_loads < 5U || draws < 26U", "cumulative profile gate")
    require(source, "staged.version = slot->version", "constant generation carry")
    require(source, "ConstantSet previous = std::move(*slot)", "transactional swap")
    require(source, "destroy_constant_set(device, previous)", "old descriptor cleanup")
    require(source, "staged.version.mark_uploaded(digest)", "constant commit")
    forbid(source, "if (populated()) {\n    return;\n  }", "one-shot shared cache")
    forbid(source, "return set.vertex_shader == draw.vertex_shader_sha256;\n      })) {\n    return;", "one-shot constant cache")
    print("renderer batch refresh integration: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
