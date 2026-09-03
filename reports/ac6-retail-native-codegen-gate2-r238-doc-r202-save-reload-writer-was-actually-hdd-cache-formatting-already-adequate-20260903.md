# AC6 retail NTSC-U/J — documentation only: r202's "save-write path" is actually HDD disc-cache formatting, and it is already adequate (r238)

Date: 2026-09-03.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
SHA-256 `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No code change this cycle.** `ctest` 10/10, pytest 211/211
(unaffected — no source touched).

## What was found: this is disc-to-HDD cache formatting, not save-game writing

r202 characterized `Function_82392878`/the function at `0x82392978`
(reached via `NtOpenFile`→`NtDeviceIoControlFile`→a `NtWriteFile` loop) as
"unmistakably a real save-file writer" based on the call *shape*
(chunked, offset-advancing, device-geometry-aware writes). r237 built on
that characterization, treating `XamTaskSchedule`'s real callback routine
as part of the same "save/reload" frontier. **Both were working from the
call shape alone — this cycle decoded the actual device-path strings
those calls open, and the picture is different.**

`Function_82392878` and `Function_82392978` open (via `NtCreateFile`) the
literal paths `\Device\Harddisk0\Partition1`,
`\Device\Harddisk0\WindowsPartition`, and `\Device\Harddisk0\Cache%u\`
(`scripts/DumpBytes.java` on `0x8201aab8`, `0x8201aa94`, `0x8201aa60`,
`0x8201aa44` — narrow ASCII, not wide strings, confirmed byte-for-byte).
These are the real Xbox 360 kernel device names for the **console's
internal/external hard drive partitions**, specifically the
dashboard-managed disc-to-HDD **install-cache partition** — a real,
documented Xbox 360 feature (faster subsequent loads by caching disc
data to HDD), entirely separate from user save-game data. Tracing one
level up (`Function_823913F8`, `Function_82391E80`,
`Function_8234DB68`) shows this is gated by a configuration check
(literal string comparisons against `"game"`/`"cache"` mode names) and is
exactly the caller `XamTaskSchedule` was traced to in r237
(`Function_82391A40`'s scheduled routine `0x823917f8` resolves into this
same cache-management chain). There is no user save-game file writer in
this traced chain at all — `XamContentCreateEx` and the entire
`XamContent*` cluster that would front real save data remain confirmed
**dead** (zero real callers, r229), consistent with this build never
reaching an actual save-write code path.

## Why this is already adequate, not a gap

Real Xbox 360 hardware treats "no compatible hard drive present" (a
completely normal, fully-supported retail configuration — many consoles
shipped and were sold without one) as a graceful failure at exactly this
device-open step: attempting to open `\Device\Harddisk0\...` on a
console with no HDD returns an honest "no such device" status, and
title code (as traced here) already handles that gracefully — it just
skips the caching optimization.

This project's `NtCreateFile`/`NtOpenFile` stub (confirmed by reading its
current implementation this cycle) already routes every guest path,
including these `\Device\Harddisk0\...` paths, through
`native_guest_media_service().open_file()`, which is bound only to the
qualified retail ISO/media — a hard-disk device path was never going to
be found there, so it already returns `STATUS_OBJECT_NAME_NOT_FOUND`.
**This is exactly the real-hardware-equivalent "no HDD" outcome**, not an
accidental gap. No fix is needed or would change anything: this whole
disc-cache chain is already behaving correctly, for the right reason
(this recompilation has no host-side console-HDD-cache concept to model,
and real hardware without one behaves the same way).

## Consequence

1. **r202's "save/reload frontier" characterization is corrected, not
   merely narrowed**: what it traced was HDD disc-cache formatting, and
   that specific chain needs no fix. This does not mean a genuine
   user-facing save/reload frontier doesn't exist elsewhere in this
   title — it means it was never actually located by r202's trace, and
   the `XamContentCreateEx`/`XamContent*` cluster that would front real
   save data is confirmed dead in this qualified build, so no live
   save-write call chain has been found at all as of this cycle.
2. **r237's linkage of `XamTaskSchedule` to "the save/reload frontier"
   is corrected the same way**: its real callback is disc-cache
   management, already gracefully skipped, not a save-write blocker.
   `XamTaskSchedule` returns to the same disposition r198/r230 already
   gave it in isolation (its own generic-default failure is handled
   gracefully by its one real caller) — it does not need the
   save/reload write-support decision to be resolved first, because that
   decision does not apply to what its callback actually does.
3. **No new engineering scope needs a go/no-go decision as a result of
   this thread.** The FATX-formatting complexity r238's own
   investigation initially worried about (before the device paths were
   decoded) does not need to be built — the feature it belongs to
   already behaves correctly for the no-HDD case this recompilation
   models.

## Next

1. `NEXT.md`'s "save/reload" line item should be understood as: no live
   save-write call chain has actually been located in this qualified
   build (the plausible front door, `XamContent*`, is confirmed dead).
   If a future cycle wants to pursue real save/reload, it needs to find
   a *different* call chain than the one r202 traced — this one is HDD
   cache management, already correctly handled.
2. This does not reopen the offline-import sweep (r148–r237 remains
   closed) — it corrects the interpretation of evidence gathered along
   the way, which is exactly the kind of self-correction this project's
   own discipline requires (STATE.md's own history: "corrige r197",
   "corrige r209/r211", etc.), named explicitly per that convention.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
