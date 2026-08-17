#!/usr/bin/env python3
"""Create a fail-closed canonical Xenon Ghidra import for AC6 PAL.

The command refuses to merge with an existing project.  The generated Ghidra
database remains ignored; only the boundary export, journal and manifest are
durable evidence.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PORTFOLIO = ROOT.parents[1]
XEX_SHA256 = "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"
DATA_TBL_SHA256 = "82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5"
XEX_SIZE = 7_483_392
PROJECT_NAME = "ace-combat-6"
PROGRAM_NAME = "default.xex"
TARGET_ID = "ac6-pal-default-xex"
LANGUAGE = "PowerPC:BE:64:Xenon"
GHIDRA_VERSION = "12.1.2"
LOADER = "XEX Loader by Warranty Voider"
IMAGE_BASE = "0x82000000"
ENTRY_POINT = "0x821F5E90"
BOUNDARY_SCHEMA = "ac6.ghidra-function-boundaries.v1"


class ImportError(RuntimeError):
    """The import did not satisfy the binary-qualified contract."""


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def ghidra_version(home: Path) -> str:
    properties = home / "Ghidra/application.properties"
    if not properties.is_file():
        raise ImportError(f"Ghidra application.properties missing: {properties}")
    for line in properties.read_text(encoding="utf-8").splitlines():
        if line.startswith("application.version="):
            return line.split("=", 1)[1].strip()
    raise ImportError("Ghidra application.version missing")


def validate_inputs(xex: Path, ghidra_home: Path) -> Path:
    if not xex.is_file() or xex.stat().st_size != XEX_SIZE:
        raise ImportError("default.xex size mismatch")
    if sha256(xex) != XEX_SHA256:
        raise ImportError("default.xex SHA-256 mismatch")
    if ghidra_version(ghidra_home) != GHIDRA_VERSION:
        raise ImportError(f"Ghidra {GHIDRA_VERSION} is required")
    headless = ghidra_home / "support/analyzeHeadless"
    if not headless.is_file():
        raise ImportError(f"analyzeHeadless missing: {headless}")
    return headless


def validate_output(raw: dict[str, object]) -> int:
    expected = {
        "schema": BOUNDARY_SCHEMA,
        "project": PROJECT_NAME,
        "program": PROGRAM_NAME,
        "sha256": XEX_SHA256,
        "language": LANGUAGE,
    }
    for key, value in expected.items():
        if raw.get(key) != value:
            raise ImportError(f"boundary export {key} mismatch")
    count = raw.get("function_count")
    functions = raw.get("functions")
    if not isinstance(count, int) or not isinstance(functions, list) or count != len(functions):
        raise ImportError("boundary export function count mismatch")
    if count <= 0:
        raise ImportError("boundary export is empty")
    return count


def evidence_path(path: Path) -> str:
    """Use a repository-relative path when possible, otherwise an absolute one."""
    try:
        return path.relative_to(ROOT).as_posix()
    except ValueError:
        return str(path)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--xex", type=Path, default=ROOT / "game-files/default.xex")
    parser.add_argument(
        "--ghidra-home",
        type=Path,
        default=PORTFOLIO / ".tools/ghidra_12.1.2_PUBLIC",
    )
    parser.add_argument("--project-parent", type=Path, default=ROOT / "ghidra-projects")
    parser.add_argument(
        "--boundary-export",
        type=Path,
        default=ROOT / "analysis/ghidra/canonical-function-boundaries.json",
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=ROOT / "analysis/ghidra/canonical-import.json",
    )
    parser.add_argument(
        "--journal",
        type=Path,
        default=ROOT / "analysis/ghidra/canonical-import.log",
    )
    parser.add_argument("--analysis-timeout", type=int, default=900)
    parser.add_argument("--max-cpu", type=int, default=16)
    parser.add_argument(
        "--no-analysis",
        action="store_true",
        help="use loader-defined .pdata functions only; still requires Xenon",
    )
    args = parser.parse_args()

    try:
        xex = args.xex.resolve()
        ghidra_home = args.ghidra_home.resolve()
        headless = validate_inputs(xex, ghidra_home)
        parent = args.project_parent.resolve()
        parent.mkdir(parents=True, exist_ok=True)
        if (parent / f"{PROJECT_NAME}.gpr").exists() or (parent / f"{PROJECT_NAME}.rep").exists():
            raise ImportError("refusing to merge with an existing Ghidra project")

        boundary_export = args.boundary_export.resolve()
        boundary_export.parent.mkdir(parents=True, exist_ok=True)
        manifest = args.manifest.resolve()
        manifest.parent.mkdir(parents=True, exist_ok=True)
        journal = args.journal.resolve()
        journal.parent.mkdir(parents=True, exist_ok=True)
        if boundary_export.exists() or manifest.exists() or journal.exists():
            raise ImportError("refusing to overwrite import evidence")

        command = [
            str(headless), str(parent), PROJECT_NAME,
            "-import", str(xex), "-processor", LANGUAGE,
            "-analysisTimeoutPerFile", str(args.analysis_timeout),
            "-max-cpu", str(args.max_cpu),
            "-scriptPath", str(ROOT / "tools/ghidra"),
            "-postScript", "ExportOracleFunctionBoundaries.java", str(boundary_export),
        ]
        if args.no_analysis:
            command.insert(command.index("-analysisTimeoutPerFile"), "-noanalysis")
        completed = subprocess.run(
            command,
            cwd=PORTFOLIO,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        journal.write_text(completed.stdout, encoding="utf-8")
        if completed.returncode != 0:
            raise ImportError(f"analyzeHeadless failed; see {journal}")
        required = [
            f"Using Loader: {LOADER}",
            f"Using Language/Compiler: {LANGUAGE}:default",
            f"Imagebase address = {IMAGE_BASE}",
            f"Entry point address = {ENTRY_POINT}",
            "REPORT: Import succeeded",
            "AC6_ORACLE_BOUNDARY_EXPORT_PASS",
        ]
        if not args.no_analysis:
            required.append("REPORT: Analysis succeeded for file:")
        missing = [marker for marker in required if marker not in completed.stdout]
        if missing:
            raise ImportError("import journal lacks: " + ", ".join(missing))
        raw = json.loads(boundary_export.read_text(encoding="utf-8"))
        function_count = validate_output(raw)
        script_paths = [Path(__file__), ROOT / "tools/ghidra/ExportOracleFunctionBoundaries.java"]
        script_hashes = {
            path.relative_to(ROOT).as_posix(): sha256(path) for path in script_paths
        }
        result = {
            "schema": "ac6-ghidra-canonical-import.v1",
            "target_id": TARGET_ID,
            "project_path": "ghidra-projects/ace-combat-6",
            "project": PROJECT_NAME,
            "program": PROGRAM_NAME,
            "module": PROGRAM_NAME,
            "xex_sha256": XEX_SHA256,
            "xex_size": XEX_SIZE,
            "data_tbl_sha256": DATA_TBL_SHA256,
            "image_base": IMAGE_BASE,
            "entry_point": ENTRY_POINT,
            "ghidra": {
                "version": GHIDRA_VERSION,
                "loader": LOADER,
                "language": LANGUAGE,
                "compiler_spec": "default",
            },
            "analysis": "loader-only" if args.no_analysis else "auto",
            "function_count": function_count,
            "boundary_export": evidence_path(boundary_export),
            "boundary_export_sha256": sha256(boundary_export),
            "import_journal": evidence_path(journal),
            "import_journal_sha256": sha256(journal),
            "script_sha256": script_hashes,
            "historical_project": "ghidra-projects/historical-a2alt-20260814",
            "merge_policy": "A2ALT-32addr is historical and must not be merged",
        }
        manifest.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0
    except (ImportError, OSError, json.JSONDecodeError) as error:
        parser.error(str(error))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
