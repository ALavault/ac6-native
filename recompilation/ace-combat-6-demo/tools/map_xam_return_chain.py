#!/usr/bin/env python3
"""Map the bounded XAM return chain using the shared generated-site mapper.

The generated C++ is read-only evidence.  Function/line resolution is owned
by :mod:`map_generated_guest_load_sites`: it starts at each generated function,
uses the adjacent qualified manifest and ``ppc_func_mapping.cpp``, and reads
the corresponding PAL bytes.  This wrapper only applies the XAM allowlist and
runtime-address contract.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import typing
from pathlib import Path

from map_generated_guest_load_sites import (  # type: ignore
    EXPECTED_BASEFILE_SHA256,
    EXPECTED_XENONRECOMP_COMMIT,
    EXPECTED_XEX_SHA256,
    FunctionRecord,
    MappingError,
    load_functions,
    load_generated_manifest,
    map_line,
    sha256,
)

CALLER_LR = 0x822F616C
CONTROLLER_OBJECT = 0x829D153C
EXCLUSIVE_ADDRESS = CONTROLLER_OBJECT + 0x80
MAX_ACCESSES = 32
EXPECTED_SIZES = {
    "load8": 1, "store8": 1, "load16": 2, "store16": 2,
    "load32": 4, "store32": 4, "load64": 8, "store64": 8,
    "load128": 16, "store128": 16, "lwarx": 4, "stwcx": 4,
    "ldarx": 8, "stdcx": 8,
}

ALLOWLIST_BYTES = {
    0x822F601C: "a1 7f 00 48",
    0x822F6020: "91 7f 00 1c",
}
ALLOWLIST_PCS = frozenset((*ALLOWLIST_BYTES, 0x822F5EA0))

ARM_RE = re.compile(
    r"^AC6_XAM_RETURN_CHAIN_ARM caller_lr=(0x[0-9A-Fa-f]{8}) "
    r"tick=(\d+) thread=(\d+) user=(\d+) flags=(0x[0-9A-Fa-f]{8}) "
    r"output=(0x[0-9A-Fa-f]{8}) result=(0x[0-9A-Fa-f]{8}) "
    r"state16=([0-9A-Fa-f]{32})$"
)
ACCESS_RE = re.compile(
    r"^AC6_XAM_RETURN_CHAIN_ACCESS kind=(\S+) address=(0x[0-9A-Fa-f]{8}) "
    r"size=(\d+) value_be=(0x[0-9A-Fa-f]+) tick=(\d+) thread=(\d+) "
    r"lr=(0x[0-9A-Fa-f]{8}) function=(\S*) generated_line=(\d+)$"
)
ATOMIC_RE = re.compile(
    r"^AC6_XAM_RETURN_CHAIN_ATOMIC kind=(\S+) address=(0x[0-9A-Fa-f]{8}) "
    r"size=(\d+) value_be=(0x[0-9A-Fa-f]+) success=([01]) tick=(\d+) "
    r"thread=(\d+) lr=(0x[0-9A-Fa-f]{8}) function=(\S*) "
    r"generated_line=(\d+) site_pc=(0x[0-9A-Fa-f]{8})$"
)
REFUSED_PREFIX = "AC6_XAM_RETURN_CHAIN_ACCESS_REFUSED "
STOP_RE = re.compile(
    r"^AC6_XAM_RETURN_CHAIN_STOP reason=(\S+) accesses=(\d+)$"
)


def fail(message: str) -> typing.NoReturn:
    raise MappingError(message)


def _normalise_function(function: str, functions: dict[str, FunctionRecord]) -> str:
    """Join runtime ``__func__`` spelling to generated function records."""
    candidates = [function]
    if function.startswith("__imp__"):
        candidates.append(function.removeprefix("__imp__"))
    else:
        candidates.append("__imp__" + function)
    for candidate in candidates:
        if candidate in functions:
            return candidate
    fail(f"function is absent from qualified generated sources: {function}")


def _parse_trace(path: Path) -> tuple[dict[str, object], list[dict[str, object]],
                                       dict[str, object]]:
    arms: list[dict[str, object]] = []
    rows: list[dict[str, object]] = []
    stops: list[dict[str, object]] = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        match = ARM_RE.fullmatch(line)
        if match:
            caller, tick, thread, user, flags, output, result, state16 = match.groups()
            arms.append({
                "caller_lr": int(caller, 16), "tick": int(tick),
                "thread": int(thread), "user": int(user),
                "flags": int(flags, 16), "output": int(output, 16),
                "result": int(result, 16), "state16": state16.upper(),
            })
            continue
        match = ACCESS_RE.fullmatch(line)
        if match:
            kind, address, size, value, tick, thread, lr, function, generated = match.groups()
            rows.append({
                "record": "access", "kind": kind, "address": int(address, 16),
                "size": int(size), "value_be": "0x" + value[2:].upper(), "tick": int(tick),
                "thread": int(thread), "lr": int(lr, 16), "function": function,
                "generated_line": int(generated), "trace_line": line_number,
            })
            continue
        match = ATOMIC_RE.fullmatch(line)
        if match:
            kind, address, size, value, success, tick, thread, lr, function, generated, site_pc = match.groups()
            rows.append({
                "record": "atomic", "kind": kind, "address": int(address, 16),
                "size": int(size), "value_be": "0x" + value[2:].upper(),
                "success": bool(int(success)), "tick": int(tick),
                "thread": int(thread), "lr": int(lr, 16), "function": function,
                "generated_line": int(generated), "site_pc": int(site_pc, 16),
                "trace_line": line_number,
            })
            continue
        match = STOP_RE.fullmatch(line)
        if match:
            stops.append({"reason": match.group(1), "accesses": int(match.group(2))})
            continue
        if line.startswith(REFUSED_PREFIX):
            fail(f"trace contains explicit refusal at line {line_number}")
        if line.startswith("AC6_XAM_RETURN_CHAIN_"):
            fail(f"malformed XAM return-chain row at line {line_number}")
    if len(arms) != 1:
        fail(f"expected exactly one successful XAM arm, got {len(arms)}")
    arm = arms[0]
    if (arm["caller_lr"] != CALLER_LR or arm["result"] != 0 or
            arm["output"] != CONTROLLER_OBJECT):
        fail("caller/result/output filter mismatch")
    if len(str(arm["state16"])) != 32:
        fail("payload state16 is not exactly 16 bytes")
    if not rows or len(rows) > MAX_ACCESSES:
        fail("empty or over-bounded XAM trace")
    if len(stops) != 1:
        fail(f"expected exactly one bounded stop, got {len(stops)}")
    if any(int(stop["accesses"]) != len(rows) for stop in stops):
        fail("stop count disagrees with bounded access count")
    if stops[0]["reason"] != "qualified_store_exclusive":
        fail("stop reason is not qualified_store_exclusive")
    return arm, rows, stops[-1] if stops else {}


def _check_value(row: dict[str, object]) -> None:
    kind = str(row["kind"])
    size = int(row["size"])
    value = str(row["value_be"])
    if EXPECTED_SIZES.get(kind) != size:
        fail(f"kind/size mismatch: {kind}/{size}")
    if len(value) != 2 + size * 2 or not re.fullmatch(r"0x[0-9A-F]{%d}" % (size * 2), value):
        fail(f"value_be width is not exactly {size} bytes")


def _map_rows(rows: list[dict[str, object]], functions: dict[str, FunctionRecord],
              basefile: bytes) -> list[dict[str, object]]:
    mapped_rows: list[dict[str, object]] = []
    for row in rows:
        function = _normalise_function(str(row["function"]), functions)
        kind = str(row["kind"])
        mapped = map_line(functions, function, int(row["generated_line"]),
                          basefile, kind)
        pc = int(str(mapped["guest_pc"]), 16)
        instruction_bytes = str(mapped["instruction_bytes"]).lower()
        if pc not in ALLOWLIST_PCS:
            fail(f"generated site is outside XAM allowlist: 0x{pc:08X}")
        expected_bytes = ALLOWLIST_BYTES.get(pc)
        if expected_bytes is not None and instruction_bytes != expected_bytes:
            fail(f"PAL instruction bytes mismatch at 0x{pc:08X}")
        if pc == 0x822F5EA0 and int(row["address"]) != EXCLUSIVE_ADDRESS:
            fail("exclusive runtime address is not controller object + 0x80")
        if int(row["address"]) > 0xFFFFFFFF:
            fail("guest runtime address exceeds 32 bits")
        _check_value(row)
        if row["record"] == "atomic":
            site_pc = int(row["site_pc"])
            if site_pc != pc:
                fail("atomic site_pc disagrees with generated function/line")
            if pc != 0x822F5EA0:
                fail("atomic PC/site is not allowlisted")
            if not bool(row["success"]):
                fail("qualified atomic site did not report success")
        mapped_rows.append({
            **{key: value for key, value in row.items() if key != "trace_line"},
            "function_resolved": function,
            "source": mapped["source"],
            "guest_pc": f"0x{pc:08X}",
            "instruction": mapped["instruction"],
            "instruction_bytes": instruction_bytes,
        })
    return mapped_rows


def build(args: argparse.Namespace) -> dict[str, object]:
    if args.generated_dir.is_symlink() or not args.generated_dir.is_dir():
        fail("generated directory is missing or symlinked")
    for path, label in ((args.xex, "XEX"), (args.basefile, "PAL basefile")):
        if path.is_symlink() or not path.is_file():
            fail(f"{label} is missing or symlinked")
    if args.manifest is None:
        manifest = args.generated_dir.parent / "manifest.json"
    else:
        manifest = args.manifest
    expected_manifest = args.generated_dir.parent / "manifest.json"
    if manifest.resolve() != expected_manifest.resolve():
        fail("generated manifest is not adjacent to current generated sources")
    xex_sha = sha256(args.xex.read_bytes())
    if xex_sha != EXPECTED_XEX_SHA256:
        fail(f"XEX identity mismatch: {xex_sha}")
    base_bytes = args.basefile.read_bytes()
    base_sha = sha256(base_bytes)
    if base_sha != EXPECTED_BASEFILE_SHA256:
        fail(f"PAL basefile identity mismatch: {base_sha}")
    manifest, manifest_sha = load_generated_manifest(manifest, xex_sha)
    functions = load_functions(args.generated_dir, len(base_bytes))
    arm, rows, stop = _parse_trace(args.trace)
    if any(int(row["thread"]) != int(arm["thread"]) for row in rows):
        fail("access thread does not match the armed guest thread")
    mapped_rows = _map_rows(rows, functions, base_bytes)
    final = mapped_rows[-1]
    if (final["kind"] != "store32" or final["guest_pc"] != "0x822F5EA0" or
            int(final["address"]) != EXCLUSIVE_ADDRESS):
        fail("last row is not the qualified exclusive store32")
    if any(int(row["address"]) == EXCLUSIVE_ADDRESS
           for row in mapped_rows[:-1]):
        fail("exclusive target appeared before the final qualified row")
    source_hashes = {
        source.name: sha256(source.read_bytes())
        for source in sorted({args.generated_dir / str(row["source"])
                               for row in mapped_rows})
    }
    ghidra_sha = json.loads(manifest.read_text(encoding="utf-8")).get(
        "ghidra_manifest_sha256")
    document = {
        "schema": "ac6-demo-xam-return-chain/v1",
        "target": {
            "id": "ac6-demo-xbox360-pal", "module": "Default.xex",
            "architecture": "Xenon big-endian PPC / Xenos",
            "xex_sha256": xex_sha, "pal_basefile_sha256": base_sha,
        },
        "identity": {
            "xex_sha256": xex_sha, "pal_basefile_sha256": base_sha,
            "xenonrecomp_commit": EXPECTED_XENONRECOMP_COMMIT,
            "generated_manifest_sha256": manifest_sha,
            "generated_source_sha256": source_hashes,
        },
        "ghidra_owner": {
            "project": "ace-combat-6-demo",
            "target_id": "ac6-demo-xbox360-pal",
            "manifest_sha256": ghidra_sha,
            "availability": "manifest-only" if ghidra_sha else "unavailable",
            "used_for_runtime_pc": False,
        },
        "inputs": {
            "trace": str(args.trace), "trace_sha256": sha256(args.trace.read_bytes()),
            "xex": str(args.xex), "basefile": str(args.basefile),
            "generated_dir": str(args.generated_dir),
            "generated_manifest": str(manifest),
        },
        "arm": arm,
        "rows": len(mapped_rows), "mapped_rows": len(mapped_rows),
        "unique_sites": len({row["guest_pc"] for row in mapped_rows}),
        "sites": mapped_rows,
        "stop": stop,
        "policy": {
            "allowlisted_pcs": [f"0x{pc:08X}" for pc in sorted(ALLOWLIST_PCS)],
            "exclusive_runtime_address": f"0x{EXCLUSIVE_ADDRESS:08X}",
            "lr_is_not_pc": True, "generated_cpp_modified": False,
            "manifest_adjacent_and_qualified": True,
            "ambiguity_is_failure": True, "fail_closed": True,
        },
    }
    return document


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--trace", type=Path, required=True)
    parser.add_argument("--basefile", type=Path, required=True)
    parser.add_argument("--xex", type=Path, required=True)
    parser.add_argument("--generated-dir", type=Path, required=True)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--json", "--output", dest="output", type=Path, required=True)
    args = parser.parse_args()
    result = build(args)
    payload = json.dumps(result, indent=2, sort_keys=True) + "\n"
    args.output.write_text(payload, encoding="utf-8")
    print(json.dumps({"schema": result["schema"], "rows": result["rows"],
                      "mapped_rows": result["mapped_rows"],
                      "unique_sites": result["unique_sites"]}, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (MappingError, OSError, json.JSONDecodeError) as error:
        raise SystemExit(f"mapping_error: {error}")
