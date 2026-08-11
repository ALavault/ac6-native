#!/usr/bin/env python3
"""Audit native source, ELF files and staging for forbidden oracle coupling."""
from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path

SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp", ".cmake", ".toml"}
FORBIDDEN_TEXT = re.compile(
    r"(?i)(?:\brexglue\b|\brex_[A-Za-z0-9_]*\b|\bac6_recomp\b|"
    r"\bxenonrecomp\b|(?:^|[\\/])generated(?:[\\/]|$))"
)
FORBIDDEN_PATH_PARTS = {".tools", "generated", "rexglue", "ac6_recomp"}
FORBIDDEN_RETAIL = re.compile(r"(?i)(?:^default\.xex$|^DATA\d*\.PAC$)")


class BoundaryError(ValueError):
    pass


def path_forbidden(path: Path) -> bool:
    return any(part.lower() in FORBIDDEN_PATH_PARTS for part in path.parts)


def audit_source(root: Path) -> int:
    if not root.is_dir():
        raise BoundaryError(f"source root absent: {root}")
    checked = 0
    for path in sorted(root.rglob("*")):
        if not path.is_file() or any(part.startswith("build") for part in path.relative_to(root).parts):
            continue
        relative = path.relative_to(root)
        if path_forbidden(relative):
            raise BoundaryError(f"forbidden source path: {relative}")
        if path.suffix.lower() not in SOURCE_SUFFIXES and path.name != "CMakeLists.txt":
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        match = FORBIDDEN_TEXT.search(text)
        if match:
            line = text.count("\n", 0, match.start()) + 1
            raise BoundaryError(f"forbidden source marker: {relative}:{line}")
        checked += 1
    return checked


def run_tool(arguments: list[str]) -> str:
    result = subprocess.run(arguments, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode != 0:
        raise BoundaryError(f"{' '.join(arguments)} failed: {result.stdout.strip()}")
    return result.stdout


def audit_binary(path: Path) -> None:
    if not path.is_file() or path.read_bytes()[:4] != b"\x7fELF":
        raise BoundaryError(f"not an ELF binary: {path}")
    readelf = run_tool(["readelf", "-d", "-Ws", str(path)])
    ldd = run_tool(["ldd", str(path)])
    match = FORBIDDEN_TEXT.search(readelf + "\n" + ldd)
    if match:
        raise BoundaryError(f"forbidden ELF dependency or symbol: {match.group(0)}")
    if "not found" in ldd:
        raise BoundaryError(f"unresolved ELF dependency: {path}")
    for line in ldd.splitlines():
        for token in line.split():
            if token.startswith("/") and path_forbidden(Path(token)):
                raise BoundaryError(f"forbidden ELF dependency path: {token}")
    payload = path.read_bytes()
    ascii_payload = payload.decode("latin1", errors="ignore")
    match = FORBIDDEN_TEXT.search(ascii_payload)
    if match:
        raise BoundaryError(f"forbidden ELF marker: {match.group(0)}")


def audit_staging(root: Path) -> int:
    if not root.is_dir():
        raise BoundaryError(f"staging root absent: {root}")
    files = 0
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root)
        if path_forbidden(relative):
            raise BoundaryError(f"forbidden staging path: {relative}")
        if path.is_file():
            if FORBIDDEN_RETAIL.fullmatch(path.name):
                raise BoundaryError(f"retail artifact in staging: {relative}")
            files += 1
    return files


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path)
    parser.add_argument("--binary", type=Path, action="append", default=[])
    parser.add_argument("--staging", type=Path)
    args = parser.parse_args()
    if args.source_root is None and not args.binary and args.staging is None:
        parser.error("at least one audit target is required")
    try:
        source_files = audit_source(args.source_root) if args.source_root else 0
        for binary in args.binary:
            audit_binary(binary)
        staging_files = audit_staging(args.staging) if args.staging else 0
    except (OSError, BoundaryError) as error:
        print(f"product_boundary=fail reason={error}")
        return 1
    print(f"product_boundary=pass source_files={source_files} "
          f"binaries={len(args.binary)} staging_files={staging_files}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
