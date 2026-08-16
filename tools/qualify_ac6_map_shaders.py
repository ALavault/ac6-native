#!/usr/bin/env python3
"""Qualify AC6 PAL Map shaders offline without retaining proprietary bytes."""

from __future__ import annotations

import csv
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from tools.ac6_fhm import parse_fhm


ARCHIVE_SHA256 = "7294a028408b5008236624cb46f6b108f3ed1c2ffd9961c5317634c47ae36a3c"
XENOSRECOMP_SHA256 = "e71e311c298208b1774f7943171b9f32bc0adb9bb14f1b19889dfcd8ade5b6a4"
DXC_SHA256 = "db50584b967fba011f571a6b63e63ae9d14a04418a52d57ff3600750b1c9940d"
SPIRV_VAL_SHA256 = "2cc19cddc1293518705467f41f55094800b319bd77b1eaf6e30bc7901d6e3406"
XENOSRECOMP_COMMIT = "990d03b28a27b50277ee5d8d942e1c5f873869d1"
XEX_SHA256 = "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8"
DECL_USAGE_NAMES = [
    "Position", "BlendWeight", "BlendIndices", "Normal", "PointSize",
    "TexCoord", "Tangent", "Binormal", "TessFactor", "PositionT", "Color",
    "Fog", "Depth", "Sample",
]


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def swap32(data: bytes) -> bytes:
    if len(data) % 4:
        raise RuntimeError("microcode size is not dword-aligned")
    return b"".join(data[index:index + 4][::-1] for index in range(0, len(data), 4))


def be32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def require_tool(path: Path, expected: str, label: str) -> None:
    if not path.is_file() or not os.access(path, os.X_OK):
        raise RuntimeError(f"{label} is missing or not executable")
    if digest(path) != expected:
        raise RuntimeError(f"{label} identity mismatch")


