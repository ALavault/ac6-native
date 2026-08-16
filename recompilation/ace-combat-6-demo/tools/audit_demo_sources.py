#!/usr/bin/env python3
"""Reject proprietary inputs and generated products from the demo source set."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
WORKSPACE = PROJECT.parents[1]
FORBIDDEN_SUFFIXES = {
    ".xex", ".pac", ".tbl", ".bin", ".rar", ".gpr", ".rep", ".o", ".a", ".so",
}
FORBIDDEN_PARTS = {"build", "codegen", "generated", "out", "__pycache__"}


def git_lines(*arguments: str) -> list[str]:
    completed = subprocess.run(
        ["git", *arguments], cwd=WORKSPACE, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr.strip() or "git command failed")
    return [line for line in completed.stdout.splitlines() if line]


def main() -> int:
    errors: list[str] = []
    tracked = git_lines("ls-files")
    for relative in tracked:
        path = Path(relative)
        if relative.startswith("demo-game-file/") or path.suffix.lower() in FORBIDDEN_SUFFIXES:
            errors.append(f"tracked proprietary/binary path: {relative}")
    candidates = git_lines(
        "ls-files", "--cached", "--others", "--exclude-standard", "--",
        str(PROJECT.relative_to(WORKSPACE)),
    )
    project_prefix = PROJECT.relative_to(WORKSPACE)
    for relative in candidates:
        local = Path(relative).relative_to(project_prefix)
        if (local.suffix.lower() in FORBIDDEN_SUFFIXES or
                any(part.lower() in FORBIDDEN_PARTS for part in local.parts)):
            errors.append(f"unignored generated/binary path: {relative}")
    ignored_boundaries = [
        "demo-game-file/ac6.rar",
        "demo-game-file/extracted/stfs-root/Default.xex",
        "ghidra-projects/ace-combat-6-demo.gpr",
        "recompilation/ace-combat-6-demo/build/generated/guest.cpp",
    ]
    for relative in ignored_boundaries:
        completed = subprocess.run(
            ["git", "check-ignore", "-q", relative], cwd=WORKSPACE, check=False,
        )
        if completed.returncode != 0:
            errors.append(f"local-only boundary is not ignored: {relative}")
    if errors:
        print("demo_source_audit=fail")
        for error in errors:
            print(f"error: {error}")
        return 1
    print(
        "demo_source_audit=pass "
        f"candidate_sources={len(candidates)} tracked_workspace_files={len(tracked)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
