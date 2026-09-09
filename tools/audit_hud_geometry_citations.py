#!/usr/bin/env python3
"""Check that hardcoded HUD pixel-rectangle geometry cites a retail trace.

A HUD/overlay draw call with four or more consecutive float-literal pixel
coordinates (the `add_outline_px(0U, 16.0F, 608.0F, 316.0F, 704.0F, 2.0F)`
shape) is a hand-picked screen-space rectangle. That is fine for a diagnostic
tool, but nothing stops it - or an equivalent - from being read as retail HUD
evidence once it sits in a gated delivery path. CLAUDE.md's HUD rule requires
any such file to carry a citation to a report documenting why the coordinates
are diagnostic-only, not retail-qualified.

This scans for the literal-geometry shape and flags a file that has it but no
`reports/` citation anywhere in the file. It does not parse C++; a citation
comment anywhere in the file satisfies it, matching how contract citations are
checked elsewhere in this repo (see audit_ac6_contract_artifacts.py).

Exits 0 when every file with hardcoded HUD-style geometry also cites a report,
1 when any does not, and 77 when this is not a git repository - the same skip
convention the native tests use.

usage: audit_hud_geometry_citations.py [ROOT ...]
  ROOT defaults to `recompilation` (the gated delivery path) if none given.
"""

import re
import subprocess
import sys
from pathlib import Path

# Four-or-more consecutive `N.NF` pixel-literal arguments to a call whose name
# suggests screen-space HUD/overlay geometry: the
# add_outline_px(0U, 16.0F, 608.0F, 316.0F, 704.0F, 2.0F) shape. Scoping by
# call name (not just "four floats in a row") avoids flagging ordinary test
# fixtures and matrix/vector literals, which share the same argument shape
# without being hand-picked screen coordinates.
_HUD_CALL_NAME = re.compile(r"(?i)(rect|outline|panel|hud|overlay|marker|reticle|radar)\w*\s*\(")
_GEOMETRY_LITERALS = re.compile(
    r"(?:[-\d.]+[Ff]\s*,\s*){3}[-\d.]+[Ff]"
)
_CITATION = re.compile(r"reports/[\w./-]+\.md")

# Vendored/generated code is not this repo's own HUD authorship and is out of
# scope: third-party libraries (e.g. imgui, pulled in for debug UI) and build
# output directories, which mirror source trees this tool already scans.
_EXCLUDED_PARTS = frozenset({"thirdparty", "build", "upstream"})


def is_excluded(path: Path) -> bool:
    return not _EXCLUDED_PARTS.isdisjoint(path.parts)


def hud_call_sites(text: str):
    for match in _HUD_CALL_NAME.finditer(text):
        yield text[match.end():match.end() + 200]


def is_git_repo() -> bool:
    result = subprocess.run(
        ["git", "rev-parse", "--is-inside-work-tree"],
        capture_output=True, text=True, check=False,
    )
    return result.returncode == 0 and result.stdout.strip() == "true"


def has_hardcoded_geometry(text: str) -> bool:
    return any(_GEOMETRY_LITERALS.search(tail) for tail in hud_call_sites(text))


def has_citation(text: str) -> bool:
    return _CITATION.search(text) is not None


def main(argv: list[str]) -> int:
    if not is_git_repo():
        return 77

    roots = [Path(root) for root in argv] or [Path("recompilation")]
    violations: list[Path] = []
    scanned = 0

    for root in roots:
        if not root.exists():
            continue
        for path in sorted(root.rglob("*")):
            if path.suffix not in (".cpp", ".cc", ".h", ".hpp"):
                continue
            if not path.is_file() or is_excluded(path):
                continue
            text = path.read_text(encoding="utf-8", errors="ignore")
            scanned += 1
            if has_hardcoded_geometry(text) and not has_citation(text):
                violations.append(path)

    if violations:
        print(
            f"hud_geometry_citations=fail scanned={scanned} "
            f"violations={len(violations)}"
        )
        for path in violations:
            print(f"  {path}: hardcoded HUD-style pixel geometry, no reports/*.md citation")
        return 1

    print(f"hud_geometry_citations=pass scanned={scanned} violations=0")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
