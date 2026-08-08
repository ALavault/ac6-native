#!/usr/bin/env python3
"""Check that every retail address a contract cites is actually established by
one of that behaviour's own evidence files.

The gap this closes: `audit_ac6_contract_artifacts.py` verifies that every cited
artefact exists and matches HEAD.  Nothing verified the *addresses*.  A behaviour
could list twenty retail addresses and cite three documents that mention none of
them, and every gate in the repository would stay green.

That is not hypothetical bookkeeping.  A contract's `retail_addresses` is the
claim "this is where the behaviour is derived from"; if no evidence file names an
address, the claim has no support in the bundle and a reader has nowhere to go.

Exit 0 when every address is mentioned by at least one evidence file of its own
behaviour, 1 otherwise.  Addresses are matched case-insensitively and in both
`0x8234CA28` and bare `8234ca28` forms, because the reports use both.
"""
import json
import pathlib
import re
import sys


def load(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


def behaviours(document):
    node = document.get("requirements", {})
    if isinstance(node, dict) and "behaviours" in node:
        return node["behaviours"]
    return {}


def evidence_text(root, behaviour):
    """Concatenate every readable evidence file of one behaviour."""
    chunks = []
    for item in behaviour.get("evidence", []):
        path = item.get("path")
        if not path:
            continue
        for candidate in (root / path, root / "reports" / "mission01-native-captures" / path):
            if candidate.is_file():
                try:
                    chunks.append(candidate.read_text(encoding="utf-8", errors="replace"))
                except OSError:
                    pass
                break
    return "\n".join(chunks).lower()


def main(argv):
    root = pathlib.Path(".")
    contracts = []
    for argument in argv[1:]:
        if argument.startswith("--artifact-root="):
            root = pathlib.Path(argument.split("=", 1)[1])
        else:
            contracts.append(argument)
    if not contracts:
        print("usage: audit_ac6_contract_addresses.py [--artifact-root=DIR] CONTRACT...")
        return 2

    cited = 0
    supported = 0
    failures = []
    for contract in contracts:
        document = load(contract)
        for name, behaviour in behaviours(document).items():
            addresses = behaviour.get("retail_addresses", [])
            if not addresses:
                continue
            haystack = evidence_text(root, behaviour)
            for address in addresses:
                cited += 1
                bare = re.sub(r"^0x", "", address, flags=re.I).lower()
                if bare in haystack or address.lower() in haystack:
                    supported += 1
                else:
                    failures.append((pathlib.Path(contract).name, name, address))

    for contract, name, address in failures:
        print(f"UNSUPPORTED {contract} {name} {address}")
    status = "pass" if not failures else "fail"
    print(f"contract_addresses={status} cited={cited} supported={supported} "
          f"unsupported={len(failures)}")
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
