# Cycle 1305 — the chain is complete, and my own fix is in dispute

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`, asserted by the harness.
- **No oracle pass was spent.** Xenia's source read as documentation.
- No product C++ changed.

## The override's own immediates

Cycle 1304 wrote *"one immediate pair is not a test of an immediate"* about
`vrlimi128`. The same applies to my replacement for `vpermwi128`, checked at
`0xAC` and nowhere else. The six sites carry six different immediates —
`0xEC`, `0xCC`, `0xAC`, `0x2C`, `0x8C`, `0x6C` — and two of them produce the row
this thread has narrowed to.

All five untested ones added. **Correct at every immediate; suite is 23/23.**

## The chain, complete

| step | instruction | before | after | checked |
|---:|---|---|---|---|
| 158 | `820a9bc4 vrlimi128 vr13,vr12,0x3,0x2` | `vr13=(0,0,0,0)`, `vr12=(1,0,0,0)` | `vr13=(0,0,1,0)` | mask `0x3` keeps `vD₀,vD₁`, rotate 2 supplies `vB₀,vB₁` ✓ |
| 162 | `820a9bd4 vor v12,v13,v13` | — | `vr12=(0,0,1,0)` | a move ✓ |
| 168 | `820a9bec vpermwi128 vr13,vr12,0xac` | `vr12=(0,0,1,0)` | `vr13=(1,1,0,0)` | my override, selectors `2,2,3,0` ✓ |
| 181–183 | dots and merges | `vr13=(1,1,0,0)` | row 0 = `(1,1,0,0)` | the basis columns make the row equal `vr13` ✓ |

Every arrow is verified arithmetically. The corrupted row follows deterministically
from `vr13 = (1,1,0,0)`, and that follows from `vr12 = (0,0,1,0)` under my
override's reading of `0xAC`.

## And that reading is now in dispute

For row 0 to be `(1,0,0,0)`, `vr13` after the permute must be `(1,0,0,0)`.

Under my reading, selectors `(2,2,3,0)` put lane 2 of the source into **both**
output elements 0 and 1 — so elements 0 and 1 are always equal, and `(1,0,0,0)`
is **unreachable from any source**. Under the module's reading, selectors
`(0,3,2,2)` give `{vB₀,vB₃,vB₂,vB₂}`, which `vB = (1,x,0,0)` satisfies.

So on this routine the module's reading is the reachable one and mine is not.

**The documentation is genuinely ambiguous, and cycle 1297 read only half of
it.** Xenia's code is
`MakeSwizzleMask(uimm >> 6, uimm >> 4, uimm >> 2, uimm >> 0)` and
`MakeSwizzleMask(x,y,z,w)` places `x` in bits 0–1 — so the **high** pair selects
element 0, which is what I implemented. Xenia's **comment on the same function**
says `(VD.x) = (VB.uimm[6-7])`, and in PowerPC bit numbering bits 6–7 of an
8-bit field are the **low** pair — which is what the module does.

Cycle 1297 quoted the code, called the module "reversed", and pinned a confirmed
defect. It should have noticed that the comment three lines above said the
opposite.

**Neither reading is vindicated here.** Cycle 1302 measured the bridged run with
the override off: at zero angles it produces all zeros, not the identity. So the
module's reading does not rescue the routine either, and this function does not
adjudicate between them.

The suite entry is downgraded accordingly — `readings disagree`, pinning the
module's measured behaviour so a change is noticed, and no longer asserting the
module is wrong. Two of the three pinned entries remain confirmed defects; this
one is a disagreement.

## Not established

- Which reading of `vpermwi128` is the hardware's. It needs a case where the two
  give different answers **and** an independent check on which is right — this
  routine gives neither.
- Whether `vr12 = (0,0,1,0)` entering the permute is itself correct. Under the
  module's reading a unit row is reachable from `(1,x,0,0)`, not from
  `(0,0,1,0)`, so under **either** reading something upstream may still be wrong.
- What `0x822A1E80` computes.

## A note on where this thread stands

Twelve cycles on one instrument. The gameplay work in the plan has not started,
and the honest reason is that the instrument it depends on was broken in four
distinct ways, three of them found only by using it. That is a defensible cost
if the instrument is the backbone — and a bad bet if it is not. Worth stating
plainly rather than letting the cycle count drift.

## Gates

```
mission01_final_gate=audit-valid JF=pass open=none
ctest: 100% tests passed, 0 failed out of 27
contract_addresses=pass cited=103 supported=103 unsupported=0
tools/tests: Ran 72 tests, OK
vmx128_behaviours=pass (23/23, 2 confirmed defects, 1 disagreement)
```

## Next

Adjudicate `vpermwi128` outside this routine. `vpkd3d128` and the other
immediate-permute forms in the image will not help; what would is a site where
the surrounding code constrains the answer — a permute whose result is
immediately stored to a known layout, or one whose input is a known constant
from the `0x8204F7E0` table. Failing that, the honest move is to run the
composite under **both** readings and report both, rather than pick one.
