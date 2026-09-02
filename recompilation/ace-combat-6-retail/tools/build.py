#!/usr/bin/env python3
"""Run the single code-generation and Linux/Vulkan build for a manifest."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from pathlib import Path


PRODUCT = Path(__file__).resolve().parents[1]
UPSTREAM_COMMIT = "09144bb092ad871584808aeead69c395edbd5200"
SDK_VERSION = "0.8.0"
SDK_TREE = "abb22fd981596dae441af88eb25b43bbe27a0c8c"
PROFILES = ("rexglue-oracle", "native")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def verify_manifest(
    target: str,
    source: Path,
    manifest: dict[str, object],
    requested_profile: str = "rexglue-oracle",
) -> None:
    definition = json.loads((PRODUCT / "targets" / f"{target}.json").read_text())
    profile = manifest.get("profile", "rexglue-oracle")
    if profile != requested_profile:
        raise RuntimeError(
            f"prepared profile {profile!r} does not match requested profile {requested_profile!r}"
        )
    if profile == "native":
        if manifest.get("schema") != "ac6.retail-target.v1" or manifest.get("target") != target:
            raise RuntimeError("native manifest identity mismatch")
        if manifest.get("graphics", {}).get("authority") != "native Xenos/Vulkan":
            raise RuntimeError("native renderer authority mismatch")
        if not source.is_dir() or not (source / "CMakeLists.txt").is_file():
            raise RuntimeError("native renderer source tree is missing")
        return
    if profile != "rexglue-oracle":
        raise RuntimeError(f"unknown build profile: {profile}")
    if (
        manifest.get("schema") != "ac6.retail-target.v1"
        or manifest.get("target") != target
        or manifest.get("upstream", {}).get("commit") != UPSTREAM_COMMIT
        or manifest.get("generation_sdk", {}).get("version") != SDK_VERSION
        or manifest.get("generation_sdk", {}).get("tree") != SDK_TREE
        or manifest.get("xex", {}).get("sha256") != definition["xex"]["sha256"]
        or manifest.get("iso", {}).get("sha256") != definition["iso"]["sha256"]
        or manifest.get("ghidra") != definition["ghidra"]
        or manifest.get("generation", {}).get("configuration")
        != definition["configuration"]
        or manifest.get("hook_map")
        != {"schema": "ac6.retail-hook-map.v1", **definition["hook_map"]}
    ):
        raise RuntimeError("prepared manifest identity mismatch")
    hook_map = PRODUCT / definition["hook_map"]["path"]
    config = source / "ac6recomp_config.toml"
    xex = source / "assets/default.xex"
    iso = Path(manifest["iso"]["source_path"])
    checks = (
        (hook_map, definition["hook_map"]["sha256"], "hook map"),
        (config, definition["configuration"]["sha256"], "configuration"),
        (xex, definition["xex"]["sha256"], "XEX"),
        (iso, definition["iso"]["sha256"], "ISO"),
    )
    for path, expected, role in checks:
        if not path.is_file() or sha256(path) != expected:
            raise RuntimeError(f"{role} identity mismatch")
    if target == "ntsc-uj" and not (source / "src/ac6_route_sync_ntsc_uj.cpp").is_file():
        raise RuntimeError("qualified NTSC-U/J route-sync overlay is missing")


def run(command: list[str], cwd: Path | None = None) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=cwd, check=True)


def find_passing_native_codegen(root: Path) -> tuple[Path, Path] | None:
    """Return the newest passing ignored guest output and its import stubs."""
    candidates = []
    for receipt_path in root.glob("codegen-*/codegen-receipt.json"):
        try:
            receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        if receipt.get("status") != "pass" or receipt.get("xex_sha256") is None:
            continue
        generated = receipt_path.parent / "generated"
        if generated.is_dir() and (generated / "ppc_recomp_shared.h").is_file():
            candidates.append((receipt_path.stat().st_mtime, generated))
    if not candidates:
        return None
    generated = max(candidates, key=lambda item: item[0])[1]
    stubs = generated.parent / "native-import-stubs.cpp"
    run([sys.executable, str(PRODUCT / "tools/materialize_native_import_stubs.py"),
         "--mapping", str(generated / "ppc_recomp_shared.h"),
         "--output", str(stubs)])
    return generated, stubs


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", required=True, choices=("ntsc-uj", "pal"))
    parser.add_argument(
        "--profile", choices=PROFILES, default="rexglue-oracle",
        help="build the temporary ReXGlue oracle or the standalone native renderer",
    )
    arguments = parser.parse_args()
    if arguments.profile == "native" and arguments.target != "ntsc-uj":
        parser.error("native profile is currently qualified only for ntsc-uj")
    root = PRODUCT / "build" / arguments.target
    if arguments.profile == "native":
        root = root / "native"
    source = root / ("native-source" if arguments.profile == "native" else "source")
    manifest_path = root / "manifest.json"
    gameplay_path = root / "gameplay-validation.json"
    if not source.is_dir() or not manifest_path.is_file():
        parser.error("run prepare.py first")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    verify_manifest(arguments.target, source, manifest, arguments.profile)
    if arguments.profile == "rexglue-oracle" and gameplay_path.is_file():
        gameplay = json.loads(gameplay_path.read_text(encoding="utf-8"))
        if gameplay.get("status") == "pass" and gameplay.get("manifest") == manifest:
            parser.error("exact manifest already has a gameplay-validated binary")

    if arguments.profile == "native":
        build = root / "native-cmake"
        configure = ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
                     "-DCMAKE_BUILD_TYPE=Release"]
        guest = find_passing_native_codegen(root)
        if guest is not None:
            generated, stubs = guest
            portfolio_root = PRODUCT.parents[3]
            configure.extend([
                "-DCMAKE_CXX_COMPILER=/usr/bin/clang++-21",
                f"-DAC6_NATIVE_GUEST_DIR={generated}",
                f"-DAC6_NATIVE_SIMDE_DIR={portfolio_root / '.tools/xenonrecomp-source/XenonRecomp/thirdparty/simde'}",
                f"-DAC6_NATIVE_IMPORT_STUBS={stubs}",
            ])
        run(configure)
        guest_targets = ["ac6_native_guest_link_test"] if guest is not None else []
        run(["cmake", "--build", str(build), "--target", "ac6recomp",
             "ac6_native_xenos_tests",
             "ac6_native_services_tests", "ac6_native_ppc_abi_tests", *guest_targets, "-j2"])
        run(["cmake", "--build", str(build), "--target", "ac6_native_frontend_tests",
             "ac6_native_runtime_tests", "ac6_native_xex_tests",
             "ac6_native_xdvdfs_tests", "ac6_native_guest_memory_tests",
             "ac6_native_guest_input_tests",
             *guest_targets, "-j2"])
        run(["ctest", "--test-dir", str(build), "--output-on-failure"])
        install_prefix = root / "native-install"
        run(["cmake", "--install", str(build), "--prefix", str(install_prefix)])
        test_binaries = [
            str((build / "ac6_native_xenos_tests").resolve()),
            str((build / "ac6_native_services_tests").resolve()),
            str((build / "ac6_native_ppc_abi_tests").resolve()),
            str((build / "ac6_native_frontend_tests").resolve()),
            str((build / "ac6_native_runtime_tests").resolve()),
            str((build / "ac6_native_xex_tests").resolve()),
            str((build / "ac6_native_xdvdfs_tests").resolve()),
            str((build / "ac6_native_guest_memory_tests").resolve()),
            str((build / "ac6_native_guest_input_tests").resolve()),
        ]
        if guest is not None:
            test_binaries.append(str((build / "ac6_native_guest_link_test").resolve()))
        receipt = {
            "schema": "ac6.retail-build.v1",
            "target": arguments.target,
            "profile": "native",
            "manifest": manifest,
            "build_directory": str(build.resolve()),
            "binary": str((build / "ac6recomp").resolve()),
            "test_binaries": test_binaries,
            "compiler": "CMake native renderer",
            "status": "gate2-codegen-linked" if guest is not None else "gate1-built",
            "ctest": "10/10" if guest is not None else "9/9",
            "guest_codegen": str(guest[0].parent.resolve()) if guest is not None else None,
            "guest_link_test": str((build / "ac6_native_guest_link_test").resolve()) if guest is not None else None,
            "install_prefix": str(install_prefix.resolve()),
        }
        (root / "build-receipt.json").write_text(
            json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        return 0

    compiler = Path("/usr/bin/clang++-21")
    c_compiler = Path("/usr/bin/clang-21")
    if not compiler.is_file() or not c_compiler.is_file():
        parser.error("Clang 21 is required")
    version = subprocess.run(
        [str(compiler), "--version"], text=True, stdout=subprocess.PIPE, check=True
    ).stdout
    if re.search(r"clang version 21(?:\.|\s)", version) is None:
        parser.error("compiler is not Clang 21")

    build = root / "cmake"
    configure = [
        "cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_C_COMPILER={c_compiler}",
        f"-DCMAKE_CXX_COMPILER={compiler}",
        "-DREXGLUE_USE_D3D12=OFF",
        "-DREXGLUE_USE_VULKAN=ON",
        "-DREXGLUE_ENABLE_FIDELITYFX=OFF",
        "-DREXGLUE_BUILD_TESTS=ON",
        "-DAC6_RETAIL_BUILD_TESTS=ON",
        "-DAC6_RETAIL_LINUX=ON",
    ]
    run(configure)
    generated_sources = source / "generated/sources.cmake"
    if not generated_sources.is_file():
        if arguments.target == "ntsc-uj":
            parser.error("NTSC-U/J code generation is already consumed; generated sources required")
        run(["cmake", "--build", str(build), "--target", "ac6recomp_codegen", "-j16"])
        if not generated_sources.is_file():
            raise RuntimeError("codegen completed without generated/sources.cmake")
        run(configure)
    else:
        print(f"+ reusing completed codegen: {generated_sources}", flush=True)
    targets = [
        "cmake", "--build", str(build), "--target", "ac6recomp",
        "ac6_posix_auto_reset_event_test", "ac6_posix_socket_ioctl_test",
        "unit_tests", "SDL3_test",
    ]
    if arguments.target == "ntsc-uj":
        targets.append("ac6_controller_input_replay_test")
    targets.append("-j16")
    run(targets)
    receipt = {
        "schema": "ac6.retail-build.v1",
        "target": arguments.target,
        "manifest": manifest,
        "build_directory": str(build.resolve()),
        "binary": str((build / "ac6recomp").resolve()),
        "compiler": version.splitlines()[0],
        "status": "built-unvalidated",
    }
    (root / "build-receipt.json").write_text(
        json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, subprocess.CalledProcessError) as error:
        returncode = getattr(error, "returncode", 2)
        print(f"build: command failed ({returncode})", file=sys.stderr)
        raise SystemExit(returncode)
