# Cycle 1207 — the four pointers are materials, the ids are GIDX, and MATE is never parsed

## Correcting cycles 1198 and 1199

Cycle 1198 called `sub+0x10..+0x1C` **four stream pointers** and cycle 1199
called `0x82355318` the **per-stream handler**, concluding "the meshes have
names". The structure I read was right; the label was wrong.

**They are four material pointers, and `0x82355318` is the material visitor.**

```
82355330  lhz  r11,0xa(r29)      ; N = u16 at material +0x0A, the texture count
8235532c  addi r30,r29,0x20      ; the first texture record at material +0x20
82355340  bl   0x8233ee40        ; per texture record
8235534c  addi r30,r30,0x18      ;   stride 0x18
82355360  addic. r11,r11,0x20    ; the parameter chain follows the array
823553b4  bl   0x8233ef88        ; the material's own resource id
```

What survives untouched: cycle 1199's headline. **"The meshes have names" rests
on `rec+0x20` in `0x823555D0`**, a different function, and its control —
13,014 of 13,014 records resolving to a printable C string — is unaffected.
Material parameter names and mesh names simply share the one string table at
`[obj+0x90]`, which is why the two readings looked like one.

## The link, end to end

`0x8233EE40`, called per texture record, is the join JV 2d needed:

```
8233ee54  lhz  r11,0xa(r31)   ; record +0x0A flags, bit 0x4000 = resolved
8233ee64  lwz  r4,0x0(r31)    ; the TEXTURE ID is the u32 at record +0x00
8233ee6c  subi r3,r11,0x7f00  ; registry 0x828C8100
8233ee70  bl   0x8233ebb0
8233eea4  stw  r10,0x4(r31)   ; cache the resolved pointer at record +0x04
```

And the other end fills the same registry. `0x82340870` is the NTXR-pack
registrar: it reads the pack count at `file+0x06`, the version at `+0x04`, walks
descriptors, and takes each entry's **GIDX identifier** via `0x8234B150`, failing
hard when there is none. `0x8234BEC8` then inserts into `0x828C8100` — *the same
registry* — creating the texture through `0x8233EA78`, the NTXR descriptor
consumer already ported in `include/ac6/ntxr_texture.h`.

**`GIDX+0x08` is the resource key**, and `0x8234CB58` asserts the same convention
from the file side: for a GIDX-wrapped payload it replaces the caller's key with
`[buf+0x08]` before skipping the header.

**This closes what cycles 1200 and 1202 refused to close.** I found 179 ids and
declined to call them GIDX handles on a resemblance. They are, and the reason is
now an instruction chain rather than two numbers sharing a prefix.

`0x8234B150` also explains why a byte search for the tag can lie: it **builds
`"GIDX"` and `"eXt\0"` one byte at a time on the stack** (`li r9,0x47 / 0x49 /
0x44 / 0x58`) rather than comparing a literal.

## Six rivals, each beaten by a test it could have passed

Walking all 292 NDXR in Mission 01's MDLP by the derived layout gives 1,227
materials and 2,161 texture records. Two orthogonal scores — the id namespace,
and the loader's own chain-termination rule (`next = p + u32[p]`, stop at 0):

| reading | ids in `0x1000xxxx` | chain terminates |
|---|---|---|
| **derived — count `+0x0A`, id `+0x00`, stride `0x18`** | **2161/2161** | **1227/1227** |
| rival: count at `+0x08` | — | 11/1227 |
| rival: count at `+0x0C` | — | 11/1227 |
| rival: id at record `+0x04` | **0/2161** | 1227/1227 |
| rival: id at record `+0x0C` | **0/2161** | 1227/1227 |
| rival: stride `0x10` | 0.57 | **0/1227** |
| rival: stride `0x20` | 0.57 | **0/1227** |

Each rival dies on one test after surviving the other. The one most worth beating
was **stride `0x10`** — the stride of the two MATE tables that bracket the
material bodies, so the natural wrong guess — and it scores **0 of 1,227**.

The loader's own guard bits, which could have failed and would have meant the
fields were misidentified: `material+0x08 & 0x4000` clear **1227/1227**,
`material+0x04` zero **1227/1227**, `record+0x0A & 0x4000` clear **2161/2161**,
`record+0x04` zero **2161/2161**.

## MATE is never parsed by this executable

Three instruments, each with a positive control in the same run:

1. instruction scan for `0x4d41` and `0x5445` — **0 and 0**; controls `0x4e44`
   → 6 hits including `8233ef48`, `0x4749` → 2;
2. byte search of every initialised block for `4d 41 54 45` — **0**; controls
   `GIDX` → `0x82067EC8`, `NDXR` → 1, `NTXR` → `0x82067EC0`;
3. because `0x8234B150` proves this code assembles tags byte by byte, a
   per-function immediate-set scan for `{0x4d,0x41,0x54,0x45}` — 2 candidates,
   neither constructing a magic; the control set `{0x47,0x49,0x44,0x58}`
   returned exactly the window containing `0x8234B150`.

**There is no MATE parser.** The materials the game consumes are the copies
embedded in the NDXR. Verified byte for byte: the NDXR-embedded material at MDLP
`0x9BE5E0` and the MATE material at `0xA011B0` agree on all `0x60` bytes but the
`u16` at `+0x10` and the parameter name offsets — the texture records are
identical.

This re-aims the caution in `AC6_MATERIAL_TEXTURE_LINK_REPORT.md`. The field
meanings are **general** and safe to promote; what is not safe is the assumption
that a MATE *file* is ever read.

## An open discrepancy I am not resolving by assumption

Cycle 1200 censused `elem+0x00` over the **537 standalone `.ndxr` files** and
found 179 distinct ids — **170 small** (`0x49E`–`0x7EB`) and 9 of the
`0x1000xxxx` form. This cycle censuses the **292 NDXR inside `001_MDLP.mdlp`**
and finds **2,161 of 2,161** in `0x1000xxxx`.

Same field, same walk, two corpora, two id spaces. The 537 include the
`mapparts_m01` terrain meshes; the 292 are the mission's models. That is a
plausible story and it is **not evidence**. Recorded as an open discrepancy.

## Not established, stated plainly

- **102 of 703 MATE materials** in this MDLP have a texture block matching no
  NDXR-embedded material, referencing 195 texture ids that appear in no NDXR
  material here — and all 195 are real GIDX ids in the corpus. Whether they are
  bound by element index from elsewhere, belong to another archive, or are
  authoring residue is unresolved.
- The texture record's `+0x0C..+0x17`. `0x8233EE40` never reads them; sampler
  state and stage assignment are elsewhere.
- What the material's own `+0x00` id resolves to in registry `0x828CCB80`.
- Whether `0x82340870` runs on the Mission 01 path. Its callee graph was read;
  no caller chain from mission load was.
- Slots 2–4 of `sub+0x10..+0x1C` are null in all 1,227 cases, so "they are also
  materials" is untested, not confirmed.

## An operational note

Two agents working in parallel collided in `/tmp`: one clobbered the other's
output file between two reads, caught only because a section-header count
disagreed with a listing five minutes earlier. **Parallel work needs private
scratch directories**, and the run that caught it did so because it re-checked an
instrument's output rather than trusting a file it had written itself.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
```

No product code changed. No oracle used.
