# AC6 retail NTSC-U/J — real fixes: `VdQueryVideoFlags`, `VdGetCurrentDisplayGamma`, `VdGetCurrentDisplayInformation` (r170)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with matching tests. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 9/9) and the full retail-native pytest suite
(154/154, up from 151/151).

Environment note: this cycle's Ghidra 12.1.2 install and its `GhidraXenon`/
`XEXLoaderWV` processor extensions, and the pinned `XenonRecomp` checkout used
by the native build, were absent from the session sandbox and were restored
before any analysis: `XenonRecomp` from its lock-pinned commit
(`ddd128bcca99fe8bfbb99bea583c972351fa6ace`,
`config/xbox360-toolchain.lock.json`), Ghidra 12.1.2 from its official public
release, and the two processor/loader extensions from this repository's own
tracked `ghidra-user/.ghidra/.ghidra_12.1.2_PUBLIC/Extensions/` copies (the
exact bytes this project's evidence has always been built against), invoked
with `JAVA_TOOL_OPTIONS="-Duser.home=$PWD/ghidra-user"` per
`analysis/microexec/README.md`'s own documented pattern. No unpinned or
guessed component was installed.

## Why these three, and in what order

r168/r169 named `VdQueryVideoFlags`, `VdGetCurrentDisplayGamma` and
`VdGetCurrentDisplayInformation` as the same plausible struct-filling family
as `VdQueryVideoMode`, unchecked. Reading each one's real call site(s) shows
that guess was only sometimes right:

- `VdQueryVideoFlags` is **not** struct-filling — it is a simple return-value
  call, the same category as r148-r167.
- `VdGetCurrentDisplayGamma` and `VdGetCurrentDisplayInformation` **are**
  pointer-output calls.

## `VdQueryVideoFlags`

Real contract: `DWORD VdQueryVideoFlags(VOID)`. One real call site,
`0x821f31cc`:

```
821f31cc  bl 0x823d05fc        ; VdQueryVideoFlags() -> r3
821f31d0  rlwinm. r11,r3,0x0,0x1f,0x1f   ; r11 = bit 0 (LSB) of r3
821f31d4  bne 0x821f31ec       ; branch if that bit is set
```

The generic offline-import fallback previously returned `kOfflineStatus`
(`0xC00000BB`) here. `0xBB` is odd, so bit 0 of that constant is set —
this forced the `bne` branch every time, not because that branch is the
correct hardware behavior, but because an NTSTATUS constant was reused as if
it were a flags bitmask. The real contract has no status/failure shape at
all; treating it as one is exactly the kind of wrong-shaped-return-value bug
r148-r167 fixed. No evidence at this one call site favors either branch
outcome, so the fix is the neutral flags value — no flags set (`0u`) — not a
value picked to force a particular downstream comparison (r53's precedent,
reaffirmed by r169).

## `VdGetCurrentDisplayGamma`

Real contract: `VOID VdGetCurrentDisplayGamma(DWORD* type, FLOAT* value)`.
One real call site, `0x821eb440`-`0x821eb454`:

```
821eb440  addi r4,r1,0x50      ; r4 = &float_out
821eb448  addi r3,r1,0x54      ; r3 = &int_out
821eb454  bl 0x823d053c        ; VdGetCurrentDisplayGamma(r3, r4)
821eb45c  lwz r14,0x54(r1)     ; read back int_out
821eb464  lfs f2,0x50(r1)      ; read back float_out
```

Both outputs are then compared against a cached table entry (indexed by the
*caller's own* argument, computed before this call) to decide whether to
rebuild a gamma-correction table; that cache starts uninitialized, so the
rebuild path runs on the first call regardless of the exact values supplied
here — no crash or hang risk either way, and no reachable code lets this
XEX's own disassembly pin the real type/gamma constants. `type=0`,
`gamma=2.2` (`0x400ccccd`) are ordinary production defaults — a generic
curve index and a standard display gamma — not read from this XEX's own
bytes and not asserted to match any external source.

## `VdGetCurrentDisplayInformation`

Real contract: struct-fill through `r3`. All **three** of this XEX's real
call sites read from it, and one cross-validates directly against r169:

**`0x821f0764`** (struct at `[r1+0x170]`):

```
821f0764  bl 0x823d051c        ; VdGetCurrentDisplayInformation(&struct)
821f0768  lhz r11,0x1b8(r1)    ; struct+0x48, u16
821f076c  lhz r10,0x1ba(r1)    ; struct+0x4a, u16
821f0770  lhz r9,0x1c6(r1)     ; struct+0x56, u16
821f0774  stw r11,0x5414(r31)  ; -> same output field VdQueryVideoMode fills
821f0778  stw r10,0x5418(r31)  ; -> same output field VdQueryVideoMode fills
821f077c  stw r9,0x541c(r31)   ; -> same output field VdQueryVideoMode fills
```

This is independent cross-validation of r169: the same three downstream
fields (`0x5414`/`0x5418`/`0x541c`) that r169 confirmed as width/height/
actual-width are fed here from `VdGetCurrentDisplayInformation`'s own
struct+0x48/+0x4a/+0x56 — and here width (`+0x48`) and actual-width
(`+0x56`) come from two **different** struct offsets, confirming they are
real, distinct fields rather than an always-equal duplicate (r169 saw them
equal only because its own call site had a single real width value to
duplicate).

**`0x821ea4d8`** (struct at `[r1+0x60]`) and **`0x821ea2a4`** (struct at
`[r1+0x1a0]`) both additionally read a byte at struct+0x05
(`lbz r11,0x65(r1)` / `lbz r11,0x1a5(r1)`) into a
`subi/cntlzw/rlwinm` normalize idiom (`!= 1`, boolean-shaped), independently
confirming a real field there at both sites — but its result feeds an
unrelated flags bitfield deep in unrelated caller logic whose own semantics
this cycle did not chase down, so no evidence pins which raw byte value is
correct.

**Implemented**: `+0x48`/`+0x56` = `1280` (width/actual_width), `+0x4a` =
`720` (height) — u16, this project's own pre-existing resolution assumption
(r169), not invented; no evidence distinguishes width from actual_width for
this project's single fixed target, so the same value is used for both.
**Not implemented**: struct+0x05 — named as a real, confirmed field (not a
gap left unnoticed), deferred because no call site pins its correct value,
following r168's own precedent of naming rather than guessing.

## Gates

`ctest` 9/9 (native profile). Full retail-native pytest suite: 154/154
(151/151 before this cycle, +3 new tests). `git status` unchanged apart from
the intended change set and pre-existing, unrelated dirty state in
`reconstruction/ace-combat-6/` and the uncommitted `ace-combat-6-demo`
archival (out of scope for this cycle, untouched).

## Next

1. `VdGetCurrentDisplayInformation`'s struct+0x05 field: identify what
   caller-side flag it feeds and derive its correct value, or confirm no
   further evidence is reachable statically.
2. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
3. No further Vd import in this family is currently named as unchecked; the
   next Gate 2 frontier needs a fresh offline-import sweep to identify one.
