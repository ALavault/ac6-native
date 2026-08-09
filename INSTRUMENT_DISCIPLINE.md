# Instrument discipline — the false negative, and how to catch it

`CLAUDE.md` says *measure the instrument before trusting it*. This is what that
means in practice, written from failures rather than from first principles —
eight in the session that started it, and eight more in the session that doubled
it.

**A vtable prefix is not an identity.** `0x822663A8`, a `li r3,0 ; blr` stub,
fills slot `+0x04` in 27 of the 811 named vtables and `+0x08` in 24, and **261
of the 811 share their `+0x04`…`+0x14` with at least one other table**. So a
six-slot comparison is not weak evidence of relatedness — it is none. Name a
class from `analysis/class-map.tsv` or from an RTTI locator at `vtable-4`, and
where there is neither, say "the object whose vtable is 0x…" and stop. Cycles
1281 and 1282 found both of this campaign's most-used family labels attached
that way: `0x820078D0` shares 11 of 96 slots with the real `galib::CGaObj`, and
`0x82009440` shares 9 of 96 with the real `ACE6::CAce6Unit`.

**Before anything else — the general case.** Every shape below is an instance of
*a correct measurement whose reporting sentence widened its scope*: "this
instruction exists" written as "it runs", "true of this corpus" as "true", "not
in 786,122 instructions" as "not in the program". **Write the measurement, then
the claim, then ask what stands between them.** If the answer is a step of
reasoning, that step is a hypothesis and needs its own control. See *a correct
measurement, over-read*, and its delegation variant *an agent's scope, written as
the repository's*.

**If you are here mid-investigation, find your symptom:**

| the shape you are in | section |
|---|---|
| a scan returned **nothing** and you are about to believe it | *The pattern*, *The specific traps* |
| you found a **write / a branch / a table** and it looks decisive | *the true positive from dead code* — four findings this campaign were live code the build never reaches |
| your rule is **right on the corpus you took it from** | *querying only one side* — check the producer against the consumer, the code against the files, one corpus against the other |
| you counted **N call sites** | *the listing is not the code* — `Ac6XenonRefs` sees only what Ghidra already disassembled; use `Ac6XenonForceScan`, which prints its own denominator |
| **nothing calls** the function you care about | *reachability by `bl`* — that covers about a quarter of this program; use `Ac6XenonFindWord` to find it as a vtable slot, and remember tail calls are `bcctr` |
| you read the **conditional above** your block | *half a rule* — also read down to the back-edge; a guard tells you what selects a block, not how many times it runs |
| a **displacement scan** gave you a clean candidate list | *the displacement collision* — read the four lines around each hit; a field belongs to the structure its neighbours belong to |
| your dump ended at a **`blr`** | *stopping at a natural boundary* — disassemble the next address anyway; three cycles missed their answer by under twenty bytes |
| you are about to add a **plausibility control** | it is only strong where the field borders a differently-encoded one; see cycle 1242 |
| **`Ac6Xrefs` returned 0** for something you know is referenced | *the Xenon project has no reference database* — it is empty for everything, including globals the code demonstrably loads; use a text or force scan instead |
| you drew a conclusion from **seven of eight** call sites | *a corollary about call sites* — an unexhausted list is indistinguishable from an exhausted one, because the evidence looks uniform until the exception |
| your **positive** result confirms what you hoped | `.pdata` is incomplete, and a false positive gets challenged far less than a false negative |
| your displacement scan returned a **clean, plausible** candidate list | *the displacement collision* — two class families here have different fields at the same offset; discriminate on the vtable or the `subi` before believing any of them |
| you stopped reading at a `blr`, a `blt`, or the end of a listing | *stopping at a natural boundary* — three refutations this session sat within **twenty bytes** of where a cycle stopped; read past the boundary before publishing |
| your search was **correct** and returned nothing useful | *the right search, run against a sibling* — 100% coverage of the wrong family is still zero evidence about yours; check the search's population, not its recall |
| you are about to publish that **two sources disagree** | *the unexamined contradiction* — a mismatch is a claim about two readings and needs both read; start with whichever came from someone else's transcription |
| your scan came back **zero** for an address or a magic | *a rule that was written, correct, and unrunnable* — run `tools/find_materialised_address.py IMAGE ADDR`: a value built as `lis`+`ori`/`addi` is invisible to branch scans and data scans alike |
| you scanned for a function address **as data** and found exactly one hit | *the `.pdata` row read as a dispatch slot* — the exception table spans 0x82079E00..0x82089FB0 and its `BeginAddress` fields look identical to a table slot; exclude that range, then say which side of it the address falls on |
| two functions sit **next to each other** and seem related | *the refuted link* — image order is not evidence; the checkable forms are a shared member offset, an exclusive call site, a common caller, and none is visible from inside either function |
| your offset **worked on every entry** of the file you derived it from | *an instrument calibrated on one specimen* — regular structures make wrong offsets self-consistent; find the container's own declared count or length and check it across files |
| your listing **ended on an ordinary instruction** and you called it the function | *the instrument sampled a third of it* — a tool that can truncate must say so; check whether two hex arguments mean a range or two starts |
| every offset in your sentence was read, and the sentence is still wrong | *the collision in the prose* — one word ("unit", "the object") naming two hierarchies; name the class, not the role |
| your **whole suite** passes and the composite is still wrong | *fixtures that inherit the subject's convention* — a case built from the p-code's own naming cannot test that naming; construct one fixture a different way and see if it still agrees |
| a captured value **equals one of the inputs** | *the wrong answer that names the register* — suspect the capture, not the arithmetic; a register still holding a step, limit or argument means execution stopped in the wrong place, and a stubbed call costs TWO steps because stubs key on the callee's entry |
| you are about to **correct** an earlier claim | *the over-correction* — a correction takes the same evidence as any other claim and gets far less; if it says "in A, not B", enumerate ALL the sites, because one call site is not a survey |
| you are substituting a **library function** for a retail routine | *the verdict that averages away a different function* — compare worst-case in ulp with no tolerance, then read each non-zero case; a handful of enormous gaps among exact matches is a subdomain guard, not rounding |
| a differential fails by **one ulp**, on some cases only | *both sides right, disagreeing on the inputs* — build every candidate rule and run them together; if they all reproduce the oracle the fixture is feeding two different numbers, and the repair a green suite rewards is the wrong one |
| your **controls** all pass, or you never asserted that they fail | *a domain that cannot express the difference* — coverage of the input space is not coverage of the distinctions; assert `disagreements > 0` per control, and for a rounding control use inputs with long binary expansions |
| your fixture is **self-describing** — byte `k` holds `k`, field `n` holds `n` | *the fixture whose answer is its own input* — check what a null implementation (a copy of one operand, a no-op) would produce; if that also matches, the case proves nothing |
| your filter returned **nothing, uniformly**, and "none of them" is a plausible answer | *a guard tighter than the data it filters* — a bound is a claim and needs a reason; run the filter against a case the repository has already named, and if a repository tool answers the question, call it instead of reimplementing it |
| you measured a **vtable's size** by reading until the words stop looking like code | *a vtable's extent read as a run of pointers* — MSVC puts each class's RTTI locator at `vtable[-1]`, so vtables are packed COL/slots/COL/slots; the class map has the boundary and "still a code pointer" cannot find it |
| a register **never appears** in a function body, so you concluded it is not an argument | *the argument that passes straight through* — a volatile register forwarded untouched to the first call is invisible in the mnemonics; check whether anything writes it before that call, not whether it is mentioned |

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

