"""Allowlisted artifact references; payloads never cross the MCP boundary."""

from __future__ import annotations

import hashlib
from pathlib import Path
from typing import Any

from ..protocol import sha256_json


class ArtifactError(ValueError):
    pass


MAX_SLICE = 64 * 1024


def _inside(path: Path, roots: tuple[Path, ...]) -> Path:
    resolved = path.resolve()
    for root in roots:
        try:
            resolved.relative_to(root.resolve())
            return resolved
        except ValueError:
            continue
    raise ArtifactError("artifact path is outside the allowlist")


def reference(path: Path, *, roots: tuple[Path, ...]) -> dict[str, Any]:
    resolved = _inside(path, roots)
    if not resolved.is_file():
        raise ArtifactError("artifact is not a regular file")
    digest = hashlib.sha256()
    with resolved.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return {
        "kind": "artifact-reference",
        "path": resolved.name,
        "sha256": digest.hexdigest(),
        "size_bytes": resolved.stat().st_size,
        "committable": False,
        "payload_exposed": False,
    }


def bounded_slice(path: Path, *, roots: tuple[Path, ...], offset: int = 0,
                  length: int = 0) -> dict[str, Any]:
    resolved = _inside(path, roots)
    if not resolved.is_file() or isinstance(offset, bool) or isinstance(length, bool):
        raise ArtifactError("invalid artifact slice")
    if offset < 0 or length < 0 or length > MAX_SLICE:
        raise ArtifactError("slice exceeds the bounded artifact policy")
    with resolved.open("rb") as stream:
        stream.seek(offset)
        data = stream.read(length)
    return {
        "kind": "bounded-artifact-slice",
        "path": resolved.name,
        "offset": offset,
        "length": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "hex": data.hex(),
        "payload_exposed": True,
        "slice_sha256": sha256_json({"offset": offset, "length": len(data), "sha256": hashlib.sha256(data).hexdigest()}),
    }


class ArtifactRegistry:
    def __init__(self, *roots: Path) -> None:
        self.roots = tuple(root.resolve() for root in roots)
        self._refs: dict[str, dict[str, Any]] = {}

    def register(self, artifact_id: str, path: Path) -> dict[str, Any]:
        if not artifact_id or "/" in artifact_id or ".." in artifact_id:
            raise ArtifactError("artifact id is unsafe")
        value = reference(path, roots=self.roots)
        value["artifact_id"] = artifact_id
        self._refs[artifact_id] = dict(value)
        return dict(value)

    def inspect(self, artifact_id: str) -> dict[str, Any]:
        if artifact_id not in self._refs:
            raise ArtifactError("unknown artifact id")
        return dict(self._refs[artifact_id])


__all__ = ["ArtifactError", "ArtifactRegistry", "bounded_slice", "reference"]
