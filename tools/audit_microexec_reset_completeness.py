#!/usr/bin/env python3
"""Every per-case field of the harness must be cleared by `resetCase()`.

Cycle 1460 found that it clears twenty-two fields and did not clear
`dumpRegions`, so batched dumps accumulated across cases. One missing line, and
it was found by accident -- by a tool written to check something else, on a
premise that turned out to be false.

The second-order question is whether there is another. That is a parse, not a
reading: the class declares its per-case state as instance fields and
`resetCase()` clears a subset, so comparing the two sets is mechanical.

WHAT MAKES THE SCAN HONEST, and it took three attempts in one cycle. Matching
`^    private ...` misses any field declared with another modifier. Matching any
four-space-indented declaration matches **local variables**, because a local's
type and name look exactly like a field's -- the widened version reported 177
"unreset fields", almost all of them locals called `index`, `bytes` and `line`.

The discriminator that works is that a declaration here **begins with an access
modifier or `static`**, which no local in this file does. Locals are indented
deeper, but indentation is a style; a modifier is a token.

    private final List<String> dumpRegions = new ArrayList<>();   field
            String line = rawLine;                                local

Static finals are constants and are excluded by name of their modifier, not by a
list -- so a new constant does not have to be registered anywhere.

usage: audit_microexec_reset_completeness.py [HARNESS.java]
exit 0 when resetCase() is complete, 1 when a field is missed.
"""

import re
import sys

DEFAULT = "scripts/MicroExecuteFunction.java"

DECLARATION = re.compile(
    r"^\s{4}((?:private|public|protected|static|final|transient|volatile)"
    r"(?:\s+(?:private|public|protected|static|final|transient|volatile))*)"
    r"\s+([\w<>\[\],.]+(?:<[^>]*>)?)\s+(\w+)\s*(?:=|;)", re.M)

# `name.clear()` or `name = ...`, which is every way this file resets a field.
RESET = re.compile(r"\b(\w+)\s*(?:\.clear\(\)|=)")


def main(argv):
    path = argv[1] if len(argv) > 1 else DEFAULT
    try:
        source = open(path, errors="replace").read()
    except OSError as exc:
        print("microexec_reset_completeness=error %s" % exc)
        return 1

    fields, statics = [], []
    for match in DECLARATION.finditer(source):
        modifiers, kind, name = match.group(1), match.group(2), match.group(3)
        line = source[match.start():source.index("\n", match.start())]
        if "(" in line.split(name, 1)[1][:2]:
            continue                       # a method signature, not a field
        (statics if "static" in modifiers.split() else fields).append((name, kind))

    body = re.search(r"private void resetCase\(\)\s*\{(.*?)\n    \}", source, re.S)
    if body is None:
        print("microexec_reset_completeness=error no resetCase() found")
        return 1
    cleared = {m.group(1) for m in RESET.finditer(body.group(1))}

    missing = [(n, k) for n, k in fields if n not in cleared]
    for name, kind in missing:
        print("  NOT RESET  %-24s %s" % (name, kind))
    print("microexec_reset_completeness=%s fields=%d constants=%d missing=%d"
          % ("pass" if not missing else "fail", len(fields), len(statics),
             len(missing)))
    return 0 if not missing else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