### The mechanism, found in cycle 1220

The gap is not random coverage loss. **`Ac6XenonRefs` iterates
`getListing().getInstructions(true)`, which yields only instructions Ghidra has
already disassembled**, while `Ac6XenonDisasm` calls `disassemble(addr)` and
creates them on demand. The 8.5% is precisely *every function auto-analysis never
reached*, and the two scripts in this repository disagree about the program by
exactly that set.

Caught live: a substring search for `0x1ad8` returned **zero**, while
`821b54c8  lwz r11,0x1ad8(r11)` exists and contains it — and a bare `1ad8` in the
same run returned eleven hits, so the matcher was working. The instruction is
simply not in the listing.

**So the check is one command.** When a negative matters, run `Ac6XenonDisasm` on
an address you expect to be involved: if it disassembles something the scan did
not report, the scan was blind there and the negative is void. This costs
nothing, and it had not been being done.

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
| functions | 8,247 (Ghidra) / **8,135** (`.pdata`, complete) |
| reachable from `main` by direct `bl` alone | 800 — **measured on Ghidra's 91.5% listing, too low** |
| **reachable by `bl` plus non-local `b`, 100% of `.text`** | **2,144 of 8,135, about 26%** (cycle 1218) |

**Direct-call reachability covers about a quarter of this program**, not the 10%
first published here. The first figure was itself taken with the instrument this
file warns about two sections above — the listing that covers 91.5% of `.text` —
and it is corrected rather than deleted because *the correction is the point*: a
discipline file carrying a wrong margin teaches the wrong margin.

The qualitative conclusion is unchanged. Three quarters of the program is still
unreachable by direct call, and every finding cycle 1213 drew from this stands.
It is an object graph with a message pump, not a call graph.

Every entry above this one is about believing something that is false. This is
the mirror: **declaring live code unreachable**, and it is available here at a hit
rate of roughly nine in ten. Cycles 1193, 1195, 1196 and 1202 were burned by live
code on a dead path. Cycle 1213 found the same binary will just as happily hide a
live path behind a vtable slot — the NDXR loader, the Mission 01 texture mounts
and the unit placement were all "unreachable" by `bl` and all run.

**A `bl`-only reachability negative in this binary is not weak evidence. It is no
evidence.** Do not write "no caller found" without saying which kind of caller was
looked for.

### The four techniques that do work

1. **Vtable resolution.** Dump `.rdata` as words; find the function's address
   inside a run of `.text` pointers; find code materialising a base inside that
   run. That code is the constructor, and the delta from the run's start is the
   slot index. This turns "vtable-only, nothing calls it" into a chain.
2. **Receiver-offset matching.** When a virtual call loads its receiver at
   `this + K`, and a constructor builds a sub-object with the vtable under test at
   the same `K`, and *the same distinctive `addis`/`subi` pair appears on both
   sides*, the identification is checkable and can fail. Cycle 1213 pinned three
   classes this way on three different literal offsets — `+0x29520`, `+0x29130`,
   `+0x35C00` — any one of which could have mismatched. Cycle 1218 sharpened it:
   of **110** slot-`+0x2C` call sites in the image, **exactly one** loads its
   receiver from `[rX+0x288]`. Zero would have meant the identification was
   wrong; many would have meant it was useless.
3. **MSVC RTTI recovery.** For each run of `.text` pointers, read the word at
   `vtable − 4` as a `RTTICompleteObjectLocator`, follow `+0x0C` to the type
   descriptor, and read the decorated name at `+8`. Cycle 1218 got **811 named
   vtables** this way, independently matching the 811 figure this campaign has
   carried since J2. It converts every "vtable-only" dead end into a class name
   that can be checked against a constructor.
4. **Tail calls through `bcctr`.** `CAce6TaskManager::Register` ends in `bcctr`,
   **not** `bcctrl`, dispatching slot `+0x0C` of its argument. Every task
   registration hook in this program — including the entry to the whole
   mission-load chain — is reached only through that one tail call, and a scan
   for `bcctrl` finds none of them. `.pdata` also yields a complete function list
   (**8,168 entries, 8,135 unique starts**) where Ghidra's is short.

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

## The fifteenth shape: the displacement collision, and the four lines that settle it

A scan for a field's displacement returns a clean, complete, plausible candidate
list — and the candidates are a **different structure with a field at the same
offset**. Three times this session:

| cycle | displacement | the candidates were |
|---|---|---|
| 1220 | `0x1AD8` | a field on an object allocated two instructions earlier, not the creator table |
| 1227 → 1228 | `0x1084` | a field-init block on some object, not the renderer's device pointer |
| 1224 | `0x10` | genuinely the right structure — the shape does not always fire |

The failure is not the scan. The scan did what it was asked. The failure is
treating *"a store exists at this offset"* as *"a store exists to this field"*,
which are different claims and only the second is ever what a cycle wants.

### The tell, and it costs four lines

**Read the neighbours.** A field belongs to the structure its neighbours belong
to.

- `0x1AD8` at `821c5ea0` sits two instructions after `bl 0x82222e98`, an
  allocation, and stores its result. Whatever that object is, it was born on the
  previous line.
- `0x1084` at `822c52e4` sits in a run of `stfs f0,0x1058` / `stfs f13,0x105c` /
  `stfs f13,0x1060` / `stfs f0,0x1068` / `stfs f0,0x1078`. **A device pointer does
  not live in a block of single-precision floats.**

Neither needed dataflow, register tracking, or a second tool. Both needed the
four lines above and below the hit, which the scan output does not print and
which I did not go and look at until the claim had already been written down
once.

### Why this deserves its own entry

The rule that would have caught it — *walk up to the conditional, walk down to
the back-edge* — is about **control flow**. This one is about **data**, and the
two fail independently. Cycle 1214 had the control flow wrong with the data
right; cycle 1227 had the data wrong with the control flow irrelevant.

**Print context around every hit before believing what the hit is a hit on.**

## The sixteenth shape: stopping at a natural boundary, twenty bytes short

Three times in one session, a cycle published a conclusion and the instruction
that refutes it was **within twenty bytes of where the reading stopped**:

