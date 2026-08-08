# Cycle 1229 — a gate for cited addresses, and it caught my own contract on the first run

## The gap

`audit_ac6_contract_artifacts.py` verifies that every artefact a contract cites
exists, is in HEAD, and matches the working tree. **Nothing verified the
addresses.**

A behaviour's `retail_addresses` is the claim *"this is where the behaviour is
derived from"*. A behaviour could list twenty of them and cite three documents
that mention none, and every gate in this repository would stay green. The
addresses are the load-bearing part of a derivation claim and they were the only
part with no check behind them.

## The tool

`tools/audit_ac6_contract_addresses.py`: for each behaviour, every address in
`retail_addresses` must appear in at least one of **that behaviour's own**
evidence files. Matching is case-insensitive and accepts both `0x8234CA28` and
bare `8234ca28`, because the reports use both forms.

It is deliberately weak. It does not check that the address disassembles, that
the citation is correct, or that the document says anything true about it. It
checks only that **a reader following the contract has somewhere to go**, which
is the minimum a citation owes and was not being enforced.

## It failed immediately, on the behaviour I added today

```
UNSUPPORTED mission01-visible-gate-v4.json ndxr_container 0x8234AE00
contract_addresses=fail cited=77 supported=76 unsupported=1
```

`0x8234AE00` is the map search — established in cycle 1202, which was **not among
`ndxr_container`'s evidence**. I had added the address while writing the behaviour
because I knew where it came from, and the bundle did not.

That is the exact failure the tool exists for, and it was mine, on the first run,
in work committed three hours earlier.

The fix is to cite the document that establishes it rather than drop the address:
cycle 1202 is now evidence, and it is the right one — it is the cycle that found
the map is at `registry+0x100` and not `+0x80`.

## After

```
contract_addresses=pass  cited=106 supported=106 unsupported=0   (all contracts)
contract_artifacts=pass  cited=30 match_head=30 readme_rows=36
mission01_final_gate=audit-valid JF=pass open=none
ctest                    27 tests, all passed
```

The older contracts were already clean: v3 at 29 of 29, v2 cites no addresses.
So the defect was introduced today, by me, in the behaviour written today — which
is worth stating because "the new check found only new problems" is a much weaker
result than a clean sweep would suggest, and pretending otherwise would be the
same overclaim the check is designed to catch.

## Not established, stated plainly

- **The check is a presence test, not a correctness test.** An address mentioned
  in a report that says something wrong about it passes. Cycle 1218's corrected
  reachability figure and cycle 1226's withheld slot number both lived inside
  documents that also contained good addresses.
- Whether every address a *derivation source* cites is in the contract. This
  checks one direction only: contract → evidence, not evidence → contract.
- The 36 README hash rows the artefact checker counts are not addresses and are
  untouched here.

## Verification

```
all four live contracts audited; 106 cited addresses, 106 supported
```

The contract changed; no product code did.
