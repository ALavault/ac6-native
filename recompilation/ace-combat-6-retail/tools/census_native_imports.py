#!/usr/bin/env python3
"""Census the direct XenonRecomp import mapping without copying generated code."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path


SCHEMA = "ac6.retail-native-import-census.v1"
IMPORT = re.compile(r"__imp__([A-Za-z0-9_]+)")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def census(mapping: Path, xex_sha256: str) -> dict[str, object]:
    text = mapping.read_text(encoding="utf-8")
    imports = sorted(set(IMPORT.findall(text)))
    if not imports:
        raise ValueError("no __imp__ symbols found")

    def category(name: str) -> str:
        if name.startswith(("NetDll_", "XNet")):
            return "network-offline"
        if name.startswith("Xam"):
            return "xam"
        if name.startswith("Vd"):
            return "vd"
        if name.startswith(("XAudio", "XMA", "Xma")):
            return "xaudio-xma"
        if name.startswith(("Nt", "Rtl", "Mm", "Io")) or "File" in name:
            return "kernel-io"
        return "kernel-crt"

    grouped: dict[str, list[str]] = {}
    for name in imports:
        grouped.setdefault(category(name), []).append(name)
    return {
        "schema": SCHEMA,
        "xex_sha256": xex_sha256.lower(),
        "mapping_sha256": sha256(mapping),
        "import_count": len(imports),
        "imports": imports,
        "categories": {key: value for key, value in sorted(grouped.items())},
        "offline_network": {
            "socket_creation": False,
            "return_status": "offline-error",
        },
        "generated_sources_tracked": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mapping", required=True, type=Path)
    parser.add_argument("--xex-sha256", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    result = census(args.mapping, args.xex_sha256)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    print(json.dumps({"schema": SCHEMA, "import_count": result["import_count"],
                      "output": str(args.output.resolve())}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