| cycle | stopped at | what was just past it |
|---|---|---|
| 1214 | the end of a 300-instruction dump | the loop's back-edge, `0x8C` later — the "fixed-member test" was a loop body |
| 1224 | a `lwzx`/`stw` that looked like the whole function | a `cmpwi r4,2` guard eight instructions up, making the published index impossible |
| 1223 | `8218d2c4 blr` | `0x8218D2C8`, the identical **clearer** of the setter it had just written up as "sets, never clears" |

None was a careless read. Each stopped at something that **looks like an
ending** — a `blr`, a return path, the edge of a dump — and in each case the
answer to the cycle's own question was on the other side of it.

### The mechanical fix, and it is embarrassingly cheap

- When a dump ends at a `blr`, **disassemble the next address anyway.** MSVC emits
  sibling overrides back to back; the setter and its clearer differ by one
  `li` immediate and sit eight instructions apart.
- When a block of interest sits near the end of a 300-instruction dump, **re-dump
  from before it** and read past it until a `blr`, an unconditional forward `b`,
  *or a backward branch*.
- Before publishing "X does A and never B", **read the next function.** The
  counter-example is more often adjacent than absent.

### Why it is its own shape

The other entries here are about instruments that returned the wrong answer. This
one is about a reading that was correct over the range it covered and wrong about
the program, with no instrument at fault. **The scan reported its denominator
honestly; the reader chose the range.**

Cycle 1223's case is the sharpest: it asked "does anything clear this byte",
examined a function, answered "no, it sets", and the clearer was the next symbol
in the file.

## The seventeenth shape: the right search, run against a sibling

Cycle 1244 could not find what starts the Set leader's FSM. The search was
correct: `CFsm::SetInitialState` is `0x8219AAE8`, established in cycle 1218, and
it has **five call sites in 100% of `.text`** counted with the force scan. All
five install `CModeTaskGame` states. None belongs to the unit family.

The answer was that `CFsm` is a **template instantiated twice**, and the two
copies differ in exactly one instruction:

```
8219ab38  subi r3,r10,0x268     CFsm<CModeTaskGame>    owner at +0x268
82295080  subi r3,r10,0xf0      CFsm<the unit class>   owner at +0xF0
```

Identical otherwise, instruction for instruction. The unit's pair is
`0x82295030` / `0x822950A0`.

**This is not a coverage failure.** The instrument was right, the denominator was
100%, the count was exact, and the conclusion — *nothing in the unit family calls
this* — was **true**. It was just not the question.

### Why it is worth its own entry

Every other shape here is about an instrument that under-reports. This one
under-reports nothing: it answers precisely what was asked, and what was asked
was one instantiation of a template whose other instantiation held the answer.

### The tell, and it is cheap

**When a C++ function you are chasing is generic — a container, a state machine,
a handle, anything with an owner offset baked in — assume there are siblings and
go find them before believing a call-site count.**

The mechanical version: take the function's body, pick an instruction that
encodes the *instance* rather than the algorithm (here the `subi` displacement),
and search for the same body with a different value. In this image that was one
force scan for `subi rD,rA,0xf0`, which returned 18 hits, 11 of them real, and
the sibling among them.

A count of call sites is a statement about **one symbol**. Templates make one
algorithm into many symbols, and nothing in a disassembly marks them as related
except that their bodies match.

## The eighteenth shape, and it is the one all the others are special cases of: a correct measurement, over-read

Cycle 1192 scanned the loaded image for the bytes `46 48 4D` and found **zero**,
with three positive controls in the same run (`NDXR` at `0x8200A24C`, `GIDX` at
`0x82067EC8`, `NTXR` at `0x82067EC0`). The measurement was exact, controlled, and
**true**. It is still true.

The sentence it produced was *"retail does not parse an FHM container"*, and that
sentence stood for fifty-six cycles, was carried into two more reports, and
grounded a design decision about the product.

Cycle 1248 found `0x82234C18`: a directory reader that takes a version byte, an
endian byte and a table offset, and **never compares a magic**. Retail parses the
format. It just never looks at its name.

**The measurement licensed "the tag does not appear". The sentence said "the
format is not read".** Those are different claims, and the gap between them is
where fifty-six cycles of a wrong premise lived.

### Why this is the general case

Look back at the others with this in hand:

- *the true positive from dead code* — "this instruction exists" read as "this
  instruction runs";
- *querying only one side* — "true of this corpus" read as "true";
- *the listing is not the code* — "not in 786,122 instructions" read as "not in
  the program";
- *reachability by `bl`* — "no direct caller" read as "unreachable";
- *the displacement collision* — "a store at this offset" read as "a store to
  this field";
- *the right search against a sibling* — "no caller of this symbol" read as "no
  caller of this function".

**Every one is a measurement whose scope was widened by the sentence that
reported it.** The instrument was honest in all six.

### The rule, and it is about writing rather than scanning

**Write the measurement, then write the claim, then ask what stands between
them.** If the answer is a step of reasoning, that step is a hypothesis and needs
its own control — not the one that produced the measurement.

Cycle 1192's missing control was cheap and available: *if the format is not
parsed, then no function reads a structure at its layout.* One scan for the
header's shape, over the 439 bundles, would have found `u16[+0x06] = 0x10`
uniformly and sent someone looking for the reader.

A zero tells you where something is not. It never tells you what is not
happening.

## The nineteenth shape: the collision in the prose

Cycle 1244 wrote that `0x820A7070` writes *"on each unit `+0x184`, `+0x170`,
`+0x118`, `+0x188` — and on the leader `+0xD8`, `+0xDC`, `+0xE0`, `+0xE4`"*.

Every offset is right. Every one was read from an instruction. And the sentence
is wrong, because **"unit" names two different class hierarchies**:

- the loop's `r31`, from factory slot `+0x14`, has vtable **`0x820078D0`** (called
  a `galib::CGaObj` until cycle 1281 showed the real one is `0x820572C0`; the two
  share 11 of 96 slots) — it gets
  the parent pointer at `+0x188`;
- `r16`, from slot `+0x10` and the only object the function registers, is an
  **`ACE6::CAce6Unit`** — it gets the child array and the order FSM.

Both are RTTI roots, so they share no base. `+0xD0`, `+0xD4` and `+0xDC` collide
too — integers in one hierarchy, floats in the other — and they disagree about
where their own matrix sub-object lives.

A reader following that sentence would put the child array and the parent pointer
on the same object. They are not on the same object.

### Why it needs its own entry

The fifteenth shape — *the displacement collision* — is this failure in a **scan**,
and its fix is mechanical: read the four lines around the hit, because a field
belongs to the structure its neighbours belong to.

This is the same failure in a **paragraph**, and no reading of neighbours
catches it. The listing was read correctly; the collision entered when two
correctly-read objects were given one English noun.

### The rule

