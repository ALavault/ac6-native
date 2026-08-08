# Instrument discipline — the false negative, and how to catch it

`CLAUDE.md` says *measure the instrument before trusting it*. This is what that
means in practice, written from eight failures in one session rather than from
first principles.

## The pattern

Every one had the same shape: **the tool worked and the frame around it was too
narrow.** Every one produced a *negative*. And a negative is the dangerous
direction, because it stops work rather than corrupting it — nothing downstream
contradicts a conclusion you never reached.

| # | the search | what it missed | how it was caught |
|---|---|---|---|
| 1 | retail archives "absent" | ran from a subdirectory | a later `ls` |
| 2 | `+0x188` writers | own output filtered to `^822` | a contradiction, 26 cycles later |
| 3 | `.mdlp` "absent" | `find -maxdepth 3` | a later full-tree count |
| 4 | the definition table | never listed the mission's own directory | reading the bundle |
| 5 | `r25` reassignment | searched `or r25,` not `lwz r25,` | a delegated full read |
| 6 | byte `+0x61` | measured the entry node, not its child | a delegated full read |
| 7 | `+0xC0` readers | two displacement spellings, offset is register-held | a third spelling |
| 8 | `li rX,0xc0` | regex syntax in a substring matcher | **a known-good hit** |

Seven were caught by accident, late. One was caught immediately.

## What made the eighth different

A **known answer to check against**. I had read `li r11,0xc0` minutes earlier, so
zero hits was impossible and the tool was wrong rather than the world.

That is the whole technique:

> Before believing a zero, run the same search against a case where you already
> know the answer is not zero.

If you have no such case, you cannot validate the search, and the negative is not
yet evidence. Say that in the report rather than banking it.

## The specific traps in this repository

