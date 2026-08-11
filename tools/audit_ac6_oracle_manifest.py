#!/usr/bin/env python3
"""Validate the revision-pinned AC6_recomp oracle qualification manifest."""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import tarfile
from pathlib import Path

SCHEMA = "ac6.recomp-oracle-manifest.v1"
ORACLE_COMMIT = "dcd41b7457fcac8242f8ef40de83d1719390d5af"
SDK_VENDOR_COMMIT = "06d1f5785153cd57c0e6b289f587adca67859714"
SDK_TREE = "741541d6035616dc406f7d74c2fe8f155913c77b"
XEX_SHA256 = "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"
XEX_SIZE = 7_483_392
CONFIG_SHA256 = "bbc441bbeb793dfd5d9bee403b69d39a68efcf59e808fe96f05dee6688951c4c"
OVERLAY_COMMIT = "c57016c272937205e45855b74de6590f3b2b39ae"
OVERLAY_TREE = "9559a819f9d621989165d6b755d0a58e2ffda5ea"
OVERLAY_X86_TREE = "7d9f7db70591bbadd487d44d8bfaf99d6426ecc4"
OVERLAY_ARCHIVE_SHA256 = "39b46c0b9b8874936a0a5168470ef4f93b0e8f6645d80969ffde28acd9a5e2b4"
BOUNDARY_TRANSFORMER_SHA256 = "586213073ff6c2094e63e5e057f0d76fb1d66a82eacf7d3149cd045c0781faab"
PATCHED_CONFIG_SHA256 = "450d6904d2338ddeb5d80f3cf4a420c9cc6853bcff119c27b06f304b472f5086"
VERIFICATION_SET_SHA256 = "631fc38553006fdcc0e2fc7d2ab2c06ad83e468621678f33d486734c60eae9db"
HOST_SOURCE_PATCH_SHA256 = "75b228bb883052874f441f3600cf7406f1d148d605f3994316630af18ca9c88f"
PATCHED_HOST_SOURCE_SHA256 = "457ecb1ef7e9920c6d12e1975c5089f8dae5ae1b65eb1a5f8eda2a2ec71b0724"
RUNTIME_BINARY_SHA256 = "815e18930794883d92a47f03a3747e59501ac2f6b9f242ea5bf0bf3029682b8f"
RUNTIME_BINARY_SIZE = 46_889_016
FALSE_STARTS = [
    "0x820FA690", "0x82106520", "0x8212C830", "0x821CA570",
    "0x821D4B20", "0x821D4B30", "0x821DD8A0", "0x821DE7D0",
    "0x821DE7E8", "0x821E6AC8", "0x821EB6E0", "0x821EDD68",
    "0x821F0AC8", "0x821F0AD0",
    "0x821F7B18", "0x821F7B28", "0x821F7C08", "0x821F7C80",
    "0x821F7CE0", "0x821F8A00", "0x821F8B38",
    "0x82251440", "0x82265D20", "0x82275F88", "0x822760B8",
    "0x822CE7B0", "0x822CE9A8", "0x822CEAC0", "0x822CFBA8",
    "0x822CFBF8", "0x822CFCE8", "0x822EF758", "0x82338AE8",
    "0x82345190", "0x82345200", "0x82345228", "0x82345250",
    "0x82348100", "0x82349730", "0x8234CA20", "0x823849F0",
    "0x82384AAC", "0x82384AE8", "0x82393E30", "0x82393EB8",
    "0x8239D8A0", "0x8239E970", "0x823A0238", "0x823A0240",
    "0x823A0298", "0x823A02D0", "0x823D1958", "0x823D20B0",
]
INDIVIDUAL_FALSE_STARTS = [
    address for address in FALSE_STARTS
    if address not in {
        "0x821F7B18", "0x821F7B28", "0x821F7C08", "0x821F7C80",
        "0x821F7CE0", "0x821F8A00", "0x821F8B38",
        "0x823849F0", "0x82384AAC", "0x82384AE8",
        "0x82393E30", "0x82393EB8", "0x8239D8A0", "0x8239E970",
        "0x823A0238", "0x823A0240", "0x823A0298", "0x823A02D0",
    }
]
VERIFICATION_SCRIPTS = [
    f"scripts/Verify{address[2:]}Boundary.java" for address in INDIVIDUAL_FALSE_STARTS
] + ["tools/ghidra/InspectOracleCodegenBoundaries.java"]
GENERATED_FILE_COUNT = 56
GENERATED_BYTES = 105_290_952
GENERATED_TREE_SHA256 = "18bb31ba94c4653fc69a1ac32f2464b8c612a7e2b5a5215537212d108bad39cc"
HOST_TOOLS = [
    {"name": "cmake", "version": "4.2.3"},
    {"name": "ninja", "version": "1.13.2"},
    {"name": "clang", "version": "Ubuntu clang 21.1.8 (6ubuntu1)"},
    {"name": "python", "version": "3.14.4"},
    {"name": "git", "version": "2.53.0"},
    {"name": "vulkan-loader", "version": "1.4.341"},
]


