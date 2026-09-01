# AC6 retail NTSC-U/J — `VdQueryVideoMode` is a real, unimplemented struct-filling gap, not a contract-shape bug like r162-r167; deferred pending real offset derivation (r168)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, `-readOnly -noanalysis`. XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used for the retail binary's own disassembly (real, static
evidence). One external reference consulted: `has207/xenia-edge`'s own
public source (`src/xenia/kernel/xboxkrnl/xboxkrnl_video.cc`,
`VdQueryVideoMode`), used only as cross-match context for what the real
XDK contract *does*, not as a source of this XEX's own exact struct
byte-layout (explicitly not assumed to match without independent
verification — see below). No native code changed this cycle.

## Why this check was worth running

Continuing the offline-import audit after r167, `VdQueryVideoMode` stood
out for its live call frequency (2 hits in a bounded probe, both before
`DATA.TBL` opens) and for a different reason than every fix so far
(r148-r167): those were all a wrong-shaped *return value*.
`VdQueryVideoMode`'s real contract is a struct-filling call through a
pointer argument (`r3 = &struct`), and the generic offline-import fallback
writes nothing through that pointer at all — a materially different,
larger kind of gap.

## What was found

Both of this XEX's own real static call sites (`sub_821F0DB0` at
`0x821F0E78`, `sub_821F2BC8` at `0x821F2C00`) are large, complex functions
that read multiple fields directly out of the struct after the call — not
merely check a return status:

```
821f0e74  addi r3,r1,0x50
821f0e78  bl 0x823d05ec        ; VdQueryVideoMode(&struct at [r1+80])
...
821f0e80  lfs f13,0x64(r1)     ; float read at struct+0x14 (20 decimal)
821f0e84  lwz r10,0x54(r1)     ; u32 read at struct+0x04
821f0e90  lwz r11,0x50(r1)     ; u32 read at struct+0x00
```

```
821f2be4  addi r3,r1,0x70
...
821f2c00  bl 0x823d05ec        ; VdQueryVideoMode(&struct at [r1+112])
821f2c04  lwz r29,0x78(r1)     ; u32 read at struct+0x08
```

These reads feed real arithmetic (a float addition combining a queried
value with a constant at `821f0e94`; bit-scan/comparison logic at
`821f2c0c` onward) in both callers — this is not a case like
`RtlTryEnterCriticalSection` where a wrong value happens to evaluate
correctly by coincidence. Left unimplemented, both callers currently
compute from whatever uninitialized stack bytes happen to occupy that
buffer.

Xenia Edge's own public source (`xboxkrnl_video.cc:204-219`) confirms the
real XDK contract: `VdQueryVideoMode` fills an `X_VIDEO_MODE` struct with
fields including `display_width`, `display_height`, `is_interlaced`,
`is_widescreen`, `is_hi_def`, `refresh_rate`, `video_standard`,
`pixel_rate`, `widescreen_flag` — consistent with the offset pattern
observed here (a `u32` pair early in the struct, plausibly width/height;
a `float` a little further in, plausibly a computed aspect or scale
value).

## Why this is not fixed this cycle

Xenia's own struct definition (`X_VIDEO_MODE`) was not locatable in the
portion of its public source fetched this cycle (its exact header could
not be found among the likely candidate paths checked), and even if it
had been, **this project's own discipline does not treat an independent
reimplementation's struct layout as authoritative for this retail binary's
own compiled offsets** — Xenia's `X_VIDEO_MODE` is Xenia's own
recreation of a Microsoft kernel structure, not a citation of the real
XDK header, and assuming byte-for-byte agreement without checking this
XEX's own disassembly against every field would repeat exactly the kind
of unverified assumption this project's evidence discipline refuses.
Implementing this stub correctly requires deriving the real field
offsets/types from this XEX's own call sites and any other reachable
callers (there may be more not yet found), which is a bounded but
genuinely separate task from this cycle's contract-shape sweep — not
rushed here to avoid shipping a plausible-looking but unverified struct
layout.

## Decision

Named as a real, identified gap — larger in scope than the r148-r167
family, and worth a dedicated future cycle: derive `VdQueryVideoMode`'s
real output struct layout for this XEX from its own disassembly (both
call sites, plus a fresh xref scan in case indirect calls exist, matching
r156's own caution about literal-`bl` scans missing computed calls), then
implement a struct fill using values this project's own native Vd layer
already tracks (display resolution, refresh rate) rather than either
leaving it unimplemented or guessing offsets.

## Gates

No native code changed, no build touched. `ctest`'s last-known state
(9/9, r167) stands unaffected. `git status` unchanged (only pre-existing,
unrelated dirty state).

## Next

1. Derive `VdQueryVideoMode`'s real struct layout from this XEX's own
   disassembly, cross-matched against (not assumed equal to) Xenia's
   own public `X_VIDEO_MODE` field list, before implementing a fill.
2. `VdQueryVideoFlags`/`VdGetCurrentDisplayGamma`/`VdGetCurrentDisplayInformation`
   (all still offline-import, all plausibly struct-filling calls in the
   same family) are not yet checked for the same category of gap.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain
   fully traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
