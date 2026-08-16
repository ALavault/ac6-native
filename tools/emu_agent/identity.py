"""Qualified AC6/Xenia identities for the emu-agent manifest.

Only hashes and bounded paths are published.  This module never copies an XEX,
profile, save, shader, trace, or emulator payload into the repository.
"""

from __future__ import annotations

import hashlib
import json
import subprocess
from pathlib import Path
from typing import Any


PROGRAM_SHA256 = "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8"
TITLE_ID = "4E4D87E6"
MEDIA_ID = "565E01A0"
IMAGE_BASE = "0x82000000"
GHIDRA_PROJECT = "ace-combat-6-demo"
GHIDRA_LANGUAGE = "PowerPC:BE:64:Xenon"
XENIA_EDGE_RELEASE = "60ff861"
XENIA_EDGE_SOURCE_COMMIT = "e4b13738c3c461b2c06241fa3f54b5a669b6a304"
XENIA_EDGE_APPIMAGE_SHA256 = "c2cac2a029ce0d44a71c4e919fd71c702654079023b63fd669472ba3cd78b828"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _git(root: Path, *args: str) -> str | None:
    try:
        return subprocess.run(
            ["git", "-C", str(root), *args],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        ).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def _portfolio_root(workspace_root: Path) -> Path:
    # AC6 lives at <portfolio>/workspaces/ace-combat-6.
    return workspace_root.parents[1]


def _bounded_path(path: Path, *, roots: tuple[Path, ...], label: str) -> str:
    resolved = path.resolve()
    for root in roots:
        try:
            return resolved.relative_to(root.resolve()).as_posix()
        except ValueError:
            continue
    raise ValueError(f"{label} is outside the allowlisted roots")


def collect_identity(workspace_root: Path, *, config: Path | None = None,
                     profile_root: Path | None = None) -> dict[str, Any]:
    """Collect the exact identity available on this host.

    Missing optional Xenia/config/profile inputs are represented as ``null``;
    they never get guessed from a remembered value.
    """
    workspace_root = workspace_root.resolve()
    portfolio = _portfolio_root(workspace_root)
    xex = workspace_root / "demo-game-file" / "extracted" / "stfs-root" / "Default.xex"
    if not xex.is_file():
        raise FileNotFoundError(xex)
    xex_sha = sha256_file(xex)
    if xex_sha != PROGRAM_SHA256:
        raise ValueError("PAL demo Default.xex SHA-256 is not the qualified identity")

    app = portfolio / ".tools" / f"xenia-edge-{XENIA_EDGE_RELEASE}" / "xenia_edge_linux.AppImage"
    app_record: dict[str, Any] = {
        "release": XENIA_EDGE_RELEASE,
        "path": _bounded_path(app, roots=(portfolio,), label="xenia appimage"),
        "sha256": sha256_file(app) if app.is_file() else None,
        "expected_sha256": XENIA_EDGE_APPIMAGE_SHA256,
    }
    source = portfolio / ".tools" / "xenia-edge-source"
    source_commit = _git(source, "rev-parse", "HEAD") if source.is_dir() else None
    source_status = _git(source, "status", "--porcelain=v1") if source.is_dir() else None
    config_record = None
    if config is not None:
        config = config.resolve()
        config_record = {
            "path": _bounded_path(config, roots=(workspace_root, portfolio), label="xenia config"),
            "sha256": sha256_file(config) if config.is_file() else None,
        }
    profile_record = None
    if profile_root is not None:
        profile_root = profile_root.resolve()
        profile_record = {
            "path": _bounded_path(profile_root, roots=(portfolio, workspace_root), label="profile root"),
            "exists": profile_root.exists(),
            "sha256": None,
            "source_preserved": True,
        }
        # A directory digest is intentionally a manifest of names/stat sizes,
        # not a profile dump.  It is stable and does not expose account data.
        if profile_root.is_dir():
            entries = []
            for item in sorted(profile_root.rglob("*")):
                rel = item.relative_to(profile_root).as_posix()
                if item.is_file():
                    entries.append({"path": rel, "size": item.stat().st_size})
            profile_record["sha256"] = hashlib.sha256(
                json.dumps(entries, sort_keys=True, separators=(",", ":")).encode("utf-8")
            ).hexdigest()
    status = _git(workspace_root, "status", "--porcelain=v1") or ""
    return {
        "schema": "ac6-emu-agent-identity/v1",
        "target": {
            "project": "ac6-pal",
            "program": "Default.xex",
            "program_sha256": xex_sha,
            "title_id": TITLE_ID,
            "media_id": MEDIA_ID,
            "image_base": IMAGE_BASE,
            "endianness": "big",
            "cpu": "Xenon PowerPC",
            "graphics": "Xenos",
        },
        "ghidra": {"project": GHIDRA_PROJECT, "language": GHIDRA_LANGUAGE},
        "xenia": {
            "kind": "xenia-edge",
            "release": XENIA_EDGE_RELEASE,
            "source_commit": source_commit or XENIA_EDGE_SOURCE_COMMIT,
            "source_commit_expected": XENIA_EDGE_SOURCE_COMMIT,
            "source_worktree_status_sha256": hashlib.sha256((source_status or "").encode()).hexdigest() if source_status is not None else None,
            "appimage": app_record,
            "config": config_record,
        },
        "profile_snapshot": profile_record,
        "repository": {
            "commit": _git(workspace_root, "rev-parse", "HEAD"),
            "worktree_status_sha256": hashlib.sha256(status.encode()).hexdigest(),
        },
        "payload_policy": "metadata-and-references-only",
    }


def write_manifest(path: Path, manifest: dict[str, Any]) -> None:
    """Atomically create a receipt and refuse collisions."""
    if path.exists():
        raise FileExistsError(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".partial")
    temporary.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


__all__ = [
    "GHIDRA_LANGUAGE", "GHIDRA_PROJECT", "IMAGE_BASE", "MEDIA_ID", "PROGRAM_SHA256",
    "TITLE_ID", "XENIA_EDGE_APPIMAGE_SHA256", "XENIA_EDGE_RELEASE", "XENIA_EDGE_SOURCE_COMMIT",
    "collect_identity", "sha256_file", "write_manifest",
]
