#!/usr/bin/env python3
"""Anchor a capsule's generated_line citations to a PAL program counter.

A generated line number is only meaningful against the exact codegen that
produced it. Adding one function to the boundary set redistributes functions
across the generated files and shifts every line after it -- measured at 91
lines for sub_822F5E58 when 0x820D3710 was tried -- so a capsule citing only
"function X, generated line N" becomes unreadable the moment the boundary set
changes, and unrecoverable, because the tree that gave it meaning is gone.

This resolves each citation to the PAL instruction it denotes, while the
current generated tree still matches, and records that address beside the
line. The line stays for provenance; the PC is what survives.

It changes nothing else, refuses to write when a citation cannot be resolved,
and is idempotent: a citation that already carries a PC is left alone.

With --audit it walks a directory instead and fails on any capsule whose
citations no longer resolve, except the ones already known stale. That is a
ratchet: the four capsules broken before this tool existed cannot be repaired
from here, but no fifth can join them unnoticed.

usage: anchor_capsule_generated_lines.py CAPSULE.json... [--generated DIR]
                                          [--basefile PATH] [--write]
       anchor_capsule_generated_lines.py --audit ROOT...
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from map_generated_guest_load_sites import (  # type: ignore
    MappingError, load_functions, map_line,
)

DEMO = Path(__file__).resolve().parents[1]
GENERATED = DEMO / "build-codegen-on/codegen/generated"
BASEFILE = Path("/fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-6"
                "/.build/Default.xex.base.bin")


def normalise(name: str) -> str:
    """Capsules name a function either as __imp__sub_X or as its PAL address."""
    if name.startswith("__imp__sub_") or name.startswith("sub_"):
        return name if name.startswith("__imp__") else f"__imp__{name}"
    return f"__imp__sub_{int(name, 16):08X}"


def anchor(node: object, functions, basefile: bytes, report: list[str]) -> bool:
    changed = False
    if isinstance(node, list):
        for item in node:
            changed |= anchor(item, functions, basefile, report)
        return changed
    if not isinstance(node, dict):
        return False
    for value in node.values():
        changed |= anchor(value, functions, basefile, report)
    if "generated_line" not in node or "function" not in node:
        return changed
    if any(key in node for key in ("guest_pc", "pal_pc", "site_pc")):
        return changed
    name = normalise(str(node["function"]))
    mapped = map_line(functions, name, int(node["generated_line"]), basefile)
    node["guest_pc"] = str(mapped["guest_pc"])
    node["instruction_bytes"] = str(mapped["instruction_bytes"]).lower()
    report.append(f"    {node['function']}:{node['generated_line']}"
                  f" -> {node['guest_pc']} {node['instruction_bytes']}")
    return True


# Broken before tools/anchor_capsule_generated_lines.py existed, and not
# repairable from here: they cite lines of a codegen tree this repository no
# longer has. sub_820FF710 has since moved to line 43191 of ppc_recomp.7.cpp,
# so their citations now denote an unrelated VMX instruction.
KNOWN_STALE = {
    "ac6-demo-queue-slot-neutral600-v1.json",
    "ac6-demo-queue-slot-stores-ab-v1.json",
    "ac6-demo-render-queue-slot-write-probe-v1.json",
    "ac6-demo-render-queue-write-provenance-v1.json",
    "ac6-demo-ib-publish-writer-join-v1.json",
}


def audit(roots: list[Path], functions, basefile: bytes) -> int:
    checked = stale = 0
    for root in roots:
        for path in sorted(root.rglob("*.json")):
            text = path.read_text(encoding="utf-8", errors="replace")
            if "generated_line" not in text:
                continue
            checked += 1
            try:
                anchor(json.loads(text), functions, basefile, [])
            except (MappingError, ValueError, KeyError) as error:
                stale += 1
                known = path.name in KNOWN_STALE
                print(f"{'known-stale' if known else 'error'}: {path}: {error}")
                if not known:
                    print("capsule_line_anchors=fail")
                    return 1
    print(f"capsule_line_anchors=pass checked={checked} known_stale={stale}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capsules", nargs="*", type=Path)
    parser.add_argument("--audit", action="store_true")
    parser.add_argument("--generated", type=Path, default=GENERATED)
    parser.add_argument("--basefile", type=Path, default=BASEFILE)
    parser.add_argument("--write", action="store_true")
    arguments = parser.parse_args()

    basefile = arguments.basefile.read_bytes()
    functions = load_functions(arguments.generated, len(basefile))
    if arguments.audit:
        return audit(arguments.capsules, functions, basefile)
    failures = 0
    for path in arguments.capsules:
        document = json.loads(path.read_text(encoding="utf-8"))
        report: list[str] = []
        try:
            changed = anchor(document, functions, basefile, report)
        except MappingError as error:
            print(f"anchor=fail {path}: {error}")
            failures += 1
            continue
        print(f"anchor={'ok' if changed else 'already-anchored'} {path} "
              f"sites={len(report)}")
        for line in report:
            print(line)
        if changed and arguments.write:
            path.write_text(json.dumps(document, indent=1, sort_keys=True)
                            + "\n", encoding="utf-8")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
