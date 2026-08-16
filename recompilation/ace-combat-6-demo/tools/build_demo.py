#!/usr/bin/env python3
"""Reproducible, fail-closed XenonRecomp build for the qualified AC6 demo."""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tomllib
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
IDENTITY = json.loads((PROJECT / "config/demo-identity.json").read_text())
LOCK = json.loads((PROJECT / "config/xenonrecomp.lock.json").read_text())
ABI = json.loads((PROJECT / "config/demo-abi.json").read_text())
EXPECTED_XEX = IDENTITY["xex"]["sha256"]
DEFAULT_BUILD_JOBS = 12
GHIDRA_SCHEMA = "ac6-demo-ghidra-chunks.v2"
GHIDRA_TARGET_ID = "ac6-demo-xbox360-pal"
GHIDRA_PROJECT_PATH = "ghidra-projects/ace-combat-6-demo"
GHIDRA_LANGUAGE = "PowerPC:BE:64:Xenon"
GHIDRA_VERSION = "12.1.2"


class BuildError(RuntimeError):
    pass


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def build_jobs() -> int:
    raw = os.environ.get("AC6_DEMO_BUILD_JOBS", str(DEFAULT_BUILD_JOBS))
    try:
        jobs = int(raw)
    except ValueError as error:
        raise BuildError(f"AC6_DEMO_BUILD_JOBS must be a positive integer: {raw}") from error
    if jobs < 1:
        raise BuildError(f"AC6_DEMO_BUILD_JOBS must be a positive integer: {raw}")
    return jobs


def run(command: list[str], cwd: Path | None = None, log: Path | None = None) -> str:
    process = subprocess.run(command, cwd=cwd, text=True, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT)
    if log is not None:
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text(process.stdout)
    if process.returncode != 0:
        raise BuildError(f"command failed ({process.returncode}): {' '.join(command)}")
    return process.stdout


def run_parallel(commands: list[tuple[list[str], Path]]) -> None:
    if not commands:
        return
    workers = min(build_jobs(), len(commands))
    with ThreadPoolExecutor(max_workers=workers) as executor:
        futures = [executor.submit(run, command, log=log)
                   for command, log in commands]
        for future in futures:
            future.result()


def verify_xex(path: Path) -> None:
    expected_size = IDENTITY["xex"]["size"]
    if not path.is_file() or path.stat().st_size != expected_size:
        raise BuildError(f"XEX size mismatch: {path}")
    actual = sha256(path)
    if actual != EXPECTED_XEX:
        raise BuildError(f"XEX identity mismatch: {actual} != {EXPECTED_XEX}")


def _qualified_digest(value: object) -> bool:
    return (isinstance(value, str) and len(value) == 64 and
            all(character in "0123456789abcdef" for character in value))


def validate_ghidra_manifest(document: object) -> dict[str, object]:
    if not isinstance(document, dict):
        raise BuildError("Ghidra chunk manifest must contain an object")
    required = {
        "schema": GHIDRA_SCHEMA,
        "target_id": GHIDRA_TARGET_ID,
        "project_path": GHIDRA_PROJECT_PATH,
        "project": "ace-combat-6-demo",
        "program": "Default.xex",
        "module": "Default.xex",
        "language": GHIDRA_LANGUAGE,
        "xex_sha256": EXPECTED_XEX,
        "image_base": "0x82000000",
        "entry_point": "0x821A7160",
    }
    for key, expected in required.items():
        if document.get(key) != expected:
            raise BuildError(f"Ghidra chunk manifest {key} is not qualified for this demo")
    ghidra = document.get("ghidra")
    if not isinstance(ghidra, dict) or ghidra != {
            "version": GHIDRA_VERSION,
            "loader": "XEX Loader by Warranty Voider",
            "language": GHIDRA_LANGUAGE,
            "compiler_spec": "default"}:
        raise BuildError("Ghidra toolchain metadata is not qualified")
    expected_ranges = {
        "text": ("0x82090000", "0x002E67C4"),
        "pdata": ("0x82077200", "0x00010438"),
    }
    for key, (address, size) in expected_ranges.items():
        record = document.get(key)
        if (not isinstance(record, dict) or record.get("address") != address or
                record.get("size") != size or
                not _qualified_digest(record.get("byte_sha256"))):
            raise BuildError(f"Ghidra {key} range is not qualified")
    journal = document.get("import_journal")
    if (not isinstance(journal, dict) or
            journal.get("schema") != "ac6-demo-ghidra-import-journal/v1" or
            not _qualified_digest(journal.get("sha256"))):
        raise BuildError("Ghidra import journal is not qualified")
    scripts = document.get("script_sha256")
    required_scripts = {
        "tools/import_ghidra_demo.py",
        "tools/ghidra/ExportQualifiedDemoChunks.java",
        "tools/ghidra/ValidateDemoBoundarySet.java",
        "config/confirmed-chunks.toml",
    }
    if (not isinstance(scripts, dict) or not required_scripts.issubset(scripts) or
            any(not _qualified_digest(value) for value in scripts.values())):
        raise BuildError("Ghidra script hashes are not qualified")
    return document


