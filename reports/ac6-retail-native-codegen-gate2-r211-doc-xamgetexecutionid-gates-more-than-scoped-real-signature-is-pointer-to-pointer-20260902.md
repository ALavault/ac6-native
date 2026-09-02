# AC6 retail NTSC-U/J — documentation only: `XamGetExecutionId`'s real signature and blast radius (r211)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. **No code change this cycle** — documentation only, same class as
r164/r178/r202. `ctest`/pytest unaffected: 196/196 pytest, 10/10 ctest
(last verified at r210, unchanged since no source was touched).

## Escalation: `XamGetExecutionId`'s wrapper gates far more than r206/r209 scoped

r206 first traced the wrapper at `0x821f7668` while investigating
`XamGetExecutionId`; r209 found it also gates `XamUserReadProfileSettings`
(4 real call sites). This cycle traced the same wrapper being called from
two more functions (`Function_821F44B8`/`Function_821F4518`, both real
call sites `0x821f44e0` and `0x821f4540`) that gate
`XamUserCreateStatsEnumerator` (`0x821f450c`, `0x821f456c`), and a third
(`Function_821F4590`, real call site `0x821f45bc`) that gates
`XamUserCreateAchievementEnumerator` (`0x821f460c`). The wrapper's own
contract — `cmplwi r3,0x0; bne <skip-real-call>` on its own check
argument — is confirmed identical across all of these call sites: if the
caller passes a nonzero check value, this wrapper's internal call to
`XamGetExecutionId` gates the real work entirely.

**Total confirmed blast radius of this one deferred item: at least 7 real
call sites** across 3 distinct XAM APIs (`XamUserReadProfileSettings` ×4,
`XamUserCreateStatsEnumerator` ×2, `XamUserCreateAchievementEnumerator`
×1), not counting whatever else routes through the same wrapper that this
sweep has not yet found.

## Correction: the real signature is a pointer-to-pointer, not a struct-fill

r206 described `XamGetExecutionId`'s contract as filling a struct at a
caller-supplied buffer. Re-reading the wrapper's own use of the result
this cycle shows that is imprecise: the wrapper does

```
addi r3,r1,0x50        ; &local_buffer
bl XamGetExecutionId    ; XamGetExecutionId(&local_buffer)
...
lwz r11,0x50(r1)        ; r11 = *local_buffer  (a POINTER, not struct data)
lhz r11,0xc(r11)        ; dereference AGAIN: read u16 at (*local_buffer)+0xC
```

The real signature is `DWORD XamGetExecutionId(PXAM_EXECUTION_INFO
*ppInfo)` — an out **pointer-to-pointer**: the call writes the address of
a real `XAM_EXECUTION_INFO`-shaped structure (most likely one already
embedded in the loaded title image, not a structure this stub would need
to allocate) into the caller's local slot, and the caller dereferences
that returned pointer to read a field at offset `+0xC` (a 16-bit value,
compared against the low 16 bits of the wrapper's own check argument).

This is a real, standardized Microsoft XDK structure by name, but this
project has not independently confirmed its exact field layout at
offset `+0xC` from this XEX's own reads (only this one dereference has
been traced so far), and fabricating a plausible-looking value there
without that confirmation would be exactly the kind of guess this
project's evidence discipline forbids — the same caution r206 already
applied when first deferring this import.

## Also checked, no fix needed: `XamUserAreUsersFriends`

Traced its one real (non-tail-jump) call site (`0x82205518`, inside
`Function_822053F8`): the caller pre-fills its own output buffer with a
default value **before** the call and only reads that buffer back
afterward — it never checks `XamUserAreUsersFriends`'s own return value
(`r3`) at all. Since the generic offline no-op does not touch that
buffer, the caller's own pre-filled default flows through unchanged
regardless of what this stub returns. This import's current behavior is
already observably correct given no real friends-list data exists to
report — confirmed adequate, not merely deferred, so no fix was made.

## Also checked, no safe fix: `XamUserGetXUID`/`XamUserGetSigninInfo` shared wrapper pattern

Both route through a different but structurally similar wrapper pattern
(`0x821f4618` for `XamUserGetXUID`, `0x821f5190` for
`XamUserGetSigninInfo`): each masks specific bits out of the **return
value itself** (not a struct field) and compares the result against a
fixed pattern (`0x70000`) to decide between two output codes. Whether
this is decoding an HRESULT-style facility code or something else
specific to this XDK version was not confirmed this cycle, and forcing a
value through this comparison without understanding it would again be a
guess. Left as the generic offline no-op.

## Next

1. If `XamGetExecutionId` is tackled, the real prerequisite is finding
   (or synthesizing, if this project's own loader already tracks XEX
   header fields like TitleId elsewhere) a real
   `XAM_EXECUTION_INFO`-shaped structure in guest memory and confirming
   what field sits at offset `+0xC` from at least one more independent
   read — not just the single dereference traced here and in r206.
2. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r211) for other bounded candidates in the meantime.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
