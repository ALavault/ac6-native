# Cycle 1280 — whose vtable, and two families that carry no RTTI

## Qualification

Flat image `analysis-input/ACE6_X360.exe`; `analysis/class-map.tsv` (811
vtables, J2-gated). `default.xex` SHA-256 `acc302c1…11bcde`. **No oracle pass
was spent.** No product code changed.

## Why

Two of the discipline's expensive shapes — *the displacement collision* and
*the right search, run against a sibling* — reduce to one question asked before
the search rather than after: **whose is this?** Answering it by hand takes a
data scan, a walk back for the vtable start, a locator read and a name read. It
was done four times this session, each time slightly differently, and once
wrongly: cycle 1265 read a `.pdata` row as a vtable slot.

`tools/whose_vtable.py` does it in one command, excluding `.pdata` and
preferring the campaign's own audited class map to re-deriving.

## Established — the tool reproduces a hand result, and finds a real gap

Positive control, `0x82266390` (the `li r3,1 ; blr` stub):

```
0x82266390 appears in 232 aligned words outside .pdata: 145 named, 87 unnamed
    at 0x82064268   vtable 0x82064264 slot +0x04   ACE6::CAce6MissionManagerCampaign
    at 0x82054F98   vtable 0x82054F7C slot +0x1C   ACE6::CAce6MissionManager
```

The first line is exactly what cycle 1273 established by hand — the campaign
manager's slot `+0x04` returns 1 — reproduced in one command against the audited
map rather than a fresh RTTI walk.

**And 87 of 232 have no name at all.** That is not a defect in the tool; it is a
fact about the image, and the two families it matters most for are ours:

| vtable | word at `vtable-4` | in the class map |
|---|---|---|
| `0x820078D0` — the campaign calls it `galib::CGaObj` | **`0x00000000`** | **no** |
| `0x82009440` — the unit family | `0x82298948`, a function address | **no** |
| `0x820568D4` — `CAce6UnitPlayer` | `0x8206E2A4`, a real locator | yes |

`0x8229C920`, the order handler cycle 1275 found in six vtables, resolves to
**zero** names: all six of those tables are in the unnamed set.

## What follows, and what does not

**The class map is RTTI-derived and honest about it** — its own header says
entries appear only when the whole chain resolves. So the absence of
`0x820078D0` and `0x82009440` is expected and not an error in it.

**Where `galib::CGaObj` came from is therefore not established by this cycle.**
The name is used throughout the reports and the discipline file; it is not from
the audited map and the vtable carries no locator, so its provenance is
something else — a string, a constructor pattern, an earlier session's reading —
and I did not determine which. **That is a question about our own naming, not
about retail**, and it is worth one cycle rather than an assumption, because
every field-offset argument that says "on the `CGaObj`" is standing on it.

## Not established

- The provenance of `galib::CGaObj` and of the unit family's name.
- Whether the 87 unnamed hits share a structure — they were counted, not read.
- The tool walks back at most 64 words for a vtable start. A slot deeper than
  that in a table with a locator would be missed, and the output says how far it
  looked rather than implying it looked everywhere.
