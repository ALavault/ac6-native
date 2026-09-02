# AC6 retail NTSC-U/J — real fix: `XeCryptSha` computes a real SHA-1 digest (r184)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10, confirms the new OpenSSL EVP code
compiles and links against the already-linked `OpenSSL::Crypto`) and the
full retail-native pytest suite (168/168, up from 167/167).

## Why this check was worth running

Continuing the offline-import sweep. `XeCryptSha` stood out as a real
cryptographic primitive with two real call sites, unlike the
contract-shape/struct-fill gaps fixed so far — a distinct category worth
checking directly.

## What was found

Real signature: `VOID XeCryptSha(const BYTE* pbInput1, DWORD cbInput1,
const BYTE* pbInput2, DWORD cbInput2, const BYTE* pbInput3, DWORD
cbInput3, BYTE* pbDigest, DWORD cbDigestSize)`. This XEX's own real call
site (`0x82390f04`) confirms it: all eight integer argument registers
(`r3`..`r10`) are populated, and `r10` (`cbDigestSize`) is `0x14` — the
exact SHA-1 digest length, not a status code:

```
82390ef0  stw r11,0x50(r1)
82390ef4  li r6,0x0
82390ef8  li r5,0x0
82390efc  li r4,0x58
82390f00  addi r3,r31,0x22c
82390f04  bl 0x823d0acc        ; XeCryptSha(pbInput1, 0x58, NULL, 0, NULL, 0, &digest, 0x14)
82390f08  addi r5,r1,0x50
82390f0c  addi r4,r31,0x4
82390f10  addi r3,r1,0x60
82390f14  bl 0x823d0abc        ; a comparison using the computed digest
```

The generic offline-import fallback previously wrote nothing through
`pbDigest` at all. The computed digest immediately feeds a comparison
(`0x823d0abc`) whose result gates real control flow — a garbage or absent
digest would fail any real reference-hash comparison downstream.

## Fix

Computes the **actual** SHA-1 over whatever real guest bytes are present
at each of the (up to three) supplied input buffers, using OpenSSL's EVP
digest interface — the same pattern this file's `native_xex.cpp` already
uses for AES-CBC XEX decryption (`OpenSSL::Crypto` is already a linked
dependency). This is the one case in this sweep where "the real value" has
no ambiguity to resolve at all: the algorithm computes it exactly, rather
than requiring a choice between plausible constants.

## Gates

`ctest` 10/10 (native profile, including a successful compile/link of the
new OpenSSL EVP code). Full retail-native pytest suite: 168/168 (167/167
before this cycle, +1 new test). `git status` unchanged apart from the
intended change set and pre-existing, unrelated dirty state.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176/r177/r179/r181/r182/r183/r184).
2. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
