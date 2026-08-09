#!/usr/bin/env python3
"""Refresh the sha256 and size of contract evidence entries for given paths.

The gate auditor pins every evidence file by hash and size, so editing a cited
source means updating the contracts that cite it. That is mechanical, it was
done five times by heredoc in one session, and the fifth time it produced a
false sentence in a commit message.

WHY THIS PRINTS TWO NUMBERS. The heredoc versions reported how many citation
sites they visited. On the run that mattered, two sites were visited and zero
values changed -- the file cited there had not been edited -- and "2" was read
as "the file I edited was refreshed". It had not been; it was cited by neither
contract. A script that reports how many sites it visited has not told you how
many it changed, and when the two can differ it must print both.

It also refuses silently-uncited paths loudly: a path that appears in no
contract is almost always a mistake at the call site, not a no-op worth
ignoring.

Formatting is preserved by editing in place with a regex rather than
round-tripping through json.dumps -- one contract here is not indent-2 and a
reformat turns a 10-line diff into 454.

usage: refresh_contract_evidence.py CONTRACT [CONTRACT...] -- PATH [PATH...]
exit 0 if every path was found in at least one contract, 1 otherwise.
"""

import hashlib
import os
import re
import sys


def digest(path):
    with open(path, "rb") as handle:
        data = handle.read()
    return hashlib.sha256(data).hexdigest(), len(data)


ENTRY_WINDOW = 1400  # characters from "path" to the entry's "size"


def refresh(contract_text, path, sha, size):
    """Return (text, visited, changed, widest) for one cited path.

    THE WINDOW IS A PARAMETER AND IT IS REPORTED. An entry longer than
    ENTRY_WINDOW would have its sha256 rewritten and its size left alone --
    which is precisely the failure cycle 1269 hit from another direction: a
    fresh hash describing content that no longer exists. The widest span
    actually seen is returned so the caller can print it beside the limit
    rather than trusting a constant nobody measures.
    """
    visited = 0
    changed = 0
    widest = 0
    out = contract_text
    needle = '"path": "%s"' % path
    index = out.find(needle)
    while index != -1:
        # A citation entry is small; 1400 characters covers path, sha256, claim
        # and size without reaching the next entry.
        window = out[index:index + ENTRY_WINDOW]
        span = window.find('"size"')
        if span >= 0:
            widest = max(widest, span + 16)
        before = window
        window = re.sub(r'"sha256":\s*"[0-9a-f]{64}"',
                        '"sha256": "%s"' % sha, window, count=1)
        window = re.sub(r'"size":\s*\d+', '"size": %d' % size, window, count=1)
        visited += 1
        if window != before:
            changed += 1
        out = out[:index] + window + out[index + ENTRY_WINDOW:]
        index = out.find(needle, index + ENTRY_WINDOW)
    return out, visited, changed, widest


def main() -> int:
    argv = sys.argv[1:]
    if "--" not in argv:
        print(__doc__.strip().splitlines()[-2])
        return 1
    split = argv.index("--")
    contracts = argv[:split]
    paths = argv[split + 1:]
    if not contracts or not paths:
        print("usage: refresh_contract_evidence.py CONTRACT... -- PATH...")
        return 1

    missing = []
    for path in paths:
        if not os.path.exists(path):
            print("  MISSING FILE  %s" % path)
            missing.append(path)
    if missing:
        return 1

    totals = {path: 0 for path in paths}
    widest_seen = 0
    for contract in contracts:
        with open(contract, encoding="utf-8") as handle:
            text = handle.read()
        original = text
        for path in paths:
            sha, size = digest(path)
            text, visited, changed, widest = refresh(text, path, sha, size)
            widest_seen = max(widest_seen, widest)
            totals[path] += visited
            if visited:
                print("  %-46s %s  sites=%d changed=%d"
                      % (path, os.path.basename(contract), visited, changed))
        if text != original:
            with open(contract, "w", encoding="utf-8") as handle:
                handle.write(text)

    uncited = [p for p in paths if totals[p] == 0]
    for path in uncited:
        print("  NOT CITED BY ANY CONTRACT  %s" % path)

    over = widest_seen > ENTRY_WINDOW
    if over:
        print("  ENTRY LONGER THAN THE WINDOW: %d > %d -- a sha256 may have been"
              " rewritten with its size left alone" % (widest_seen, ENTRY_WINDOW))
    status = "pass" if not uncited and not over else "fail"
    print("refresh_contract_evidence=%s paths=%d uncited=%d "
          "widest_entry=%d window=%d"
          % (status, len(paths), len(uncited), widest_seen, ENTRY_WINDOW))
    return 0 if status == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
