"""Isolated Xenia profile copies with a no-overwrite guard."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import shutil
from typing import Any


class ProfileIsolationError(ValueError):
    pass


def _is_relative(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def _within(path: Path, roots: tuple[Path, ...], label: str) -> Path:
    path = path.resolve()
    if not any(_is_relative(path, root.resolve()) for root in roots):
        raise ProfileIsolationError(f"{label} is outside the allowlist")
    return path


def profile_manifest(path: Path) -> dict[str, Any]:
    path = path.resolve()
    if not path.is_dir():
        raise ProfileIsolationError("profile root is not a directory")
    entries = []
    for item in sorted(path.rglob("*")):
        if item.is_file():
            entries.append({"path": item.relative_to(path).as_posix(), "size": item.stat().st_size})
    digest = hashlib.sha256(json.dumps(entries, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    return {"path": path.name, "sha256": digest, "file_count": len(entries), "source_preserved": True}


def copy_isolated_profile(source: Path, destination: Path, *, roots: tuple[Path, ...]) -> dict[str, Any]:
    source = _within(source, roots, "profile source")
    destination = _within(destination, roots, "profile destination")
    if not source.is_dir():
        raise ProfileIsolationError("profile source is not a directory")
    if destination == source or _is_relative(destination, source):
        raise ProfileIsolationError("profile destination aliases or nests the source")
    if destination.exists():
        raise ProfileIsolationError("profile destination already exists; refusing overwrite")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source, destination)
    result = profile_manifest(destination)
    result.update({"source": source.name, "destination": destination.name, "copied": True})
    return result


__all__ = ["ProfileIsolationError", "copy_isolated_profile", "profile_manifest"]
