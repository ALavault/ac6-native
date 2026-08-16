#!/usr/bin/env python3
"""Emit reproducible build-evidence hashes for the runtime frontier report."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--ghidra-manifest", type=Path, required=True)
    parser.add_argument("--boundary-config", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    values = {
        "kCodegenManifestSha256": sha256(args.manifest),
        "kGhidraManifestSha256": sha256(args.ghidra_manifest),
        "kBoundaryConfigSha256": sha256(args.boundary_config),
    }
    lines = [
        "#pragma once",
        "",
        "#include <string_view>",
        "",
        "namespace ac6demo {",
    ]
    lines.extend(
        f'inline constexpr std::string_view {name} = "{value}";'
        for name, value in values.items()
    )
    lines.extend(["}", ""])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
