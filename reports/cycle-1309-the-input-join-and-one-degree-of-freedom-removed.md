# Cycle 1309 — the input join, and one degree of freedom removed

## Qualification

- Ghidra project `ghidra-projects/ace-combat-6`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** Xenia's source and its unit tests read as
  documentation. No game code ran.
- No product C++ changed.

## The instance, and the whole join

Both callers of the `DriverContext` constructor build the same address:

```
lis r11,-0x7d6f ; subi r11,r11,0x2200   ->  0x8290DE00
addi r3,r31,0x24                        ->  the context at 0x8290DE24
```

So the four controllers are at **`0x8290DE3C`, `0x8290DEC4`, `0x8290DF4C`,
`0x8290DFD4`**, and controller 0's split LY pair is `0x8290DE64` / `0x8290DE66`.

`0x82337E88` is the lazy initialiser (`[svc+0x00] = 1` after construction) and
`0x823D39F0` the static one, which also registers an atexit through
`0x82380040`.

**The global is materialised at exactly nine sites**, and seven of them are in
one 200-byte cluster:

| thunk | shape | tail-calls |
|---|---|---|
| `0x82337E18` | `(svc, 7)` | `0x82343928` |
| `0x82337E28` | `(svc, a, b)` | `0x82343838` |
| `0x82337E40` | `(svc, a, b)` | `0x82343888` |
| `0x82337E58` | `(svc, a, b)` | `0x823438E0` |
| `0x82337E70` | `(svc, a, b)` | `0x82343970` |
| `0x82337ED0` | init | `0x82337E88` then `0x823437F0(svc, 1)` |

`0x8290DE24` and `0x8290DE3C` are materialised **zero** times, so nothing
addresses the context or a controller directly — every consumer goes through
these six entry points. That is what makes the join finite.

## Eleven call sites, four functions

| entry | callers |
|---|---|
| `0x82337E18` | `0x821CA908` |
| `0x82337E28` | `0x821CAA50` ×2, `0x821CBB58` |
| `0x82337E40` | `0x821CAA50` ×3 |
| `0x82337E58` | `0x821D5EF8` |
| `0x82337E70` | `0x821CAA50`, `0x821CBB58` |
| `0x82337ED0` | `0x821D5EF8` |

**`0x821CAA50` carries seven of the eleven** and is 744 instructions — the input
consumer. `0x821CA908` is 82 instructions, `0x821CBB58` is 46 and is slot `+0x04`
of `CDirtyDiscErrorCall`, which is a disc-error prompt reading the pad and not
gameplay.

`0x821D5EF8` is **the boot resource mount already in the ladder's hop table**, and
it both initialises the input service and takes one reading — the same function
the mission-load path goes through.

So the chain is closed end to end, statically, with no bridge:

```
XamInputGetState        0x823D737C
  wrapper               0x823911C0
  readers               0x8234D3F0 / 0x8234D478
  poll entry            0x8234D510   DriverController vtable +0x10
  axis stage            0x8234D110   table 0x8201250C
  button stage          0x8234D378
  instance              0x8290DE3C…  four, inside 0x8290DE24
  API                   0x82337E18…E70, six entry points
  consumer              0x821CAA50   744 instructions, 7 of 11 sites
```

**This supersedes the rejected claims.** `CURRENT_PLAN.md` named
`0x821CE088`, `0x82215418` and `0x82215210` as input roles and then rejected them
for the canonical project without naming a replacement. The replacement is
`0x821CAA50`, and it is reached by following pointers rather than by matching a
displacement.

## One degree of freedom removed on `vpermwi128`

It was put to me that Xenia carries **conformance tests** for `vpermwi128` at
immediates `0x1B`, `0xE4`, `0x00` and `0xFF`, and that this settles the lane
order without an oracle.

**The tests are not there.** `grep -rln vpermwi` over
`src/xenia/cpu/testing/` returns nothing; there is no per-instruction test for
it.

**But the adjacent test does remove the one thing cycle 1305 could not check.**
`swizzle_test.cc` asserts that `MakeSwizzleMask(0,1,2,3)` applied to `(0,1,2,3)`
yields `(0,1,2,3)` and `MakeSwizzleMask(3,2,1,0)` yields `(3,2,1,0)` — so
argument *i* selects the source lane for result lane *i*, in order. Cycle 1305
flagged exactly this as unverified ("or is `MakeSwizzleMask`'s argument order
`(w,z,y,x)`?").

With that pinned, `InstrEmit_vpermwi128`'s
`MakeSwizzleMask(uimm>>6, uimm>>4, uimm>>2, uimm>>0)` is unambiguous:
**the high bit-pair selects element 0**, which is what the harness override
implements, and Xenia's own comment — `(VD.x) = (VB.uimm[6-7])` in PowerPC
numbering — contradicts the code it sits above.

That is a real upgrade in status: from *two documentations disagree* to *one
source is internally consistent and has a test pinning the helper it relies on,
the other is a disassembler module with three separately measured defects.* It is
still one source and it is still not a measurement, so the suite entry stays
`readings disagree` with the module's behaviour pinned — but high-first is now
the better-supported default, which is what the harness already runs.

**And the discriminating vectors are the ones the image does not contain.**
`0x00` gives `(x,x,x,x)` and `0xFF` gives `(w,w,w,w)` under *both* readings, so
neither discriminates. `0x1B` and `0xE4` do, and cycle 1306's census found
neither in 545 sites. The proposed conformance run cannot be performed on a real
site.

**Nothing needs regenerating.** It was suggested that every prior capsule whose
instruction set contains `vpermwi128` be invalidated. The 138 `*Bin` capsules
contain no vector instruction at all (cycle 1294), and the `analysis/microexec/vmx128`
capsules are *about* the instruction and record which reading produced them. The
scope of the invalidation is empty.

**And the SLEIGH patch is not needed for correctness.** The address-level
override reaches every site and is validated at all six immediates in the image;
rebuilding the language would be tidier and would invalidate the 138-case
calibration that currently pins the harness. That trade is not worth taking to
change a default the harness can already set per spec.

None of this unblocks `0x822A1E80`: cycle 1306 measured that high-first, applied
at all six sites with the register bridge on, still does not return the identity
at zero angles. The order suggested — fix the permute, then resume — has already
been run in the other order, and the answer was no.

## Not established

- What each of the six API entry points returns. Their targets
  `0x823438xx`–`0x82343970` are unread.
- What `0x821CAA50` does with the values. 744 instructions, unread.
- Which reading of `vpermwi128` the hardware uses. Better supported is not
  measured.

## Gates

```
mission01_final_gate=audit-valid JF=pass open=none
ctest: 100% tests passed, 0 failed out of 27
contract_addresses=pass cited=103 supported=103 unsupported=0
tools/tests: Ran 72 tests, OK
```

## Next

Read the six API targets. They are small — the cluster spans `0x823437F0` to
`0x82343970`, under 400 bytes — and they sit exactly between the derived field
layout and the 744-instruction consumer. Naming what each returns turns the
input path from a chain of addresses into a behaviour a contract entry can cite.
