#!/usr/bin/env python3
"""Qualify retail media and materialize an ignored AC6_recomp work tree."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any


PRODUCT = Path(__file__).resolve().parents[1]
WORKSPACE = PRODUCT.parents[1]
UPSTREAM = PRODUCT / "upstream/AC6_recomp"
UPSTREAM_COMMIT = "09144bb092ad871584808aeead69c395edbd5200"
UPSTREAM_REPOSITORY = "https://github.com/sal063/AC6_recomp.git"
SDK_VERSION = "0.8.0"
SDK_TREE = "abb22fd981596dae441af88eb25b43bbe27a0c8c"
SCHEMA = "ac6.retail-target.v1"
PROFILES = ("rexglue-oracle", "native")
LEGACY_REPLAY_PATCH = (
    WORKSPACE
    / "analysis/oracle/ac6-recomp-ab90b-us/patches/poll-exact-xam-controller-replay-v4.patch"
)
LEGACY_POSIX_PATCH = (
    WORKSPACE
    / "analysis/oracle/ac6-recomp-ab90b-us/patches/linux-vulkan-minimal-v1.patch"
)


class PreparationError(RuntimeError):
    pass


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise PreparationError(f"JSON object required: {path}")
    return value


def git(*arguments: str) -> str:
    completed = subprocess.run(
        ["git", "-C", str(UPSTREAM), *arguments],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode:
        raise PreparationError(completed.stderr.strip() or "git failed")
    return completed.stdout.strip()


def qualify_file(path: Path, expected: dict[str, Any], role: str) -> dict[str, Any]:
    resolved = path.resolve()
    if not resolved.is_file():
        raise PreparationError(f"{role} is not a file: {resolved}")
    size = resolved.stat().st_size
    if size != expected["size"]:
        raise PreparationError(f"{role} size mismatch: {size} != {expected['size']}")
    digest = file_sha256(resolved)
    if digest != expected["sha256"]:
        raise PreparationError(f"{role} SHA-256 mismatch: {digest}")
    return {"source_path": str(resolved), "size": size, "sha256": digest}


def validate_hook_map_identity(
    hook_map: dict[str, Any], definition: dict[str, Any],
    qualified_xex: dict[str, Any], target: str,
) -> None:
    expected_ghidra = definition["ghidra"]
    if (
        hook_map.get("schema") != "ac6.retail-hook-map.v1"
        or hook_map.get("target") != target
        or hook_map.get("xex", {}).get("sha256") != qualified_xex["sha256"]
        or hook_map.get("xex", {}).get("module") != definition["xex"]["module"]
        or hook_map.get("ghidra", {}).get("project") != expected_ghidra["project"]
        or hook_map.get("ghidra", {}).get("program") != expected_ghidra["program"]
        or hook_map.get("ghidra", {}).get("language") != expected_ghidra["language"]
    ):
        raise PreparationError("target hook-map identity mismatch")


def qualify_hook_map(
    definition: dict[str, Any], qualified_xex: dict[str, Any], target: str
) -> dict[str, Any]:
    reference = definition.get("hook_map")
    if not isinstance(reference, dict):
        raise PreparationError("target hook-map reference is missing")
    relative = reference.get("path")
    if not isinstance(relative, str):
        raise PreparationError("target hook-map path is missing")
    path = PRODUCT / relative
    if not path.is_file() or file_sha256(path) != reference.get("sha256"):
        raise PreparationError("target hook-map digest mismatch")
    hook_map = load_json(path)
    validate_hook_map_identity(hook_map, definition, qualified_xex, target)
    return {
        "schema": hook_map["schema"],
        "path": relative,
        "sha256": reference["sha256"],
    }


def apply_patch(source: Path, patch: Path, includes: list[str], excludes: list[str]) -> None:
    command = ["git", "apply", "--whitespace=nowarn"]
    command.extend(f"--exclude={item}" for item in excludes)
    command.extend(f"--include={item}" for item in includes)
    command.append(str(patch))
    environment = dict(os.environ)
    # The ignored working copy lives below the portfolio Git repository. Stop
    # git-apply from discovering that parent, otherwise patch paths are matched
    # against the portfolio root and every filtered patch is silently skipped.
    environment["GIT_CEILING_DIRECTORIES"] = str(source.parent)
    completed = subprocess.run(
        command,
        cwd=source,
        env=environment,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode:
        raise PreparationError(f"overlay patch failed ({patch.name}): {completed.stderr.strip()}")


def replace_once(path: Path, before: str, after: str) -> None:
    text = path.read_text(encoding="utf-8")
    if text.count(before) != 1:
        raise PreparationError(f"overlay anchor count for {path}: {text.count(before)}")
    path.write_text(text.replace(before, after), encoding="utf-8")


def patch_cmake(source: Path, target: str) -> None:
    cmake = source / "CMakeLists.txt"
    source_anchor = "    src/ac6_texture_overrides.cpp\n"
    replacement = "    src/ac6_texture_overrides.cpp\n"
    if target == "ntsc-uj":
        replacement += (
            "    src/ac6_route_sync_ntsc_uj.cpp\n"
            "    src/ac6_controller_input_replay.cpp\n"
            "    src/ac6_controller_input_replay_io.cpp\n"
            "    src/ac6_controller_input_replay_runtime.cpp\n"
        )
    replace_once(cmake, source_anchor, replacement)
    add_executable_anchor = "if(WIN32)\n    add_executable(ac6recomp WIN32 ${AC6RECOMP_SOURCES})\n"
    add_executable_replacement = (
        "if(NOT WIN32)\n"
        "    list(REMOVE_ITEM AC6RECOMP_SOURCES\n"
        "        src/ac6_texture_overrides.cpp\n"
        "        src/ac6_backend_fixes/ac6_fullres_effects.cpp\n"
        "        src/ac6_backend_fixes/ac6_widescreen.cpp)\n"
        "    list(APPEND AC6RECOMP_SOURCES\n"
        "        src/ac6_texture_overrides_linux.cpp\n"
        "        src/ac6_backend_fixes/ac6_fullres_effects_linux.cpp\n"
        "        src/ac6_backend_fixes/ac6_widescreen_linux.cpp)\n"
        "endif()\n\n"
        + add_executable_anchor
    )
    replace_once(cmake, add_executable_anchor, add_executable_replacement)
    tail_anchor = "if(CMAKE_CXX_COMPILER_ID MATCHES \"Clang|GNU\")\n    target_compile_options(ac6recomp PRIVATE\n"
    tests = """