- **`Ac6XenonRefs` matches substrings, not regex.** It now warns on regex syntax
  (cycle 1188). A zero under `^`, `$`, `\`, `[`, `|`, `+`, `*` means nothing.
- **`Ac6XenonDisasm` stops at the first undefined byte.** A short result is a
  halt, not an end. Restart past it.
- **Displacement searches miss computed addresses.** `0x188(rN)` will not find
  `addi rX,rY,0x188`, and neither finds `li rX,0xc0` + `stvx128 vrA,rB,rX`. Vector
  slots are almost always register-held.
- **Register searches miss the load form.** `or rN,` is not `lwz rN,`.
- **The canonical Ghidra project cannot decode VMX128** and halts. The Xenon
  import exists for exactly this; use it for anything touching vector code.
- **`-noanalysis` leaves the reference database empty.** `Ac6Xrefs` returns zero
  for data addresses regardless of truth. Use it for call targets, not fields.
- **`find` without `-maxdepth`** — the extracted assets are four to six levels
  deep.

## Two checkers that exist because of this

- `tools/audit_ac6_contract_artifacts.py` — the gate hashes the working tree, so
  an artefact regenerated and never staged passes locally and fails a fresh
  clone. It also checks the hash tables inside capture READMEs, which no contract
  cites and which had three stale rows.
- `Ac6XenonRefs`' regex warning, above.

Both were written after the failure they catch, and both were made to fail once
on purpose before being trusted. **A checker that has never been seen to fail is
not a checker.**

And the artefact checker then made the mistake it exists to catch. It scraped
every `"path"` string out of a contract, which worked on the three it was tested
against and failed on the rest — provenance fields naming a workspace root are
not artefacts, and older contracts cite paths relative to the capture directory
rather than the repository root. Fixed to walk the parsed document and take only
entries carrying both a `path` and a `sha256`, and to resolve against either
root.

Two lessons, and the second is the one that generalises:

- **validating a tool on the cases you had in mind is not validating it**. Three
  contracts passed; five existed.
- once fixed, it found something real: `mission01-native-gate.json`, the
  superseded v1 contract, cites **7 artefacts that no longer exist**. The live
  contracts — v2, v3, v4 — are clean at 17, 19 and 22.

## A ninth instance, and it is a new shape: the true positive from dead code

Cycle 1192 published a structure — the DPL member table, `{first member, member
count}` u16 entries at `*0x8293BA38` into `0x44`-byte records at `*0x8293BA3C` —
and cycle 1193 found that **none of that code runs in this build**. It is one
side of a format switch on the byte at `0x8293BA18`, which `0x821D61F4` sets to a
literal `2` immediately before the table is mounted; all three functions that
test it route the other way.

Every previous entry in this file is a **false negative** — a scan that returned
nothing and was believed. This one is the opposite and is worse, because nothing
about the output looked wrong. The scan for the two globals returned eleven rows,
including both the writer at `821cc2e4` and the writer at `821cc304` twenty
instructions later. Both were real instructions at real addresses storing real
values. I read the first, followed it, and wrote it down.

**A cross-reference tells you an instruction exists. It does not tell you the
instruction executes.** The two are different claims and this repository's
evidence standard only ever enforced the first.

The check that would have caught it costs one step: **walk up from the hit to the
nearest conditional and ask what selects it.** Here the branch was fourteen
instructions above the write, in the same function, testing a byte with exactly
one writer in the image — a literal. Reachability was cheap and I did not spend
it.

Two structural warnings this generalises to:

- **A format switch reads as a discovery.** Both branches parse a plausible
  container, both look like the format you were hunting, and the dead one is
  often the more elaborate — `0x44`-byte records with a name field are more
  interesting than sixteen anonymous bytes, which is part of why I stopped there.
- **The independent control is the one that broke the tie**, and it was already
  in hand. `DATA.TBL` is 14,824 bytes and `8 + 926 * 0x10` is 14,824 exactly,
  while the `0x44` shape divides into nothing. Cycle 1192 had that arithmetic,
  recorded that it did not fit, and filed the mismatch as an open question about
  a missing build step rather than as evidence against the structure. **A control
  that contradicts the finding is not an open question.**

## The Xenon project has no reference database, and it fails silently

`Ac6Xrefs.java` asks Ghidra's reference manager, and against
`ghidra-projects-xenon/ac6-xenon` it returns **zero for everything** —
`0x8293BA38`, and equally `0x82910C80`, which `0x8233BE20` demonstrably loads.
The project was imported with analysis off, so data references were never built.

The instrument is not wrong about a target; it is empty. Its own docstring warns
that text matching misses references, which is true in the canonical project and
irrelevant here, and the failure mode inverts: in this project the text scan is
the only one that works and the reference query is the one that lies.

Cycle 1193 caught it in one command by querying a global it knew was referenced.
**Any zero from `Ac6Xrefs` against the Xenon project should be read as "the
database is empty" until a known-good target says otherwise.**

## The eleventh shape, and it is the one that keeps recurring: querying only one side

Three cycles in a single session — 1201→1202, 1203 caught in flight, and
1208→1209 — failed the same way, and the shape is worth naming because it does
not look like the others.

| cycle | the claim | what broke it |
|---|---|---|
| 1201 | "two derivations meet at `+0x80`" | `0x8234AE00` adds `0x80` again; the map is one level deeper |
| 1203 | "`% 28` holds for only 17%, so the product is wrong" | restricted to the 36 descriptors the product assigns 28 to, it is 100% |
| 1208 | "the two id clusters are the two arms of the `0x10000000` branch" | 192 of 205 GIDX ids in the **texture** files are already below the threshold |

None of these is a false negative, and none is a scan that returned nothing.
Every one had **real evidence, correctly read, from one side of a join** — and in
every case **the corpus that would have falsified it was already in the
workspace and was not queried.**

Cycle 1208 is the sharpest instance because it wrote, in its own text, "this is
not a story fitted to the numbers." It was. A threshold existed in the code; two
clusters straddled it; the identification followed. What was never asked was the
only question that could have failed: *what do the texture files themselves say
their ids are?* One scan, one minute, and the answer was 192 of 205 already below
the threshold — so the clusters are one namespace and the branch is irrelevant to
them.

**The rule: before publishing a join, query it from the far side.** A rule
derived from the consumer must be checked against the producer; a rule derived
from the code must be checked against the files; a rule derived from one corpus
must be checked against the other. If the far side cannot be queried, that is
itself the finding and belongs in "not established" rather than in the
conclusion.

This is the same standard cycle 1198 wrote for controls — *a test that cannot
fail proves nothing* — applied one level up. A control run only on the side that
produced the hypothesis cannot fail either.

## And a corollary about call sites

`0x8233F250` has eight call sites. **Seven pass a negative literal id** with an
`.rdata` blob — built-in resources that land in reserved slots where the insert
is explicitly refused. Reading those seven supports a clean, well-controlled
conclusion: *nothing in this image inserts a positive key into that map.* The
eighth takes its id from its caller and is the entire population path.

**Seven literals are not a census.** An unexhausted call-site list is
indistinguishable, from the inside, from an exhausted one — the evidence looks
uniform precisely because the exception has not been reached yet. Enumerate
completely, or state in the report that the enumeration is partial and how far it
got.

## The twelfth shape: the listing is not the code

`Ac6XenonRefs` reports the number of instructions it scanned, and that number is
**786,122**. `.text` runs `0x82090000`–`0x823D772C` and holds **859,595**
instructions. **Every negative taken with this instrument in this repository has
covered 91.5% of the code, not all of it.**

This is not hypothetical. `0x82351070 b 0x82355998` — the branch that makes
`0x82351060` reachable as a vtable draw slot, and therefore the entry to the
whole NDXR render traversal — **is absent from Ghidra's listing.** It was found
only by decoding every word of the image directly.

The instrument reports its own denominator and nobody read it. `scanned_instructions
786122` has been printed at the bottom of every scan output this session.

### What this does and does not invalidate

A byte-level scan of memory blocks does **not** go through the listing, so:

- cycle 1192's "`46 48 4D` occurs zero times anywhere in the loaded image" stands;
- cycle 1207's "`4d 41 54 45` occurs zero times" stands, with its
  `GIDX`/`NDXR`/`NTXR` positive controls;
- cycle 1205's zero `0x2005` records is a data census and is untouched.

An instruction-text scan **does**, so:

- "no instruction carries `0x4d20`" (cycle 1192) is weaker than it reads;
- **every "N call sites" figure taken from `Ac6XenonRefs` is a lower bound, not a
  census.**

That last point compounds the corollary above it. There, seven of eight call
sites supported a false conclusion because the eighth existed. Here, the eighth
might never have been listed.

### The rule

**State the denominator with the negative.** A scan that finds nothing in 786,122
of 859,595 instructions has found nothing in 91.5% of the code, and the honest
sentence says so. When a negative is load-bearing, either raise it to a byte-level
scan of the image or decode the range directly — Ghidra's listing is a view of
the code, not the code.

### And a note on the flat dump

`analysis-input/ACE6_X360.exe` is a flat image: **file offset = VA − 0x82000000.**
The PE section table's `PointerToRawData` values are the *packed* ones and are
wrong — `.text` reads `0x8CA00` and actually sits at `0x90000`. A scan built on
the header values returns empty for every known-good control, which is at least a
loud failure; it was caught that way before anything was believed.

## The thirteenth shape, and it is the mirror of everything above: reachability by `bl`

Measured on this image:

| | count |
|---|---|
| direct `bl 0x…` | 36,472 |
| indirect `bctrl` | 7,827 |
| tail `bctr` | 652 |
| functions | 8,247 |
| **reachable from `main` by direct `bl` alone** | **800** |

**Direct-call reachability covers about 10% of this program.** It is an object
graph with a message pump, not a call graph.

Every entry above this one is about believing something that is false. This is
the mirror: **declaring live code unreachable**, and it is available here at a hit
rate of roughly nine in ten. Cycles 1193, 1195, 1196 and 1202 were burned by live
code on a dead path. Cycle 1213 found the same binary will just as happily hide a
live path behind a vtable slot — the NDXR loader, the Mission 01 texture mounts
and the unit placement were all "unreachable" by `bl` and all run.

**A `bl`-only reachability negative in this binary is not weak evidence. It is no
evidence.** Do not write "no caller found" without saying which kind of caller was
looked for.

### The two techniques that do work

1. **Vtable resolution.** Dump `.rdata` as words; find the function's address
   inside a run of `.text` pointers; find code materialising a base inside that
   run. That code is the constructor, and the delta from the run's start is the
   slot index. This turns "vtable-only, nothing calls it" into a chain.
2. **Receiver-offset matching.** When a virtual call loads its receiver at
   `this + K`, and a constructor builds a sub-object with the vtable under test at
   the same `K`, and *the same distinctive `addis`/`subi` pair appears on both
   sides*, the identification is checkable and can fail. Cycle 1213 pinned three
   classes this way on three different literal offsets — `+0x29520`, `+0x29130`,
   `+0x35C00` — any one of which could have mismatched.

### And the trap that sits on top of it

Cycle 1213's texture mounts: the nine `0x82335F18` call sites sitting **directly**
in the three mission-load functions all pass `mode = 1`. The six that matter are
one level down, in a function reached only through a vtable slot. Searching the
obvious function for the obvious argument returns a clean, complete, wrong answer
— nine sites examined, none matching, conclusion drawn.

**Where you look is a hypothesis too, and it needs the same control as the rest.**

## The fourteenth shape: half a rule

Cycle 1213 wrote the rule *walk up to the nearest conditional and establish what
selects the block*. Cycle 1214 applied it to `0x820FB050`, correctly, and
published a fixed-member existence test with a corpus census behind it. Cycle
1215 found the block is a **loop body** — the back-edge is at `820fb11c`, `0x8C`
bytes below where the reading stopped.

Everything in cycle 1214 is accurate and beside the point.

**The rule has a twin: walk DOWN to the nearest back-edge.** A conditional tells
you what selects a block. It does not tell you *how many times the block runs, or
over what*. Those are different questions and only the second one distinguishes
"the loader runs when member N is present" from "the loader runs once per member,
for all 256 of them."

### The tell that was already written down

Cycle 1214's own "not established" list contains this sentence:

> what `r26` — the index at the guard — holds when the guard runs. The cached
> probes use literals; **the guard uses a register**.

**A register where the surrounding code uses literals is a loop counter.** The
observation was made, recorded, and filed under "unknown" instead of chased. When
a "not established" item is one instruction away from being established, it is
not an open question; it is an unfinished read.

### The mechanical version

When a dump is truncated — `Ac6XenonDisasm` stops at 300 instructions — and the
block of interest sits near or past the end, **re-dump from before the block and
read past it until a `blr`, an unconditional forward `b`, or a backward branch is
seen.** Cycle 1214 read its block from a second dump that began 0x1A0 bytes
before it and stopped 0x8C bytes short of the answer, which is exactly the window
where this fails silently.