**Name the class, not the role.** "The unit", "the object", "the entity", "the
manager" are all free to denote two things in one flow, and a report is where
that becomes invisible — a disassembly at least forces you to write down a
register.

When a cycle describes several offsets on "the X", check that every one was read
from the same register, and if it was not, say which class each belongs to. Cycle
1250 needed a vtable length and an RTTI walk to separate two objects that four
reports had been calling by the same word.

## The twentieth shape: an agent's scope, written as the repository's

A delegated investigation reported, correctly, that converting NTXR pixels *"needs
`0x821FCA48`, the X360 tiler, which is not ported"*. That sentence was true of
**what the agent had looked at**. It is false of the repository:
`src/ntxr_texture.cpp` has untiled Xenos Tiled2D since long before — with
`xenos_tiled_2d_offset`, `pad_to_tile` at 32 blocks, BC1/BC2/BC3 decoding, a
corpus pixel hash and an endianness control scoring 468/170/30.

I carried it into a task list before checking. One `grep` corrected it.

### Why delegation makes this specific

An agent is given a brief, a scratch directory and a question. It does not know
what the product already contains unless the brief says so, and **a brief that
listed everything the repository can do would be longer than the question**. So
"not ported", "not available", "would need to be built" are, from an agent,
always statements about the brief's horizon.

The failure is not the agent's. It answered what it was asked, and said so. **The
failure is incorporating the sentence at the scope it was written in.**

### The rule

**Before propagating a delegated "X does not exist" into a task, a report or a
decision, grep for X.** It costs one command, and the alternative is a task list
that describes work already done — which is worse than one that omits work,
because it looks complete.

This is the eighteenth shape — *a correct measurement, over-read* — with the
measurement taken by somebody else. The widening happens at the hand-off rather
than at the writing, and there is no instrument between the two except the
reader.

## The twenty-first shape: the instrument sampled a third of it, and said nothing

`Ac6XenonDisasm` takes a list of **start addresses** and emits up to 300
instructions from each. Cycle 1254 passed `820A7070 820A7EB0` meaning the range,
and received two blocks: 300 instructions from the function's start, and 300
more from an address past its end. Coverage of the intended range: **300 of
912**.

Nothing in the output said so. The first block ended on `lwz r16,...` — an
ordinary instruction, no marker, no trailer — and a listing that stops on an
ordinary instruction is indistinguishable from a function that ends there.

The claim under test was a **negative**: "the Set index is never compared
against a literal." Over 300 instructions it returned six occurrences of `r21`
and no comparison. Over the corrected 912 it returns thirteen occurrences and
**two** comparisons, and both turn out to be innocent for reasons that had to be
read: one is on the register *after* `subi r21,r15,0x1` overwrites it, the other
is the loop bound. The conclusion survived. The method did not — a third of a
function was standing in for all of it, and the survival was luck.

### The rule

**An instrument that can truncate must say when it truncated.** `Ac6XenonRefs`
prints its scanned count; `Ac6XenonForceScan` prints `scanned/already_listed/
forced/undisassemblable/hits` precisely so a zero states its denominator.
`Ac6XenonDisasm` printed nothing and was the one used for exhaustiveness claims.
It now prints a per-block trailer naming the count and whether the cap cut it.

The second half of the rule is narrower and cost the same hour: **check the
argument semantics of a tool before reading a negative out of it.** Two hex
values are a range in most tools here and are two starts in this one. The
difference is invisible in the output, which is exactly why it belongs in the
output.

## The twenty-second shape: an instrument calibrated on one specimen, and it agreed with itself

Cycle 1256 needed to read a resource id out of a container. It built the reader
twice, from one file each time, and both readers were confidently wrong.

The first anchored on a six-entry wrapper whose id was known from its filename:
records at `0x10`, stride `0x50`, id at `+0x48`. **All six entries agreed**, and
their ids came out consecutive — `0x10002215` through `0x1000221A` — which reads
like corroboration and is not: consecutive ids are what that file has, not
evidence about where ids live. Over the corpus the same reader produced 336
distinct file contents carrying 33 distinct ids, and the value it was reading in
a one-entry wrapper was the ASCII tag `GIDX` itself.

The second, older, located the id as "the first word at or above `0x10000000`",
borrowing the threshold from the mount code. It returned **five different
offsets** over 346 files.

Neither failure was subtle in hindsight, and neither was visible from the file
it was built on. **A layout derived from one specimen is a description of that
specimen.**

### The rule

**Before trusting a layout, find the control the container gives you about
itself** — a declared count, a length field, a terminator, a magic. Here it was
free and decisive: the number of `GIDX` chunks must equal the entry count the
header declares, and it does in **346 of 346** wrappers. That control was
available before either wrong reader was written, and it rejects both instantly.

The corollary is about the shape of the corroboration. "All six entries agreed"
is one specimen's internal consistency, and it is exactly what a wrong offset in
a regular structure produces. Agreement **across** specimens is evidence;
agreement **within** one is arithmetic.

## The twenty-third shape: the refuted link, and what a real one would have looked like

Cycle 1263 drafted a link between a table-clearing routine and the vertex stride
rule: the clearer's loop nest is `3 × 6 × 8`, which is exactly the arity of the
stride formula's two index spaces, and it sits four instructions before the
stride builder. Then it tested the adjacency and refused it — the two functions
have entirely separate single callers, and **image order is not evidence**.

Two cycles later the link turned out to be real, on evidence of a completely
different kind. The clearer's caller and the builder's caller are sibling
functions over the same object, calling their respective routines on the same
two members, `this+0x28` and `this+0x170`. One builds what the other clears.

**The refutation was not wasted, and publishing the first argument would have
been worse than being wrong.** Had cycle 1263 shipped the adjacency reading, the
caller would have been found later and would have *agreed with it* — and the
agreement would have retroactively licensed "these functions are near each
other" as a way of arguing. The habit survives its bad instances by being
confirmed in the good ones.

### The rule

**When a plausible link fails its test, the question to carry forward is not
"is the link real" but "what would a real link look like".** Adjacency, similar
names and similar constants are all unfalsifiable in the small; a shared member
offset in a shared object, an exclusive call site, a common caller — those are
checkable. Name the checkable form before looking for it, then look.

The corollary is about where to look: cycle 1263 could not reach the answer
because it never left the two functions it was comparing. **A relation between
two things is often not visible from either of them.**


## The twenty-fourth shape: the `.pdata` row read as a dispatch slot

Cycle 1265 concluded that two functions are "reached through tables rather than
by call", because each appeared exactly once as an aligned 32-bit word elsewhere
in the image. Both occurrences were the function's own **`.pdata` row**.

The exception table spans `0x82079E00`…`0x82089FB0` — 8,246 entries of eight
bytes, an address followed by a packed prolog/length word. Dumped without
context it reads exactly like a dispatch table:

