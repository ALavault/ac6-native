# Cycle 1260 — the mode-1 id, and my own hypothesis refuted

## Qualification

`default.xex` SHA-256 `acc302c1…11bcde`; flat image `analysis-input/ACE6_X360.exe`.
Delegated investigation, canonical project for cross-references; **the
load-bearing instructions were re-read here** in `ghidra-projects-xenon/ac6-xenon`
before publication. **No oracle pass was spent.**

## Established — the mode-1 id is a caller-supplied base plus the entry ordinal

`mode` is the fourth argument of the mount loop `0x82340870`, masked to bit 0
**once, before the loop**, so the arm is fixed for the whole call:

```
82340888  or     r23,r5,r5        ; r23 = arg3, the id base
8234088c  or     r26,r6,r6        ; r26 = arg4, the mode
823408e4  cmpwi  cr6,r28,0x0      ; r28 = entry count
823408e8  ble    cr6,0x8234095c
823408ec  rlwinm r26,r26,0x0,0x1f,0x1f   ; mode & 1, once
823408f0  cmplwi cr6,r26,0x0             ; <- the loop top
823408f4  beq    cr6,0x82340904          ; (mode&1)==0 -> the GIDX arm
```

The `mode & 1 == 1` arm is three instructions:

```
823408f8  or     r5,r23,r23
823408fc  addi   r23,r23,0x1
82340900  b      0x82340914
```

against the GIDX arm for contrast:

```
82340904  or     r3,r31,r31
82340908  bl     0x8234b150       ; the GIDX id
8234090c  or.    r5,r3,r3
82340910  blt    0x82340968       ; negative -> return -3
```

Both join at `82340914`, the `< 0x10000000` bias test, and reach
`8234093c bl 0x8234bec8` with `r5` as the registry key.

**The base is caller-supplied.** `r23` is loaded once in the prologue from the
mount loop's third argument, and the only entry into `0x82340870` is the
tail-branch thunk `0x82335F18`, which shifts the arguments and supplies the
archive-bias record:

```
82335f20  lis   r10,-0x7d73
82335f28  or    r5,r4,r4          ; thunk arg1 -> the id base
82335f2c  subi  r3,r10,0x6900     ; 0x828C9700, bias at +0x08
82335f34  b     0x82340870
```

So **`id(k) = base + k`**, then `+ [0x828C9708]` when the result is below
`0x10000000`. The index is the entry's ordinal **within this one pack**: `r23`
starts from the argument on every call and is incremented once per entry, in
lockstep with the iteration counter. It is not a process-global counter, and the
mode-0 arm never touches it.

**Arm counts**, taken twice independently — a raw `bl`-opcode scan of the flat
image and Ghidra's reference manager — agreeing on 53 thunk call sites:
**16 mode-1**, **36 mode-0**, and **one whose mode is runtime**
(`821D16C8`, the async NTXR-mount task; of its 19 submitting call sites 16 pass
1 and 3 pass 0).

## The hypothesis I asked to be tested, and what happened to it

Cycle 1256 measured two ids carrying all the duplication in the corpus —
`0x08000000` (115 entries) and `0x0F000000` (28) — and proposed they were
**bases awaiting an ordinal**. That was written as a hypothesis to test, and the
test came back split:

- **`0x0F000000`: partly supported, and not in the shape proposed.** It exists
  on the mode-1 path, but as the seed and reset of the id allocator
  `0x821AEB08` (`821aeb40 lis r11,3840`, `821aeb60 lis r3,3840`), reached from
  two mount sites. The allocator returns pre-increment plus one, so its emitted
  range is `0x0F000001`…`0x0F00F000` — ids for **dynamically created textures**,
  not a per-pack constant.
- **`0x08000000`: refuted.** An exhaustive scan of every `addis rX, 0, imm` in
  the image finds exactly seven `lis rX, 0x0800`, and none is on a mount path:
  three are GPU register-bit ORs, three are 128 MB memory-size caps, one is a
  CRT float helper. The only two data words equal to `0x08000000` above
  `0x82390000` are a 16.16 constant table and a row of a shader-state table
  whose last word steps `0x00000000`…`0x11000000`. **No path produces
  `0x08000000 + ordinal`.**

The static bases that do reach the mode-1 arm were read individually rather
than inferred: `0x9201`–`0x9207`, `0xA012`, `0xA013`, `0x03000001`–`0x03000009`,
`0x03050000`, `0x0F100000`, `0x0EF00000`, plus two rolling allocators seeded
`0x0E000000` and `0x0F000000`.

## Not established, stated plainly

- **No mount site was tied to the 346-wrapper extraction root.** Nothing here
  shows which of the 53 sites mounts those containers, so whether the 115
  entries carrying `0x08000000` are mounted mode-1 (their file ids discarded)
  or mode-0 (their file ids used) is unknown. That needs a trace or an
  archive-to-call-site map that was not built.
- **The consecutive low-id blocks (`0x1049`, `0x104a`, …) are unexplained.** No
  mode-1 base found lands near them, so they are *probably* file-authored GIDX
  values on the mode-0 arm — and "probably" is the honest word, because no
  control rules out a call site whose base comes from a record that was not
  traced.
- **Two mode-1 bases remain record-sourced and untraced** (`821D1AD0` reads one
  from a table; `8228B598` gets one through a virtual getter).
- **The collision consequence is unmeasured.** With a first-wins insert and 115
  identical keys, 114 registrations would be refused — but that is arithmetic
  over a file-side measurement, not a reading of the arm that produces it.

## Corrections

- **Cycle 1256's working hypothesis was mine, and half of it was wrong.** I
  wrote that `0x08000000` and `0x0F000000` are "plausibly bases awaiting an
  entry ordinal". One is not a base at all and the other is an allocator seed
  for runtime-created textures. The consecutive-block observation that motivated
  it — `0x1049, 0x104a, …` — remains unexplained by any base found, so the
  pattern that suggested the rule is still unaccounted for. **A shape that looks
  like `base + index` is not evidence of `base + index`.**

## Instrument notes

- The delegated work built a flat-image disassembler (llvm-objdump over an ELF
  wrapper) because `Ac6XenonDisasm`'s 300-instruction cap makes multi-function
  sweeps slow. It was controlled against the canonical Ghidra project on six
  independent cross-reference counts and every address matched — which is the
  right way round: a new instrument earns its use by agreeing with the old one
  on something both can measure.
- **A linear-scan constant propagator written for the same task produced one
  wrong answer**, reporting base `0x9207` at `820FBB08` by OR-ing across a
  branch where the true value is `0x9205` or `0x9206`. Every base above was
  afterwards read by hand out of the disassembly. A propagator that does not
  model control flow is a heuristic, and its output is a list of candidates.