class ManifestError(ValueError):
    pass


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def tree_sha256(root: Path) -> tuple[int, int, str]:
    require(root.is_dir(), "generated root absent")
    files = sorted(path for path in root.rglob("*") if path.is_file())
    digest = hashlib.sha256()
    byte_count = 0
    for path in files:
        require(not path.is_symlink(), f"generated symlink: {path}")
        relative = path.relative_to(root).as_posix().encode("utf-8")
        digest.update(relative)
        digest.update(b"\0")
        with path.open("rb") as source:
            for chunk in iter(lambda: source.read(1024 * 1024), b""):
                byte_count += len(chunk)
                digest.update(chunk)
        digest.update(b"\0")
    return len(files), byte_count, digest.hexdigest()


def file_set_sha256(root: Path, relative_paths: list[str]) -> str:
    digest = hashlib.sha256()
    for relative in sorted(relative_paths):
        path = safe_path(root, relative)
        require(path.is_file() and not path.is_symlink(), f"evidence file absent: {relative}")
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
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
    require(tools == HOST_TOOLS, "host_tools")
    build_context = document.get("build_context")
    require(isinstance(build_context, dict) and
            build_context.get("preset") == "linux-amd64-release", "build_context.preset")
    require(build_context.get("cache_overrides") == {
        "CMAKE_C_COMPILER": "/usr/bin/clang",
        "CMAKE_CXX_COMPILER": "/usr/bin/clang++",
        "CMAKE_CXX_FLAGS": "-march=x86-64-v3 -I${SIMDE_OVERLAY_ROOT}",
    }, "build_context.cache_overrides")

    overlay = document.get("build_overlay")
    require(isinstance(overlay, dict), "build_overlay")
    require(overlay.get("source_commit") == OVERLAY_COMMIT, "build_overlay.source_commit")
    require(overlay.get("tree_sha1") == OVERLAY_TREE, "build_overlay.tree_sha1")
    require(overlay.get("x86_tree_sha1") == OVERLAY_X86_TREE,
            "build_overlay.x86_tree_sha1")
    require(overlay.get("archive_sha256") == OVERLAY_ARCHIVE_SHA256,
            "build_overlay.archive_sha256")
    require(overlay.get("required_header") == "simde/x86/avx.h",
            "build_overlay.required_header")
    require(overlay.get("scope") == "oracle-build-only" and
            overlay.get("product_role") == "none", "build_overlay boundary")

    boundary = document.get("boundary_correction")
    require(isinstance(boundary, dict), "boundary_correction")
    require(boundary.get("transformer_sha256") == BOUNDARY_TRANSFORMER_SHA256,
            "boundary_correction.transformer_sha256")
    require(boundary.get("patched_configuration_sha256") == PATCHED_CONFIG_SHA256,
            "boundary_correction.patched_configuration_sha256")
    require(boundary.get("removed_function_starts") == FALSE_STARTS,
            "boundary_correction.removed_function_starts")
    transformer = safe_path(artifact_root, boundary.get("transformer", ""))
    require(transformer.is_file() and sha256(transformer) == BOUNDARY_TRANSFORMER_SHA256,
            "boundary transformer identity")
    require(boundary.get("verification_scripts") == VERIFICATION_SCRIPTS,
            "boundary_correction.verification_scripts")
    require(boundary.get("verification_set_sha256") == VERIFICATION_SET_SHA256,
            "boundary_correction.verification_set_sha256")
    require(file_set_sha256(artifact_root, VERIFICATION_SCRIPTS) == VERIFICATION_SET_SHA256,
            "boundary verification set identity")
    host_patch = safe_path(artifact_root, boundary.get("host_source_patch", ""))
    require(boundary.get("host_source_patch_sha256") == HOST_SOURCE_PATCH_SHA256 and
            host_patch.is_file() and sha256(host_patch) == HOST_SOURCE_PATCH_SHA256,
            "boundary host source patch identity")
    require(boundary.get("patched_host_source_sha256") == PATCHED_HOST_SOURCE_SHA256,
            "boundary patched host source identity")

    codegen = document.get("codegen")
    require(isinstance(codegen, dict) and codegen.get("status") == "generated",
            "codegen.status")
    require(codegen.get("generated_file_count") == GENERATED_FILE_COUNT,
            "codegen.generated_file_count")
    require(codegen.get("generated_bytes") == GENERATED_BYTES,
            "codegen.generated_bytes")
    require(codegen.get("generated_tree_sha256") == GENERATED_TREE_SHA256,
            "codegen.generated_tree_sha256")
    require(codegen.get("generated_output_is_product") is False,
            "codegen.generated_output_is_product")

    runtime = document.get("runtime_build")
    require(isinstance(runtime, dict) and runtime.get("status") == "built",
            "runtime_build.status")
    require(runtime.get("format") == "ELF64 little-endian PIE x86-64",
            "runtime_build.format")
    require(runtime.get("binary_size") == RUNTIME_BINARY_SIZE and
            runtime.get("binary_sha256") == RUNTIME_BINARY_SHA256,
            "runtime_build binary identity")
    require(runtime.get("product_role") == "none", "runtime_build product boundary")
    smoke = runtime.get("smoke")
    require(isinstance(smoke, dict) and smoke == {
        "audio_driver": "dummy",
        "vulkan_device": "NVIDIA RTX PRO 4000 Blackwell",
        "swapchain": "1280x720",
        "frontier_source": "0x8234530C",
        "frontier_target": "0x8234524C",
        "result": "unresolved-branch",
        "gate_evidence": False,
    }, "runtime_build.smoke")

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


