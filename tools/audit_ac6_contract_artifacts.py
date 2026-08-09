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

Exits 0 when every cited path in every current contract is in HEAD and identical
to the working tree, 1 when any drifts or is missing, and 77 when this is not a
git repository - the same skip convention the native tests use. A contract with
``superseded_by`` is retained as history and excluded only after its replacement
path has been resolved and parsed.

It also checks the hash tables capture bundles write into their README - rows of
the form `| `file` | `sha256` |`. Those index real artefacts and are audited by
nothing else: a contract cites the JSON inside a bundle, not the README beside
it, so a hand-edited digest in one can sit wrong indefinitely. One did.

usage: audit_ac6_contract_artifacts.py CONTRACT [CONTRACT...]
"""

import hashlib
import re
import subprocess
import sys
from pathlib import Path

from contract_audit_scope import ContractScopeError, current_contracts, print_superseded

SHA256 = re.compile(r"[0-9a-f]{64}")


def cited_paths(document: dict) -> set[str]:
    """Paths of evidence entries only.

    A contract carries `"path"` in places that are not artefacts - provenance
    fields naming a workspace root, for instance. Scraping every `"path"` string
    made this tool pass on three contracts and fail on the rest, which is the
    same too-narrow-a-frame mistake it exists to catch. So walk the parsed
    document and take only entries that look like evidence: a dict with both a
    `path` and a `sha256`.
    """
    found: set[str] = set()

    def walk(node: object) -> None:
        if isinstance(node, dict):
            path, digest = node.get("path"), node.get("sha256")
            if isinstance(path, str) and isinstance(digest, str) and SHA256.fullmatch(digest):
                found.add(path)
            for value in node.values():
                walk(value)
        elif isinstance(node, list):
            for value in node:
                walk(value)

    walk(document)
    return found


# `| `name` | `64 hex` |` - the row shape the capture bundles use.
README_ROW = re.compile(r"^\|\s*`([^`]+)`\s*\|\s*`([0-9a-f]{64})`\s*\|", re.M)


def check_readme_tables(root: Path) -> tuple[int, list[str]]:
    """Verify every `file -> sha256` row a bundle README declares."""
    checked = 0
    wrong: list[str] = []
    for readme in sorted(root.glob("reports/**/README.md")):
        text = readme.read_text(encoding="utf-8", errors="replace")
        for name, digest in README_ROW.findall(text):
            target = readme.parent / name
            if not target.is_file():
                continue  # rows naming external inputs are not this tool's business
            checked += 1
            actual = hashlib.sha256(target.read_bytes()).hexdigest()
            if actual != digest:
                wrong.append(f"{readme}: {name} declared {digest[:16]}... is {actual[:16]}...")
    return checked, wrong


def main() -> int:
    args = [a for a in sys.argv[1:] if not a.startswith("--artifact-root")]
    roots = [a.split("=", 1)[1] for a in sys.argv[1:] if a.startswith("--artifact-root=")]
    # Older contracts cite paths relative to the capture directory rather than
    # the repository root. The gate auditor takes --artifact-root for the same
    # reason; this resolves against each candidate and takes the first that
    # exists, so one invocation can cover contracts written to either
    # convention. A path that resolves under none of them is a real failure.
    roots = roots or ["."]
    roots = roots + ["reports/mission01-native-captures"]
    if not args:
        print("usage: audit_ac6_contract_artifacts.py [--artifact-root=DIR] CONTRACT...",
              file=sys.stderr)
        return 2
    if subprocess.run(["git", "rev-parse", "--git-dir"],
                      capture_output=True).returncode != 0:
        print("contract_artifacts=skip reason=not a git repository")
        return 77

    try:
        active, superseded = current_contracts(args)
    except ContractScopeError as exc:
        print(f"contract_artifacts=fail reason=invalid_scope detail={exc}")
        return 1
    print_superseded(superseded)

    paths: set[str] = set()
    for contract in active:
        paths |= cited_paths(contract.document)

    drift: list[str] = []
    missing: list[tuple[str, str]] = []
    matched = 0
    for path in sorted(paths):
        target = next((Path(r) / path for r in roots if (Path(r) / path).exists()),
                      Path(path))
        path = str(target)
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
    rows, wrong = check_readme_tables(Path("."))
    for line in wrong:
        print(f"readme_hash=fail {line}")
    if wrong:
        return 1
    print(f"contract_artifacts=pass contracts={len(active)} "
          f"superseded={len(superseded)} cited={len(paths)} "
          f"match_head={matched} readme_rows={rows}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
