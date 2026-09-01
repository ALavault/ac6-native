# AC6 retail NTSC-U/J — the pinned Xenia Edge release (`60ff861`) reproduces and extends r157's oracle capture; the result is now fully qualified (r158)

Date: 2026-09-01.

## Qualification

**Oracle capture, not a Gate-2 native-runtime code change.** Direct
continuation of r157's named priority: obtain the pinned Xenia Edge
release to convert that capture from provisional to qualified.

- Repository: [`has207/xenia-edge`](https://github.com/has207/xenia-edge),
  release tag [`60ff861`](https://github.com/has207/xenia-edge/releases/tag/60ff861),
  commit `60ff8616696e81726f09053874c12adc7716537f` — matching exactly what
  `reports/cycle-1734-xenia-edge-native-profile.md` and
  `scripts/run_xenia_edge_native.sh` pin.
- Downloaded fresh via the GitHub Releases API
  (`releases/tags/60ff861` → asset `xenia_edge_linux.AppImage`, API-reported
  digest `sha256:c2cac2a029ce0d44a71c4e919fd71c702654079023b63fd669472ba3cd78b828`).
  The downloaded file's own SHA-256, computed independently on this host,
  is **`c2cac2a029ce0d44a71c4e919fd71c702654079023b63fd669472ba3cd78b828`** —
  an exact match to both the API's digest and the pinned hash the existing
  script and cycle-1734 report require. Installed at the canonical path
  `.tools/xenia-edge-60ff861/xenia_edge_linux.AppImage` (shared tooling
  root, outside this repository's own git tree).
- Target: same as r157 — retail NTSC-U/J ISO, SHA-256
  `204c5e645d79da8776699c12f17bd069f869fbdb10ae79015d1a0ef2b743c98c`
  (re-verified this cycle).
- Isolated Xvfb `:180`, isolated `HOME`/profile root under session scratch
  (`/fastdata/tmp/claude-1007/r158-xenia-edge/`, fresh and separate from
  both r157's and the demo track's own profile roots). No files under
  version control modified except this report and its evidence directory
  (`reports/ac6-retail-native-xenia-edge-oracle-r158-pinned-20260901/`,
  gitignored PNGs/log per this repo's existing policy, cited by
  `sha256sums.txt`).

## What was found

The pinned release reproduces r157's capture exactly and progresses
further:

1. Same title correctly identified: `Extracted title_id 4E4D07D1` from the
   same ISO, same shader/pipeline sequence in the same order (identical
   `Shader 472913F460D4B446` / `8F1C48BA92C8E43E` translations, identical
   pipeline creation log lines) — this is the same underlying emulator
   behavior as r157, not a coincidence.
2. **The legal/trademark screen is pixel-identical to r157's capture** —
   `01-trademark-screen.png`'s SHA-256
   (`f55350740938da01ccc1582ed4875d2d1e669d5f1939c47c9fa5c79da2cbbba7`) is
   the exact same hash as r157's `01-trademark-screen.png`, despite the two
   AppImage binaries themselves having different file hashes. This is
   strong, direct reproducibility evidence: two separately-invoked Xenia
   Edge builds render the identical frame from the identical retail
   content.
3. **New progress beyond r157**: after dismissing the profile-creation
   dialog, this run reached a second legal/credits screen (satellite
   imagery providers — Japan Space Imaging, GeoEye, INTA Spaceturk,
   DigitalGlobe/HitachiSoft, Bink Video —
   `02-imagery-credits-screen.png`), then a genuine **ESRB online-play
   notice screen** (`03-esrb-online-notice.png`, black background, "ESRB
   Notice: Game experience may change during online play.") — a further,
   later point in the real boot sequence than r157's own capture reached.

## What this establishes

**r157's finding is no longer provisional.** The pinned, hash-verified
Xenia Edge release (`60ff861`, matching this project's own existing
documentation) reproduces the same boot-past-DATA.TBL result, and this
cycle's run reaches even further into the real boot sequence (ESRB notice,
a screen that only appears after the legal/credits sequence completes).
The conclusion r157 drew — that the `sub_821F7C80`/`[r1+88]`
uninitialized-stack stall this campaign has traced since r130 is specific
to this project's own recompilation, not a real-game behavior — now rests
on a fully qualified, reproducible oracle capture rather than an unpinned
build of unknown provenance.

## What this does not establish

Same limits as r157: no `DATA.TBL`-by-name confirmation in the log (file
opens aren't logged by name at this verbosity); no direct evidence of what
real Xbox 360 hardware stores at the equivalent stack location (Xenia
Edge's own memory model differs from real hardware and from this project's
recompilation); no campaign gameplay reached (still boot-sequence screens,
not menus or a title-select state). The targeted `[r1+88]` comparison r157
named as the next step has not yet been attempted.

## Decision

r157's finding is promoted from provisional to qualified. The DATA.TBL
sub-thread's status stands as r157 described it: r144's "no further lever
without an oracle" conclusion no longer holds, and a real native fix is a
defensible next investment. This cycle's incremental contribution is
verification, not new direction — the next actionable step is unchanged
from r157's own list.

## Gates

No native code changed; `ctest`'s last-known state (9/9, r155) stands
unaffected. `git status` unchanged (only pre-existing, unrelated dirty
state). Neither the download nor the capture touched anything under
version control besides this report and its evidence manifest.

## Next

1. Design and run the targeted comparison at the exact `[r1+88]` read
   this campaign traced (r141/r142/r153) — now backed by a qualified
   oracle, this is the concrete next investigation r157 named.
2. Both of Gate 2's other named frontiers are unchanged: `sub_82390880`/
   `sub_821F5630` caller question closed (r156); `IM_LOAD_IMMEDIATE`→SPIR-V
   remains policy-blocked pending Xenos fetch-signature qualification —
   this cycle's capture did not reach shader-microcode-level evidence.
