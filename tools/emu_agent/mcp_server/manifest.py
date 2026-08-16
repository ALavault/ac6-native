"""Metadata-only work manifest helpers.

The helper accepts already-qualified identities from a caller.  It does not
run git, inspect the filesystem, or copy a savestate/movie/retail payload.
Optional values are kept as hashes or opaque run identities only.
"""

from __future__ import annotations

import re
from typing import Any

from ..exploration import canonical_digest


_HEX64 = re.compile(r"^[0-9a-fA-F]{64}$")
_HEX40 = re.compile(r"^[0-9a-fA-F]{40}$")


def _identity(value: Any, name: str, *, commit: bool = False) -> str:
    if not isinstance(value, str) or not value or len(value) > 128 or "\x00" in value:
        raise ValueError(f"{name} must be a short opaque identity")
    if commit and not (_HEX40.fullmatch(value) or _HEX64.fullmatch(value)):
        raise ValueError(f"{name} must be a git commit hash")
    return value


def _digest(value: Any, name: str, *, required: bool = False) -> str | None:
    if value is None:
        if required:
            raise ValueError(f"{name} is required")
        return None
    if not isinstance(value, str) or not _HEX64.fullmatch(value):
        raise ValueError(f"{name} must be a SHA-256 digest")
    return value.lower()


def build_work_manifest(
    *,
    git_commit: str,
    git_status_digest: str,
    pal_elf_sha256: str,
    image_base: str,
    pcsx2_appimage_sha256: str | None = None,
    config_sha256: str | None = None,
    savestate_identity: str | None = None,
    movie_identity: str | None = None,
    run_identity: str | None = None,
) -> dict[str, Any]:
    """Build a payload-free manifest from caller-supplied metadata."""

    image_base = _identity(image_base, "image_base")
    result: dict[str, Any] = {
        "schema": "ac5-emu-agent/work-manifest-v1",
        "evidence_class": "metadata-only",
        "git": {
            "commit": _identity(git_commit, "git_commit", commit=True),
            "status_digest": _digest(git_status_digest, "git_status_digest", required=True),
        },
        "pal_target": {
            "elf_sha256": _digest(pal_elf_sha256, "pal_elf_sha256", required=True),
            "image_base": image_base,
        },
        "pcsx2_appimage_sha256": _digest(pcsx2_appimage_sha256, "pcsx2_appimage_sha256"),
        "config_sha256": _digest(config_sha256, "config_sha256"),
        "identities": {
            "savestate": None if savestate_identity is None else _identity(savestate_identity, "savestate_identity"),
            "movie": None if movie_identity is None else _identity(movie_identity, "movie_identity"),
            "run": None if run_identity is None else _identity(run_identity, "run_identity"),
        },
        "content_policy": "hashes and opaque identities only; no retail data included",
    }
    result["manifest_digest"] = canonical_digest(result)
    return result


__all__ = ["build_work_manifest"]


def build_ac6_work_manifest(identity: dict[str, Any]) -> dict[str, Any]:
    """Wrap a locally collected AC6 identity without exposing payloads."""
    if not isinstance(identity, dict) or identity.get("schema") != "ac6-emu-agent-identity/v1":
        raise ValueError("identity must be an ac6-emu-agent identity")
    result = {
        "schema": "ac6-emu-agent/work-manifest-v1",
        "evidence_class": "metadata-only",
        "identity": identity,
        "content_policy": "hashes and references only; no retail payload",
    }
    result["manifest_digest"] = canonical_digest(result)
    return result


__all__.append("build_ac6_work_manifest")