if(AC6_RETAIL_BUILD_TESTS)
    enable_testing()
    find_package(Threads REQUIRED)

    if(TARGET unit_tests AND UNIX)
        target_link_libraries(unit_tests PRIVATE
            "$<LINK_GROUP:RESCAN,rex::graphics,rex::kernel,rex::audio,rex::system>")
    endif()

    add_executable(ac6_posix_auto_reset_event_test
        tests/ac6_posix_auto_reset_event_test.cpp)
    target_link_libraries(ac6_posix_auto_reset_event_test PRIVATE rex::core Threads::Threads)
    target_compile_options(ac6_posix_auto_reset_event_test PRIVATE -UNDEBUG)
    add_test(NAME ac6_posix_auto_reset_event_test COMMAND ac6_posix_auto_reset_event_test)

    add_executable(ac6_posix_socket_ioctl_test
        tests/ac6_posix_socket_ioctl_test.cpp)
    target_link_libraries(ac6_posix_socket_ioctl_test PRIVATE rex::core)
    add_test(NAME ac6_posix_socket_ioctl_test COMMAND ac6_posix_socket_ioctl_test)
"""
    if target == "ntsc-uj":
        tests += """
    add_executable(ac6_controller_input_replay_test
        tests/ac6_controller_input_replay_test.cpp
        src/ac6_controller_input_replay.cpp
        src/ac6_controller_input_replay_io.cpp)
    target_include_directories(ac6_controller_input_replay_test PRIVATE src)
    target_link_libraries(ac6_controller_input_replay_test PRIVATE rex::core Threads::Threads)
    target_compile_options(ac6_controller_input_replay_test PRIVATE -UNDEBUG)
    add_test(NAME ac6_controller_input_replay_test COMMAND ac6_controller_input_replay_test)
