#!/usr/bin/env python3
"""Audit the AC6 PAL oracle inventory and capture-build qualification."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

from apply_ac6_oracle_patch_stack import (
    load_stack,
    preflight_details,
    record_paths,
    worktree_paths_sha256,
)


ROOT = Path(__file__).resolve().parents[1]
CONTRACT = Path(
    "analysis/oracle/ac6-recomp-dcd41b/reproducibility-v1.json"
)
SCHEMA = "ac6.oracle-reproducibility.v1"
XEX_SHA256 = (
    "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"
)
ORACLE_COMMIT = "dcd41b7457fcac8242f8ef40de83d1719390d5af"
ARCHITECTURE_IDS = {
    "xenia-ppc-context",
    "xenia-xenos",
    "xenia-shared-memory",
    "xenonrecomp-readme",
    "xenosrecomp-readme",
}


class ReproducibilityError(ValueError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ReproducibilityError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def safe_path(root: Path, relative: object) -> Path:
    require(
        isinstance(relative, str)
        and relative != ""
        and not Path(relative).is_absolute(),
        "artifact path",
    )
    candidate = (root / relative).resolve()
    try:
        candidate.relative_to(root.resolve())
    except ValueError as error:
        raise ReproducibilityError("artifact path escapes root") from error
    return candidate


def validate_file(root: Path, record: dict, label: str) -> Path:
    path = safe_path(root, record.get("path"))
    require(
        path.is_file()
        and path.stat().st_size == record.get("size")
        and sha256(path) == record.get("sha256"),
        f"{label} identity",
    )
    return path


def run_git(root: Path, *arguments: str) -> bytes:
    result = subprocess.run(
        ["git", "-C", str(root), *arguments],
        check=False,
        capture_output=True,
    )
    if result.returncode != 0:
        detail = (result.stderr or result.stdout).decode(
            "utf-8", errors="replace"
        ).strip()
        raise ReproducibilityError(
            f"git {' '.join(arguments[:3])}: {detail or result.returncode}"
        )
    return result.stdout


def generated_tree(root: Path) -> tuple[int, int, str]:
    files = sorted(path for path in root.rglob("*") if path.is_file())
    digest = hashlib.sha256()
    byte_count = 0
    for path in files:
        require(not path.is_symlink(), "generated symlink")
        digest.update(path.relative_to(root).as_posix().encode("utf-8"))
        digest.update(b"\0")
        payload = path.read_bytes()
        byte_count += len(payload)
        digest.update(payload)
        digest.update(b"\0")
    return len(files), byte_count, digest.hexdigest()


def validate_document(document: dict, artifact_root: Path) -> None:
    require(document.get("schema") == SCHEMA, "reproducibility schema")
    target = document.get("target", {})
    require(
        target
        == {
            "platform": "Xbox 360 PAL",
            "module": "default.xex",
            "size": 7_483_392,
            "sha256": XEX_SHA256,
        },
        "PAL target",
    )

    retail = document.get("retail_inventory")
    expected_retail = {
        "default.xex",
        "DATA.TBL",
        "DATA00.PAC",
        "DATA01.PAC",
        "bgmpack.bin",
        "moviepack.bin",
        "demopack_eng.bin",
        "demopack_jpn.bin",
        "voicepack_eng.bin",
        "voicepack_jpn.bin",
    }
    require(
        isinstance(retail, list)
        and {record.get("path") for record in retail} == expected_retail
        and len(retail) == len(expected_retail),
        "retail inventory",
    )
    for record in retail:
        require(
            isinstance(record.get("size"), int)
            and record["size"] > 0
            and isinstance(record.get("sha256"), str)
            and len(record["sha256"]) == 64,
            f"retail record: {record.get('path')}",
        )

    ghidra = document.get("ghidra", {})
    require(
        ghidra.get("canonical_project") == "ace-combat-6"
        and ghidra.get("program") == "default.xex"
        and ghidra.get("historical_status") == "needs-revalidation"
        and ghidra.get("cross_project_export_merge_allowed") is False,
        "Ghidra authority",
    )
    validate_file(artifact_root, ghidra["bridge"], "Ghidra bridge")
    validate_file(
        artifact_root, ghidra["project_properties"], "Ghidra project"
    )

    catalog = document.get("architecture_catalog", {})
    require(
        catalog.get("portfolio_relative_path")
        == ".tools/knowledge-base/architecture-v1/catalog.json"
        and catalog.get("size") == 17_423
        and isinstance(catalog.get("sha256"), str)
        and len(catalog["sha256"]) == 64,
        "architecture catalog contract",
    )

    oracle = document.get("oracle", {})
    require(
        oracle.get("commit") == ORACLE_COMMIT
        and oracle.get("product_role")
        == "control-flow, ABI, and bounded-capture evidence only",
        "oracle boundary",
    )
    clean = oracle.get("clean_reference", {})
    require(
        clean.get("detached") is True
        and clean.get("status_entry_count") == 0
        and clean.get("status_sha256") == hashlib.sha256(b"").hexdigest(),
        "clean oracle reference",
    )
    dirty = oracle.get("preserved_dirty_checkouts")
    require(
        isinstance(dirty, list)
        and [item.get("name") for item in dirty] == ["reference", "gapfill"]
        and all(item.get("preserve_without_cleanup") is True for item in dirty),
        "preserved dirty checkouts",
    )

    stack = document.get("patch_stack", {})
    stack_path = safe_path(artifact_root, stack.get("path"))
    require(
        stack_path.is_file() and sha256(stack_path) == stack.get("sha256"),
        "patch stack identity",
    )
    base, records = load_stack(stack_path, artifact_root)
    require(
        base == ORACLE_COMMIT
        and len(records) == stack.get("patch_count") == 13,
        "patch stack boundary",
    )
    stack_document = json.loads(stack_path.read_text(encoding="utf-8"))
    qualification = stack_document.get("qualification", {})
    require(
        qualification.get("changed_file_count")
        == stack.get("changed_file_count")
        and qualification.get("changed_tree_sha256")
        == stack.get("changed_tree_sha256"),
        "patch stack overlay identity",
    )
    stack_configuration = stack_document.get("configuration", {})
    configuration = document.get("configuration", {})
    require(
        all(
            stack_configuration.get(key) == configuration.get(key)
            for key in ("boundary_sha256", "runtime_sha256", "capture_sha256")
        ),
        "capture configuration identity",
    )

    capture = document.get("capture_build", {})
    require(
        capture.get("compiler") == "Ubuntu clang 21.1.8 (6ubuntu1)"
        and capture.get("function_count") == 10_478
        and capture.get("generated_file_count") == 56
        and capture.get("generated_bytes") == 104_738_907
        and capture.get("binary_size") == 93_821_832
        and capture.get("host_ctests") == {"passed": 3, "failed": 0},
        "capture build",
    )
    require(
        capture.get("codegen_diagnostics")
        == {"status": "open", "gate_evidence": False},
        "codegen diagnostic boundary",
    )
    smoke = capture.get("smoke", {})
    require(
        smoke.get("frontier_source") == "0x8234530C"
        and smoke.get("frontier_target") == "0x8234524C"
        and smoke.get("result") == "unresolved-branch"
        and smoke.get("gate_evidence") is False,
        "capture smoke boundary",
    )

    timing = document.get("timing", {})
    require(
        timing.get("oracle_presentation_fps") == 30
        and timing.get("oracle_input_and_state_hz") == 30
        and timing.get("native_simulation_hz") == 60
        and timing.get("native_ticks_per_oracle_sample") == 2,
        "oracle timing contract",
    )
    boundary = document.get("qualification_boundary", {})
    manifest_path = safe_path(artifact_root, boundary.get("boundary_manifest"))
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    require(
        manifest.get("target", {}).get("sha256") == XEX_SHA256
        and manifest.get("oracle", {}).get("commit") == ORACLE_COMMIT
        and boundary.get("boundary_manifest_remains_separate") is True
        and boundary.get("capture_route_status") == "open"
        and boundary.get("capture_status") == "not-captured",
        "capture qualification boundary",
    )


def validate_retail(document: dict, retail_root: Path) -> None:
    for record in document["retail_inventory"]:
        validate_file(retail_root, record, f"retail {record['path']}")


def validate_catalog(document: dict, catalog_path: Path) -> None:
    record = document["architecture_catalog"]
    require(
        catalog_path.is_file()
        and catalog_path.stat().st_size == record["size"]
        and sha256(catalog_path) == record["sha256"],
        "architecture catalog identity",
    )
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    ids = {
        entry.get("id")
        for entry in catalog.get("entries", [])
        if entry.get("target") == "AC6"
    }
    require(ids == ARCHITECTURE_IDS, "architecture catalog AC6 entries")


def validate_reference(document: dict, reference_root: Path) -> None:
    require(
        run_git(reference_root, "rev-parse", "HEAD").decode().strip()
        == ORACLE_COMMIT,
        "reference HEAD",
    )
    branch = subprocess.run(
        ["git", "-C", str(reference_root), "symbolic-ref", "-q", "--short", "HEAD"],
        check=False,
        capture_output=True,
    )
    require(branch.returncode != 0, "reference must be detached")
    status = run_git(
        reference_root, "status", "--porcelain=v1", "--untracked-files=all"
    )
    expected = document["oracle"]["clean_reference"]
    require(
        len(status.splitlines()) == expected["status_entry_count"]
        and sha256_bytes(status) == expected["status_sha256"],
        "reference status",
    )


def validate_preserved_checkout(
    document: dict, name: str, checkout_root: Path
) -> None:
    records = {
        record["name"]: record
        for record in document["oracle"]["preserved_dirty_checkouts"]
    }
    require(name in records, f"unknown preserved checkout: {name}")
    record = records[name]
    require(
        run_git(checkout_root, "rev-parse", "HEAD").decode().strip()
        == record["commit"],
        f"preserved {name} HEAD",
    )
    branch = run_git(
        checkout_root, "symbolic-ref", "-q", "--short", "HEAD"
    ).decode().strip()
    status = run_git(
        checkout_root, "status", "--porcelain=v1", "--untracked-files=all"
    )
    tracked_diff = run_git(checkout_root, "diff", "--binary") + run_git(
        checkout_root, "diff", "--cached", "--binary"
    )
    require(
        branch == record["branch"]
        and len(status.splitlines()) == record["status_entry_count"]
        and sha256_bytes(status) == record["status_sha256"]
        and sha256_bytes(tracked_diff) == record["tracked_diff_sha256"],
        f"preserved {name} inventory",
    )


def validate_runtime(
    document: dict, runtime_root: Path, artifact_root: Path
) -> None:
    require(
        run_git(runtime_root, "rev-parse", "HEAD").decode().strip()
        == ORACLE_COMMIT,
        "runtime HEAD",
    )
    branch = subprocess.run(
        ["git", "-C", str(runtime_root), "symbolic-ref", "-q", "--short", "HEAD"],
        check=False,
        capture_output=True,
    )
    require(branch.returncode != 0, "runtime must be detached")
    runtime = document["oracle"]["runtime_overlay"]
    status = run_git(
        runtime_root, "status", "--porcelain=v1", "--untracked-files=all"
    )
    tracked_diff = run_git(runtime_root, "diff", "--binary") + run_git(
        runtime_root, "diff", "--cached", "--binary"
    )
    require(
        len(status.splitlines()) == runtime["status_entry_count"]
        and sha256_bytes(status) == runtime["status_sha256"]
        and sha256_bytes(tracked_diff) == runtime["tracked_diff_sha256"],
        "runtime overlay status",
    )
    require(
        sha256(runtime_root / "ac6recomp_config.toml")
        == document["configuration"]["capture_sha256"],
        "runtime capture configuration",
    )

    stack_path = safe_path(artifact_root, document["patch_stack"]["path"])
    base, records = load_stack(stack_path, artifact_root)
    result = preflight_details(runtime_root, base, records)
    stack = document["patch_stack"]
    require(
        result.tree == stack["git_tree"]
        and result.changed_file_count == stack["changed_file_count"]
        and result.changed_tree_sha256 == stack["changed_tree_sha256"],
        "runtime patch overlay",
    )
    expected_paths = set().union(
        *(record_paths(runtime_root, record) for record in records)
    )
    require(len(expected_paths) == stack["changed_file_count"], "runtime paths")
    require(
        worktree_paths_sha256(runtime_root, expected_paths)
        == stack["changed_tree_sha256"],
        "runtime patch bytes",
    )

    capture = document["capture_build"]
    require(
        generated_tree(runtime_root / "generated")
        == (
            capture["generated_file_count"],
            capture["generated_bytes"],
            capture["generated_tree_sha256"],
        ),
        "capture generated tree",
    )
    binary = runtime_root / "out/build/linux-amd64-release/ac6recomp"
    require(
        binary.is_file()
        and binary.stat().st_size == capture["binary_size"]
        and sha256(binary) == capture["binary_sha256"],
        "capture binary",
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--contract", type=Path, default=CONTRACT)
    parser.add_argument("--artifact-root", type=Path, default=ROOT)
    parser.add_argument("--retail-root", type=Path)
    parser.add_argument("--architecture-catalog", type=Path)
    parser.add_argument("--reference-root", type=Path)
    parser.add_argument("--runtime-root", type=Path)
    parser.add_argument(
        "--preserved-checkout", action="append", default=[],
        metavar="NAME=PATH",
    )
    parser.add_argument("--simde-archive", type=Path)
    arguments = parser.parse_args()
    try:
        artifact_root = arguments.artifact_root.resolve()
        contract = safe_path(artifact_root, str(arguments.contract))
        document = json.loads(contract.read_text(encoding="utf-8"))
        validate_document(document, artifact_root)
        if arguments.retail_root:
            validate_retail(document, arguments.retail_root.resolve())
        if arguments.architecture_catalog:
            validate_catalog(document, arguments.architecture_catalog.resolve())
        if arguments.reference_root:
            validate_reference(document, arguments.reference_root.resolve())
        checked_preserved: set[str] = set()
        for raw_checkout in arguments.preserved_checkout:
            require("=" in raw_checkout, "preserved checkout argument")
            name, raw_path = raw_checkout.split("=", 1)
            require(name not in checked_preserved, "duplicate preserved checkout")
            validate_preserved_checkout(document, name, Path(raw_path).resolve())
            checked_preserved.add(name)
        if arguments.simde_archive:
            overlay = document["oracle"]["simde_overlay"]
            archive = arguments.simde_archive.resolve()
            require(
                archive.is_file()
                and sha256(archive) == overlay["archive_sha256"],
                "SIMDe overlay archive identity",
            )
        if arguments.runtime_root:
            validate_runtime(
                document, arguments.runtime_root.resolve(), artifact_root
            )
    except (
        OSError,
        KeyError,
        json.JSONDecodeError,
        ReproducibilityError,
        ValueError,
    ) as error:
        print(f"oracle_reproducibility=fail error={error}", file=sys.stderr)
        return 1
    print(
        "oracle_reproducibility=pass "
        f"retail={len(document['retail_inventory'])} patches=13 "
        "oracle_fps=30 native_hz=60"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