```
8233e580 40001903 8233e5e8 40002e03 8233e6b0 40001903 8233e718 40001905
```

and a scan for `0x8233E6B0` reports one hit, truthfully and uselessly. **A
function with an exception record appears once as data by construction.**

The same scan has carried two load-bearing negatives this session, and one of
them survives only because its subject is not in the table: `0x8234CDC0`, the
registry insert, has **no** `.pdata` row, so its zero data hits really did mean
no vtable and no dispatch table reaches it. The scan did not know the
difference. Neither did its author at the time.

### The rule

**Exclude `0x82079E00`…`0x82089FB0` before reporting a function address as
data, and state which side of that line the address falls on.** One hit outside
it is a reference; one hit inside it is the function existing.

The general form is the eighteenth shape again — a correct measurement, over-read
— but the specific trap is worth its own entry because the output is
*indistinguishable*: the scan reports "1" in both cases, and the number is right
in both cases.

**And the repository already knew.** Cycle 1225, which wrote this instrument,
printed the block name for every hit and labelled the row itself:

```
821b5808  at 0x820655CC  .rdata      <- a vtable slot
821b5808  at 0x8207EBA8  .pdata         (an unwind record, not a reference)
```

The distinction was lost forty cycles later, when the same scan was
re-implemented in Python directly over the flat image — faster, correct in what
it counted, and without block names. **A scan re-expressed in another language
is a new scan. It inherits the earlier one's arithmetic and none of its
judgement**, which is the lesson the `PrintableStringLength` control taught about
tests, arriving a second time about instruments.

## The twenty-fifth shape: a rule that was written, correct, and unrunnable

This file has warned about register-held materialisation since the `0x29c80`
case: an offset built as `lis rX,0x2; ori rY,rX,0x9c80` is found by no text scan
for `0x29c80`, and a zero from such a scan is void. The warning is right, it is
indexed, and it has been read.

It still cost thirty cycles. Cycle 1244 published *"`0x82297540` has zero
instruction references"* and made it the placement chain's single open hop; the
address is materialised at six sites, and 135 of Mission 01's 230 units stayed
unplaced behind that sentence until cycle 1275.

**The rule was unrunnable.** Following it meant hand-decoding `addis` immediates
across seven megabytes and pairing each with a later `ori` or `addi` on the same
register — an afternoon, for one address, with no tool. Every other scan here
was one command. So the rule was obeyed where it was cheap and skipped where it
was not, which is the same as not having it.

### The rule about rules

**A discipline entry that names a check without providing a way to run it is a
warning, not a control.** It will be honoured on the days it is convenient and
forgotten on the days it matters — and the days it matters are the ones where
the obvious scan already returned a clean, plausible zero.

When you write a shape, ask what command a reader runs to obey it. If the answer
is "by hand", the entry is unfinished. `tools/find_materialised_address.py` is
what finishes this one, five instrument-discipline sections after the warning
was first written.

The corollary is a way to audit this file: **for each shape, name the command.**
Where there is none, either write the tool or say plainly in the entry that the
check is manual and expensive, so a reader knows the cost before deciding to
skip it rather than discovering it afterwards.

## The twenty-seventh shape: fixtures that inherit the subject's convention

Cycles 1296 to 1299 built a validation suite for the vector layer: one retail
instruction per case, every input seeded, the output captured, compared against a
value worked out by hand. Sixteen cases, eleven mnemonics, 179 sites, all green.
Cycle 1298 wrote *"every vector instruction in the closure is correct"* on the
strength of it, and the composite those instructions make up was still producing
an answer that did not depend on its input.

**The fault was in the one place none of the sixteen could look.** On Xenon there
are 128 vector registers and both instruction families address the same ones; in
this SLEIGH module they are disjoint storage, so a value an AltiVec-form
instruction writes is invisible to the VMX128-form instruction that reads it.

Each case was written by reading the instruction's own p-code and seeding what it
named: `vmrghw` at `vs42`/`vs40`, captured at `vs38`; `vmulfp128` at
`vr0`/`vr13`, captured at `vr12`. **No case ever crossed between the two
namings**, because no instruction's p-code ever asked it to.

The cases were not weak individually. The flaw is that they were all built the
same way, so their blind spot was shared instead of averaged out — sixteen
independent measurements of the same thing, none of them independent of the
convention under test.

This is the mirror of *an instrument calibrated on one specimen* (the
twenty-second). There, one specimen agreed with itself; here, sixteen specimens
agree with each other because they were cut from the same template.

### The rule

**When a suite is green and the thing it validates is not, suspect the property
every case shares.** A fixture derived from the subject's own description — its
p-code, its header, its declared count — inherits whatever that description gets
wrong, and no number of such fixtures adds an independent check.

Build at least one fixture from somewhere else. In this case the check took one
run: seed a register under the AltiVec name, read it under the VMX128 name, and
see whether the value is there. It was not, and that single crossing was worth
more than the sixteen that did not.

## The twenty-eighth shape: the fixture whose answer is its own input

Cycle 1320 added two cases for `vperm`, and built their fixture the obvious way:
the 32-byte concatenation with every byte distinct and **byte `k` equal to `k`**.
Distinct bytes are the right instinct — a misplaced lane shows as a value rather
than as a coincidence — and the identity is what makes the expectation readable.

It also makes it worthless. Under `cat[k] = k`, the expected output is

```
out[i] = cat[control[i]] = control[i]
```

**the control vector itself.** So a hypothetical implementation that merely
copied its third operand to the destination would pass both cases, and so would
one that permuted correctly, and the fixture cannot separate them.

