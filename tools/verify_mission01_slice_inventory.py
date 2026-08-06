#!/usr/bin/env python3
"""Fail-closed verification of the external Mission 01 slice inventory."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import struct
from pathlib import Path


def load_contract():
    source = Path(__file__).with_name("extract_ndxr_native_slices.py")
    spec = importlib.util.spec_from_file_location("ac6_slice_contract", source)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load NDXR contract helper")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.contract


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("inventory", type=Path)
    args = parser.parse_args()
    contract = load_contract()
    rows = 0
    for line_number, line in enumerate(args.inventory.read_text().splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) != 4:
            raise SystemExit(f"line {line_number}: expected role/asset/path/sha256")
        role, asset, path_text, expected = fields
        if role not in {"terrain", "sky"} or not asset.isdigit() or len(expected) != 64:
            raise SystemExit(f"line {line_number}: malformed metadata")
        path = Path(path_text)
        if not path.is_file():
            raise SystemExit(f"line {line_number}: missing slice {path}")
        payload = path.read_bytes()
        digest = hashlib.sha256(payload).hexdigest()
        if digest != expected:
            raise SystemExit(f"line {line_number}: SHA-256 mismatch for {path}")
        try:
            vertices, indices, primitives, stride = contract(payload)
        except (IndexError, ValueError, struct.error) as error:
            raise SystemExit(f"line {line_number}: invalid NDXR {path}: {error}") from error
        if vertices == 0 or indices == 0 or primitives == 0 or stride == 0:
            raise SystemExit(f"line {line_number}: empty NDXR contract {path}")
        rows += 1
    if rows == 0:
        raise SystemExit("inventory contains no slices")
    print(f"slice_inventory=pass rows={rows}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
