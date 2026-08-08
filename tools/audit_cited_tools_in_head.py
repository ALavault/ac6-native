#!/usr/bin/env python3
"""Check that every `tools/...` path cited by a tracked report or contract is
actually in HEAD.

A cycle report is a claim plus the command that produced it. If the command is
not in HEAD, a fresh clone cannot rerun the cycle, and the claim has become
unfalsifiable -- which is the one thing this campaign is not allowed to ship.

This is the sibling of audit_ac6_contract_artifacts.py. That one checks the
OUTPUTS a contract cites; this one checks the INSTRUMENTS a report cites. The
gap between them is real: cycle 1083 published two corrections derived from
tools/ac6_parser_inspect.py while calling it, in its own words, "la liste de
candidats non suivie" -- it knew, said so, and shipped anyway. Nothing checked.

KNOWN_ABSENT below records paths that are cited and will never be in HEAD,
each with the reason. They are not tolerated silently: an entry here is a
statement that the cycle is not reproducible in this repository and why. Adding
one is a decision to be written in a cycle report, not a way to get the tool
green.

usage: audit_cited_tools_in_head.py [--root=DIR]
exit 0 when every cited tool is in HEAD or explicitly known-absent.
"""

import os
import re
import subprocess
import sys

# Paths cited by tracked reports that are not in HEAD and cannot be, with the
# reason each is out of reach. Everything here predates the native
# reconstruction: these cycles ran in an earlier workspace whose layout is gone.
KNOWN_ABSENT = {
    "tools/ac6-run.sh": "prior workspace (bridge era); the launcher now lives in scripts/",
    "tools/.../ac6-gapfill/tools/ac6-run.sh": "elided path inside a prior worktree",
    "tools/ac6-recomp-reference/.claude/worktrees/ac6-gapfill/tools/ac6-run.sh":
        "prior worktree of the recomp reference checkout",
    "tools/ac6-recomp-reference/.claude/worktrees/ac6-gapfill/tools/ac6_mode1_codec.py":
        "prior worktree of the recomp reference checkout",
    "tools/ac6-recomp-reference/.claude/worktrees/ac6-gapfill/tools/extract_ac6_pac.py":
        "prior worktree of the recomp reference checkout",
    "tools/entry9_static_inventory.py": "prior workspace, entry-9 inventory era",
    "tools/parse_ac6_swg.py": "prior workspace, SWG probe era",
    "tools/render-current-handoff.py": "prior workspace, handoff-rendering era",
    "tools/scan_ac6_message_symbols.py": "prior workspace, message-symbol era",
    "tools/test_ac6_xenia_launcher_status.py": "prior workspace, launcher-status era",
    "tools/verify_inputs.py": "prior workspace, input-qualification era",
}

DOC_ROOTS = ("reports/", "analysis/contracts/")
DOC_FILES = ("CLAUDE.md", "AGENTS.md", "MISSION01_LADDER.md",
             "INSTRUMENT_DISCIPLINE.md")

# .yml and .yaml are in this list because leaving them out was a real miss:
# reports/ac6-agent-quality-tooling-20260808.md cites three
# tools/quality/*.yml rule files, and the first version of this checker --
# written the same evening -- scanned only py/java/sh and reported pass over a
# report whose entire reproduction section pointed at untracked files. A
# configuration that decides what a scan reports is an instrument.
TOOL_PATH = re.compile(r"tools/[A-Za-z0-9_./-]+\.(?:py|java|sh|yml|yaml)")


def git(*args, root="."):
    return subprocess.run(("git",) + args, capture_output=True, text=True,
                          cwd=root).stdout.split()


def main() -> int:
    root = "."
    for arg in sys.argv[1:]:
        if arg.startswith("--root="):
            root = arg.split("=", 1)[1]

    tracked = git("ls-files", root=root)
    in_head = set(git("ls-tree", "-r", "--name-only", "HEAD", root=root))
    if not tracked:
        print("cited_tools_in_head=error not a git repository or empty tree")
        return 1

    sources = [f for f in tracked
               if f.startswith(DOC_ROOTS) or f in DOC_FILES]

    cited = {}
    for name in sources:
        try:
            with open(os.path.join(root, name), encoding="utf-8",
                      errors="ignore") as handle:
                text = handle.read()
        except OSError:
            continue
        for path in TOOL_PATH.findall(text):
            cited.setdefault(path, set()).add(name)

    missing = {p: c for p, c in cited.items()
               if p not in in_head and p not in KNOWN_ABSENT}
    for path, citers in sorted(missing.items()):
        shown = sorted(citers)[:2]
        extra = f" (+{len(citers) - 2} more)" if len(citers) > 2 else ""
        print(f"  NOT IN HEAD  {path}  cited by {', '.join(shown)}{extra}")

    # A KNOWN_ABSENT entry that has become reachable is also a finding: the
    # exemption is stale and should be deleted rather than left as a lie.
    stale = sorted(p for p in KNOWN_ABSENT if p in in_head)
    for path in stale:
        print(f"  STALE EXEMPTION  {path} is in HEAD; remove it from KNOWN_ABSENT")

    status = "pass" if not missing and not stale else "fail"
    print(f"cited_tools_in_head={status} cited={len(cited)} "
          f"in_head={len(cited) - len(missing) - len(KNOWN_ABSENT & cited.keys())} "
          f"known_absent={len(KNOWN_ABSENT & cited.keys())} "
          f"missing={len(missing)} stale_exemptions={len(stale)}")
    return 0 if status == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
