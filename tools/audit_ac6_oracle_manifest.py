#!/usr/bin/env python3
"""Validate the revision-pinned AC6_recomp oracle qualification manifest."""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

SCHEMA = "ac6.recomp-oracle-manifest.v1"
ORACLE_COMMIT = "dcd41b7457fcac8242f8ef40de83d1719390d5af"
SDK_VENDOR_COMMIT = "06d1f5785153cd57c0e6b289f587adca67859714"
SDK_TREE = "741541d6035616dc406f7d74c2fe8f155913c77b"
XEX_SHA256 = "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"
XEX_SIZE = 7_483_392
CONFIG_SHA256 = "bbc441bbeb793dfd5d9bee403b69d39a68efcf59e808fe96f05dee6688951c4c"


class ManifestError(ValueError):
    pass


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ManifestError(message)


def git(root: Path, *arguments: str, allow_failure: bool = False) -> str:
    result = subprocess.run(
        ["git", "-C", str(root), *arguments], text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    if result.returncode != 0 and not allow_failure:
        raise ManifestError(f"git {' '.join(arguments)}: {result.stderr.strip()}")
    return result.stdout.strip()


def safe_path(root: Path, relative: str) -> Path:
    require(relative != "" and not Path(relative).is_absolute(), "artifact path must be relative")
    candidate = (root / relative).resolve()
    try:
        candidate.relative_to(root.resolve())
    except ValueError as error:
        raise ManifestError(f"artifact path escapes root: {relative}") from error
    return candidate


def validate_document(document: dict, artifact_root: Path) -> int:
    require(document.get("schema") == SCHEMA, "schema")
    oracle = document.get("oracle")
    require(isinstance(oracle, dict), "oracle")
    require(oracle.get("repository") == "https://github.com/sal063/AC6_recomp.git",
            "oracle.repository")
    require(oracle.get("commit") == ORACLE_COMMIT, "oracle.commit")
    require(oracle.get("required_worktree_state") == "clean-detached", "oracle worktree state")

    sdk = document.get("sdk")
    require(isinstance(sdk, dict), "sdk")
    require(sdk.get("integration") == "vendored-tree", "sdk.integration")
    require(sdk.get("vendor_commit") == SDK_VENDOR_COMMIT, "sdk.vendor_commit")
    require(sdk.get("tree_sha1") == SDK_TREE, "sdk.tree_sha1")

    configuration = document.get("configuration")
    require(isinstance(configuration, dict), "configuration")
    require(configuration.get("path") == "ac6recomp_config.toml", "configuration.path")
    require(configuration.get("sha256") == CONFIG_SHA256, "configuration.sha256")

    target = document.get("target")
    require(isinstance(target, dict), "target")
    require(target.get("platform") == "Xbox 360 PAL", "target.platform")
    require(target.get("module") == "default.xex", "target.module")
    require(target.get("sha256") == XEX_SHA256, "target.sha256")
    require(target.get("size") == XEX_SIZE, "target.size")
    require(target.get("ghidra_project") == "ghidra-projects/ace-combat-6",
            "target.ghidra_project")

    tools = document.get("host_tools")
    require(isinstance(tools, list) and tools, "host_tools")
    for tool in tools:
        require(isinstance(tool, dict) and isinstance(tool.get("name"), str) and
                isinstance(tool.get("version"), str) and tool["version"], "host tool entry")

    probes = document.get("probes")
    require(isinstance(probes, list) and probes, "probes")
    names: set[str] = set()
    for probe in probes:
        require(isinstance(probe, dict), "probe entry")
        name, relative, digest = probe.get("name"), probe.get("contract"), probe.get("sha256")
        require(isinstance(name, str) and name and name not in names, "probe.name")
        require(isinstance(relative, str) and isinstance(digest, str) and len(digest) == 64,
                f"probe {name} identity")
        contract = safe_path(artifact_root, relative)
        require(contract.is_file(), f"probe contract absent: {relative}")
        require(sha256(contract) == digest, f"probe contract hash: {relative}")
        names.add(name)
    return len(probes)


def validate_oracle_root(document: dict, root: Path) -> None:
    require(root.is_dir(), "oracle root absent")
    require(git(root, "rev-parse", "HEAD") == ORACLE_COMMIT, "oracle HEAD")
    branch = git(root, "symbolic-ref", "-q", "--short", "HEAD", allow_failure=True)
    require(branch == "", "oracle worktree is attached to a branch")
    require(git(root, "status", "--porcelain=v1", "--untracked-files=all") == "",
            "oracle worktree is dirty")
    config = root / document["configuration"]["path"]
    require(config.is_file() and sha256(config) == CONFIG_SHA256, "oracle configuration")
    require(git(root, "rev-parse", "HEAD:thirdparty/rexglue-sdk") == SDK_TREE,
            "vendored SDK tree")
    last_vendor_change = git(root, "log", "-1", "--format=%H", "--", "thirdparty/rexglue-sdk")
    require(last_vendor_change == SDK_VENDOR_COMMIT, "vendored SDK commit")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--artifact-root", type=Path, default=Path("."))
    parser.add_argument("--oracle-root", type=Path)
    parser.add_argument("--xex", type=Path)
    args = parser.parse_args()
    try:
        document = json.loads(args.manifest.read_text(encoding="utf-8"))
        probe_count = validate_document(document, args.artifact_root)
        if args.oracle_root is not None:
            validate_oracle_root(document, args.oracle_root)
        if args.xex is not None:
            require(args.xex.is_file(), "XEX absent")
            require(args.xex.stat().st_size == XEX_SIZE and sha256(args.xex) == XEX_SHA256,
                    "XEX identity")
    except (OSError, json.JSONDecodeError, ManifestError) as error:
        print(f"oracle_manifest=fail reason={error}")
        return 1
    print(f"oracle_manifest=pass probes={probe_count} "
          f"oracle_root={'checked' if args.oracle_root else 'not-requested'} "
          f"xex={'checked' if args.xex else 'not-requested'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
