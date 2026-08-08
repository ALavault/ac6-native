# Cycle 1209 — correcting cycle 1208: there is one id space, not two

## What I got wrong, forty minutes ago

Cycle 1208 took two clusters of texture ids — terrain in `0x49E`–`0x2BE1`, models
in `0x1000xxxx` — found the branch in `0x82340870` that compares an id against
`0x10000000`, and concluded:

> The two id spaces are the two arms of that compare. Ids at or above
> `0x10000000` are global keys; ids below are pack-local and rebased.

And, worse, drew a consequence for the port:

> Terrain textures cannot be resolved from the mesh file alone, even with the
> whole material chain derived.

**Both are wrong.** The cycle even asserted "this is not a story fitted to the
numbers", which is precisely what it was: a threshold, two clusters straddling
it, and an identification made without the one control that could have broken it.

## The control I did not run, and had the corpus for

Look at the id space in the **texture** files, not the mesh files.

```
NTXR files scanned      : 1052
distinct GIDX ids found : 205
  range                 : 0x49E .. 0x10002CB0
  ids BELOW 0x10000000  : 192 of 205        <- the crux
  sample small ids      : 0x1049, 0x104A, 0x104B, 0x104C ...

NDXR distinct element ids : 179
  present in the NTXR GIDX set : 179 of 179
  absent                       : none
```

**The small ids are GIDX ids.** They are written in the texture files, under a
`GIDX` tag, exactly like the large ones. `0x49E` is the minimum of both sets.
There are not two namespaces; there is one, and its values simply straddle a
threshold that also happens to appear in a branch.

And the join closes **179 of 179 with nothing rebased**. If the small ids needed
`[0x828C9700+0x08]` added before they became keys, the raw file values on the two
sides could not match — and they match exactly. So the bias is zero on this
content, or identical on both sides, and either way the mesh file **is** enough.

Run independently here with my own scanner after reading the claim, not taken on
report.

## What survives from 1208

- The **measurement**: `mapparts` files reference small ids (12,954), `tree` and
  `ws11` reference `0x1000`-form ids (60), and no file mixes them. That is still
  true and still classified by a field independent of the ids.
- The **branch exists**. `82340914 lis r11,0x1000` / `8234091c bge` /
  `82340920 lwz r11,0x8(r27)` is real and was verified instruction by
  instruction.

What died is the **identification** of the one with the other, and the port
constraint I derived from it. A branch that could rebase small ids does not prove
that these small ids are rebased.

## The shape of the error

This is the third time this session, and the shapes are converging:

| cycle | the error |
|---|---|
| 1201 → 1202 | followed `+0x80` one level and stopped; the map was one deeper |
| 1203 (self-caught) | aggregated two format populations and misread a per-format rule |
| **1208 → 1209** | found a threshold and two clusters straddling it, and called the coincidence a mechanism |

Each time the missing step was the same: **I had the corpus that would have
falsified it and did not query it from the other side.** Cycle 1198 wrote down
that an in-bounds test which cannot fail proves nothing; 1208 failed the same
standard with a different instrument.

## Two things the same work settled

**Registry B's keys are three values.** Measured here over all 13,014 materials:

| `material+0x00` | count |
|---|---|
| `0x30000010` | 12,963 |
| `0x30000001` | 36 |
| `0x30000090` | 15 |

A third namespace tag, `0x3`, distinct from GIDX's. **The whole mission uses
three shader contexts.** That closes cycle 1207's open item on what the
material's own id resolves to: registry `0x828CCB80` is fed from **NSXR** shader
packs — `0x82338500` reading magic `0x4E535852` via the pointer table at
`0x826762A0`, count at `+0x0A`, records from `+0x20`, stride `u32[rec+0x18]` —
mounted at boot by `0x821D55B0`, which `0x821D5EF8` calls, the same function that
mounts the two PACs.

**The two registries are different classes**, and the proof is a stride rather
than a name: the reserved-slot arrays are indexed `× 0x120` by `0x8234AE00` and
`× 0x218` by `0x8234BD08`. Two independent arithmetic checks land exactly:
`0x828C8100 + 0x400 + 16 × 0x120` = `0x828C9700`, the NTXR archive-owner global;
`0x828CA000 + 16 × 0x218` = `0x828CC180`, the shader cache. A wrong stride would
have missed by kilobytes.

## An instrument lesson worth more than the finding

`0x8233F250`, registry B's create path, has eight call sites. **Seven pass a
negative literal id** (`-0x9` … `-0xF`) with a `.rdata` blob — built-in shaders
that land in the reserved slots, where `0x8234AE78` explicitly refuses the map.
Reading those seven supports a clean, well-controlled conclusion: *nothing in this
image inserts a positive key into registry B's map*.

The eighth, `0x82343F60`, takes its id from its caller and is the entire
population path.

**Seven literals are not a census**, and a conclusion drawn from an unexhausted
call-site list looks exactly like a conclusion drawn from an exhausted one.

## Not established, stated plainly

- Which of the ~50 `0x82335F18` call sites mounts Mission 01's NTXR packs. The
  GIDX join proves it must be a `mode & 1 == 0` site; it was not found.
- `[0x828C9700+0x08]`, the id bias. The join is consistent with zero; no writer
  was read.
- **No NSXR file exists anywhere in this workspace** — zero, against 1,052 NTXR
  in the same tree. So the shader half has a code derivation and **no file-side
  control at all**, and the three keys are measured on the consumer side only.
- The 25 NTXR textures no NDXR references, and cycle 1207's 195 MATE-only ids.
- `0x8234CF68` / `0x8234CDC0` and the `0x10`-byte node layout, unchanged since
  cycle 1202.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
1052 NTXR, 205 GIDX ids, 192 below the threshold; 179/179 NDXR ids joined
```

No product code changed.