def validate_patched_oracle_root(document: dict, root: Path) -> None:
    require(root.is_dir(), "patched oracle root absent")
    require(git(root, "rev-parse", "HEAD") == ORACLE_COMMIT, "patched oracle HEAD")
    branch = git(root, "symbolic-ref", "-q", "--short", "HEAD", allow_failure=True)
    require(branch == "", "patched oracle worktree is attached to a branch")
    changes = {line.strip() for line in
               git(root, "status", "--porcelain=v1", "--untracked-files=all").splitlines()}
    require(changes == {"M ac6recomp_config.toml", "M src/d3d_hooks.cpp"},
            "patched oracle has unexpected changes")
    require(git(root, "diff", "--check") == "", "patched oracle whitespace")
    require(git(root, "diff", "--numstat", "--", "ac6recomp_config.toml",
                "src/d3d_hooks.cpp") ==
            "0\t53\tac6recomp_config.toml\n0\t20\tsrc/d3d_hooks.cpp",
            "patched oracle change count")
    config = root / document["configuration"]["path"]
    require(config.is_file() and sha256(config) == PATCHED_CONFIG_SHA256,
            "patched oracle configuration")
    host_source = root / "src/d3d_hooks.cpp"
    require(host_source.is_file() and sha256(host_source) == PATCHED_HOST_SOURCE_SHA256,
            "patched oracle host source")


def validate_overlay_archive(document: dict, archive_path: Path) -> None:
    require(archive_path.is_file(), "overlay archive absent")
    require(sha256(archive_path) == OVERLAY_ARCHIVE_SHA256, "overlay archive identity")
    try:
        with tarfile.open(archive_path, "r:*") as archive:
            names = set(archive.getnames())
    except tarfile.TarError as error:
        raise ManifestError(f"overlay archive unreadable: {error}") from error
    require(document["build_overlay"]["required_header"] in names,
            "overlay required header absent")


def validate_generated_root(document: dict, root: Path) -> None:
    count, byte_count, digest = tree_sha256(root)
    require(count == GENERATED_FILE_COUNT, "generated file count")
    require(byte_count == GENERATED_BYTES, "generated byte count")
    require(digest == GENERATED_TREE_SHA256, "generated tree identity")


def validate_runtime_binary(path: Path) -> None:
    require(path.is_file() and path.stat().st_size == RUNTIME_BINARY_SIZE,
            "runtime binary size")
    require(sha256(path) == RUNTIME_BINARY_SHA256, "runtime binary identity")
    with path.open("rb") as source:
        header = source.read(20)
    require(header[:6] == b"\x7fELF\x02\x01" and header[18:20] == b"\x3e\x00",
            "runtime binary ELF identity")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--artifact-root", type=Path, default=Path("."))
    parser.add_argument("--oracle-root", type=Path)
    parser.add_argument("--patched-oracle-root", type=Path)
    parser.add_argument("--xex", type=Path)
    parser.add_argument("--overlay-archive", type=Path)
    parser.add_argument("--generated-root", type=Path)
    parser.add_argument("--runtime-binary", type=Path)
    args = parser.parse_args()
    try:
        document = json.loads(args.manifest.read_text(encoding="utf-8"))
        probe_count = validate_document(document, args.artifact_root)
        if args.oracle_root is not None:
            validate_oracle_root(document, args.oracle_root)
        if args.patched_oracle_root is not None:
            validate_patched_oracle_root(document, args.patched_oracle_root)
        if args.xex is not None:
            require(args.xex.is_file(), "XEX absent")
            require(args.xex.stat().st_size == XEX_SIZE and sha256(args.xex) == XEX_SHA256,
                    "XEX identity")
        if args.overlay_archive is not None:
            validate_overlay_archive(document, args.overlay_archive)
        if args.generated_root is not None:
            validate_generated_root(document, args.generated_root)
        if args.runtime_binary is not None:
            validate_runtime_binary(args.runtime_binary)
    except (OSError, json.JSONDecodeError, ManifestError) as error:
        print(f"oracle_manifest=fail reason={error}")
        return 1
    print(f"oracle_manifest=pass probes={probe_count} "
          f"oracle_root={'checked' if args.oracle_root else 'not-requested'} "
          f"patched_oracle={'checked' if args.patched_oracle_root else 'not-requested'} "
          f"xex={'checked' if args.xex else 'not-requested'} "
          f"overlay={'checked' if args.overlay_archive else 'not-requested'} "
          f"generated={'checked' if args.generated_root else 'not-requested'} "
          f"runtime={'checked' if args.runtime_binary else 'not-requested'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
