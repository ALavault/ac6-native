#!/usr/bin/env python3
"""Preflight and apply the sealed AC6_recomp oracle patch stack."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shlex
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path

from build_ac6_oracle_capture_config import (
    CaptureConfigError,
    apply_overrides,
    load_policy as load_capture_policy,
)
from build_ac6_oracle_runtime_config import (
    RuntimeConfigError,
    apply_hook_policy,
    load_hook_policy,
)
from generate_ac6_oracle_config import (
    BoundaryError,
    append_switch_tables,
    load_boundaries,
    load_switch_tables,
)


SCHEMA = "ac6.oracle-host-patch-stack.v1"
MANIFEST = Path("analysis/oracle/ac6-recomp-dcd41b/manifest.json")
ALLOWED_APPLY_ARGS = {"--unidiff-zero"}


class StackError(ValueError):
    pass


@dataclass(frozen=True)
class PatchRecord:
    order: int
    path: Path
    display_path: str
    apply_args: tuple[str, ...]


@dataclass(frozen=True)
class PathSnapshot:
    relative: Path
    kind: str
    mode: int
    payload: bytes | str | None
    missing_parents: tuple[Path, ...]


@dataclass(frozen=True)
class WorktreeSnapshot:
    paths: tuple[PathSnapshot, ...]
    index_path: Path
    index_payload: bytes
    index_mode: int


@dataclass(frozen=True)
class PreflightResult:
    tree: str
    changed_file_count: int
    changed_tree_sha256: str


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise StackError(message)


def project_path(root: Path, relative: object) -> Path:
    require(isinstance(relative, str) and relative != "", "patch path")
    path = Path(relative)
    require(not path.is_absolute(), "absolute patch path")
    candidate = (root / path).resolve()
    try:
        candidate.relative_to(root.resolve())
    except ValueError as error:
        raise StackError("patch path escapes artifact root") from error
    return candidate


def run_git(
    root: Path,
    *arguments: str,
    environment: dict[str, str] | None = None,
    allow_failure: bool = False,
) -> str:
    result = subprocess.run(
        ["git", "-C", str(root), *arguments],
        check=False,
        capture_output=True,
        text=True,
        env=environment,
    )
    if result.returncode != 0 and not allow_failure:
        detail = (result.stderr or result.stdout).strip().splitlines()
        suffix = detail[-1] if detail else f"exit {result.returncode}"
        raise StackError(f"git {' '.join(arguments[:3])}: {suffix}")
    return result.stdout.rstrip("\n")


def run_git_bytes(
    root: Path,
    *arguments: str,
    environment: dict[str, str] | None = None,
) -> bytes:
    result = subprocess.run(
        ["git", "-C", str(root), *arguments],
        check=False,
        capture_output=True,
        env=environment,
    )
    if result.returncode != 0:
        detail = (result.stderr or result.stdout).decode(
            "utf-8", errors="replace"
        ).strip().splitlines()
        suffix = detail[-1] if detail else f"exit {result.returncode}"
        raise StackError(f"git {' '.join(arguments[:3])}: {suffix}")
    return result.stdout


def load_stack(stack_path: Path, artifact_root: Path) -> tuple[str, list[PatchRecord]]:
    document = json.loads(stack_path.read_text(encoding="utf-8"))
    require(document.get("schema") == SCHEMA, "patch stack schema")
    base = document.get("base_commit")
    require(
        isinstance(base, str) and len(base) == 40 and all(c in "0123456789abcdef" for c in base),
        "patch stack base commit",
    )
    require(document.get("application") == "ordered", "patch stack application")
    raw_records = document.get("patches")
    require(isinstance(raw_records, list) and raw_records, "empty patch stack")

    records: list[PatchRecord] = []
    for expected_order, record in enumerate(raw_records, start=1):
        require(isinstance(record, dict), f"patch record {expected_order}")
        require(record.get("order") == expected_order, "patch stack order")
        display_path = record.get("path")
        path = project_path(artifact_root, display_path)
        require(path.is_file(), f"patch absent: {display_path}")
        digest = record.get("sha256")
        require(isinstance(digest, str) and sha256(path) == digest, f"patch hash: {display_path}")
        raw_args = record.get("apply_args", [])
        require(
            isinstance(raw_args, list)
            and all(isinstance(item, str) and item in ALLOWED_APPLY_ARGS for item in raw_args),
            f"patch apply args: {display_path}",
        )
        records.append(
            PatchRecord(expected_order, path, str(display_path), tuple(raw_args))
        )
    return base, records


def derive_capture_configuration(
    stack_path: Path, artifact_root: Path, boundary_payload: bytes
) -> bytes:
    try:
        document = json.loads(stack_path.read_text(encoding="utf-8"))
        configuration = document["configuration"]
        required = {
            "boundary_sha256", "runtime_sha256", "capture_sha256",
            "hook_policy", "hook_policy_sha256", "capture_overrides",
            "capture_overrides_sha256", "boundary_export",
            "boundary_export_sha256", "switch_tables", "switch_tables_sha256",
            "runtime_decisions", "runtime_decisions_sha256",
            "capture_decisions", "capture_decisions_sha256", "source", "file_path",
        }
        require(isinstance(configuration, dict) and set(configuration) == required,
                "patch stack configuration")
        require(
            sha256_bytes(boundary_payload) == configuration["boundary_sha256"],
            "boundary configuration identity",
        )

        def sealed_payload(path_key: str, hash_key: str) -> bytes:
            path = project_path(artifact_root, configuration[path_key])
            require(path.is_file(), f"configuration artifact absent: {path_key}")
            payload = path.read_bytes()
            require(sha256_bytes(payload) == configuration[hash_key],
                    f"configuration artifact hash: {path_key}")
            return payload

        hook_payload = sealed_payload("hook_policy", "hook_policy_sha256")
        boundary_export = sealed_payload(
            "boundary_export", "boundary_export_sha256"
        )
        switch_payload = sealed_payload("switch_tables", "switch_tables_sha256")
        capture_policy_payload = sealed_payload(
            "capture_overrides", "capture_overrides_sha256"
        )
        runtime_decisions = json.loads(
            sealed_payload("runtime_decisions", "runtime_decisions_sha256")
        )
        capture_decisions = json.loads(
            sealed_payload("capture_decisions", "capture_decisions_sha256")
        )
        require(
            runtime_decisions.get("input_configuration_sha256")
            == configuration["boundary_sha256"]
            and runtime_decisions.get("hook_policy_sha256")
            == configuration["hook_policy_sha256"]
            and runtime_decisions.get("boundary_export_sha256")
            == configuration["boundary_export_sha256"]
            and runtime_decisions.get("switch_tables_sha256")
            == configuration["switch_tables_sha256"]
            and runtime_decisions.get("output_configuration_sha256")
            == configuration["runtime_sha256"],
            "runtime configuration decisions",
        )
        require(
            capture_decisions.get("input_configuration_sha256")
            == configuration["runtime_sha256"]
            and capture_decisions.get("boundary_export_sha256")
            == configuration["boundary_export_sha256"]
            and capture_decisions.get("policy_sha256")
            == configuration["capture_overrides_sha256"]
            and capture_decisions.get("output_configuration_sha256")
            == configuration["capture_sha256"],
            "capture configuration decisions",
        )
        functions = load_boundaries(json.loads(boundary_export))
        hooks = load_hook_policy(
            json.loads(hook_payload), configuration["boundary_sha256"]
        )
        runtime_payload = append_switch_tables(
            apply_hook_policy(boundary_payload, hooks),
            load_switch_tables(json.loads(switch_payload), functions),
        )
        require(sha256_bytes(runtime_payload) == configuration["runtime_sha256"],
                "runtime configuration derivation")
        capture_policy = load_capture_policy(
            capture_policy_payload,
            configuration["runtime_sha256"],
            configuration["boundary_export_sha256"],
            {function.entry for function in functions},
        )
        capture_payload = apply_overrides(runtime_payload, capture_policy)
        require(sha256_bytes(capture_payload) == configuration["capture_sha256"],
                "capture configuration derivation")
        file_path = f'file_path = "{configuration["file_path"]}"'.encode()
        require(file_path in capture_payload, "capture configuration portable path")
        return capture_payload
    except (BoundaryError, CaptureConfigError, RuntimeConfigError) as error:
        raise StackError(f"capture configuration: {error}") from error


def validate_target(root: Path, base: str, artifact_root: Path) -> None:
    require(root.is_dir(), "oracle runtime worktree absent")
    require(run_git(root, "rev-parse", "HEAD") == base, "oracle runtime HEAD")
    branch = run_git(
        root, "symbolic-ref", "-q", "--short", "HEAD", allow_failure=True
    )
    require(branch == "", "oracle runtime must be detached")

    status = run_git(root, "status", "--porcelain=v1", "--untracked-files=all")
    require(status.splitlines() == [" M ac6recomp_config.toml"], "unexpected runtime changes")
    manifest_path = (artifact_root / MANIFEST).resolve()
    document = json.loads(manifest_path.read_text(encoding="utf-8"))
    require(document["oracle"]["commit"] == base, "stack/manifest oracle commit")
    target = document["target"]
    require(
        target.get("module") == "default.xex"
        and target.get("platform") == "Xbox 360 PAL",
        "stack/manifest PAL target",
    )
    xex = root / "assets/default.xex"
    require(
        xex.is_file()
        and xex.stat().st_size == target.get("size")
        and sha256(xex) == target.get("sha256"),
        "runtime PAL XEX identity",
    )
    config = root / document["configuration"]["path"]
    expected = document["boundary_correction"]["patched_configuration_sha256"]
    require(config.is_file() and sha256(config) == expected, "runtime configuration identity")


def git_apply_arguments(record: PatchRecord, *, cached: bool, check: bool) -> list[str]:
    arguments = ["apply"]
    if cached:
        arguments.append("--cached")
    else:
        arguments.append("--index")
    if check:
        arguments.append("--check")
    arguments.extend(record.apply_args)
    arguments.append(str(record.path))
    return arguments


def record_paths(root: Path, record: PatchRecord) -> set[Path]:
    paths: set[Path] = set()
    lines = record.path.read_text(encoding="utf-8").splitlines()

    def add_path(raw: str, expected_prefix: str) -> None:
        if raw == "/dev/null":
            return
        require(raw.startswith(expected_prefix),
                f"patch path prefix: {record.display_path}")
        relative = Path(raw[2:])
        require(not relative.is_absolute() and ".." not in relative.parts,
                f"patch path escapes runtime: {record.display_path}")
        require(
            "generated" not in relative.parts
            and not relative.name.startswith("ppc_recomp."),
            f"generated output path forbidden: {record.display_path}",
        )
        candidate = (root / relative).resolve(strict=False)
        try:
            candidate.relative_to(root.resolve())
        except ValueError as error:
            raise StackError(
                f"patch path escapes runtime: {record.display_path}"
            ) from error
        paths.add(relative)

    for line in lines:
        if not line.startswith("diff --git "):
            continue
        try:
            fields = shlex.split(line)
        except ValueError as error:
            raise StackError(f"malformed patch header: {record.display_path}") from error
        require(len(fields) == 4 and fields[:2] == ["diff", "--git"],
                f"malformed patch header: {record.display_path}")
        add_path(fields[2], "a/")
        add_path(fields[3], "b/")

    if not paths:
        for index, line in enumerate(lines[:-1]):
            if not line.startswith("--- ") or not lines[index + 1].startswith("+++ "):
                continue
            old_path = line[4:].split("\t", 1)[0]
            new_path = lines[index + 1][4:].split("\t", 1)[0]
            add_path(old_path, "a/")
            add_path(new_path, "b/")
    require(bool(paths), f"patch has no paths: {record.display_path}")
    return paths


def worktree_paths_sha256(root: Path, paths: set[Path]) -> str:
    digest = hashlib.sha256()
    for relative in sorted(paths):
        path = root / relative
        require(path.is_file() and not path.is_symlink(),
                f"overlay path is not a regular file: {relative}")
        digest.update(relative.as_posix().encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def snapshot_worktree(
    root: Path, records: list[PatchRecord], extra_paths: tuple[Path, ...] = ()
) -> WorktreeSnapshot:
    relative_paths = sorted(
        set(extra_paths).union(*(record_paths(root, record) for record in records))
    )
    snapshots: list[PathSnapshot] = []
    for relative in relative_paths:
        path = root / relative
        missing_parents = []
        parent = path.parent
        while parent != root and not parent.exists():
            missing_parents.append(parent.relative_to(root))
            parent = parent.parent
        if path.is_symlink():
            snapshots.append(PathSnapshot(
                relative, "symlink", path.lstat().st_mode,
                os.readlink(path), tuple(missing_parents),
            ))
        elif path.is_file():
            snapshots.append(PathSnapshot(
                relative, "file", path.stat().st_mode,
                path.read_bytes(), tuple(missing_parents),
            ))
        elif path.exists():
            raise StackError(f"patch target is not a file: {relative}")
        else:
            snapshots.append(PathSnapshot(
                relative, "missing", 0, None, tuple(missing_parents),
            ))

    raw_index_path = Path(run_git(root, "rev-parse", "--git-path", "index"))
    index_path = raw_index_path if raw_index_path.is_absolute() else root / raw_index_path
    require(index_path.is_file(), "runtime Git index absent")
    return WorktreeSnapshot(
        tuple(snapshots), index_path, index_path.read_bytes(), index_path.stat().st_mode
    )


def atomic_write(path: Path, payload: bytes, mode: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.ac6-restore-", dir=path.parent
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "wb") as destination:
            destination.write(payload)
            destination.flush()
            os.fsync(destination.fileno())
        os.chmod(temporary, mode & 0o7777)
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def restore_worktree(root: Path, snapshot: WorktreeSnapshot) -> None:
    for item in snapshot.paths:
        path = root / item.relative
        if path.is_symlink() or path.is_file():
            path.unlink()
        elif path.exists():
            raise StackError(f"cannot restore non-file patch target: {item.relative}")
        if item.kind == "file":
            require(isinstance(item.payload, bytes), "file snapshot payload")
            atomic_write(path, item.payload, item.mode)
        elif item.kind == "symlink":
            require(isinstance(item.payload, str), "symlink snapshot payload")
            path.parent.mkdir(parents=True, exist_ok=True)
            path.symlink_to(item.payload)
        else:
            require(item.kind == "missing", "path snapshot kind")

    for item in reversed(snapshot.paths):
        for relative in item.missing_parents:
            try:
                (root / relative).rmdir()
            except OSError:
                pass
    atomic_write(snapshot.index_path, snapshot.index_payload, snapshot.index_mode)


def preflight_details(
    root: Path, base: str, records: list[PatchRecord]
) -> PreflightResult:
    with tempfile.TemporaryDirectory(prefix="ac6-oracle-stack-") as temporary:
        index = Path(temporary) / "index"
        environment = os.environ.copy()
        environment["GIT_INDEX_FILE"] = str(index)
        run_git(root, "read-tree", base, environment=environment)
        for record in records:
            run_git(
                root,
                *git_apply_arguments(record, cached=True, check=True),
                environment=environment,
            )
            run_git(
                root,
                *git_apply_arguments(record, cached=True, check=False),
                environment=environment,
            )
        paths = sorted(set().union(*(record_paths(root, record) for record in records)))
        digest = hashlib.sha256()
        for relative in paths:
            digest.update(relative.as_posix().encode("utf-8"))
            digest.update(b"\0")
            digest.update(
                run_git_bytes(
                    root, "show", f":{relative.as_posix()}",
                    environment=environment,
                )
            )
            digest.update(b"\0")
        return PreflightResult(
            run_git(root, "write-tree", environment=environment),
            len(paths),
            digest.hexdigest(),
        )


def preflight(root: Path, base: str, records: list[PatchRecord]) -> str:
    return preflight_details(root, base, records).tree


def validate_qualification(
    stack_path: Path, records: list[PatchRecord], result: PreflightResult
) -> None:
    qualification = json.loads(stack_path.read_text(encoding="utf-8")).get(
        "qualification"
    )
    required = {
        "qualified_patch_count", "clean_application_pass", "runtime_route_status",
        "changed_file_count", "changed_tree_algorithm", "changed_tree_sha256",
        "capture_profile_byte_match",
    }
    require(isinstance(qualification, dict) and set(qualification) == required,
            "patch stack qualification")
    qualified_count = qualification["qualified_patch_count"]
    require(isinstance(qualified_count, int) and 0 <= qualified_count <= len(records),
            "qualified patch count")
    require(qualification["clean_application_pass"] is True,
            "clean application qualification")
    require(qualification["runtime_route_status"] in {"open", "passed"},
            "runtime route qualification")
    require(
        qualification["changed_file_count"] == result.changed_file_count
        and qualification["changed_tree_algorithm"]
        == "sha256(sorted(path + NUL + bytes + NUL))"
        and qualification["changed_tree_sha256"] == result.changed_tree_sha256,
        "changed overlay qualification",
    )
    require(qualification["capture_profile_byte_match"] is True,
            "capture profile qualification")


def apply_stack(
    root: Path,
    records: list[PatchRecord],
    expected_tree: str,
    capture_configuration: bytes | None = None,
) -> None:
    initial_status = run_git(root, "status", "--porcelain=v1", "--untracked-files=all")
    initial_index = run_git(root, "write-tree")
    configuration_path = Path("ac6recomp_config.toml")
    extra_paths = (configuration_path,) if capture_configuration is not None else ()
    snapshot = snapshot_worktree(root, records, extra_paths)
    try:
        if capture_configuration is not None:
            config = root / configuration_path
            atomic_write(config, capture_configuration, config.stat().st_mode)
        for record in records:
            run_git(root, *git_apply_arguments(record, cached=False, check=True))
            run_git(root, *git_apply_arguments(record, cached=False, check=False))
        require(run_git(root, "write-tree") == expected_tree, "runtime overlay tree mismatch")
        require(run_git(root, "diff", "--cached", "--check") == "",
                "runtime overlay staged whitespace")
        require(run_git(root, "diff", "--check") == "", "runtime overlay whitespace")
        run_git(root, "restore", "--staged", "--source=HEAD", "--", ":/")
    except (OSError, StackError) as error:
        try:
            restore_worktree(root, snapshot)
            verification_environment = os.environ.copy()
            verification_environment["GIT_OPTIONAL_LOCKS"] = "0"
            restored_status = run_git(
                root, "status", "--porcelain=v1", "--untracked-files=all",
                environment=verification_environment,
            )
            restored_index = run_git(
                root, "write-tree", environment=verification_environment
            )
            require(restored_status == initial_status and restored_index == initial_index,
                    "runtime state differs from pre-apply snapshot")
        except (OSError, StackError) as rollback_error:
            raise StackError(
                f"{error}; rollback failed: {rollback_error}"
            ) from error
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stack", type=Path)
    parser.add_argument("runtime_root", type=Path)
    parser.add_argument("--artifact-root", type=Path, default=Path("."))
    parser.add_argument("--apply", action="store_true")
    arguments = parser.parse_args()
    try:
        artifact_root = arguments.artifact_root.resolve()
        stack_path = arguments.stack.resolve()
        base, records = load_stack(stack_path, artifact_root)
        runtime_root = arguments.runtime_root.resolve()
        validate_target(runtime_root, base, artifact_root)
        capture_configuration = derive_capture_configuration(
            stack_path, artifact_root,
            (runtime_root / "ac6recomp_config.toml").read_bytes(),
        )
        preflight_result = preflight_details(runtime_root, base, records)
        validate_qualification(stack_path, records, preflight_result)
        tree = preflight_result.tree
        if arguments.apply:
            apply_stack(runtime_root, records, tree, capture_configuration)
    except (OSError, KeyError, json.JSONDecodeError, StackError) as error:
        print(f"AC6_ORACLE_PATCH_STACK_FAIL reason={error}")
        return 1
    mode = "applied" if arguments.apply else "checked"
    print(
        f"AC6_ORACLE_PATCH_STACK_PASS mode={mode} patches={len(records)} "
        f"tree={tree} capture_config_sha256={sha256_bytes(capture_configuration)} "
        f"overlay_sha256={preflight_result.changed_tree_sha256} "
        f"stack_sha256={sha256(stack_path)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
