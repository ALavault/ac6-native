#!/usr/bin/env python3
"""Report EVERY derivation citation gap in a contract, not just the first.

The gate auditor raises on the first failure it meets and stops. That is right
for a gate -- it answers pass or fail -- and wrong for the person fixing it,
because the size of the problem stays hidden until the last one is gone.

Cycle 1261 paid for exactly that. The gate named one uncited address in
`ndxr_container`; enumerating the block found TEN, and nine of them were not
missing citations at all but addresses the behaviour declared and no product
source implemented. Fixing them one at a time, re-running between each, would
have taken ten passes and would have made each removal look like a small tidy
rather than what it was.

This tool answers a different question from the gate's. It reports:

  - every derivation file that fails to cite every one of its behaviour's
    retail_addresses, with the count and the addresses;
  - every behaviour carrying MORE THAN ONE derivation file, because the gate
    requires each of them to cite ALL addresses independently, which for a
    header/implementation pair forces either duplication or a false claim. That
    was the actual defect in ndxr_container: the header cited 28 of 28 and the
    .cpp 8 of 28, and the fix was to stop calling the .cpp a derivation.

It does not enforce anything the gate does not already enforce, so a green run
here is not a substitute for the gate. It is the map you want in your hand
before you start editing.

usage: audit_contract_derivations.py CONTRACT [CONTRACT...]
exit 0 when no gap and no multiple-derivation behaviour is found.
"""

import json
import os
import sys


def behaviours(node, key=None, found=None):
    """Yield (name, record) for every object carrying retail_addresses."""
    if found is None:
        found = []
    if isinstance(node, dict):
        if isinstance(node.get("retail_addresses"), list) and "evidence" in node:
            found.append((key, node))
        for child_key, value in node.items():
            behaviours(value, child_key, found)
    elif isinstance(node, list):
        for value in node:
            behaviours(value, key, found)
    return found


def main() -> int:
    contracts = sys.argv[1:]
    if not contracts:
        print("usage: audit_contract_derivations.py CONTRACT [CONTRACT...]")
        return 1

    gaps = 0
    multiples = 0
    checked = 0

    for contract in contracts:
        try:
            with open(contract, encoding="utf-8") as handle:
                document = json.load(handle)
        except (OSError, ValueError) as exc:
            print("  UNREADABLE  %s  %s" % (contract, exc))
            return 1

        for name, record in behaviours(document):
            derivations = [item["path"] for item in record["evidence"]
                           if item.get("kind") == "derivation"]
            addresses = record["retail_addresses"]
            if not derivations:
                continue
            checked += 1

            if len(derivations) > 1:
                multiples += 1
                print("  MULTIPLE DERIVATIONS  %s.%s  (%d files, each of which "
                      "the gate requires to cite all %d addresses)"
                      % (os.path.basename(contract), name, len(derivations),
                         len(addresses)))

            for path in derivations:
                try:
                    with open(path, encoding="utf-8", errors="replace") as handle:
                        text = handle.read().lower()
                except OSError:
                    print("  UNREADABLE DERIVATION  %s.%s  %s"
                          % (os.path.basename(contract), name, path))
                    gaps += 1
                    continue
                missing = [a for a in addresses
                           if a.lower().removeprefix("0x") not in text]
                if missing:
                    gaps += 1
                    print("  %s.%s  %s cites %d of %d"
                          % (os.path.basename(contract), name,
                             os.path.basename(path),
                             len(addresses) - len(missing), len(addresses)))
                    for address in missing:
                        print("        uncited  %s" % address)

    status = "pass" if gaps == 0 and multiples == 0 else "fail"
    print("contract_derivations=%s behaviours=%d gaps=%d multiple_derivations=%d"
          % (status, checked, gaps, multiples))
    return 0 if status == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
