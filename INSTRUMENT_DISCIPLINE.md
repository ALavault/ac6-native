# Instrument discipline — the false negative, and how to catch it

`CLAUDE.md` says *measure the instrument before trusting it*. This is what that
means in practice, written from failures rather than from first principles —
eight in the session that started it, and eight more in the session that doubled
it.

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
| you scanned for a function address **as data** and found exactly one hit | *the `.pdata` row read as a dispatch slot* — the exception table spans 0x82079E00..0x82089FB0 and its `BeginAddress` fields look identical to a table slot; exclude that range, then say which side of it the address falls on |
| two functions sit **next to each other** and seem related | *the refuted link* — image order is not evidence; the checkable forms are a shared member offset, an exclusive call site, a common caller, and none is visible from inside either function |
| your offset **worked on every entry** of the file you derived it from | *an instrument calibrated on one specimen* — regular structures make wrong offsets self-consistent; find the container's own declared count or length and check it across files |
| your listing **ended on an ordinary instruction** and you called it the function | *the instrument sampled a third of it* — a tool that can truncate must say so; check whether two hex arguments mean a range or two starts |
| every offset in your sentence was read, and the sentence is still wrong | *the collision in the prose* — one word ("unit", "the object") naming two hierarchies; name the class, not the role |

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

- the loop's `r31`, from factory slot `+0x14`, is a **`galib::CGaObj`** — it gets
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
