# Correcting `bdb437e6`'s own "not established": the offset-9 setters were checked, and it isn't clean

## Qualification

AC6 demo PAL, same XEX SHA-256. Static evidence: the valid, 12000-tick,
START-at-3000 reachability atlas already used and cross-checked in
`a1d0106e` (`/fastdata/lavaulta/tmp/ac6-atlas-title-start/start.atlas.json`,
2779 functions — cached from that cycle, not regenerated here).

## The mistake, caught by my own sanity check

`bdb437e6` left "whether any reachable code ever writes a nonzero value to
`CSwgCallback+9`" as not established, having found ~25 `stb rN,9(rM)` sites
across the codebase but not checked their reachability. Checking that
immediately after committing, my first pass built a Python set of the
atlas's function addresses and compared against `int(hexaddr, 16)` — but the
atlas stores `address` as a **string** (`"0x82090210"`), not a JSON number.
Every membership test was comparing an `int` against a set of `str` and
could never match, so the first pass reported all ~25 sites — including
`sub_820CD990` (the constructor) and `sub_820CE368` (the confirmed,
5294-times-called consumer) — as "unreached." That both known-live
functions came back unreached is exactly the kind of self-contradiction
`CLAUDE.md` asks a measurement to surface before it's trusted; catching it
here cost one extra check, not another cycle correcting this one later.

## The corrected check

Comparing strings (case-normalized) instead: `sub_820CD990` and
`sub_820CE368` both show reached, as expected — the atlas and the corrected
method are sound. Of the ~25 static `stb ...,9(...)` sites, several ARE
reached, including three that store an immediate `1`:
`sub_8216C900`, `sub_8219CBF8`, `sub_8227ABD8` (plus `sub_8219B5E8`,
`sub_8219BE90`, `sub_8216C9A8`, `sub_821B1810`, `sub_820CDCD8` reached with
other stored values).

## Why this probably still doesn't change the conclusion

Read in full, the two most legible reached candidates don't look like
`CSwgCallback` methods:

- `sub_8219CBF8` calls `XNotifyGetNext` on `[this+4]` as a handle, dispatches
  on the returned notification id (`9`/`10`/`11`, then a further
  `-11..-16` range check — the XAM-notification-id family the project plan
  already names), and touches fields at `+8`, `+9`, `+10`, `+11`, `+12`,
  `+16`, `+32`, `+36`, `+52`, `+68`, `+72`, `+116`, `+120`, up to `+268`.
  `CSwgCallback`'s own constructor and vtable give no evidence of a field
  anywhere near `+268` — this is a much larger object, most plausibly the
  XAM notification-listener pump the plan file describes, not this class.
- `sub_8216C900` sets `+9` and `+10` as a pair of independent bit-flags from
  two bits of its second argument — a generic two-flag setter shape common
  enough that it could belong to any small struct in the image; nothing in
  its body ties it to `CSwgCallback` specifically.

Neither reading is proof — I did not trace either function's callers back
to confirm or rule out a `CSwgCallback` (or its `this`-aliased) instance,
and both remain open. This report only replaces "not checked" with "checked
and inconclusive": the reachable candidates read as belonging to unrelated,
larger objects, but that is a plausibility judgment from reading the body,
not a control on the object's identity.

## Not established

- Whether `sub_8219CBF8`'s `this` (`r31` at entry) can ever alias a
  `CSwgCallback` instance — not traced.
- Whether `sub_8216C900` is ever called with a `CSwgCallback` instance in
  `r3` — not traced.
- The four other reached-but-unresolved-value candidates
  (`sub_8219B5E8`, `sub_8219BE90`, `sub_8216C9A8`, `sub_821B1810`,
  `sub_820CDCD8`) were not read at all.

## Gates

No source changed; report-only commit.
