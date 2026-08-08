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
