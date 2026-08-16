#!/usr/bin/env python3
"""Create one fail-closed Xenon Ghidra import and its qualified manifest."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
WORKSPACE = PROJECT.parents[1]
PORTFOLIO = WORKSPACE.parents[1]
EXPECTED_XEX = "de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8"
EXPECTED_SIZE = 1_454_080
TARGET_ID = "ac6-demo-xbox360-pal"
PROJECT_NAME = "ace-combat-6-demo"
PROGRAM_NAME = "Default.xex"
LANGUAGE = "PowerPC:BE:64:Xenon"
GHIDRA_VERSION = "12.1.2"
LOADER = "XEX Loader by Warranty Voider"
CANONICAL_PROJECT_PATH = "ghidra-projects/ace-combat-6-demo"
EXPORT_SCRIPT = PROJECT / "tools/ghidra/ExportQualifiedDemoChunks.java"
CONFIRMED_CHUNKS = PROJECT / "config/confirmed-chunks.toml"


class ImportError(RuntimeError):
    """The import was not qualified for the pinned demo."""


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def canonical_json(document: object) -> str:
    return json.dumps(document, indent=2, sort_keys=True) + "\n"


def ghidra_version(ghidra_home: Path) -> str:
    properties = ghidra_home / "Ghidra/application.properties"
    if not properties.is_file():
        raise ImportError(f"Ghidra application properties not found: {properties}")
    for line in properties.read_text(encoding="utf-8").splitlines():
        if line.startswith("application.version="):
            return line.split("=", 1)[1].strip()
    raise ImportError("Ghidra application.version is missing")


def verify_inputs(xex: Path, ghidra_home: Path) -> Path:
    if not xex.is_file() or xex.stat().st_size != EXPECTED_SIZE or sha256(xex) != EXPECTED_XEX:
        raise ImportError("Default.xex identity mismatch")
    if ghidra_version(ghidra_home) != GHIDRA_VERSION:
        raise ImportError(f"Ghidra {GHIDRA_VERSION} is required")
    headless = ghidra_home / "support/analyzeHeadless"
    if not headless.is_file():
        raise ImportError(f"analyzeHeadless not found: {headless}")
    return headless


def script_hashes() -> dict[str, str]:
    paths = [
        Path(__file__).resolve(),
        EXPORT_SCRIPT,
        PROJECT / "tools/ghidra/InspectDemoBoundary.java",
        PROJECT / "tools/ghidra/QualifyDemoChunk.java",
        PROJECT / "tools/ghidra/ValidateDemoBoundarySet.java",
        CONFIRMED_CHUNKS,
    ]
    return {path.relative_to(PROJECT).as_posix(): sha256(path) for path in paths}


def validate_log(log: str) -> dict[str, int]:
    required = [
        f"Using Loader: {LOADER}",
        f"Using Language/Compiler: {LANGUAGE}:default",
        "Entry point address = 0x821A7160",
        "Imagebase address = 0x82000000",
        "REPORT: Analysis succeeded for file:",
        "REPORT: Import succeeded",
    ]
    missing = [marker for marker in required if marker not in log]
    if missing:
        raise ImportError("Ghidra import journal lacks: " + ", ".join(missing))
    match = re.search(
        r"AC6_DEMO_GHIDRA_CHUNKS_PASS pdata=(\d+) chunks=(\d+) "
        r"data_ranges=(\d+) skipped=(\d+)", log
    )
    if match is None:
        raise ImportError("qualified export completion marker is missing")
    boundary_match = re.search(r"AC6_DEMO_BOUNDARY_SET_PASS count=(\d+)", log)
    if boundary_match is None:
        raise ImportError("canonical boundary-set marker is missing")
    pdata, chunks, data_ranges, skipped = (int(value) for value in match.groups())
    return {
        "pdata_functions": pdata,
        "chunks": chunks,
        "data_ranges": data_ranges,
        "skipped_chunks": skipped,
        "requalified_boundaries": int(boundary_match.group(1)),
    }


def qualify_export(raw: dict[str, object], journal_sha256: str,
                   hashes: dict[str, str]) -> dict[str, object]:
    required = {
        "schema": "ac6-demo-ghidra-chunks.export.v2",
        "target_id": TARGET_ID,
        "project": PROJECT_NAME,
        "program": PROGRAM_NAME,
        "module": PROGRAM_NAME,
        "language": LANGUAGE,
        "xex_sha256": EXPECTED_XEX,
    }
    for key, expected in required.items():
        if raw.get(key) != expected:
            raise ImportError(f"raw Ghidra export has unqualified {key}")
    result = dict(raw)
    result["schema"] = "ac6-demo-ghidra-chunks.v2"
    result["project_path"] = CANONICAL_PROJECT_PATH
    result["ghidra"] = {
        "version": GHIDRA_VERSION,
        "loader": LOADER,
        "language": LANGUAGE,
        "compiler_spec": "default",
    }
    result["script_sha256"] = hashes
    result["import_journal"] = {
        "schema": "ac6-demo-ghidra-import-journal/v1",
        "sha256": journal_sha256,
    }
    return result


def run_import(args: argparse.Namespace) -> tuple[dict[str, object], dict[str, object]]:
    xex = args.xex.resolve()
    ghidra_home = args.ghidra_home.resolve()
    headless = verify_inputs(xex, ghidra_home)
    project_parent = args.project_parent.resolve()
    project_parent.mkdir(parents=True, exist_ok=True)
    if ((project_parent / f"{PROJECT_NAME}.gpr").exists() or
            (project_parent / f"{PROJECT_NAME}.rep").exists()):
        raise ImportError("refusing to merge with an existing Ghidra project")
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    raw_output = output.with_name(output.name + ".raw-export")
    raw_log = args.raw_log.resolve() if args.raw_log else (
        project_parent / f"{PROJECT_NAME}.import.log"
    )
    command = [
        str(headless), str(project_parent), PROJECT_NAME,
        "-import", str(xex),
        "-processor", LANGUAGE,
        "-analysisTimeoutPerFile", str(args.analysis_timeout),
        "-scriptPath", str(PROJECT / "tools/ghidra"),
        "-postScript", "ValidateDemoBoundarySet.java", str(CONFIRMED_CHUNKS),
        "-postScript", EXPORT_SCRIPT.name, str(raw_output), str(CONFIRMED_CHUNKS),
    ]
    completed = subprocess.run(
        command, cwd=PORTFOLIO, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, check=False,
    )
    raw_log.write_text(completed.stdout, encoding="utf-8")
    if completed.returncode != 0:
        raise ImportError(f"analyzeHeadless failed; see {raw_log}")
    counts = validate_log(completed.stdout)
    try:
        raw = json.loads(raw_output.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ImportError(f"invalid raw Ghidra export: {error}") from error
    hashes = script_hashes()
    journal: dict[str, object] = {
        "schema": "ac6-demo-ghidra-import-journal/v1",
        "target_id": TARGET_ID,
        "project": PROJECT_NAME,
        "canonical_project_path": CANONICAL_PROJECT_PATH,
        "program": PROGRAM_NAME,
        "xex_sha256": EXPECTED_XEX,
        "ghidra_version": GHIDRA_VERSION,
        "loader": LOADER,
        "language": LANGUAGE,
        "compiler_spec": "default",
        "image_base": "0x82000000",
        "entry_point": "0x821A7160",
        "sections": raw.get("text"),
        "pdata": raw.get("pdata"),
        "export_counts": counts,
        "script_sha256": hashes,
    }
    journal_text = canonical_json(journal)
    journal_digest = hashlib.sha256(journal_text.encode("utf-8")).hexdigest()
    manifest = qualify_export(raw, journal_digest, hashes)
    args.journal.resolve().parent.mkdir(parents=True, exist_ok=True)
    args.journal.resolve().write_text(journal_text, encoding="utf-8")
    output.write_text(canonical_json(manifest), encoding="utf-8")
    raw_output.unlink(missing_ok=True)
    return manifest, journal


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xex", type=Path, required=True)
    parser.add_argument(
        "--ghidra-home", type=Path,
        default=PORTFOLIO / ".tools/ghidra_12.1.2_PUBLIC",
    )
    parser.add_argument(
        "--project-parent", type=Path, default=WORKSPACE / "ghidra-projects",
        help="parent that will receive ace-combat-6-demo.gpr/.rep",
    )
    parser.add_argument(
        "--output", type=Path,
        default=WORKSPACE / "analysis/demo/ac6-demo-ghidra-manifest.json",
    )
    parser.add_argument(
        "--journal", type=Path,
        default=WORKSPACE / "analysis/demo/ac6-demo-ghidra-import-journal.json",
    )
    parser.add_argument("--raw-log", type=Path)
    parser.add_argument("--analysis-timeout", type=int, default=900)
    args = parser.parse_args()
    if args.analysis_timeout <= 0:
        parser.error("--analysis-timeout must be positive")
    return args


def main() -> int:
    try:
        manifest, _ = run_import(parse_args())
    except ImportError as error:
        print(f"ac6-demo Ghidra import refused: {error}", file=sys.stderr)
        return 1
    print(
        "AC6_DEMO_GHIDRA_IMPORT_PASS "
        f"chunks={len(manifest.get('chunks', []))} "
        f"data_ranges={len(manifest.get('data_ranges', []))}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
