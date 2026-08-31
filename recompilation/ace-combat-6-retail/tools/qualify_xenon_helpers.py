#!/usr/bin/env python3
"""Qualify Xenon ABI helper starts from an extracted big-endian basefile."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


HELPER_SIGNATURES = {
    "savegprlr_14": "f9c1ff68f9e1ff70fa01ff78fa21ff80",
    "restgprlr_14": "e9c1ff68e9e1ff70ea01ff78ea21ff80",
    "savefpr_14": "d9ccff70d9ecff78da0cff80da2cff88",
    "restfpr_14": "c9ccff70c9ecff78ca0cff80ca2cff88",
    "savevmx_14": "3960fee07dcb61ce3960fef07deb61ce",
    "restvmx_14": "3960fee07dcb60ce3960fef07deb60ce",
    "savevmx_64": "3960fc00100b61cb3960fc10102b61cb",
    "restvmx_64": "3960fc00100b60cb3960fc10102b60cb",
}


class HelperQualificationError(RuntimeError):
    pass


def qualify(basefile: Path, base_address: int = 0x82000000) -> dict[str, object]:
    data = basefile.read_bytes()
    helpers: dict[str, str] = {}
    for name, encoded in HELPER_SIGNATURES.items():
        signature = bytes.fromhex(encoded)
        hits: list[int] = []
        offset = 0
        while True:
            offset = data.find(signature, offset)
            if offset < 0:
                break
            if offset % 4 == 0:
                hits.append(offset)
            offset += 1
        if len(hits) != 1:
            raise HelperQualificationError(
                f"{name} signature count {len(hits)} (expected exactly one)"
            )
        helpers[name] = f"0x{base_address + hits[0]:08X}"
    return {
        "schema": "ac6.xenon-helper-qualification.v1",
        "status": "qualified",
        "base_address": f"0x{base_address:08X}",
        "basefile_size": len(data),
        "helpers": helpers,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--basefile", required=True, type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        result = qualify(args.basefile)
        serialized = json.dumps(result, indent=2, sort_keys=True) + "\n"
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(serialized, encoding="utf-8")
        print(serialized, end="")
        return 0
    except (OSError, ValueError, HelperQualificationError) as error:
        print(f"helper-qualification: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
