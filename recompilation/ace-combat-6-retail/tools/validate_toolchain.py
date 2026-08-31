#!/usr/bin/env python3
"""Check pinned, clean Xbox 360 analysis-tool checkouts."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Any


PRODUCT = Path(__file__).resolve().parents[1]
PORTFOLIO = PRODUCT.parents[3]
DEFAULT_LOCK = PRODUCT / "config/xbox360-toolchain.lock.json"


class ToolchainError(RuntimeError):
    pass


def git(path: Path, *args: str) -> str:
    result = subprocess.run(
        ["git", "-C", str(path), *args],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode:
        raise ToolchainError(result.stderr.strip() or f"git failed for {path}")
    return result.stdout.strip()


def validate_lock(lock_path: Path = DEFAULT_LOCK) -> dict[str, Any]:
    document = json.loads(lock_path.read_text(encoding="utf-8"))
    if document.get("schema") != "ac6.xbox360-toolchain-lock.v1":
        raise ToolchainError("toolchain lock schema mismatch")
    tools = document.get("tools")
    if not isinstance(tools, dict) or not tools:
        raise ToolchainError("toolchain lock has no tools")
    checked: dict[str, dict[str, str]] = {}
    for name, record in tools.items():
        if not isinstance(record, dict):
            raise ToolchainError(f"{name} record is not an object")
        commit = record.get("commit")
        relative = record.get("checkout")
        if not isinstance(commit, str) or len(commit) != 40:
            raise ToolchainError(f"{name} commit is invalid")
        if not isinstance(relative, str) or Path(relative).is_absolute():
            raise ToolchainError(f"{name} checkout must be relative to portfolio root")
        checkout = (PORTFOLIO / relative).resolve()
        try:
            checkout.relative_to(PORTFOLIO.resolve())
        except ValueError as error:
            raise ToolchainError(f"{name} checkout escapes portfolio") from error
        if not (checkout / ".git").exists():
            raise ToolchainError(f"{name} checkout is not a git worktree: {checkout}")
        actual = git(checkout, "rev-parse", "HEAD")
        if actual != commit:
            raise ToolchainError(f"{name} commit mismatch: {actual} != {commit}")
        if git(checkout, "rev-parse", "--abbrev-ref", "HEAD") != "HEAD":
            raise ToolchainError(f"{name} checkout is not detached")
        status = git(checkout, "status", "--porcelain")
        if status:
            raise ToolchainError(f"{name} checkout is dirty")
        expected_submodules = record.get("submodules", {})
        if not isinstance(expected_submodules, dict):
            raise ToolchainError(f"{name} submodule lock is invalid")
        for submodule, expected in expected_submodules.items():
            sub_path = checkout / submodule
            if not sub_path.is_dir() or git(sub_path, "rev-parse", "HEAD") != expected:
                raise ToolchainError(f"{name} submodule mismatch: {submodule}")
            if git(sub_path, "status", "--porcelain"):
                raise ToolchainError(f"{name} submodule is dirty: {submodule}")
        checked[name] = {"commit": actual, "status": "clean-detached", "path": str(checkout)}
    catalog = document.get("architecture_catalog")
    catalog_status = "not-declared"
    if isinstance(catalog, dict):
        catalog_path = catalog.get("path")
        if isinstance(catalog_path, str):
            catalog_status = "present" if (PORTFOLIO / catalog_path).is_file() else "absent"
    return {"schema": document["schema"], "tools": checked, "architecture_catalog": catalog_status}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--lock", type=Path, default=DEFAULT_LOCK)
    args = parser.parse_args()
    try:
        print(json.dumps(validate_lock(args.lock), indent=2, sort_keys=True))
        return 0
    except (OSError, json.JSONDecodeError, ToolchainError) as error:
        print(f"toolchain: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
