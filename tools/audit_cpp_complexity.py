#!/usr/bin/env python3
"""Deterministic, dependency-free C/C++ size budget audit."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
EXCLUDED = {".git", "build", "generated", "thirdparty", "third_party", "external", "vendor"}
LIMITS = {"source": 1200, "header": 1200, "test": 1000, "function": 220}
CONTROL = re.compile(r"\b(if|for|while|switch|catch|else|do|try)\s*\(")
FUNCTION = re.compile(r"(?:[~\w:*&<>]+\s+)*[~\w:]+\s*\([^;{}]*\)\s*(?:const\b|noexcept\b|override\b|final\b|&|\s)*\{")


def _strip(text: str) -> str:
    text = re.sub(r"//[^\n]*|/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    return re.sub(r'("(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\')', lambda m: " " * len(m.group(0)), text)


def function_sizes(text: str) -> list[dict[str, int | str]]:
    clean = _strip(text)
    result = []
    for match in FUNCTION.finditer(clean):
        prefix = clean[max(0, match.start() - 30):match.start()]
        if CONTROL.search(prefix + match.group(0)):
            continue
        opening = clean.find("{", match.start(), match.end())
        depth = 0
        end = None
        for index in range(opening, len(clean)):
            if clean[index] == "{": depth += 1
            elif clean[index] == "}" and (depth := depth - 1) == 0:
                end = index; break
        if end is None: continue
        start_line = clean.count("\n", 0, match.start()) + 1
        end_line = clean.count("\n", 0, end) + 1
        result.append({"name": " ".join(match.group(0).split())[:-1].strip(), "lines": end_line - start_line + 1, "line": start_line})
    return result


def files(root: Path):
    for path in sorted(root.rglob("*")):
        if path.suffix.lower() not in EXTENSIONS or any(
            part.lower() in EXCLUDED or part.lower().startswith("build")
            for part in path.relative_to(root).parts
        ):
            continue
        yield path


def baseline_entry(old: dict, relative: str) -> tuple[dict | None, str | None]:
    matches = [(key, value) for key, value in old.items()
               if key == relative or key.endswith("/" + relative)]
    if len(matches) > 1:
        return None, "baseline has duplicate paths for " + relative + ": " + ", ".join(
            key for key, _ in matches
        )
    return (matches[0][1], None) if matches else (None, None)


def audit(root: Path, baseline_path: Path) -> tuple[dict, list[str]]:
    baseline = json.loads(baseline_path.read_text()) if baseline_path.is_file() else {"files": {}}
    old = baseline.get("files", {})
    report = {"schema": 1, "limits": LIMITS, "files": {}}
    errors: list[str] = []
    for path in files(root):
        rel = path.relative_to(root).as_posix()
        physical = len(path.read_text(encoding="utf-8", errors="replace").splitlines())
        kind = "test" if "/test" in rel or rel.startswith("tests/") else ("header" if path.suffix.lower() in {".h", ".hh", ".hpp", ".hxx"} else "source")
        funcs = function_sizes(path.read_text(encoding="utf-8", errors="replace"))
        item = {"kind": kind, "lines": physical, "functions": funcs}
        report["files"][rel] = item
        previous, duplicate_error = baseline_entry(old, rel)
        if duplicate_error:
            errors.append(duplicate_error)
            previous = None
        exempt = bool(previous and previous.get("exempt")) and item == {k: previous.get(k) for k in ("kind", "lines", "functions")}
        if physical > LIMITS[kind] and not exempt: errors.append(f"{rel}: {physical} physical lines > {LIMITS[kind]} {kind} budget")
        for fn in funcs:
            if fn["lines"] > LIMITS["function"] and not exempt: errors.append(f"{rel}:{fn['line']}: {fn['name']} is {fn['lines']} lines > 220 function budget")
        if previous and not exempt:
            if physical > previous.get("lines", 0): errors.append(f"{rel}: aggravated baseline ({previous.get('lines')} -> {physical} lines)")
            old_max = max((f.get("lines", 0) for f in previous.get("functions", [])), default=0)
            new_max = max((f["lines"] for f in funcs), default=0)
            if new_max > old_max: errors.append(f"{rel}: aggravated function baseline ({old_max} -> {new_max} lines)")
    return report, errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--baseline", type=Path, default=Path("analysis/source_complexity_baseline.json"))
    parser.add_argument("--check", action="store_true", help="fail on budget or baseline violations")
    args = parser.parse_args()
    report, errors = audit(args.root.resolve(), args.baseline)
    for rel, item in report["files"].items(): print(f"{rel}: {item['lines']} lines, {len(item['functions'])} functions")
    if errors:
        print("complexity_audit=fail")
        print("\n".join(f"error: {error}" for error in errors))
        return 1 if args.check else 0
    print(f"complexity_audit=pass files={len(report['files'])}")
    return 0


if __name__ == "__main__": raise SystemExit(main())
