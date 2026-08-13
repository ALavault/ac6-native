#!/usr/bin/env python3
"""Verify the external, bounded Mission 01 PAL native-asset cache.

The repository stores only the JSON seal.  Geometry and texture payloads are
provided through explicit cache directories and are checked by content hash;
retail containers never become part of the seal.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import re
from pathlib import Path
from typing import Any, Callable


SCHEMA = "ac6.mission01-pal-native-extraction.v1"
FORBIDDEN_PATH = re.compile(r"tracker|tracking|telemetry", re.IGNORECASE)


def sha256_path(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_helper(name: str) -> Any:
    source = Path(__file__).with_name(name)
    spec = importlib.util.spec_from_file_location("ac6_extraction_helper", source)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {source}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"asset_extraction=fail error={message}")


def validate_rows(
    rows: list[dict[str, Any]], root: Path, contract: Callable[[bytes], Any] | None,
    texture_inspect: Callable[[bytes], Any] | None,
) -> int:
    checked = 0
    for row in rows:
        relative = row.get("path")
        expected = row.get("sha256")
        require(isinstance(relative, str) and relative, "slice_path")
        require(isinstance(expected, str) and len(expected) == 64, "slice_hash")
        require(not FORBIDDEN_PATH.search(relative), "forbidden_path")
        path = (root / relative).resolve()
        require(path.is_file() and root.resolve() in path.parents, "slice_missing")
        payload = path.read_bytes()
        require(len(payload) == row.get("bytes"), "slice_size")
        require(sha256_path(path) == expected, "slice_sha256")
        if contract is not None:
            vertices, indices, primitives, stride = contract(payload)
            require(vertices > 0 and indices > 0 and primitives > 0 and stride > 0,
                    "ndxr_contract")
        if texture_inspect is not None:
            width, height, _format = texture_inspect(payload)
            require(width == row.get("width") and height == row.get("height"),
                    "ntxr_dimensions")
        checked += 1
    return checked


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--asset-root", type=Path, required=True)
    parser.add_argument("--aircraft-dir", type=Path, required=True)
    parser.add_argument("--terrain-dir", type=Path, required=True)
    parser.add_argument("--texture-dir", type=Path, required=True)
    args = parser.parse_args()

    document = json.loads(args.manifest.read_text(encoding="utf-8"))
    require(document.get("schema") == SCHEMA, "schema")
    source = document.get("source", {})
    xex = args.asset_root / "default.xex"
    data_tbl = args.asset_root / "DATA.TBL"
    require(xex.is_file() and data_tbl.is_file(), "source_files")
    require(sha256_path(xex) == source.get("xex_sha256"), "xex_sha256")
    require(sha256_path(data_tbl) == source.get("data_tbl_sha256"), "data_tbl_sha256")

    slices = document.get("native_slices", {})
    require(isinstance(slices, dict), "slice_groups")
    ndxr = load_helper("extract_ndxr_native_slices.py")
    ntxr = load_helper("extract_ntxr_native_slices.py")
    checked = 0
    checked += validate_rows(slices.get("aircraft_ndxr", []), args.aircraft_dir,
                             ndxr.contract, None)
    checked += validate_rows(slices.get("terrain_ndxr", []), args.terrain_dir,
                             ndxr.contract, None)
    checked += validate_rows(slices.get("terrain_ntxr", []), args.texture_dir,
                             None, ntxr.inspect)
    require(checked == 18, "slice_count")
    policy = document.get("policy", {})
    require(policy.get("retail_containers_copied") is False and
            policy.get("payload_bytes_versioned") is False and
            policy.get("non_product_tracking_files_copied") is False,
            "payload_policy")
    print(f"asset_extraction=pass slices={checked} xex={source['xex_sha256'][:12]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
