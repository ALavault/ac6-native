#!/usr/bin/env python3
"""Record deterministic checkpoint-1 evidence from two clean imports/builds."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
WORKSPACE = PROJECT.parents[1]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def tree_digest(root: Path) -> tuple[str, int]:
    digest = hashlib.sha256()
    files = sorted(path for path in root.rglob("*") if path.is_file())
    for path in files:
        relative = path.relative_to(root).as_posix().encode("utf-8")
        digest.update(len(relative).to_bytes(4, "big"))
        digest.update(relative)
        digest.update(bytes.fromhex(sha256(path)))
    return digest.hexdigest(), len(files)


def require_equal(left: Path, right: Path, label: str) -> str:
    left_digest = sha256(left)
    right_digest = sha256(right)
    if left_digest != right_digest:
        raise ValueError(f"{label} differs between clean runs")
    return left_digest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--codegen-a", type=Path, required=True)
    parser.add_argument("--codegen-b", type=Path, required=True)
    parser.add_argument("--import-b-manifest", type=Path, required=True)
    parser.add_argument("--import-b-journal", type=Path, required=True)
    parser.add_argument(
        "--output", type=Path,
        default=WORKSPACE / "analysis/demo/ac6-demo-foundation-evidence.json",
    )
    args = parser.parse_args()
    codegen_a = args.codegen_a.resolve()
    codegen_b = args.codegen_b.resolve()
    canonical_manifest = WORKSPACE / "analysis/demo/ac6-demo-ghidra-manifest.json"
    canonical_journal = WORKSPACE / "analysis/demo/ac6-demo-ghidra-import-journal.json"
    ghidra = json.loads(canonical_manifest.read_text(encoding="utf-8"))
    codegen = json.loads((codegen_a / "manifest.json").read_text(encoding="utf-8"))
    import_manifest_digest = require_equal(
        canonical_manifest, args.import_b_manifest.resolve(), "Ghidra manifest"
    )
    import_journal_digest = require_equal(
        canonical_journal, args.import_b_journal.resolve(), "Ghidra journal"
    )
    codegen_manifest_digest = require_equal(
        codegen_a / "manifest.json", codegen_b / "manifest.json", "codegen manifest"
    )
    guest_object_digest = require_equal(
        codegen_a / "generated-guest.o", codegen_b / "generated-guest.o",
        "relocatable guest object",
    )
    generated_a, generated_files = tree_digest(codegen_a / "generated")
    generated_b, _ = tree_digest(codegen_b / "generated")
    objects_a, object_files = tree_digest(codegen_a / "objects")
    objects_b, _ = tree_digest(codegen_b / "objects")
    if generated_a != generated_b or objects_a != objects_b:
        raise ValueError("generated source/object tree differs between clean runs")
    if codegen.get("ghidra_manifest_sha256") != import_manifest_digest:
        raise ValueError("codegen did not consume the canonical Ghidra manifest")
    if codegen.get("boundary_diagnostics") != 0 or codegen.get("unsupported_instructions") != 0:
        raise ValueError("codegen diagnostics remain open")
    report = {
        "schema": "ac6-demo-foundation-evidence/v1",
        "target_id": "ac6-demo-xbox360-pal",
        "supported": False,
        "identity": {
            "xex_sha256": ghidra["xex_sha256"],
            "image_base": ghidra["image_base"],
            "entry_point": ghidra["entry_point"],
        },
        "ghidra": {
            "project_path": ghidra["project_path"],
            "language": ghidra["language"],
            "version": ghidra["ghidra"]["version"],
            "loader": ghidra["ghidra"]["loader"],
            "chunks": len(ghidra["chunks"]),
            "data_ranges": len(ghidra["data_ranges"]),
            "manifest_sha256": import_manifest_digest,
            "journal_sha256": import_journal_digest,
            "independent_imports_byte_identical": True,
        },
        "codegen": {
            key: codegen[key] for key in (
                "xenonrecomp_commit", "switch_tables", "pdata_functions",
                "confirmed_functions", "qualified_non_pdata_functions",
                "ghidra_chunks", "configured_function_records",
                "confirmed_data_ranges", "import_stub_records",
                "generated_cpp_files", "compiled_cpp_files",
                "boundary_diagnostics", "unsupported_instructions",
            )
        },
        "reproducibility": {
            "clean_codegen_runs": 2,
            "manifest_sha256": codegen_manifest_digest,
            "generated_tree_sha256": generated_a,
            "generated_files": generated_files,
            "object_tree_sha256": objects_a,
            "object_files": object_files,
            "relocatable_guest_object_sha256": guest_object_digest,
            "byte_identical": True,
        },
    }
    args.output.resolve().parent.mkdir(parents=True, exist_ok=True)
    args.output.resolve().write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print("AC6_DEMO_FOUNDATION_EVIDENCE_PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