def prepare_toolchain(args: argparse.Namespace, root: Path) -> tuple[Path, str]:
    source = Path(args.xenonrecomp_root) if args.xenonrecomp_root else None
    checkout = root / "XenonRecomp"
    if source is None and not checkout.exists():
        run(["git", "clone", "--filter=blob:none", "--no-checkout", LOCK["repository"], str(checkout)],
            log=root / "clone.log")
    elif source is not None and not checkout.exists():
        run(["git", "clone", "--no-hardlinks", "--no-checkout", str(source), str(checkout)],
            log=root / "clone.log")
    if not checkout.is_dir():
        raise BuildError(f"missing XenonRecomp checkout: {checkout}")
    run(["git", "-C", str(checkout), "checkout", "--detach", LOCK["commit"]],
        log=root / "checkout.log")
    run(["git", "-C", str(checkout), "submodule", "update", "--init", "--depth", "1"],
        log=root / "submodules.log")
    actual = run(["git", "-C", str(checkout), "rev-parse", "HEAD"]).strip()
    if actual != LOCK["commit"]:
        raise BuildError(f"XenonRecomp commit mismatch: {actual}")

    patches = [PROJECT / "patches/xenonrecomp-strict-recompiler.patch",
               PROJECT / "patches/xenonrecomp-strict-config.patch"]
    for index, patch in enumerate(patches):
        if not patch.is_file():
            raise BuildError(f"missing source patch: {patch}")
        check = subprocess.run(
            ["git", "-C", str(checkout), "apply", "--check", str(patch)],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (root / f"patch-check-{index}.log").write_text(check.stdout)
        if check.returncode == 0:
            run(["git", "-C", str(checkout), "apply", str(patch)],
                log=root / f"patch-{index}.log")
            continue
        already_applied = subprocess.run(
            ["git", "-C", str(checkout), "apply", "--reverse", "--check", str(patch)],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (root / f"patch-reverse-check-{index}.log").write_text(already_applied.stdout)
        if already_applied.returncode != 0:
            raise BuildError(f"patch is neither applicable nor already applied: {patch}")

    tool_build = root / "XenonRecomp-build"
    run(["cmake", "-S", str(checkout), "-B", str(tool_build), "-G", "Ninja",
         "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_COMPILER=clang",
         "-DCMAKE_CXX_COMPILER=clang++"], log=root / "tool-cmake.log")
    run(["cmake", "--build", str(tool_build), "--parallel", str(build_jobs())],
        log=root / "tool-build.log")
    return checkout, actual


def confirmed_functions(manifest: Path | None,
                        digests: dict[tuple[int, int], str] | None = None
                        ) -> list[tuple[int, int]]:
    functions: dict[int, int] = {}
    manifest_functions: dict[int, int] = {}
    if manifest is not None:
        if not manifest.is_file():
            raise BuildError(f"Ghidra chunk manifest not found: {manifest}")
        try:
            document = json.loads(manifest.read_text())
        except (OSError, json.JSONDecodeError) as error:
            raise BuildError(f"invalid Ghidra chunk manifest: {error}") from error
        document = validate_ghidra_manifest(document)
        chunks = document.get("chunks", [])
        if not isinstance(chunks, list):
            raise BuildError("Ghidra chunk manifest chunks must be an array")
        for entry in chunks:
            try:
                address_value = entry["address"]
                size_value = entry["size"]
                address = (int(address_value, 0) if isinstance(address_value, str)
                           else int(address_value))
                size = int(size_value, 0) if isinstance(size_value, str) else int(size_value)
            except (KeyError, TypeError, ValueError) as error:
                raise BuildError(f"invalid confirmed Ghidra chunk: {entry}") from error
            if address % 4 != 0 or size <= 0 or size % 4 != 0:
                raise BuildError(f"unaligned or empty confirmed Ghidra chunk: {entry}")
            expected_digest = entry.get("byte_sha256")
            if (not isinstance(expected_digest, str) or len(expected_digest) != 64 or
                    any(c not in "0123456789abcdefABCDEF" for c in expected_digest)):
                raise BuildError(f"confirmed Ghidra chunk has invalid byte_sha256: {entry}")
            functions[address] = size
            manifest_functions[address] = size
            if digests is not None:
                digests[(address, size)] = expected_digest.lower()
    with (PROJECT / "config/confirmed-chunks.toml").open("rb") as stream:
        table = tomllib.load(stream)
    configured_functions: dict[int, int] = {}
    for entry in table.get("function", []):
        address = int(entry["address"])
        size = int(entry["size"])
        expected_digest = entry.get("byte_sha256")
        if expected_digest is None:
            raise BuildError(f"confirmed chunk has no byte_sha256: 0x{address:X}")
        # The XEX file itself is compressed/containerized; byte hashes for
        # configured chunks are checked later against the extracted basefile.
        if len(expected_digest) != 64 or any(c not in "0123456789abcdefABCDEF"
                                            for c in expected_digest):
            raise BuildError(f"invalid confirmed chunk byte_sha256: 0x{address:X}")
        functions[address] = size
        configured_functions[address] = size
        if digests is not None:
            digests[(address, size)] = expected_digest.lower()
    for address, size in manifest_functions.items():
        covered = any(owner <= address and address + size <= owner + owner_size
                      for owner, owner_size in configured_functions.items())
        if covered and address not in configured_functions:
            functions.pop(address, None)
    return sorted(functions.items())


def confirmed_data(manifest: Path | None = None,
                   digests: dict[tuple[int, int], str] | None = None
                   ) -> list[tuple[int, int]]:
    ranges: dict[int, int] = {}
    if manifest is not None:
        try:
            document = json.loads(manifest.read_text())
        except (OSError, json.JSONDecodeError) as error:
            raise BuildError(f"invalid Ghidra data manifest: {error}") from error
        document = validate_ghidra_manifest(document)
        entries = document.get("data_ranges", [])
        if not isinstance(entries, list):
            raise BuildError("Ghidra data manifest data_ranges must be an array")
        for entry in entries:
            try:
                address_value = entry["address"]
                size_value = entry["size"]
                address = (int(address_value, 0) if isinstance(address_value, str)
                           else int(address_value))
                size = int(size_value, 0) if isinstance(size_value, str) else int(size_value)
                expected_digest = entry["byte_sha256"]
            except (KeyError, TypeError, ValueError) as error:
                raise BuildError(f"invalid Ghidra data range: {entry}") from error
            if (address % 4 != 0 or size <= 0 or size % 4 != 0 or
                    not isinstance(expected_digest, str) or len(expected_digest) != 64 or
                    any(c not in "0123456789abcdefABCDEF" for c in expected_digest)):
                raise BuildError(f"invalid Ghidra data range: {entry}")
            if address in ranges and ranges[address] != size:
                raise BuildError(f"conflicting Ghidra data range: 0x{address:X}")
            ranges[address] = size
            if digests is not None:
                digests[(address, size)] = expected_digest.lower()
    path = PROJECT / "config/confirmed-data.toml"
    try:
        with path.open("rb") as stream:
            table = tomllib.load(stream)
    except OSError as error:
        raise BuildError(f"confirmed data manifest unavailable: {error}") from error
    for entry in table.get("range", []):
        try:
            address = int(entry["address"])
            size = int(entry["size"])
            expected_digest = entry["byte_sha256"]
        except (KeyError, TypeError, ValueError) as error:
            raise BuildError(f"invalid confirmed data range: {entry}") from error
        if (address % 4 != 0 or size <= 0 or size % 4 != 0 or
                not isinstance(expected_digest, str) or len(expected_digest) != 64 or
                any(c not in "0123456789abcdefABCDEF" for c in expected_digest)):
            raise BuildError(f"invalid confirmed data range: {entry}")
        if address in ranges and ranges[address] != size:
            raise BuildError(f"conflicting confirmed data range: 0x{address:X}")
        ranges[address] = size
        if digests is not None:
            digests[(address, size)] = expected_digest.lower()
    return sorted(ranges.items())


def resolve_xex1tool(argument: Path | None) -> Path:
    if argument is not None:
        return argument
    candidate = shutil.which("xex1tool")
    if candidate:
        return Path(candidate)
    candidate = PROJECT.parents[1] / ".build/xex1tool/xex1tool"
    if candidate.is_file():
        return candidate
    raise BuildError("xex1tool is required to extract .pdata and qualify all imports")


def pdata_functions(xex: Path, executable: Path, root: Path) -> list[tuple[int, int]]:
    metadata = IDENTITY["pdata"]
    base_address = int(metadata["base_address"], 0)
    virtual_address = int(metadata["virtual_address"], 0)
    size = int(metadata["size"])
    root.mkdir(parents=True, exist_ok=True)
    basefile = root / "xex-basefile.bin"
    run([str(executable), "-b", str(basefile), str(xex)], log=root / "basefile.log")
    data = basefile.read_bytes()
    offset = virtual_address - base_address
    if offset < 0 or offset + size > len(data) or size % 8 != 0:
        raise BuildError("XEX basefile does not contain the qualified .pdata range")
    actual = hashlib.sha256(data[offset:offset + size]).hexdigest()
    if actual != metadata["sha256"]:
        raise BuildError(f".pdata identity mismatch: {actual} != {metadata['sha256']}")
    functions: dict[int, int] = {}
    for row in range(0, size, 8):
        address, packed = struct.unpack_from(">II", data, offset + row)
        length = ((packed >> 8) & 0x3FFFFF) * 4
        if address != 0 and length > 0:
            functions[address] = length
    if not functions:
        raise BuildError("qualified .pdata contains no function records")
    return sorted(functions.items())


def parse_import_listing(output: str) -> list[dict[str, object]]:
    module = None
    records: list[dict[str, object]] = []
    header = re.compile(r"^# (\S+) v")
    entry = re.compile(r"^\s+(\d+)\)\s+(.+?)\s*$")
    for line in output.splitlines():
        match = header.match(line)
        if match:
            # xex1tool prints a second '# (min ...)' version line for the
            # current module; it is not a new import namespace.
            candidate = match.group(1)
            if not candidate.startswith("("):
                module = candidate
            continue
        match = entry.match(line)
        if match and module is not None:
            records.append({"module": module, "ordinal": int(match.group(1)),
                            "name": match.group(2)})
    return records


def imports_from_xex1tool(xex: Path, executable: Path) -> list[dict[str, object]]:
    output = run([str(executable), "-i", str(xex)])
    records = parse_import_listing(output)
    if len(records) != 238:
        raise BuildError(f"expected 238 XEX imports, found {len(records)}")
    return records


def generated_import_symbols(shared_header: Path) -> set[str]:
    pattern = re.compile(r"^PPC_EXTERN_FUNC\((__imp__[A-Za-z_][A-Za-z0-9_]*)\);$")
    symbols: set[str] = set()
    for line in shared_header.read_text().splitlines():
        match = pattern.match(line)
        if match:
            symbols.add(match.group(1))
    if not symbols:
        raise BuildError(f"generated import declarations unavailable: {shared_header}")
    return symbols


def variable_import_names(checkout: Path) -> set[str]:
    """Read the pinned XenonRecomp export kinds for unresolved data imports."""
    table = checkout / "XenonUtils/xbox/xboxkrnl_table.inc"
    if not table.is_file():
        raise BuildError(f"XenonRecomp variable-import table unavailable: {table}")
    pattern = re.compile(
        r"^\s*XE_EXPORT\([^,]+,\s*0x[0-9A-Fa-f]+,\s*"
        r"([A-Za-z_][A-Za-z0-9_]*)\s*,\s*kVariable\s*\),?\s*$"
    )
    names = {match.group(1) for line in table.read_text().splitlines()
             if (match := pattern.match(line))}
    if not names:
        raise BuildError(f"XenonRecomp variable-import table is empty: {table}")
    return names


def classify_imports(imports: list[dict[str, object]],
                     generated_symbols: set[str] | None = None,
                     variable_symbols: set[str] | None = None
                     ) -> tuple[list[tuple[dict[str, object], str]],
                                list[dict[str, object]]]:
    """Match XEX imports to generated callables or pinned data exports."""
    functions: list[tuple[dict[str, object], str]] = []
    data: list[dict[str, object]] = []
    symbols: set[str] = set()
    variable_symbols = variable_symbols or set()
    for record in imports:
        module = record["module"]
        ordinal = record["ordinal"]
        name = record["name"]
        if not (isinstance(module, str) and isinstance(name, str) and
                isinstance(ordinal, int)):
            raise BuildError(f"invalid XEX import record: {record}")
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
            raise BuildError(f"XEX import cannot form a C++ symbol: {name}")
        candidates = ["__imp__" + name]
        if name.endswith("_0"):
            candidates.append("__imp__" + name[:-2])
        if generated_symbols is None:
            matches = [] if name in variable_symbols else [candidates[0]]
        else:
            matches = [candidate for candidate in candidates
                       if candidate in generated_symbols]
        if name in variable_symbols:
            if matches:
                raise BuildError(f"XEX data import unexpectedly generated as callable: {name}")
            data.append(record)
            continue
        if len(matches) != 1:
            raise BuildError(f"XEX import has no unique generated symbol: {name}")
        symbol = matches[0]
        if symbol in symbols:
            raise BuildError(f"duplicate XEX import symbol: {symbol}")
        symbols.add(symbol)
        functions.append((record, symbol))
    return functions, data


def write_import_stubs(root: Path, imports: list[dict[str, object]],
                       generated_symbols: set[str] | None = None,
                       variable_symbols: set[str] | None = None) -> Path:
    """Emit callable traps plus records for pinned non-callable exports."""
    source = root / "import_stubs.cpp"
    functions, data = classify_imports(imports, generated_symbols, variable_symbols)
    lines = [
        '#include "ppc_context.h"',
        '#include "ac6demo/runtime_error.hpp"',
        "#include <cstddef>",
        "#include <cstdint>",
        "#include <string>",
        "",
        'extern "C" [[noreturn]] void AC6_PPC_IMPORT_TRAP(PPCContext&, uint8_t*, const char*,',
        "                                      std::uint16_t);",
        'extern "C" bool AC6_PPC_IMPORT_DISPATCH(PPCContext&, uint8_t*, const char*,',
        "                                           const char*, std::uint16_t);",
        'extern "C" [[noreturn]] void AC6_PPC_DATA_IMPORT_TRAP(const char*,',
        "                                           std::uint16_t);",
        'extern "C" void AC6_PPC_SET_TICK(std::uint64_t) noexcept;',
        "",
        "struct AC6ImportRecord {",
        "  const char* module;",
        "  const char* name;",
        "  std::uint16_t ordinal;",
        "  bool data;",
        "};",
        "",
    ]
    for record, symbol in functions:
        module = record["module"]
        ordinal = record["ordinal"]
        lines.extend([
            # PPC_EXTERN_FUNC in XenonRecomp declares imports with C++
            # linkage; keep the exact mangled ABI so -r catches omissions.
            f'void {symbol}(PPCContext& ctx, uint8_t* base) {{',
            f"  if (!AC6_PPC_IMPORT_DISPATCH(ctx, base, {json.dumps(module)}, "
            f"{json.dumps(record['name'])}, static_cast<std::uint16_t>({ordinal}))) {{",
            f"    AC6_PPC_IMPORT_TRAP(ctx, base, {json.dumps(module)}, "
            f"static_cast<std::uint16_t>({ordinal}));",
            "  }",
            "}",
            "",
        ])
    lines.extend([
        'extern "C" const AC6ImportRecord AC6_PPC_IMPORT_RECORDS[] = {',
    ])
    data_names = {id(record) for record in data}
    for record in imports:
        module = record["module"]
        name = record["name"]
        ordinal = record["ordinal"]
        lines.append(
            f"  {{{json.dumps(module)}, {json.dumps(name)}, "
            f"static_cast<std::uint16_t>({ordinal}), "
            f"{'true' if id(record) in data_names else 'false'}}},"
        )
    lines.extend([
        "};",
        'extern "C" const std::size_t AC6_PPC_IMPORT_RECORD_COUNT =',
        "    sizeof(AC6_PPC_IMPORT_RECORDS) / sizeof(AC6_PPC_IMPORT_RECORDS[0]);",
        "",
        "namespace { thread_local std::uint64_t ac6_import_tick = 0; }",
        "",
        'extern "C" void AC6_PPC_SET_TICK(std::uint64_t tick) noexcept {',
        "  ac6_import_tick = tick;",
        "}",
        "",
        'extern "C" [[noreturn]] void AC6_PPC_IMPORT_TRAP(PPCContext& context,',
        "                                                     uint8_t*, const char* module,",
        "                                                     std::uint16_t ordinal) {",
        "  throw ac6demo::RuntimeTrap(std::string(\"unimplemented import \") + module +",
        "      \" ordinal \" + std::to_string(ordinal), ac6_import_tick,",
        "      static_cast<std::uint32_t>(context.lr), ordinal);",
        "}",
        "",
        'extern "C" [[noreturn]] void AC6_PPC_DATA_IMPORT_TRAP(const char* module,',
        "                                                          std::uint16_t ordinal) {",
        "  throw ac6demo::RuntimeTrap(std::string(\"unimplemented data import \") + module +",
        "      \" ordinal \" + std::to_string(ordinal), ac6_import_tick, 0, ordinal);",
        "}",
        "",
        f"// callable traps: {len(functions)}; data records: {len(data)}",
    ])
    source.write_text("\n".join(lines))
    return source


def write_config(root: Path, xex: Path, switches: Path,
                 functions: list[tuple[int, int]],
                 data_ranges: list[tuple[int, int]]) -> Path:
    config = root / "demo.toml"
    relative_xex = os.path.relpath(xex, root)
    relative_switches = os.path.relpath(switches, root)
    lines = [
        "[main]",
        f'file_path = "{relative_xex}"',
        'out_directory_path = "generated"',
        f'switch_table_file_path = "{relative_switches}"',
        "strict = true",
        "skip_lr = false",
        "skip_msr = false",
        "ctr_as_local = false",
        "xer_as_local = false",
        "reserved_as_local = false",
        "cr_as_local = false",
        "non_argument_as_local = false",
        "non_volatile_as_local = false",
    ]
    for name in ("savegprlr_14", "restgprlr_14", "savefpr_14", "restfpr_14",
                 "savevmx_14", "restvmx_14", "savevmx_64", "restvmx_64"):
        lines.append(f"{name}_address = {ABI[name]}")
    if functions:
        lines.append("functions = [")
        for address, size in functions:
            lines.append(f"{{ address = 0x{address:X}, size = 0x{size:X} }},")
        lines.append("]")
    if data_ranges:
        lines.append("invalid_ranges = [")
        for address, size in data_ranges:
            lines.append(f"{{ address = 0x{address:X}, size = 0x{size:X} }},")
        lines.append("]")
    config.write_text("\n".join(lines) + "\n")
    return config


def verify_confirmed_bytes(root: Path, functions: list[tuple[int, int]],
                           expected: dict[tuple[int, int], str]) -> None:
    basefile = (root / "xex-basefile.bin").read_bytes()
    base_address = int(IDENTITY["xex"]["image_base"], 0)
    for address, size in functions:
        digest = expected.get((address, size))
        if digest is None:
            continue
        offset = address - base_address
        if offset < 0 or offset + size > len(basefile):
            raise BuildError(f"confirmed chunk outside extracted image: 0x{address:X}")
        actual = hashlib.sha256(basefile[offset:offset + size]).hexdigest()
        if actual != digest:
            raise BuildError(f"confirmed chunk byte identity mismatch at 0x{address:X}: "
                             f"{actual} != {digest}")


def syntax_check(root: Path, checkout: Path) -> int:
    generated = root / "generated"
    files = sorted(generated.glob("*.cpp"))
    if not files:
        raise BuildError("XenonRecomp produced no C++ files")
    base_context = root / "ppc_context_base.h"
    shutil.copy2(checkout / "XenonUtils/ppc_context.h", base_context)
    include = ["-I", str(generated), "-I", str(root), "-I", str(checkout / "thirdparty/simde")]
    commands = [
        (["clang++", "-std=c++20", "-fsyntax-only", *include, str(source)],
         root / "syntax" / (source.name + ".log"))
        for source in files
    ]
    run_parallel(commands)
    return len(files)


def compile_check(root: Path, checkout: Path, import_stubs: Path) -> int:
    """Compile every generated TU to an object without linking or executing it."""
    generated = root / "generated"
    files = sorted(generated.glob("*.cpp"))
    if not files:
        raise BuildError("XenonRecomp produced no C++ files")
    objects = root / "objects"
    objects.mkdir(parents=True, exist_ok=True)
    include = ["-I", str(generated), "-I", str(root), "-I", str(PROJECT / "include"),
               "-I", str(checkout / "thirdparty/simde")]
    sources = [*files, import_stubs]
    commands = []
    for source in sources:
        object_file = objects / (source.stem + ".o")
        commands.append((
            ["clang++", "-std=c++20", "-c", *include, str(source), "-o",
             str(object_file)],
            root / "compile" / (source.name + ".log")))
    run_parallel(commands)
    # GNU ld otherwise injects a random build-id into a relocatable link,
    # defeating the byte-identical generation gate even when every input
    # object is identical.
    run(["clang++", "-r", "-Wl,--build-id=none",
         *(str(path) for path in sorted(objects.glob("*.o"))),
         "-o", str(root / "generated-guest.o")], log=root / "compile" / "link.log")
    return len(files)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--xex", required=True, type=Path)
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("--project-root", type=Path, default=PROJECT)
    parser.add_argument("--xenonrecomp-root", type=Path)
    parser.add_argument("--xex1tool", type=Path)
    parser.add_argument("--ghidra-manifest", type=Path)
    args = parser.parse_args()
    if args.project_root.resolve() != PROJECT.resolve():
        raise BuildError("project-root must be the checked-in AC6 demo product")
    xex = args.xex.resolve()
    verify_xex(xex)
    root = args.build_root.resolve()
    root.mkdir(parents=True, exist_ok=True)
    checkout, commit = prepare_toolchain(args, root)
    analyse = root / "XenonRecomp-build/XenonAnalyse/XenonAnalyse"
    recomp = root / "XenonRecomp-build/XenonRecomp/XenonRecomp"
    switches = root / "demo-switch-tables.toml"
    run([str(analyse), str(xex), str(switches)], log=root / "analyse.log")
    imports_tool = resolve_xex1tool(args.xex1tool)
    pdata = pdata_functions(xex, imports_tool, root)
    confirmed_digests: dict[tuple[int, int], str] = {}
    ghidra_functions = confirmed_functions(args.ghidra_manifest, confirmed_digests)
    data_ranges = confirmed_data(args.ghidra_manifest, confirmed_digests)
    ghidra_document = json.loads(args.ghidra_manifest.read_text())
    ghidra_chunk_records = len(ghidra_document["chunks"])
    with (PROJECT / "config/confirmed-chunks.toml").open("rb") as stream:
        configured_function_records = len(tomllib.load(stream).get("function", []))
    functions = dict(pdata)
    functions.update(ghidra_functions)
    functions = sorted(functions.items())
    filtered_data_ranges: list[tuple[int, int]] = []
    for data_address, data_size in data_ranges:
        data_end = data_address + data_size
        containing = [
            (function_address, function_size)
            for function_address, function_size in functions
            if function_address <= data_address < function_address + function_size
        ]
        if containing:
            function_address, function_size = containing[0]
            function_end = function_address + function_size
            if data_end <= function_end:
                continue
            raise BuildError(f"qualified data range crosses function boundary: "
                             f"0x{data_address:X}")
        filtered_data_ranges.append((data_address, data_size))
    data_ranges = filtered_data_ranges
    pdata_addresses = {address for address, _ in pdata}
    occupied = [(address, size, "pdata-function" if address in pdata_addresses
                 else "qualified-function") for address, size in functions]
    occupied.extend((address, size, "data") for address, size in data_ranges)
    occupied.sort()
    for (left_address, left_size, left_kind), (right_address, right_size, right_kind) in zip(
            occupied, occupied[1:]):
        if left_address + left_size > right_address:
            # A qualified callable may be an inner entry of either a .pdata
            # owner or another explicitly qualified owner. This is required
            # for callback tables whose target is inside a bounded Ghidra
            # chunk; unrelated ranges must still remain disjoint.
            nested_inner = (left_kind in {"pdata-function", "qualified-function"}
                             and right_kind == "qualified-function"
                             and right_address > left_address
                             and right_address + right_size <= left_address + left_size)
            if nested_inner:
                continue
            raise BuildError(f"overlapping qualified {left_kind}/{right_kind} ranges: "
                             f"0x{left_address:X} and 0x{right_address:X}")
    verify_confirmed_bytes(root, functions + data_ranges, confirmed_digests)
    config = write_config(root, xex, switches, functions, data_ranges)
    generated = root / "generated"
    generated.mkdir(exist_ok=True)
    context = PROJECT / "tools/ppc_context_adapter.h"
    adapter = root / "ppc_context_adapter.h"
    shutil.copy2(context, adapter)
    if adapter.read_bytes() != context.read_bytes():
        raise BuildError("ppc_context_adapter copy mismatch")
    imports = imports_from_xex1tool(xex, imports_tool)
    run([str(recomp), str(config), str(adapter)], log=root / "recomp.log")
    source_count = syntax_check(root, checkout)
    symbols = generated_import_symbols(generated / "ppc_recomp_shared.h")
    variables = variable_import_names(checkout)
    function_imports, data_imports = classify_imports(imports, symbols, variables)
    import_stubs = write_import_stubs(root, imports, symbols, variables)
    compiled_count = compile_check(root, checkout, import_stubs)
    manifest = {
        "schema": "ac6-demo-codegen-manifest/v2",
        "target_id": GHIDRA_TARGET_ID,
        "xex_sha256": EXPECTED_XEX,
        "ghidra_manifest_sha256": sha256(args.ghidra_manifest.resolve()),
        "xenonrecomp_commit": commit,
        "switch_tables": sum(1 for line in switches.read_text().splitlines()
                              if line.startswith("[[switch]]")),
        "pdata_functions": len(pdata),
        "confirmed_functions": len(functions),
        "qualified_non_pdata_functions": len(ghidra_functions),
        "ghidra_chunks": ghidra_chunk_records,
        "configured_function_records": configured_function_records,
        "confirmed_data_ranges": len(data_ranges),
        "imports": imports,
        "import_stub_records": len(imports),
        "import_function_stubs": len(function_imports),
        "import_data_stub_records": len(data_imports),
        "generated_cpp_files": source_count,
        "compiled_cpp_files": compiled_count,
        "relocatable_guest_object": "generated-guest.o",
        "generated_output": "build-only",
        "boundary_diagnostics": 0,
        "unsupported_instructions": 0,
    }
    (root / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps({
        key: manifest[key] for key in (
            "schema", "target_id", "xex_sha256", "ghidra_manifest_sha256",
            "xenonrecomp_commit", "switch_tables", "pdata_functions",
            "confirmed_functions", "qualified_non_pdata_functions",
            "ghidra_chunks", "configured_function_records",
            "confirmed_data_ranges", "import_stub_records",
            "generated_cpp_files", "compiled_cpp_files",
            "boundary_diagnostics", "unsupported_instructions",
        )
    }, indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except BuildError as error:
        print(f"ac6-demo codegen refused: {error}", file=sys.stderr)
        raise SystemExit(1) from None
