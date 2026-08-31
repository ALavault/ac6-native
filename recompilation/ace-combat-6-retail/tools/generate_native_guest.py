#!/usr/bin/env python3
"""Run qualified XenonAnalyse/XenonRecomp into an ignored native build tree.

Generated C++ is evidence/working input only.  Any generator diagnostic keeps
the receipt open and returns non-zero; the native runtime must not consume an
unqualified output.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any


PRODUCT = Path(__file__).resolve().parents[1]
XENON_COMMIT = "ddd128bcca99fe8bfbb99bea583c972351fa6ace"
ANALYSE_ERROR = re.compile(r"(?im)^ERROR:.*$")
UNRECOGNIZED = re.compile(r"(?im)^Unrecognized instruction at 0x([0-9A-F]+):\s*(\S+)")
HELPER_KEYS = (
    "restgprlr_14_address", "savegprlr_14_address", "restfpr_14_address",
    "savefpr_14_address", "restvmx_14_address", "savevmx_14_address",
    "restvmx_64_address", "savevmx_64_address",
)


class GenerationError(RuntimeError):
    pass


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def diagnostics(text: str) -> dict[str, Any]:
    errors = ANALYSE_ERROR.findall(text)
    unrecognized = [
        {"address": f"0x{address.upper()}", "instruction": instruction}
        for address, instruction in UNRECOGNIZED.findall(text)
    ]
    return {
        "switch_errors": errors,
        "unrecognized_instructions": unrecognized,
        "count": len(errors) + len(unrecognized),
    }


def qualified_helpers(config: Path) -> dict[str, str]:
    text = config.read_text(encoding="utf-8")
    values: dict[str, str] = {}
    for key in HELPER_KEYS:
        match = re.search(rf"(?m)^\s*{re.escape(key)}\s*=\s*(0x[0-9A-Fa-f]+)\s*$", text)
        if match is None:
            raise GenerationError(f"{key} is unspecified")
        values[key] = match.group(1).upper()
    if len(set(values.values())) != len(values):
        raise GenerationError("ABI helper addresses are not unique")
    return values


def qualified_boundaries(path: Path, expected_xex: str,
                         switch_log: Path | None = None,
                         function_map: Path | None = None) -> tuple[list[tuple[int, int]], list[int]]:
    """Load only the function spans exported from the canonical US Ghidra project."""
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise GenerationError(f"function boundary export is unreadable: {error}") from error
    if (
        document.get("schema") != "ac6.ghidra-function-boundaries.v1"
        or document.get("project") != "ac6-us"
        or document.get("program") != "default.xex"
        or document.get("sha256") != expected_xex
        or document.get("language") != "PowerPC:BE:64:Xenon"
    ):
        raise GenerationError("function boundary export identity mismatch")
    entries = document.get("functions")
    if not isinstance(entries, list) or document.get("function_count") != len(entries):
        raise GenerationError("function boundary export count is inconsistent")
    result: list[tuple[int, int]] = []
    seen: set[int] = set()
    for item in entries:
        if not isinstance(item, dict) or not isinstance(item.get("entry"), str):
            raise GenerationError("function boundary entry is malformed")
        try:
            address = int(item["entry"], 16)
            ranges = item["ranges"]
            ends = [int(span[1], 16) for span in ranges]
        except (KeyError, TypeError, ValueError, IndexError) as error:
            raise GenerationError("function boundary range is malformed") from error
        if address in seen or not ends or any(end < address for end in ends):
            raise GenerationError(f"function boundary span is invalid at 0x{address:X}")
        size = max(ends) - address + 1
        if address > 0xFFFFFFFF or size > 0xFFFFFFFF:
            raise GenerationError("function boundary exceeds Xenon address width")
        seen.add(address)
        result.append((address, size))
    if switch_log is None:
        return result, []
    try:
        log_text = switch_log.read_text(encoding="utf-8")
    except OSError as error:
        raise GenerationError(f"switch diagnostic log is unreadable: {error}") from error
    owner_addresses = sorted({int(value, 16) for value in
                              re.findall(r"Switch case at ([0-9A-Fa-f]+) is", log_text)})
    selected: list[tuple[int, int]] = []
    target_by_owner: dict[int, int] = {}
    for owner, target in re.findall(
            r"Switch case at ([0-9A-Fa-f]+) is trying to jump outside function:\s*([0-9A-Fa-f]+)",
            log_text):
        owner_value, target_value = int(owner, 16), int(target, 16)
        target_by_owner[owner_value] = max(target_value, target_by_owner.get(owner_value, 0))
    for address, size in result:
        end = address + size - 1
        if any(address <= owner <= end for owner in owner_addresses):
            selected.append((address, size))
    missing = [owner for owner in owner_addresses
               if not any(address <= owner < address + size for address, size in result)]
    # A switch dispatch instruction can be the first Ghidra-unrecovered code
    # in a function.  Materialize only that bounded owner-to-last-target span;
    # never widen all 8k functions or guess across the next canonical entry.
    map_addresses: list[int] = []
    if function_map is not None:
        try:
            map_text = function_map.read_text(encoding="utf-8")
        except OSError as error:
            raise GenerationError(f"function map is unreadable: {error}") from error
        map_addresses = sorted({int(value, 16) for value in
                                re.findall(r"\{\s*0x([0-9A-Fa-f]+),\s*sub_", map_text)})
        if not map_addresses:
            raise GenerationError("qualified function map contains no function entries")
    for owner in missing:
        target = target_by_owner.get(owner)
        if target is None or target < owner:
            raise GenerationError(f"switch owner has no bounded target span: 0x{owner:X}")
        start = max((address for address in map_addresses if address <= owner),
                    default=owner)
        selected.append((start, target - start + 4))
    selected = sorted(set(selected))
    if len(selected) != len(owner_addresses):
        raise GenerationError("switch owner boundary selection is inconsistent")
    return selected, owner_addresses


def materialize_config(config: Path, destination: Path,
                       boundaries: list[tuple[int, int]] | None) -> None:
    text = config.read_text(encoding="utf-8")
    if boundaries is not None:
        text += "\n\n# Canonical ac6-us Ghidra function boundaries; generated at build time.\n"
        text += "functions = [\n"
        text += "".join(f"    {{ address = 0x{address:X}, size = 0x{size:X} }},\n"
                         for address, size in boundaries)
        text += "]\n"
    destination.write_text(text, encoding="utf-8")


def run_generator(
    *,
    xex: Path,
    output: Path,
    config: Path,
    analyser: Path,
    recompiler: Path,
    context: Path,
    target: str,
    boundaries_path: Path | None = None,
    switch_log: Path | None = None,
    function_map: Path | None = None,
) -> dict[str, Any]:
    definition = json.loads((PRODUCT / "targets" / f"{target}.json").read_text())
    expected = definition["xex"]["sha256"]
    actual = sha256(xex)
    if actual != expected:
        raise GenerationError(f"XEX SHA-256 mismatch: {actual}")
    for tool, label in ((analyser, "XenonAnalyse"), (recompiler, "XenonRecomp"), (context, "PPC context"), (config, "config")):
        if not tool.is_file():
            raise GenerationError(f"{label} is missing: {tool}")
    helpers = qualified_helpers(config)
    boundaries: list[tuple[int, int]] | None = None
    switch_owners: list[int] = []
    if boundaries_path is not None:
        boundaries, switch_owners = qualified_boundaries(
            boundaries_path, actual, switch_log, function_map)
    if output.exists() and any(output.iterdir()):
        raise GenerationError(f"output must be a new empty directory: {output}")
    output.mkdir(parents=True, exist_ok=True)
    assets = output / "assets"
    assets.mkdir()
    generated = output / "generated"
    generated.mkdir()
    shutil.copy2(xex, assets / "default.xex")
    materialize_config(config, output / "config.toml", boundaries)
    switch_tables = output / "switch_tables.toml"
    analyse = subprocess.run(
        [str(analyser), str(xex), str(switch_tables)],
        cwd=output, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        check=False,
    )
    analyse_log = analyse.stdout
    (output / "analyse.log").write_text(analyse_log, encoding="utf-8")
    switch_diagnostics = diagnostics(analyse_log)
    if analyse.returncode != 0 or not switch_tables.is_file():
        receipt = {
            "schema": "ac6.retail-native-codegen.v1", "status": "failed",
            "target": target, "xex_sha256": actual, "xenonrecomp_commit": XENON_COMMIT,
            "phase": "analyse", "diagnostics": switch_diagnostics,
        }
        (output / "codegen-receipt.json").write_text(
            json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        raise GenerationError("XenonAnalyse failed")
    recomp = subprocess.run(
        [str(recompiler), str(output / "config.toml"), str(context)],
        cwd=output, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        check=False,
    )
    recomp_log = recomp.stdout
    (output / "recomp.log").write_text(recomp_log, encoding="utf-8")
    recomp_diagnostics = diagnostics(recomp_log)
    all_diagnostics = {
        "switch_errors": switch_diagnostics["switch_errors"] + recomp_diagnostics["switch_errors"],
        "unrecognized_instructions": switch_diagnostics["unrecognized_instructions"] + recomp_diagnostics["unrecognized_instructions"],
    }
    all_diagnostics["count"] = len(all_diagnostics["switch_errors"]) + len(all_diagnostics["unrecognized_instructions"])
    generated_files = sorted(path for path in generated.rglob("*") if path.is_file())
    receipt = {
        "schema": "ac6.retail-native-codegen.v1",
        "status": "pass" if recomp.returncode == 0 and all_diagnostics["count"] == 0 else "open-diagnostics",
        "target": target,
        "xex_sha256": actual,
        "xenonrecomp_commit": XENON_COMMIT,
        "qualified_helpers": helpers,
        "boundary_function_count": len(boundaries) if boundaries is not None else 0,
        "switch_owner_count": len(switch_owners),
        "switch_owners": [f"0x{value:X}" for value in switch_owners],
        "output": str(output.resolve()),
        "generated_files": len(generated_files),
        "generated_bytes": sum(path.stat().st_size for path in generated_files),
        "diagnostics": all_diagnostics,
        "retail_bytes_tracked": False,
    }
    (output / "codegen-receipt.json").write_text(
        json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    if receipt["status"] != "pass":
        raise GenerationError("XenonRecomp output has open diagnostics")
    return receipt


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", choices=("ntsc-uj",), default="ntsc-uj")
    parser.add_argument("--xex", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--config", required=True, type=Path)
    parser.add_argument("--analyser", required=True, type=Path)
    parser.add_argument("--recompiler", required=True, type=Path)
    parser.add_argument("--context", required=True, type=Path)
    parser.add_argument("--boundaries", type=Path,
                        help="canonical ac6-us Ghidra function-boundary export")
    parser.add_argument("--switch-log", type=Path,
                        help="prior XenonRecomp diagnostic log used to select owners")
    parser.add_argument("--function-map", type=Path,
                        help="prior generated function map used only to cross-match owner starts")
    args = parser.parse_args()
    try:
        receipt = run_generator(
            xex=args.xex.resolve(), output=args.output.resolve(), config=args.config.resolve(),
            analyser=args.analyser.resolve(), recompiler=args.recompiler.resolve(),
            context=args.context.resolve(), target=args.target,
            boundaries_path=args.boundaries.resolve() if args.boundaries else None,
            switch_log=args.switch_log.resolve() if args.switch_log else None,
            function_map=args.function_map.resolve() if args.function_map else None,
        )
        print(json.dumps(receipt, indent=2, sort_keys=True))
        return 0
    except (OSError, json.JSONDecodeError, GenerationError, subprocess.SubprocessError) as error:
        print(f"native-codegen: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
