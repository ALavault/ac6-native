#!/usr/bin/env python3
"""Fail-closed audit for a native AC6 installation prefix."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


FORBIDDEN_TEXT = re.compile(rb"rexglue|xenia|xenonrecomp|xenosrecomp", re.IGNORECASE)
FORBIDDEN_NAME = re.compile(
    r"rexglue|xenia|xenonrecomp|xenosrecomp|d3d12|direct3d[ _-]*12",
    re.IGNORECASE,
)
FORBIDDEN_SUFFIXES = {
    ".cpp", ".cc", ".cxx", ".xex", ".iso", ".pac", ".tbl", ".shader",
}


class ReleaseAuditError(RuntimeError):
    pass


def audit(prefix: Path) -> dict[str, object]:
    if not prefix.is_dir():
        raise ReleaseAuditError(f"installation prefix is missing: {prefix}")
    expected = prefix / "bin/ac6recomp"
    if not expected.is_file():
        raise ReleaseAuditError("installed bin/ac6recomp is missing")
    if (prefix / "bin/bin").exists():
        raise ReleaseAuditError("nested bin/bin installation")
    forbidden: list[str] = []
    scanned = 0
    for path in prefix.rglob("*"):
        if path.is_symlink():
            try:
                path.resolve().relative_to(prefix.resolve())
            except ValueError:
                forbidden.append(str(path.relative_to(prefix)))
            continue
        if not path.is_file():
            continue
        relative = path.relative_to(prefix)
        if path.suffix.lower() in FORBIDDEN_SUFFIXES:
            forbidden.append(str(relative))
            continue
        scanned += 1
        try:
            data = path.read_bytes()
        except OSError as error:
            raise ReleaseAuditError(f"cannot read installed file {relative}: {error}") from error
        if FORBIDDEN_NAME.search(str(relative)) or FORBIDDEN_TEXT.search(data):
            forbidden.append(str(relative))
    if forbidden:
        raise ReleaseAuditError("forbidden native-install content: " + ", ".join(sorted(forbidden)[:8]))
    return {
        "schema": "ac6.retail-native-install-audit.v1",
        "status": "pass",
        "prefix": str(prefix.resolve()),
        "scanned_files": scanned,
        "forbidden": 0,
        "nested_bin_bin": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--prefix", required=True, type=Path)
    parser.add_argument("--receipt", type=Path)
    args = parser.parse_args()
    try:
        result = audit(args.prefix)
        if args.receipt:
            args.receipt.parent.mkdir(parents=True, exist_ok=True)
            args.receipt.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0
    except (OSError, ReleaseAuditError) as error:
        print(f"release-audit: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
