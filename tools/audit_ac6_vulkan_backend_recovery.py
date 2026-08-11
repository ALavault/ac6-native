#!/usr/bin/env python3
"""Validate the content-addressed AC6 Vulkan recovery checkpoint."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


class RecoveryError(ValueError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RecoveryError(message)


def relative_path(root: Path, value: object, label: str) -> Path:
    require(isinstance(value, str) and value != "", f"{label}: invalid path")
    candidate = Path(value)
    require(not candidate.is_absolute() and ".." not in candidate.parts,
            f"{label}: path escapes root")
    return root / candidate


def verify_file(path: Path, size: object, digest: object, label: str) -> None:
    require(path.is_file(), f"{label}: absent: {path}")
    require(isinstance(size, int) and size >= 0, f"{label}: invalid size")
    require(isinstance(digest, str) and len(digest) == 64,
            f"{label}: invalid sha256")
    payload = path.read_bytes()
    require(len(payload) == size, f"{label}: size mismatch")
    require(hashlib.sha256(payload).hexdigest() == digest,
            f"{label}: sha256 mismatch")


def audit(document: object, artifact_root: Path,
          origin_root: Path | None) -> tuple[int, int, dict[str, int]]:
    require(isinstance(document, dict), "manifest is not an object")
    require(document.get("schema") == "ac6.vulkan-backend-recovery.v1",
            "unsupported schema")
    snapshot = document.get("source_snapshot")
    require(isinstance(snapshot, dict), "missing source_snapshot")
    require(snapshot.get("provenance") == "unmanaged filesystem snapshot",
            "source provenance must remain explicit")
    require(snapshot.get("git_commit") is None,
            "unmanaged snapshot must not claim a Git commit")
    source_files = snapshot.get("files")
    require(isinstance(source_files, list) and source_files,
            "empty source snapshot")
    source_paths: set[str] = set()
    for index, record in enumerate(source_files):
        require(isinstance(record, dict), f"source file {index}: invalid record")
        path = record.get("path")
        require(isinstance(path, str) and path not in source_paths,
                f"source file {index}: duplicate path")
        source_paths.add(path)
        if origin_root is not None:
            verify_file(relative_path(origin_root, path, f"source file {index}"),
                        record.get("size"), record.get("sha256"),
                        f"source file {index}")

    imported = document.get("imported")
    require(isinstance(imported, list) and imported, "empty imported set")
    for index, record in enumerate(imported):
        require(isinstance(record, dict), f"imported {index}: invalid record")
        current = relative_path(artifact_root, record.get("current_path"),
                                f"imported {index}")
        require(current.is_file(), f"imported {index}: current file absent")
        digest = record.get("sha256")
        require(isinstance(digest, str) and
                hashlib.sha256(current.read_bytes()).hexdigest() == digest,
                f"imported {index}: current sha256 mismatch")
        require(record.get("origin_path") in source_paths,
                f"imported {index}: origin not in snapshot")

    responsibilities = document.get("adapted_mechanisms")
    expected = {"device", "resources", "pipelines", "commands", "readback",
                "surface", "swapchain"}
    require(isinstance(responsibilities, list), "invalid adapted_mechanisms")
    seen_responsibilities: set[str] = set()
    for index, record in enumerate(responsibilities):
        require(isinstance(record, dict), f"mechanism {index}: invalid record")
        responsibility = record.get("responsibility")
        require(isinstance(responsibility, str) and
                responsibility not in seen_responsibilities,
                f"mechanism {index}: duplicate responsibility")
        seen_responsibilities.add(responsibility)
        paths = record.get("paths")
        require(isinstance(paths, list) and paths,
                f"mechanism {index}: empty paths")
        for path in paths:
            require(relative_path(artifact_root, path,
                                  f"mechanism {index}").is_file(),
                    f"mechanism {index}: current path absent")
    require(seen_responsibilities == expected,
            "responsibility set is incomplete")

    product_root = artifact_root / "reconstruction/ace-combat-6"
    excluded = document.get("excluded_contracts")
    require(isinstance(excluded, list) and excluded, "empty excluded contracts")
    for index, path in enumerate(excluded):
        require(not relative_path(product_root, path,
                                  f"excluded {index}").exists(),
                f"excluded contract imported: {path}")

    reports = document.get("historical_reports")
    require(isinstance(reports, list) and reports, "empty report classification")
    classified_paths: set[str] = set()
    counts = {"reproduit": 0, "indice": 0, "superseded": 0}
    for index, record in enumerate(reports):
        require(isinstance(record, dict), f"report {index}: invalid record")
        status = record.get("status")
        require(status in counts, f"report {index}: invalid status")
        paths = record.get("paths")
        require(isinstance(paths, list) and paths, f"report {index}: empty paths")
        for path in paths:
            require(isinstance(path, str) and path not in classified_paths,
                    f"report {index}: duplicate path")
            require(relative_path(artifact_root, path,
                                  f"report {index}").is_file(),
                    f"report {index}: path absent")
            classified_paths.add(path)
        require(isinstance(record.get("reason"), str) and record.get("reason"),
                f"report {index}: missing reason")
        counts[status] += 1
    return len(source_files), len(classified_paths), counts


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--artifact-root", type=Path, default=Path("."))
    parser.add_argument("--origin-root", type=Path)
    args = parser.parse_args()
    try:
        document = json.loads(args.manifest.read_text(encoding="utf-8"))
        source_count, report_count, counts = audit(
            document, args.artifact_root.resolve(),
            args.origin_root.resolve() if args.origin_root else None)
    except (OSError, json.JSONDecodeError, RecoveryError) as error:
        print(f"vulkan_backend_recovery=fail reason={error}")
        return 1
    print("vulkan_backend_recovery=pass "
          f"source_files={source_count} report_paths={report_count} "
          f"reproduit={counts['reproduit']} indice={counts['indice']} "
          f"superseded={counts['superseded']} "
          f"origin_checked={1 if args.origin_root else 0}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
