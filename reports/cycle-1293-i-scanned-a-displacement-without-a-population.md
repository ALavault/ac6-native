# Cycle 1293 — I scanned a displacement without a population

## Qualification

Flat image `analysis-input/ACE6_X360.exe`; `.pdata` and `analysis/class-map.tsv`
for attribution. `default.xex` SHA-256 `acc302c1…11bcde`. **No oracle pass was
spent.** No product code changed.

## What I did

Cycle 1292 scanned the image for stores at displacement `+0x3A4` — the script
interpreter's record-array base — found 18, and reported the one that assigns
rather than zeroes: `0x820962A8`.

Attributing each of the 18 to its `.pdata` function turns that reading over.

| where | what |
|---|---|
| `0x820962A8` in `0x820960F0` | `stw` — the only assignment |
| `0x8225979C` in `0x822596F8` (74 instr) | `stw` — a constructor's zeroing |
| `0x82262E98/EBC/EF0/F50` in `0x82262A28` | **`stfs`** |
| `0x82263DD0` in **`0x82263A50`** | **`stfs`** — the camera manager's per-frame update |
| `0x82278448`, `0x822786B4`, `0x822826D0`, `0x822EDF70`, `0x822EE828`, four in `0x8233Dxxx`, `0x823D37A4` | **`stfs`** |

**Fourteen of the eighteen are floats.** `0x82263A50` is the camera update, where
cycle 1283 established `manager+0x3A0/+0x3A4` are the shake rotations applied to
the locator. Those stores have nothing to do with a script record array.

**`+0x3A4` is a displacement, not a field.** Different classes have different
things there, and I scanned it without establishing whose. That is the
**fifteenth shape** — *the displacement collision* — which this file describes
as returning "a clean, complete, plausible candidate list" that is "a different
structure with a field at the same offset". Three times last session; once more
here, by me, while holding `tools/whose_vtable.py`, written two days ago to make
exactly this cheap.

## What survives

- `0x820962A8` is a real assignment, and `tools/whose_vtable.py` names its
  function: **`0x820960F0` is slot `+0xFC` of `CX360MissionManagerOnline`**,
  from the audited class map rather than a prefix comparison.
- Slot `+0xFC` is overridden per class — base `0x8225B048`, campaign
  `0x822F2E60`, `CX360<Campaign>` `0x822DDBE8`, online `0x8206D16C`, replay
  `0x822E3560` — so it is a genuine per-mode hook.
- **The campaign's override spans 18 instructions** and contains none of the
  eighteen stores. Whatever it does, it does not build a record array.

So the only script-record base assignment found is on the **online** path, and
the campaign path's equivalent is too small to be one.

## What that does and does not mean

It does **not** mean the campaign has no script records. It means the search was
malformed: a displacement scan cannot answer a question about one class until
the class is fixed. The right order is the one the discipline states and I
inverted — **establish the population, then search it.**

The next step is therefore to identify the interpreter's `this` — the class
whose `+0x3A4` holds the base and whose `+0x3AC` holds the cursor — from
`0x8225A600`'s callers or from the vtable of the object those FSM state handlers
run on, and only then to look for its writers.

## Not established

- The interpreter's class.
- What the campaign's 18-instruction `+0xFC` override does.
- Whether `0x820960F0` is reached in a campaign mission at all. It is the
  *online* manager's slot; nothing here says the campaign manager never
  delegates to it, and nothing says it does.
- The store scan still omits `stwx`; the precomputed-base form was added this
  cycle and found three sites, none in the regions of interest.
