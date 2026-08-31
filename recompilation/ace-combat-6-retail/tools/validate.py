#!/usr/bin/env python3
"""Run focused POSIX/profile/replay and Vulkan-only static validation."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path


PRODUCT = Path(__file__).resolve().parents[1]
RUNTIMES = ("rexglue-oracle", "native")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def checked(command: list[str], **kwargs: object) -> subprocess.CompletedProcess[str]:
    print("+", " ".join(command), flush=True)
    return subprocess.run(command, text=True, check=True, **kwargs)


def forbidden_d3d12_symbols(symbols: str) -> list[str]:
    """Return symbol lines naming the forbidden D3D12 backend."""
    # Match backend names in the symbol text, not hexadecimal addresses.  An
    # address such as 0x00d3d120 contains the byte sequence "d3d12" and must
    # not make a Vulkan-only build fail validation.
    return re.findall(r"(?im)^.*\b(?:D3D12|Direct3D 12)\b.*$", symbols)


def campaign_validation_command(
    target: str, require_release: bool, runtime: str = "rexglue-oracle"
) -> list[str] | None:
    if target != "ntsc-uj":
        return None
    command = [
        sys.executable, str(PRODUCT / "tools/validate_campaign.py"),
        "--target", target,
    ]
    if runtime == "native":
        command.extend(("--runtime", "native"))
    if require_release:
        command.append("--require-release")
    return command


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", required=True, choices=("ntsc-uj", "pal"))
    parser.add_argument(
        "--runtime", choices=RUNTIMES, default="rexglue-oracle",
        help="validate the temporary oracle or standalone native renderer",
    )
    parser.add_argument(
        "--require-release", action="store_true",
        help="require the NTSC-U/J 15-mission campaign release contract",
    )
    arguments = parser.parse_args()
    if arguments.require_release and arguments.target != "ntsc-uj":
        parser.error("--require-release is available only for ntsc-uj")
    if arguments.runtime == "native" and arguments.target != "ntsc-uj":
        parser.error("native runtime is currently qualified only for ntsc-uj")
    root = PRODUCT / "build" / arguments.target
    if arguments.runtime == "native":
        root = root / "native"
    if arguments.runtime == "native":
        manifest_path = root / "manifest.json"
        build_receipt_path = root / "build-receipt.json"
        if not manifest_path.is_file() or not build_receipt_path.is_file():
            parser.error("native Gate 1 build receipt is missing")
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        build_receipt = json.loads(build_receipt_path.read_text(encoding="utf-8"))
        guest_linked = isinstance(build_receipt.get("guest_codegen"), str)
        expected_status = "gate2-codegen-linked" if guest_linked else "gate1-built"
        expected_ctest = "9/9" if guest_linked else "8/8"
        if (
            manifest.get("profile") != "native"
            or build_receipt.get("profile") != "native"
            or build_receipt.get("status") != expected_status
            or build_receipt.get("ctest") != expected_ctest
            or build_receipt.get("manifest") != manifest
        ):
            parser.error("native runtime requires a native preparation/build profile")
        native_binary = Path(build_receipt.get("binary", ""))
        if not native_binary.is_file():
            parser.error("native ac6recomp binary is missing")
        checked([str(native_binary), "--self-test"])
        test_binaries = build_receipt.get("test_binaries")
        expected_tests = 9 if guest_linked else 8
        if not isinstance(test_binaries, list) or len(test_binaries) != expected_tests:
            parser.error("native test binary manifest is incomplete")
        test_paths = [Path(value) for value in test_binaries if isinstance(value, str)]
        if len(test_paths) != expected_tests or any(not path.is_file() for path in test_paths):
            parser.error("native test binary is missing")
        # The receipt's `binary` is now the product CLI, not the first test
        # binary. Execute every listed contract test explicitly; CTest remains
        # the aggregate proof, while this loop keeps the validator fail-closed
        # if a test is omitted from the CTest graph.
        for test_path in test_paths:
            checked([str(test_path)])
        install_prefix = Path(build_receipt.get("install_prefix", ""))
        if not install_prefix.is_dir():
            parser.error("native installation prefix is missing")
        install_audit = root / "native-install-audit.json"
        checked([
            sys.executable, str(PRODUCT / "tools/audit_native_release.py"),
            "--prefix", str(install_prefix), "--receipt", str(install_audit),
        ])
        symbols = checked(
            ["nm", "-C", str(native_binary)], stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        ).stdout
        forbidden = re.findall(
            r"(?im)^.*\b(?:ReXGlue|Xenia|XenonRecomp|XenosRecomp|D3D12)\b.*$",
            symbols,
        )
        if forbidden:
            raise RuntimeError(f"native renderer links forbidden symbol: {forbidden[0]}")
        fixture = PRODUCT / "native/fixtures/xenos-capsule-minimal.json"
        checked([sys.executable, str(PRODUCT / "tools/xenos_capsule.py"), str(fixture)])
        if arguments.require_release:
            raise RuntimeError(
                "native full release gate is still open: campaign, save and runtime are not qualified"
            )
        receipt = {
            "schema": "ac6.retail-native-validation.v1",
            "status": "pass",
            "target": arguments.target,
            "runtime": "native",
            "manifest": manifest,
            "test_binary": str(native_binary.resolve()),
            "service_test_binaries": [str(path.resolve()) for path in test_paths],
            "capsule": str(fixture.resolve()),
            "install_audit": str(install_audit.resolve()),
            "interface": "ac6recomp <ISO|assets/>",
            "rexglue_dependency": False,
            "forbidden_symbols": 0,
            "release_ready": False,
        }
        (root / "static-validation.json").write_text(
            json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        print(root / "static-validation.json")
        return 0

    build = root / "cmake"
    binary = build / "ac6recomp"
    manifest_path = root / "manifest.json"
    build_receipt_path = root / "build-receipt.json"
    if not binary.is_file() or not manifest_path.is_file() or not build_receipt_path.is_file():
        parser.error("built manifest is missing")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    build_receipt = json.loads(build_receipt_path.read_text(encoding="utf-8"))
    if manifest.get("profile", "rexglue-oracle") != "rexglue-oracle":
        parser.error("oracle runtime requires the rexglue-oracle preparation/build profile")
    if (
        build_receipt.get("status") != "built-unvalidated"
        or build_receipt.get("target") != arguments.target
        or build_receipt.get("manifest") != manifest
        or Path(build_receipt.get("binary", "")) != binary.resolve()
    ):
        parser.error("build receipt does not match the exact manifest/binary")
    hook_map = manifest.get("hook_map", {})
    hook_map_path = PRODUCT / hook_map.get("path", "")
    if (
        hook_map.get("schema") != "ac6.retail-hook-map.v1"
        or not hook_map_path.is_file()
        or sha256(hook_map_path) != hook_map.get("sha256")
    ):
        parser.error("hook-map identity mismatch")
    config = root / "source/ac6recomp_config.toml"
    if (
        not config.is_file()
        or sha256(config) != manifest.get("generation", {}).get("configuration", {}).get("sha256")
    ):
        parser.error("recompilation configuration identity mismatch")

    names = [
        "ac6_posix_auto_reset_event_test",
        "ac6_posix_socket_ioctl_test",
        "unit\\.(profile alias:.*|atomic write:.*|write marker:.*|mount check:.*)",
    ]
    if arguments.target == "ntsc-uj":
        names.append("ac6_controller_input_replay_test")
    expression = "^(" + "|".join(names) + ")$"
    checked([
        "ctest", "--test-dir", str(build), "--output-on-failure", "-R", expression
    ])

    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8", errors="replace")
    if (
        re.search(r"(?m)^REXGLUE_USE_D3D12:[^=]+=OFF$", cache) is None
        or re.search(r"(?m)^REXGLUE_USE_VULKAN:[^=]+=ON$", cache) is None
        or re.search(r"(?m)^SDL_DUMMYAUDIO:[^=]+=ON$", cache) is None
    ):
        raise RuntimeError("Vulkan-only/headless-audio CMake linkage contract failed")
    symbols = checked(
        ["nm", "-C", str(binary)], stdout=subprocess.PIPE, stderr=subprocess.PIPE
    ).stdout
    if "VulkanGraphicsSystem" not in symbols or "VulkanCommandProcessor" not in symbols:
        raise RuntimeError("full ReXGlue Vulkan backend is not linked")
    if arguments.target == "ntsc-uj":
        markers = (
            b"type28=", b"selector44=", b"state40=", b"[ac6-campaign-transition]",
            b"ac6_log_world_submission_owner", b"[ac6-us-mode-owner]",
            b"[ac6-us-world-owner]", b"resolve_first_lr=", b"resolve_last_dest=",
            b"[ac6-us-campaign-service]",
            b"frontbuffer_pa=",
            b"[ac6-current-level]", b"[ac6-current-level-set]",
            b"[ac6-save-manager]", b"[ac6-post-mission]",
            b"[ac6-frontier-frame]", b"[ac6-frontier-pass]",
            b"[ac6-frontier-resolve]",
            b"[ac6-postprocess-draw]", b"[ac6-postprocess-resolve]",
            b"[ac6-postprocess-content]", b"[ac6-postprocess-decoded]",
            b"[ac6-d5b4-final-white]",
            b"[ac6-d5b4-depth-stencil-bypass]",
            b"[ac6-d5b4-cull-bypass]",
        )
        contents = binary.read_bytes()
        missing = [item.decode("ascii") for item in markers if item not in contents]
        if missing:
            raise RuntimeError("qualified NTSC-U/J wrappers are not linked: " + ", ".join(missing))
    forbidden = forbidden_d3d12_symbols(symbols)
    if forbidden:
        raise RuntimeError(f"D3D12 symbols linked: {forbidden[0]}")

    campaign_command = campaign_validation_command(
        arguments.target, arguments.require_release, arguments.runtime)
    campaign_validation = None
    if campaign_command is not None:
        campaign_validation = checked(
            campaign_command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT
        ).stdout.strip()

    prefix = PRODUCT / "install" / arguments.target
    if prefix.exists():
        shutil.rmtree(prefix)
    checked(["cmake", "--install", str(build), "--prefix", str(prefix)])
    if (prefix / "bin/bin").exists():
        raise RuntimeError("nested bin/bin installation")
    installed = prefix / "bin/ac6recomp"
    if not installed.is_file():
        raise RuntimeError("installed ac6recomp is missing")

    receipt = {
        "schema": "ac6.retail-static-validation.v1",
        "status": "pass",
        "target": arguments.target,
        "manifest": manifest,
        "binary": {"path": str(installed.resolve()), "size": installed.stat().st_size,
                   "sha256": sha256(installed)},
        "tests": expression,
        "graphics": "full-ReXGlue-Vulkan",
        "headless_audio": "SDL-dummy",
        "d3d12_symbols": 0,
        "bin_bin_absent": True,
        "campaign_validation": campaign_validation,
    }
    (root / "static-validation.json").write_text(
        json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(root / "static-validation.json")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"validate: {error}", file=sys.stderr)
        raise SystemExit(2)