def run(arguments: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(arguments, text=True, capture_output=True, check=False)


def verify_archive(root: Path) -> None:
    sums = root / "SHA256SUMS"
    if not sums.is_file():
        raise RuntimeError("archive SHA256SUMS is missing")
    for line in sums.read_text().splitlines():
        expected, relative = line.split(maxsplit=1)
        relative = relative.removeprefix("*")
        path = root / relative
        if not path.is_file() or digest(path) != expected:
            raise RuntimeError(f"archive member identity mismatch: {relative}")


def error_class(stderr: str, returncode: int) -> str:
    if returncode < 0:
        return f"signal_{-returncode}"
    if "use of undeclared identifier 'iFog0'" in stderr:
        return "undeclared_iFog0"
    if "redefinition of parameter 'iTexCoord1'" in stderr:
        return "duplicate_iTexCoord1"
    return "tool_error_" + hashlib.sha256(stderr.encode()).hexdigest()[:16]


def parse_fields(output: str) -> dict[str, object]:
    fields: dict[str, object] = {}
    for token in output.split():
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        if value in {"true", "false"}:
            fields[key] = value == "true"
        elif value.isdecimal():
            fields[key] = int(value)
        else:
            fields[key] = value
    return fields


def parse_vertex_elements(container: bytes) -> list[dict[str, object]]:
    if len(container) < 0x24 or container[:4] != b"\x10\x2a\x11\x01":
        raise RuntimeError("vertex ShaderContainer header is invalid")
    shader_offset = be32(container, 0x18)
    if shader_offset + 0x24 > len(container):
        raise RuntimeError("vertex shader reflection header is out of bounds")
    table_skip = be32(container, shader_offset + 0x18)
    count = be32(container, shader_offset + 0x1C)
    table_offset = shader_offset + 0x24 + 4 * table_skip
    if count > 64 or table_offset + 4 * count > len(container):
        raise RuntimeError("vertex element table is out of bounds")
    elements = []
    for index in range(count):
        value = be32(container, table_offset + 4 * index)
        usage = (value >> 12) & 0xF
        if usage >= len(DECL_USAGE_NAMES):
            raise RuntimeError("vertex element usage is unknown")
        elements.append({
            "address": f"0x{value & 0xFFF:X}",
            "usage": DECL_USAGE_NAMES[usage],
            "usage_index": (value >> 16) & 0xF,
        })
    return elements


def semantic_aliases(elements: list[dict[str, object]]) -> list[dict[str, object]]:
    groups: dict[tuple[str, int], list[str]] = {}
    for element in elements:
        key = (str(element["usage"]), int(element["usage_index"]))
        groups.setdefault(key, []).append(str(element["address"]))
    return [
        {"usage": usage, "usage_index": usage_index, "addresses": addresses}
        for (usage, usage_index), addresses in sorted(groups.items())
        if len(addresses) > 1
    ]


def normalize_semantic_alias_parameters(
    hlsl: Path, aliases: list[dict[str, object]],
) -> list[dict[str, object]]:
    if not aliases:
        return []
    lines = hlsl.read_text().splitlines(keepends=True)
    try:
        signature_start = next(index for index, line in enumerate(lines) if line == "void main(\n")
        signature_end = next(
            index for index in range(signature_start + 1, len(lines))
            if lines[index].startswith("\tout ")
        )
    except StopIteration as error:
        raise RuntimeError("XenosRecomp vertex signature is malformed") from error
    removed: set[int] = set()
    normalized = []
    for alias in aliases:
        usage = str(alias["usage"])
        usage_index = int(alias["usage_index"])
        marker = f" i{usage}{usage_index} : {usage.upper()}{usage_index},"
        matches = [
            index for index in range(signature_start + 1, signature_end)
            if marker in lines[index]
        ]
        expected = len(alias["addresses"])
        if len(matches) != expected or len({lines[index] for index in matches}) != 1:
            raise RuntimeError("semantic alias does not map to identical HLSL parameters")
        removed.update(matches[1:])
        normalized.append({**alias, "removed_identical_parameters": expected - 1})
    hlsl.write_text("".join(line for index, line in enumerate(lines) if index not in removed))
    return normalized


def main() -> int:
    if len(sys.argv) != 8:
        raise RuntimeError(
            "usage: qualify_ac6_map_shaders.py ASSET_ROOT ARCHIVE REXGLUE_CLI "
            "XENOSRECOMP DXC SPIRV_VAL OUTPUT"
        )
    asset_root, archive, rexglue_cli, xenosrecomp, dxc, spirv_val, output = (
        Path(value).resolve() for value in sys.argv[1:]
    )
    temporary = os.environ.get("TMPDIR")
    if temporary != "/fastdata/lavaulta/tmp":
        raise RuntimeError("TMPDIR must be /fastdata/lavaulta/tmp")
    if output.exists():
        raise RuntimeError("output collision")
    if digest(archive) != ARCHIVE_SHA256:
        raise RuntimeError("shader archive identity mismatch")
    require_tool(xenosrecomp, XENOSRECOMP_SHA256, "XenosRecomp")
    require_tool(dxc, DXC_SHA256, "DXC")
    require_tool(spirv_val, SPIRV_VAL_SHA256, "spirv-val")
    if not rexglue_cli.is_file() or not os.access(rexglue_cli, os.X_OK):
        raise RuntimeError("ReXGlue CLI is missing or not executable")

    workspace = Path(__file__).resolve().parent.parent
    extractor = workspace / "tools/extract_ac6_pac.py"
    common_header = workspace.parent.parent / ".tools/xenosrecomp-source/XenosRecomp/shader_common.h"
    with tempfile.TemporaryDirectory(prefix="ac6-map-shader-gate.", dir=temporary) as directory:
        root = Path(directory)
        listing = run(["tar", "--zstd", "-tf", str(archive)])
        if listing.returncode:
            raise RuntimeError("cannot list shader archive")
        prefix = "ac6-pal-shader-identification-20260816/"
        members = listing.stdout.splitlines()
        if not members or any(
            not member.startswith(prefix) or "/../" in member or member.startswith("/")
            for member in members
        ):
            raise RuntimeError("shader archive contains an unsafe member")
        unpack = root / "archive"
        unpack.mkdir()
        extracted = run(["tar", "--zstd", "-xf", str(archive), "-C", str(unpack)])
        if extracted.returncode:
            raise RuntimeError("cannot extract shader archive")
        archive_root = unpack / prefix.rstrip("/")
        verify_archive(archive_root)

        extracted_pac = root / "pac"
        decoded = run([
            sys.executable, str(extractor), str(asset_root), "--indices", "163",
            "--output", str(extracted_pac), "--decompress",
        ])
        if decoded.returncode:
            raise RuntimeError("cannot decode DATA.TBL entry 163")
        payload = extracted_pac / "payloads/0163.decompressed.bin"
        children = parse_fhm(payload.read_bytes())
        if children is None or len(children) != 49 or any(
            child.magic != "NSXR" or child.notes for child in children
        ):
            raise RuntimeError("entry 163 is not the qualified 49-NSXR corpus")
        nsxr = {f"{child.index:04d}.bin": child.data for child in children}

        with (archive_root / "nsxr-map-terrain-shaders.tsv").open(newline="") as stream:
            rows = list(csv.DictReader(stream, delimiter="\t"))
        if len(rows) != 108 or len({row["sha256"] for row in rows}) != 78:
            raise RuntimeError("Map shader inventory identity mismatch")

        results: list[dict[str, object]] = []
        artifacts = root / "artifacts"
        artifacts.mkdir()
        for index, row in enumerate(rows):
            data = nsxr[row["nsxr"]]
            record = int(row["record_offset"], 16)
            trim = int(row["layout_trim"], 16)
            code_offset = record + be32(data, record + 4) + trim
            code_size = be32(data, record + 8) - trim
            microcode = data[code_offset:code_offset + code_size]
            if len(microcode) != code_size or digest_bytes(swap32(microcode)) != row["sha256"]:
                raise RuntimeError("Map shader microcode identity mismatch")
            item: dict[str, object] = {
                "canonical_sha256": row["sha256"],
                "raw_sha256": digest_bytes(microcode),
                "stage": "vertex" if row["stage"] == "vert" else "pixel",
                "bytes": code_size,
                "nsxr": row["nsxr"],
                "record_offset": row["record_offset"],
                "updb": row["updb"],
                "families": row["families"].split(","),
                "seen_by_local_xenia": row["seen_by_local_xenia"] == "yes",
            }
            shader_dir = artifacts / str(index)
            shader_dir.mkdir()
            if row["stage"] == "frag":
                source = shader_dir / "shader.bin"
                disassembly = shader_dir / "shader.txt"
                spirv = shader_dir / "shader.spv"
                source.write_bytes(microcode)
                process = run([
                    str(rexglue_cli), "--stage", "pixel", "--input", str(source),
                    "--spirv", str(spirv), "--disassembly", str(disassembly),
                    "--expected-sha256", item["raw_sha256"], "--register-count", "auto",
                ])
                item["translator"] = "ReXGlue"
                item["translator_exit"] = process.returncode
                if process.returncode == 0:
                    item["analysis"] = parse_fields(process.stdout)
                    validation = run([
                        str(spirv_val), "--target-env", "vulkan1.1",
                        "--scalar-block-layout", str(spirv),
                    ])
                    item["spirv_val_exit"] = validation.returncode
                    if validation.returncode == 0:
                        item["spirv_bytes"] = spirv.stat().st_size
                        item["spirv_sha256"] = digest(spirv)
                else:
                    item["blocker"] = error_class(process.stderr, process.returncode)
            else:
                container_size = be32(data, record + 4) + be32(data, record + 8)
                container = data[record:record + container_size]
                if len(container) != container_size or container[:4] != b"\x10\x2a\x11\x01":
                    raise RuntimeError("vertex ShaderContainer is invalid")
                source = shader_dir / "container.bin"
                hlsl = shader_dir / "shader.hlsl"
                spirv = shader_dir / "shader.spv"
                disassembly = shader_dir / "shader.txt"
                raw_source = shader_dir / "shader.bin"
                source.write_bytes(container)
                raw_source.write_bytes(microcode)
                item["container_bytes"] = container_size
                item["container_sha256"] = digest_bytes(container)
                elements = parse_vertex_elements(container)
                aliases = semantic_aliases(elements)
                item["vertex_elements"] = elements
                item["vertex_semantic_aliases"] = aliases
                analysis_process = run([
                    str(rexglue_cli), "--stage", "vertex", "--input", str(raw_source),
                    "--spirv", str(spirv), "--disassembly", str(disassembly),
                    "--expected-sha256", item["raw_sha256"], "--register-count", "auto",
                    "--analysis-only",
                ])
                if analysis_process.returncode:
                    raise RuntimeError("ReXGlue vertex analysis failed")
                item["analysis"] = parse_fields(analysis_process.stdout)
                process = run([str(xenosrecomp), str(source), str(hlsl), str(common_header)])
                item["translator"] = "XenosRecomp"
                item["translator_exit"] = process.returncode
                if process.returncode == 0:
                    normalized = normalize_semantic_alias_parameters(hlsl, aliases)
                    if normalized:
                        item["normalization"] = {
                            "kind": "deduplicate_identical_container_semantic_parameters",
                            "aliases": normalized,
                        }
                    item["hlsl_bytes"] = hlsl.stat().st_size
                    item["hlsl_sha256"] = digest(hlsl)
                    compilation = run([
                        str(dxc), "-spirv", "-fspv-target-env=vulkan1.1", "-T", "vs_6_0",
                        "-E", "main", str(hlsl), "-Fo", str(spirv),
                    ])
                    item["dxc_exit"] = compilation.returncode
                    if compilation.returncode == 0:
                        validation = run([
                            str(spirv_val), "--target-env", "vulkan1.1", str(spirv),
                        ])
                        item["spirv_val_exit"] = validation.returncode
                        if validation.returncode == 0:
                            item["spirv_bytes"] = spirv.stat().st_size
                            item["spirv_sha256"] = digest(spirv)
                    else:
                        item["blocker"] = error_class(compilation.stderr, compilation.returncode)
                else:
                    item["blocker"] = error_class(process.stderr, process.returncode)
                if item.get("blocker") == "duplicate_iTexCoord1" and aliases:
                    item["blocker"] = "xenosrecomp_vertex_semantic_alias_collision"
                if (
                    item.get("blocker") == "signal_11"
                    and item["analysis"].get("point_size_edge_flag_kill_vertex_mask") == 1
                ):
                    item["blocker"] = "xenosrecomp_unsupported_point_size_export"
            item["qualified"] = (
                item.get("translator_exit") == 0
                and item.get("dxc_exit", 0) == 0
                and item.get("spirv_val_exit") == 0
            )
            results.append(item)

        by_hash: dict[str, list[dict[str, object]]] = {}
        for item in results:
            by_hash.setdefault(str(item["canonical_sha256"]), []).append(item)
        if any(
            any(bool(item["qualified"]) for item in group)
            != all(bool(item["qualified"]) for item in group)
            for group in by_hash.values()
        ):
            raise RuntimeError("one microcode has inconsistent container qualification")
        qualified = [group[0] for group in by_hash.values() if group[0]["qualified"]]
        blocked = [group[0] for group in by_hash.values() if not group[0]["qualified"]]
        receipt = {
            "schema": "ac6-demo-map-shader-offline-gate/v1",
            "target": {"id": "ac6-demo-xbox360-pal", "xex_sha256": XEX_SHA256},
            "tooling": {
                "archive_sha256": ARCHIVE_SHA256,
                "rexglue_cli_sha256": digest(rexglue_cli),
                "xenosrecomp_commit": XENOSRECOMP_COMMIT,
                "xenosrecomp_sha256": XENOSRECOMP_SHA256,
                "dxc_sha256": DXC_SHA256,
                "spirv_val_sha256": SPIRV_VAL_SHA256,
            },
            "summary": {
                "occurrences": len(results),
                "unique_microcodes": len(by_hash),
                "qualified_unique": len(qualified),
                "blocked_unique": len(blocked),
                "qualified_vertex": sum(item["stage"] == "vertex" for item in qualified),
                "qualified_pixel": sum(item["stage"] == "pixel" for item in qualified),
                "blocked_vertex": sum(item["stage"] == "vertex" for item in blocked),
                "blocked_pixel": sum(item["stage"] == "pixel" for item in blocked),
            },
            "occurrences": results,
            "policy": {
                "proprietary_bytes_tracked": False,
                "generated_hlsl_or_spirv_tracked": False,
                "temporary_only": True,
                "fail_closed": True,
            },
        }

    output.parent.mkdir(parents=True, exist_ok=True)
    encoded = json.dumps(receipt, indent=2, sort_keys=True).encode() + b"\n"
    with tempfile.NamedTemporaryFile(dir=output.parent, delete=False) as stream:
        temporary_output = Path(stream.name)
        stream.write(encoded)
    os.replace(temporary_output, output)
    print(
        f"qualified={receipt['summary']['qualified_unique']}/78 "
        f"blocked={receipt['summary']['blocked_unique']} output={output}"
    )
    return 0


def digest_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError) as error:
        print(f"fail-closed: {error}", file=sys.stderr)
        raise SystemExit(2) from error
