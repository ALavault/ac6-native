#!/usr/bin/env python3
"""Cross-match the qualified XEX imports with XenonRecomp thunk addresses."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

EXPECTED_XEX = "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8"
TARGET_ID = "ac6-demo-xbox360-pal"


class ImportMapError(RuntimeError):
    pass


def canonical(value: object) -> bytes:
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--codegen-manifest", type=Path, required=True)
    parser.add_argument("--mapping", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    manifest = json.loads(args.codegen_manifest.read_text())
    required = {"schema": "ac6-demo-codegen-manifest/v2",
                "target_id": TARGET_ID, "xex_sha256": EXPECTED_XEX,
                "confirmed_functions": 12857, "import_stub_records": 238,
                "boundary_diagnostics": 0, "unsupported_instructions": 0}
    for key, expected in required.items():
        if manifest.get(key) != expected:
            raise ImportMapError(f"unqualified codegen manifest {key}")
    imports = manifest.get("imports", [])
    by_symbol: dict[str, dict[str, object]] = {}
    for record in imports:
        name = str(record["name"])
        if name in by_symbol:
            raise ImportMapError(f"duplicate import name: {name}")
        by_symbol[name] = record
    pattern = re.compile(r"\{ 0x(8237[0-9A-Fa-f]{4}), __imp__([A-Za-z_][A-Za-z0-9_]*) \},")
    records = []
    for match in pattern.finditer(args.mapping.read_text()):
        address = int(match.group(1), 16)
        if not 0x82375984 <= address < 0x823767C4:
            continue
        symbol = match.group(2)
        candidates = [symbol]
        if symbol not in by_symbol:
            candidates.append(symbol + "_0")
        matches = [by_symbol[name] for name in candidates if name in by_symbol]
        if len(matches) != 1:
            raise ImportMapError(f"no unique XEX import for {symbol}")
        source = matches[0]
        records.append({"address": f"0x{address:08X}",
                        "module": source["module"], "ordinal": source["ordinal"],
                        "name": source["name"]})
    records.sort(key=lambda row: int(row["address"], 0))
    expected_addresses = list(range(0x82375984, 0x823767C4, 16))
    if len(records) != 228 or [int(row["address"], 0) for row in records] != expected_addresses:
        raise ImportMapError(f"callable import table mismatch: {len(records)}")
    document = {"schema": "ac6-demo-import-thunks/v1", "target_id": TARGET_ID,
                "xex_sha256": EXPECTED_XEX,
                "codegen_manifest_sha256": hashlib.sha256(args.codegen_manifest.read_bytes()).hexdigest(),
                "table": {"address": "0x82375984", "record_size": 16,
                          "record_count": 228}, "imports": records}
    payload = canonical(document)
    temporary = args.output.with_name(args.output.name + ".new")
    if args.output.exists() or temporary.exists():
        raise ImportMapError("refusing import-map output collision")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary.write_bytes(payload)
    temporary.replace(args.output)
    print("AC6_DEMO_IMPORT_THUNKS_PASS records=228 sha256=" + hashlib.sha256(payload).hexdigest())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