"""
    tests += "endif()\n\ninstall(TARGETS ac6recomp RUNTIME DESTINATION bin)\n\n"
    replace_once(cmake, tail_anchor, tests + tail_anchor)
    replace_once(
        cmake,
        "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n",
        "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n"
        "option(AC6_RETAIL_BUILD_TESTS \"Build focused Linux retail tests\" ON)\n"
        "set(AC6_RETAIL_LINUX ON CACHE INTERNAL \"AC6 retail Linux product\")\n"
        "if(WIN32 OR REXGLUE_USE_D3D12 OR NOT REXGLUE_USE_VULKAN)\n"
        "    message(FATAL_ERROR \"AC6 retail product requires Linux and Vulkan-only ReXGlue\")\n"
        "endif()\n",
    )
    replace_once(
        cmake,
        "if(EXISTS \"${CMAKE_CURRENT_SOURCE_DIR}/generated/rexglue.cmake\")\n",
        "# ReXGlue's x86 byte-swap primitives use _mm_shuffle_epi8 directly.\n"
        "# Clang does not enable that SSSE3 intrinsic without an explicit target flag.\n"
        "if(UNIX AND CMAKE_SYSTEM_PROCESSOR MATCHES \"^(x86_64|AMD64|amd64)$\")\n"
        "    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-mssse3>)\n"
        "endif()\n\n"
        "if(EXISTS \"${CMAKE_CURRENT_SOURCE_DIR}/generated/rexglue.cmake\")\n",
    )
    replace_once(
        source / "thirdparty/rexglue-sdk/thirdparty/CMakeLists.txt",
        "if(AC6_ORACLE_LINUX_MINIMAL)\n"
        "    set(SDL_DUMMYAUDIO ON CACHE BOOL \"\" FORCE)\n",
        "if(AC6_ORACLE_LINUX_MINIMAL OR AC6_RETAIL_LINUX)\n"
        "    set(SDL_DUMMYAUDIO ON CACHE BOOL \"\" FORCE)\n",
    )


def patch_stock_settings(source: Path) -> None:
    replace_once(
        source / "src/main.cpp",
        'rex::cvar::SetSessionDefault("video_mode_width", "1920");\n'
        '    rex::cvar::SetSessionDefault("video_mode_height", "1080");\n'
        '    rex::cvar::SetSessionDefault("resolution", "1080p");\n'
        '    rex::cvar::SetSessionDefault("window_width", "1920");\n'
        '    rex::cvar::SetSessionDefault("window_height", "1080");',
        'rex::cvar::SetSessionDefault("video_mode_width", "1280", "ac6_retail_stock");\n'
        '    rex::cvar::SetSessionDefault("video_mode_height", "720", "ac6_retail_stock");\n'
        '    rex::cvar::SetSessionDefault("resolution", "720p", "ac6_retail_stock");\n'
        '    rex::cvar::SetSessionDefault("window_width", "1280", "ac6_retail_stock");\n'
        '    rex::cvar::SetSessionDefault("window_height", "720", "ac6_retail_stock");\n'
        '    rex::cvar::SetSessionDefault("resolution_scale", "1", "ac6_retail_stock");\n'
        '    rex::cvar::SetSessionDefault("draw_resolution_scale_x", "1", "ac6_retail_stock");\n'
        '    rex::cvar::SetSessionDefault("draw_resolution_scale_y", "1", "ac6_retail_stock");\n'
        '    rex::cvar::SetSessionDefault("async_shader_compilation", "false", "ac6_retail_stock");\n'
        '    rex::cvar::SetSessionDefault("ac6_unlock_fps", "false", "ac6_retail_stock");\n'
        '    rex::cvar::SetSessionDefault("ac6_dynamic_vblank", "false", "ac6_retail_stock");\n'
        '    rex::cvar::SetSessionDefault("ac6_terrain_hd", "false", "ac6_retail_stock");\n'
        '    rex::cvar::SetSessionDefault("ac6_fullres_effects", "false", "ac6_retail_stock");\n'
        '    rex::cvar::SetSessionDefault("ac6_widescreen", "false", "ac6_retail_stock");\n'
        '    rex::cvar::SetSessionDefault("ac6_texture_swaps_enabled", "false", "ac6_retail_stock");',
    )
    replacements = {
        "src/render_hooks.cpp": [
            ("REXCVAR_DEFINE_BOOL(ac6_unlock_fps, true,", "REXCVAR_DEFINE_BOOL(ac6_unlock_fps, false,"),
            ("REXCVAR_DEFINE_BOOL(ac6_dynamic_vblank, true,", "REXCVAR_DEFINE_BOOL(ac6_dynamic_vblank, false,"),
        ],
        "thirdparty/rexglue-sdk/src/graphics/command_processor.cpp": [
            ("REXCVAR_DEFINE_BOOL(async_shader_compilation, true,", "REXCVAR_DEFINE_BOOL(async_shader_compilation, false,"),
        ],
        "thirdparty/rexglue-sdk/src/graphics/flags.cpp": [
            ("REXCVAR_DEFINE_BOOL(ac6_terrain_hd, true,", "REXCVAR_DEFINE_BOOL(ac6_terrain_hd, false,"),
        ],
    }
    for relative, pairs in replacements.items():
        for before, after in pairs:
            replace_once(source / relative, before, after)


def materialize_native(
    target: str, definition: dict[str, Any], xex: dict[str, Any], iso: dict[str, Any]
) -> Path:
    """Materialize only Gate 1 native renderer sources.

    Native preparation deliberately does not copy AC6_recomp, generated C++ or
    retail assets.  It creates a testable renderer tree and records the sealed
    media identities for the later runtime gate.
    """
    target_root = PRODUCT / "build" / target / "native"
    source = target_root / "native-source"
    manifest_path = target_root / "manifest.json"
    if source.exists():
        shutil.rmtree(source)
    target_root.mkdir(parents=True, exist_ok=True)
    shutil.copytree(PRODUCT / "native", source)
    hook_map = qualify_hook_map(definition, xex, target)
    manifest = {
        "schema": SCHEMA,
        "target": target,
        "profile": "native",
        "region": definition["region"],
        "xex": {**definition["xex"], **xex},
        "iso": iso,
        "ghidra": definition["ghidra"],
        "hook_map": hook_map,
        "upstream": {"commit": UPSTREAM_COMMIT, "used": False},
        "generation_sdk": {"name": "none", "generated_sources_committed": False},
        "generation": {
            "compiler": "Clang 21",
            "language": "C++20",
            "configuration": None,
            "generated_sources_committed": False,
        },
        "graphics": {
            "authority": "native Xenos/Vulkan",
            "d3d12": False,
            "vulkan": True,
            "resolution": "1280x720",
            "scale": 1,
            "fps": 30,
            "enhancements": False,
        },
        "capsule": {
            "schema": "ac6.xenos-capsule.v1",
            "fixture": "native/fixtures/xenos-capsule-minimal.json",
        },
    }
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    return manifest_path


def materialize(
    target: str,
    definition: dict[str, Any],
    xex: dict[str, Any],
    iso: dict[str, Any],
    profile: str,
) -> Path:
    if profile == "native":
        return materialize_native(target, definition, xex, iso)
    target_root = PRODUCT / "build" / target
    source = target_root / "source"
    manifest_path = target_root / "manifest.json"
    gameplay_receipt = target_root / "gameplay-validation.json"
    if manifest_path.is_file() and gameplay_receipt.is_file():
        old_manifest = load_json(manifest_path)
        receipt = load_json(gameplay_receipt)
        binary_path = Path(receipt.get("binary", {}).get("path", ""))
        if (
            receipt.get("status") == "pass"
            and receipt.get("manifest") == old_manifest
            and binary_path.is_file()
            and file_sha256(binary_path) == receipt.get("binary", {}).get("sha256")
        ):
            raise PreparationError("exact manifest already has a gameplay-validated binary")

    if source.exists():
        shutil.rmtree(source)
    target_root.mkdir(parents=True, exist_ok=True)
    shutil.copytree(
        UPSTREAM,
        source,
        ignore=shutil.ignore_patterns(".git", "generated", "assets", "out"),
    )

    if target == "ntsc-uj":
        apply_patch(source, LEGACY_REPLAY_PATCH, ["*"], ["CMakeLists.txt"])
    apply_patch(
        source,
        LEGACY_POSIX_PATCH,
        [
            "src/ac6_controller_input_replay_runtime.cpp",
            "tests/ac6_posix_auto_reset_event_test.cpp",
            "tests/ac6_posix_socket_ioctl_test.cpp",
            "thirdparty/rexglue-sdk/src/core/socket_posix.cpp",
            "thirdparty/rexglue-sdk/src/core/threading_posix.cpp",
            "thirdparty/rexglue-sdk/src/native/ui/rex_app.cpp",
            "thirdparty/rexglue-sdk/src/rexglue/CMakeLists.txt",
            "thirdparty/rexglue-sdk/src/system/xsocket.cpp",
            "thirdparty/rexglue-sdk/thirdparty/CMakeLists.txt",
        ],
        [],
    )
    shutil.copy2(PRODUCT / "overlay/ac6_texture_overrides_linux.cpp", source / "src")
    if target == "ntsc-uj":
        shutil.copy2(PRODUCT / "overlay/ac6_route_sync_ntsc_uj.cpp", source / "src")
    shutil.copy2(
        PRODUCT / "overlay/ac6_fullres_effects_linux.cpp",
        source / "src/ac6_backend_fixes",
    )
    shutil.copy2(
        PRODUCT / "overlay/ac6_widescreen_linux.cpp",
        source / "src/ac6_backend_fixes",
    )
    patch_cmake(source, target)
    patch_stock_settings(source)
    assets = source / "assets"
    assets.mkdir()
    shutil.copy2(Path(xex["source_path"]), assets / "default.xex")

    hook_map = qualify_hook_map(definition, xex, target)
    configuration = definition["configuration"]
    config_path = source / "ac6recomp_config.toml"
    if file_sha256(config_path) != configuration["sha256"]:
        raise PreparationError("recompilation configuration digest mismatch")
    manifest = {
        "schema": SCHEMA,
        "target": target,
        "profile": "rexglue-oracle",
        "region": definition["region"],
        "xex": {**definition["xex"], **xex},
        "iso": iso,
        "ghidra": definition["ghidra"],
        "hook_map": hook_map,
        "upstream": {
            "repository": UPSTREAM_REPOSITORY,
            "commit": UPSTREAM_COMMIT,
            "tree": git("rev-parse", "HEAD^{tree}"),
            "license": "BSD-3-Clause",
        },
        "generation_sdk": {
            "name": "ReXGlue",
            "version": SDK_VERSION,
            "tree": SDK_TREE,
        },
        "generation": {
            "compiler": "Clang 21",
            "language": "C++23",
            "configuration": definition["configuration"],
            "generated_sources_committed": False,
        },
        "graphics": {
            "authority": "ReXGlue Xenos/Vulkan",
            "d3d12": False,
            "vulkan": True,
            "resolution": "1280x720",
            "scale": 1,
            "fps": 30,
            "enhancements": False,
        },
    }
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return manifest_path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", required=True, choices=("ntsc-uj", "pal"))
    parser.add_argument("--xex", required=True, type=Path)
    parser.add_argument("--iso", required=True, type=Path)
    parser.add_argument(
        "--profile", choices=PROFILES, default="rexglue-oracle",
        help="materialize the temporary ReXGlue oracle or Gate 1 native sources",
    )
    arguments = parser.parse_args()
    try:
        definition = load_json(PRODUCT / "targets" / f"{arguments.target}.json")
        if definition.get("qualification_state") != "ready":
            raise PreparationError(str(definition.get("qualification_state")))
        if arguments.profile == "native" and arguments.target != "ntsc-uj":
            raise PreparationError("native profile is currently qualified only for ntsc-uj")
        if arguments.profile == "rexglue-oracle":
            if git("rev-parse", "HEAD") != UPSTREAM_COMMIT:
                raise PreparationError("AC6_recomp submodule commit mismatch")
            if git("status", "--porcelain"):
                raise PreparationError("AC6_recomp submodule must be clean")
        xex = qualify_file(arguments.xex, definition["xex"], "XEX")
        iso = qualify_file(arguments.iso, definition["iso"], "ISO")
        result = materialize(arguments.target, definition, xex, iso, arguments.profile)
        print(result)
        return 0
    except (PreparationError, OSError, json.JSONDecodeError) as error:
        print(f"prepare: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
