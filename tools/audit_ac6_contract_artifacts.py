#!/usr/bin/env python3
"""Check that every artefact a contract cites is committed and matches HEAD.

The gate auditor hashes files in the working tree. That is the right thing for
it to do - it is auditing what is there - but it leaves one gap it cannot see:
an artefact that was regenerated and never staged. The contract's hash gets
refreshed against the file on disk, the gate passes locally, and a fresh clone
fails because the committed artefact is the old one.

That happened once (the NTXR decode artefact, after the byte-swap control changed
what the test writes) and was caught by reading `git status`, not by the gate.
This makes the check routine instead of lucky.

Exits 0 when every cited path is in HEAD and identical to the working tree, 1
when any drifts or is missing, and 77 when this is not a git repository - the
same skip convention the native tests use.

usage: audit_ac6_contract_artifacts.py CONTRACT [CONTRACT...]
"""

import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path


def cited_paths(contract: Path) -> set[str]:
    text = contract.read_text(encoding="utf-8")
    json.loads(text)  # a malformed contract is a failure, not a skip
    return {match.group(1) for match in re.finditer(r'"path": "([^"]+)"', text)}


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: audit_ac6_contract_artifacts.py CONTRACT [CONTRACT...]", file=sys.stderr)
        return 2
    if subprocess.run(["git", "rev-parse", "--git-dir"],
                      capture_output=True).returncode != 0:
        print("contract_artifacts=skip reason=not a git repository")
        return 77

    paths: set[str] = set()
    for name in sys.argv[1:]:
        paths |= cited_paths(Path(name))

    drift: list[str] = []
    missing: list[tuple[str, str]] = []
    matched = 0
    for path in sorted(paths):
        target = Path(path)
        if not target.exists():
            missing.append((path, "absent from the working tree"))
            continue
        committed = subprocess.run(["git", "show", f"HEAD:{path}"], capture_output=True)
        if committed.returncode != 0:
            missing.append((path, "not committed"))
            continue
        if hashlib.sha256(target.read_bytes()).hexdigest() != \
           hashlib.sha256(committed.stdout).hexdigest():
            drift.append(path)
        else:
            matched += 1

    for path in drift:
        print(f"contract_artifacts=fail reason=drift path={path}")
    for path, why in missing:
        print(f"contract_artifacts=fail reason={why.replace(' ', '_')} path={path}")
    if drift or missing:
        return 1
    print(f"contract_artifacts=pass cited={len(paths)} match_head={matched}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
