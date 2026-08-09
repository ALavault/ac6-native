# Cycle 1287 — a sweep of my own tools for invisible parameters

## Qualification

`default.xex` SHA-256 `acc302c1…11bcde`. **No oracle pass was spent.** No
product code changed.

## Why

Cycle 1286 found that `find_materialised_address.py` reported a count whose
value depended on an unprinted window — 138 at four instructions, 144 at twelve,
157 at sixty-four. The rule it broke is one this repository has kept since
`Ac6XenonForceScan`: **a scan states the parameter its answer depends on.** So
the question is which of the other eight tools written this session break it
too.

## The sweep

| tool | parameter | stated? |
|---|---|---|
| `audit_instrument_discipline_index.py` | — | n/a |
| `audit_contract_derivations.py` | — | n/a |
| `check_listing_against_pdata.py` | — (`.pdata` supplies the length) | n/a |
| `audit_cited_tools_in_head.py` | the extension set `py\|java\|sh\|yml\|yaml` | in a comment, not the output |
| `audit_capture_images_match_metrics.py` | the name→key match | reports unmatched images explicitly |
| `count_indirect_branches.py` | `LOOKBACK = 8` | **yes**, in the per-site message |
| `whose_vtable.py` | `--span 64` | **yes**, in the per-hit message |
| `refresh_contract_evidence.py` | `1400` characters per entry | **no** |

**One genuinely invisible**, and it is the one that edits contracts.

## The risk it carried, measured

`refresh_contract_evidence.py` rewrites `sha256` and `size` within a fixed
1400-character window from each `"path"`. An entry longer than that would have
its **hash rewritten and its size left alone** — a fresh hash describing content
that no longer exists, which is exactly the failure cycle 1269 hit from the
other direction.

Measured across both contracts: the longest `"path"` → `"size"` distance is
**597 characters**, so the margin is 2.3×. Safe today, and nothing checked it.

The tool now measures the widest entry it actually spans, prints it beside the
limit, and **fails** if one exceeds it:

```
refresh_contract_evidence=pass paths=1 uncited=0 widest_entry=159 window=1400
```

## What the sweep says about the session's tooling

Eight tools, one invisible parameter, and it was in the tool that writes rather
than the ones that read. That is the ordering one would least want. The two that
do state their windows — `count_indirect_branches.py` and `whose_vtable.py` —
state them in the *negative* message, which is the case that matters: they say
how far they looked when they found nothing.

`audit_cited_tools_in_head.py` remains partial: its extension set is a real
parameter, it is documented at the regex, and it is not in the output. It was
already wrong once — it matched only `py|java|sh` and passed a report whose
whole reproduction section cited `.yml` files. Left as it is and recorded here,
because widening it further is a guess about future extensions rather than a
measurement.