Both cases did match on the first run — and the behaviour under test **never
fired**. Ghidra's own `PPCEmulateInstructionStateModifier` implements
`vectorPermute` a layer below SLEIGH and wins over a registered callback, so the
value came from somewhere else entirely. The suite's `matched but no behaviour
fired` guard is the only thing that saw it; on the value alone, both cases were
green and both were empty.

This is not the twenty-seventh wearing a new face. That one is about a fixture
inheriting the subject's *convention*. This one is about a fixture whose right
answer is a **copy of one of its own inputs**, so the null hypothesis — the
instruction moved an operand, or did nothing at all — survives the test.

### The rule

**Before trusting a green case, ask what a null implementation would produce.**
A copy of operand 1, a copy of operand 3, the destination unchanged, all zeros.
If any of those equals the expectation, the case has no power against it.

Here the fix cost one line: scramble the concatenation, `byte k = (7k + 0x13) &
0xFF`, distinct over the range and equal to `k` for no `k`. Then the expected
output is a value no operand holds, and only a real permute produces it.

Two of this file's other shapes are the same failure at a different scale. *An
instrument calibrated on one specimen* is a fixture that agrees with itself;
*fixtures that inherit the subject's convention* is a set of fixtures that agree
with each other. This is one fixture that agrees with the null hypothesis.

## The twenty-ninth shape: a vtable's extent read as a run of pointers

Cycle 1334 wanted `galib::CGaLocator`'s interface, read its vtable, and stopped
at the first word that was not a code pointer. **Ninety-one slots.** The class
map's next named vtable begins **eight bytes** after the base.

Cycle 1335 found the mechanism, and it is worse than "the run was too long".
MSVC places each class's RTTI Complete Object Locator at `vtable[-1]`, so a
`.rdata` region of vtables is packed

```
COL | slot | slot | ... | COL | slot | ... | COL | ...
```

and a COL is **data**. So the run does not merely fail to end where the object
ends — it **alternates** code and data, and a terminator based on "does this word
look like code" is answering a question about the section, not about the class.

`CGaLocator`'s vtable turned out to be **one slot**: its destructor. The word
after it is `CGaObjDesc`'s COL, whose type descriptor spells
`.?AVCGaObjDesc@galib@@` — which is exactly what the class map already said sat
at that address.

### The rule

**A vtable's extent comes from the class map, not from the bytes.** The next
named vtable's base is the boundary, and `analysis/class-map.tsv` has 811 of
them. Reading pointers until they stop looking like pointers measures the
section.

**And that boundary is an UPPER bound, not the extent** — cycle 1338 corrected
cycle 1337 on exactly this. `ACE6::CAce6UnitOtherPlayer` looked 31 slots wide
because the next named vtable is 31 words away; eight of those words are the
ASCII `"GeneralDataProcess"`, `"(isi)"`, `"fogParam"` and two pointers, a script
binding table parked between two vtables. The class has 23 slots like its
siblings.

So the map says where the next vtable starts and nothing about where this one
stops. A class that comes out **wider than the one it derives from** is the tell,
and one byte dump settles it.

Two cheap confirmations when the map is silent: the COL sits at `vtable[-1]` and
its type descriptor carries the mangled name, so a suspected boundary can be
checked by decoding the word before it; and a destructor re-installs its own
vtable, so slot 0 usually names the class by materialising its base.

This is the sibling of *stopping at a natural boundary* — there, a `blr` looked
like an end and was not; here, a non-code word looked like an end and was
somebody else's beginning.

## The thirtieth shape: the argument that passes straight through

Cycle 1352 read all 45 instructions of `0x82211DF8`, found no `f1` anywhere, and
wrote: *"It receives no float. `0x82211DF8` takes `r3` alone."* It even flagged
that as a plan prediction meeting a measurement and losing.

**It receives `f1`.** Cycle 1356 read its caller, which does `fmr f1,f31` before
each of the five calls, and `0x82211DF8` forwards that register untouched into
`bl 0x82211B40` — its first call, reached before anything could clobber a
volatile register.

A pass-through argument **is invisible in the mnemonics**. Nothing mentions it
precisely because nothing has to: the callee's ABI put it where the next callee's
ABI expects it. Searching a body for a register name finds every *use* and no
*forward*.

### The rule

**Absence from a body is not absence from a signature.** For a volatile argument
register, the question is not "is it mentioned" but "who writes it first" — and
that search does not stop at the first call.

**Cycle 1357 corrected this rule one cycle after it was written.** It said the
argument reaches "that call", meaning the first one. `f1` reaches `0x82211DF8`,
survives `0x82211B40` (51 instructions, no floating-point operation at all),
survives `0x82211C10` (121 instructions, likewise), and is consumed by
`0x82211988` — the **third** call. A volatile register survives every callee that
does not write it, so the destination can be arbitrarily far down the chain.

The rule is therefore: **follow the register forward through the callees, testing
each for a write, until one uses it.** Stopping at the first call finds the wrong
consumer as confidently as reading one body finds none.

The cheap check is the caller, and it is the one direction a single-function read
never covers. This is the sibling of *reachability by `bl`*: there, a function
looked unreferenced because the reference was elsewhere; here, an argument looked
absent because its use was one frame further down the stack.

## The audit this file owes itself

The twenty-fifth shape says: for each shape, name the command. Run against this
file, **fifteen of sixteen shapes named none.** That number is worse than the
truth — several are reading disciplines that cannot have one — so here is the
three-way split, which is the useful form.

**Has a command, and it must be run.**

| shape | command |
|---|---|
| a rule that was written, correct, and unrunnable | `tools/find_materialised_address.py IMAGE ADDR` |
| the `.pdata` row read as a dispatch slot | `Ac6XenonFindWord` — read the three-way `aligned/unaligned/pdata` count, not the total |
| the instrument sampled a third of it | `tools/check_listing_against_pdata.py IMAGE ADDR --listing FILE` — `.pdata` declares the length, so the comparison is arithmetic, not judgement; and read `Ac6XenonDisasm`'s per-block trailer |
| a dispatcher you believe you have read | `tools/count_indirect_branches.py IMAGE START END` — it counts the `bctr` and recovers each jump table; `0x82263A50` has three and was read twice as though it had one |
| reachability by `bl` | `Ac6XenonForceScan` prints `scanned/already_listed/forced/undisassemblable/hits`; a zero without its denominator is not a negative |
| an agent's scope, written as the repository's | `grep` for the thing the agent said does not exist, before propagating it |
| a correct measurement, over-read | `tools/audit_contract_derivations.py` for the contract case; elsewhere, write the measurement and the claim on two lines and compare them |

**Manual, cheap, and therefore no excuse.**

*stopping at a natural boundary* — read twenty bytes past where you stopped.
*half a rule* — read the conditional above and the back-edge below.
*the collision in the prose* — name the class, not the role.
*querying only one side* — ask the other side the same question.

**Manual and expensive, so the cost is stated rather than discovered.**

*an instrument calibrated on one specimen* — needs a control the container gives
about itself (a declared count, a length, a terminator) and then a run across
the whole corpus. Building that control is the work; there is no shortcut, and
skipping it is how two readers were shipped in one cycle.

*the displacement collision* and *the right search, run against a sibling* —
both need the population established before the search. That is now a command:
`tools/whose_vtable.py IMAGE ADDR` names every vtable holding an address,
excluding `.pdata` and preferring `analysis/class-map.tsv` to a fresh RTTI walk.
**It answers for 145 of 232 hits on a shared stub and for none of six on the
unit family**, because `0x820078D0` holds zero at `vtable-4` and `0x82009440`
holds a function address — neither is in the audited map. So the command settles
the question where RTTI exists and tells you plainly where it does not, which is
the part a hand walk got wrong once.

*the refuted link* — no command; the discipline is to name the checkable form
(shared member offset, exclusive call site, common caller) before looking for it.

*the true positive from dead code* and *the listing is not the code* — both are
about `.pdata` and auto-analysis coverage, and both are checked by the
denominators the two scans above already print.

**What this audit changes**: nothing about the shapes, and everything about how
they are used. A reader mid-investigation now knows, per symptom, whether they
are one command away from an answer or an afternoon away — and that is the
difference between a check that gets run and one that gets skipped exactly when
it matters.

## The twenty-sixth shape: the unexamined contradiction

Cycle 1289 ended by naming a mismatch — the data puts a non-angle where the
fallback initialiser puts 46° — and calling it *"the first thing to check before
anyone treats the record layout as understood."* It was flagged as unresolved,
not asserted, which is the only reason it cost one cycle.

**It was not in the image.** The initialiser's store used a different register
than the report said. Cycle 1283 had it one register wrong, and I repeated it
without reading the loop.

Every other shape in this file is about a **positive** claim receiving too
little scrutiny. This is the mirror, and it is easier to fall into: a
contradiction *feels* like the careful thing to say. Writing "these two
disagree" reads as honesty, invites no challenge, and is filed as an open
question rather than a finding — so it escapes the check that "these two agree"
would have drawn.

A mismatch is a claim about **two** readings, so it needs both of them read. It
is strictly more work than an agreement, and it habitually gets less.

### The rule

**Before publishing a discrepancy, re-read the instruction that produced each
side.** Not the report that quotes it, and not your own earlier summary — the
instruction. If either side came from someone else's transcription, that is the
side to read first: cycle 1285's operand-field bug, cycle 1277's off-by-one
store address and this one all entered the same way.

The command, where the sides are a listing and a constant:
`tools/check_listing_against_pdata.py` for the listing's completeness, then read
the store and its register by hand. There is no tool for "did you read the right
register", and saying so is the point of the twenty-fifth shape.

## The thirty-first shape: a guard tighter than the data it filters

Cycle 1370 wrote a throwaway RTTI reader — vtable minus four gives the complete
object locator, `+0x0C` gives the type descriptor, `+8` gives the name — and
guarded each pointer with `0x82000000 <= p < 0x82400000`. Every class in the
binary came back **unnamed**, including `galib::CGaLocator`, which the campaign
has been naming correctly for twenty cycles.

AC6's type descriptors live at **`0x8268F…`**. The guard was a sanity check
written from the section bounds this campaign reads most often — `.rdata` ends at
`0x82079DD3`, `.text` at `0x823D772B` — and the descriptors are in neither.

The failure mode is what makes this a shape rather than a typo:

- **It produced a uniform, plausible answer.** Not a crash, not a partial
  result, not one odd row. "This binary has no RTTI" is a *thing that happens*,
  and several classes here genuinely have no COL, so the false answer had
  corroboration.
- **The correct version was already in the repository.**
  `tools/whose_vtable.py` guards with `BASE <= p < BASE + len(data)` and has
  been right the whole time. The defect was introduced by re-implementing a
  working instrument inline rather than calling it.
- **It was one guard away from deleting a third of the class map.** Had the bad
  reader been used to rebuild `analysis/class-map.tsv` instead of to answer one
  question, 811 named vtables would have become 0 and the count of unnamed ones
  would have "grown" from 306 to 1,117 with no error anywhere.

### A companion the shape needed, added at cycle 1383

Three days after this was written, its author concluded that four p-code
operations "must be supplied" to the emulator, costed them, and argued for
implementing them. **All four were already implemented**, behind a `vmx on`
directive that is off by default and that the failing spec never set. One line
took the run from 601 steps to 1034.

So the rule below is necessary and not sufficient. Calling the repository's tools
is not enough when the tool is *already the one you are running* and the missing
piece is a directive it accepts. The companion:

**Before concluding an instrument cannot do something, read its own directive
list.** `MicroExecuteFunction.java` documents every spec keyword in its header
comment; the answer was thirty lines above the code being read. The failure was
not ignorance of the harness but confidence that the error message named a
capability rather than a switch.

### The rule

**A bound is a claim, and it needs a reason.** `< 0x82400000` was not read from
anything; it was a round number near where the sections this campaign usually
reads happen to stop. When the filter's job is "is this a pointer into the
image", the bound is the image — `BASE + len(data)` — and anything narrower must
name the section it comes from and why that section is the right one.

And the cheaper rule that would also have caught it: **if a repository tool
already answers the question, call it.** The reimplementation is where the bound
was invented; `whose_vtable.py` never had it.

The control that catches this class of defect in one line is a **known-good
positive**: run the filter against something whose answer you already know. Here
that is any vtable in `class-map.tsv`. A filter that returns "nothing found" for
a case the repository has already named is broken, not informative — which is the
eighth entry of *The pattern* restated for guards instead of searches.

The test is `test_a_descriptor_past_the_rdata_bound_is_still_read` in
`tools/tests/test_ac6_static_tooling.py`.

## The thirty-second shape: a domain that cannot express the difference

Cycle 1372's first test run for the flight integrator:

```
controls: unfused=0 floor-on-all=1215 double-precision=0
```

Two of three controls — multiply-then-add instead of a fused `fmadds`, and
double precision instead of single — **agreed with the reference on all 1,215
sweep points**. Nine behavioural cases passed. The port was correct, and the
suite proved nothing about the arithmetic the fused forms exist for.

The cause is not a self-describing fixture (the twenty-eighth shape) and not a
fixture inheriting the subject's convention (the twenty-third). Every input was
**exactly representable in binary** — `13.0F`, `0.25F`, `100.0F`, `4000.0F` —
so the product had no bits below the sum's ulp, and fusing had nothing to round
away. The domain was fine for the branch cases and empty for the rounding cases.

### Why it is easy to walk into

A sweep of round numbers looks *more* rigorous than a handful of odd ones: 1,215
points, three axes, positive and negative. Coverage of the input space and
coverage of the *distinctions* are different quantities, and only the second one
is what a control measures.

Fused and unfused agree almost everywhere. After the fix — `0.1F`, `13.7F`,
`907.3F`, positions near 4·10³ — the counts were **6 and 5 out of 1,215**. The
difference the whole port turns on shows up in half a percent of a reasonable
domain, so it has to be hunted rather than expected to appear.

### The rule

**Every control asserts that it disagrees.** Not "the port matches the
reference" — that is the test. The control's own assertion is
`check(disagreements > 0, "CONTROL ... must disagree")`, and it is the only thing
that can distinguish a suite that verifies a rule from a suite that verifies an
identity.

And when a control is about **rounding**, the domain needs values that are not
exactly representable. A quick test of the fixture itself: if every literal in it
has a short binary expansion, no rounding control in that suite can ever fire.

Generate the sweep from **one shared function** used by both the comparison and
the controls. Two loops with hand-copied values drift, and the drift is invisible
because both still pass.

## The thirty-third shape: both sides right, disagreeing on the inputs

Cycle 1373's differential for the flight integrator failed on two of ten cases,
one ulp each, and only on the cases whose inputs had long decimals:

```
+68: retail 4115.916015625        port 4115.91650390625
+72: retail 0.008657408878207207  port 0.008657407946884632
```

Every instinct points at the rule. A one-ulp gap on a fused multiply-add is what
an un-fused port looks like, and the fix is one character.

**The rule was correct.** All four candidate models — single-rounded or double
intermediates, crossed with fused or unfused — reproduced retail exactly on both
cases. What differed was the *fixture*: the emulator was seeded with
`repr(float32(13.7))` and the Python oracle computed from the literal `13.7`,
which is a **double**. Two correct implementations of one rule, fed different
numbers.

### Why it is worse than a wrong rule

A wrong rule fails everywhere and gets found. This failed on **two cases out of
ten**, in the direction a known real defect fails in, with a fix that makes the
suite green. Un-fusing the port would have "worked", and the campaign would have
shipped an integrator that disagrees with retail on every frame where the
rounding matters — which cycle 1372 measured at about half a percent of a
reasonable domain, i.e. often enough to drift and rarely enough never to be
caught by inspection.

The near-miss is the point: the repair that a green suite rewards is the wrong
one.

### The rule

**Before changing the rule, prove the two sides received the same inputs.** A
differential compares two computations *and* two input encodings, and only one of
those is what the cycle is about. Concretely, where the sides are a Python oracle
and a seeded emulator:

- every literal the oracle uses must be rounded to the width the seed carries —
  `f32(...)` on entry, once, not sprinkled through the arithmetic;
- print or hash the inputs both sides actually used when a case fails, not just
  the outputs.

And the cheap move that made this a ten-minute cycle instead of a wrong commit:
**build all the candidate rules at once and run them together.** When *every*
candidate reproduces the oracle, the disagreement is not in the rule, and that
conclusion is only available if the candidates were compared side by side rather
than tried one at a time until one passed.

## The thirty-fourth shape: the verdict that averages away a different function

Cycle 1376 measured `0x820936E8` against `atan2` and the first run printed:

```
atan2  cases=18  worst=1061752795 ulp
```

Sixteen of eighteen cases were **exact**. Two were not — the two where both
arguments are tiny. The routine has a guard: when `|a|` and `|b|` are both below
2⁻¹⁶ it returns **zero** rather than an angle, and `atan2(1e-6, 1e-6)` is π/4.

That is not a rounding disagreement. It is **a different function on a
subdomain**, and the subdomain is the common case: an aircraft flying level.
Substituting `std::atan2` unguarded puts a 45-degree error into the orientation
on exactly the frames where nothing looks wrong.

### What made it visible, and what would have hidden it

The comparison reported the **worst** case, in **ulp**, with no tolerance:

- an **average** over 18 cases: 16 zeros and 2 large values still reads as
  "close", and closer still if the two are reported as radians rather than ulp;
- a **tolerance** — `abs(got - want) < 1e-3` — passes sixteen and fails two, and
  two failures out of eighteen invites "an edge case, probably fine";
- **ulp** makes the failure enormous and unignorable, because two floats either
  are the same word or are not, and the distance between π/4 and 0 in float steps
  is a nine-digit number.

The large number is the useful part of the output. A metric that compresses a
categorical difference into a small one has not measured anything.

### The rule

**When comparing a retail routine against a library function, report the worst
case in ulp, never an average and never a tolerance.** Then read the cases that
are not zero *individually* — a subdomain guard, a different branch, a different
special-case convention will all show up as a handful of enormous gaps among a
majority of exact matches, and that pattern is the signature of "same formula,
different function" rather than "same function, different rounding".

The instrument is `tools/audit_flight_math_seams.py`, which prints
`identical` / `within N ulp` per routine and refuses to summarise further.

## The thirty-fifth shape: the over-correction

Cycle 1377 wrote that the aircraft's handling is "a performance curve resampled
by speed, not a static set of numbers". Cycle 1378 corrected it: the resampling
belongs to the sibling class, and it is driven from slot 29, which is a **reset**.

Slot 29 is a reset. The correction was still wrong. The sibling's own step at
slot 15 calls the same lookup on **every frame**, so 1377's original sentence was
right about the class it was actually describing, and 1378 replaced a true
statement with a false one while sounding more careful.

### Why this direction is easy to miss

Every other shape in this file guards against a claim receiving too little
scrutiny. A **correction** receives almost none. It arrives with the authority of
having already caught something, it is written in the voice of someone being
rigorous, and the reader who would have challenged the original assumes the
challenge already happened.

The 1378 correction was itself derived from real evidence -- slot 29 is genuinely
a reset, and 0x8200F270 genuinely leaves it empty. It failed because it answered
"where is the lookup called from?" with the first caller it found and stopped.
An original claim with one supporting site would have been challenged; the same
claim wearing the clothes of a correction was not.

### The rule

**A correction is a new claim and takes the same evidence as any other.** In
particular, when the correction is "X happens in place A, not place B", the
enumeration of *all* the places must be shown -- one call site is not a survey.

The cheap check here was `callers of 0x82283480`: three of them, of which 1378
read one. The tooling for this already exists and takes seconds; the failure was
believing the search was finished because the answer was satisfying.

## The thirty-sixth shape: the wrong answer that names the register

Cycle 1380 captured a mid-function value by stopping the emulator with `steps`
at the instruction that would consume it. Row 2 failed on all eight cases, and
the "retail" column read **0.016666667, 1000.0, 100.377** -- in each case exactly
the *step* that case had been given.

Not a plausible angle. Not eight wrong numbers. Eight copies of one input.

The window had stopped eleven instructions early, and `f1` still held what
`fmr f1,f30` put there. The cause was in the harness rather than the target:
**stubs are keyed on the callee's entry address**, so a stubbed call costs TWO
steps -- the `bl`, then the stub firing at the callee's first instruction and
setting `PC = LR`. Row 1 passed through no calls and was exact from the first
run; row 2 passed through two.

### Why the failure was cheap, and when it would not have been

The disagreement was **categorical**: the captured values were not near the
expected ones, they were identical to a known input. One glance at the column
identified the register and therefore the stopping point.

Had the window stopped one instruction early instead of eleven, `f1` would have
held a partial product -- a plausible-looking number in the right range -- and
the natural reading would have been "the multiply order is wrong", sending the
cycle to re-read arithmetic that was already correct.

### The rule

**When a captured value equals one of the inputs, suspect the capture, not the
arithmetic.** A register that still holds an argument, a step, a limit or a
constant is the signature of stopping in the wrong place, and it is checkable in
one line: compare the result against every input before comparing it against the
expectation.

And for any technique that stops execution by counting: **the count is part of
the claim.** Assert something that changes when it is wrong -- the number of
calls reached, the exit kind, the PC -- so a miscount fails loudly instead of
capturing a different register. Cycle 1373 learned this for `callee_entries`;
this is the same rule for `steps` past a stub.
