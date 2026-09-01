# AC6 retail NTSC-U/J — a real Xenia Edge oracle boots this exact retail title past everything our native recompilation reaches; the DATA.TBL/uninitialized-stack stall is confirmed recompilation-specific, not a real-game behavior (r157)

Date: 2026-09-01.

## Qualification

**Not a Gate-2 native-runtime code change.** This is an oracle capture,
following r156's user-authorized "Oracle and invest" and the user's
mid-cycle redirect to try Xenia Edge (Linux-native) rather than the
Wine/Windows route. XEX/ISO target: retail NTSC-U/J,
`4E4D07D1`, ISO SHA-256
`204c5e645d79da8776699c12f17bd069f869fbdb10ae79015d1a0ef2b743c98c`
(verified — exact match to the sealed identity in `NEXT.md`, not the PAL
disc the repo's existing Xenia scripts default to).

Emulator: `xenia_edge_linux.AppImage`, SHA-256
`cdd86d2e661ce2029f1d94c3033d95e1691e5850a318c3251dfc32f4defa66a3`. **This
does not match** the pinned release (`c2cac2a029ce0d44a71c4e919fd71c702654079023b63fd669472ba3cd78b828`)
`scripts/run_xenia_edge_native.sh` expects — this binary was found instead
at `ac6_demo_work/xenia-edge-test/` (an existing, extensively-used demo-track
scratch build with its own tuned NVIDIA profile directories from prior
cycles). **This capture is therefore provisional, not a qualified oracle
run** under this project's own identity discipline: the binary's exact
provenance (build commit, release tag) is not established here, only that
it is a real, working, previously-exercised Xenia Edge build on this host.
Treat every finding below as real, observed behavior from *a* Xenia build
against the *correct* retail ISO — not yet pinned to a specific,
citable Xenia Edge release.

Isolated Xvfb `:179`, isolated `HOME`/profile root under session scratch
(`/fastdata/tmp/claude-1007/r157-xenia-edge/`, not the demo track's own
profile — kept separate for clean attribution), fresh empty content/cache
roots. No files under version control were modified during the run;
screenshots and the full log are copied into
`reports/ac6-retail-native-xenia-edge-oracle-r157-20260901/` with a
`sha256sums.txt` for this report's own citations.

## What was found

Unlike every previous attempt this session (r156's native Linux Xenia
Canary, which stalled on an `SDL_OpenAudioDevice()` failure before any
rendering; and the pre-existing `xenia_identity_cross_check` record noting
"title terminated before gameplay"), **this Xenia Edge session boots the
real retail NTSC-U/J title cleanly, past every point this campaign's own
native recompilation has ever reached**:

1. Title correctly identified from the ISO: `Extracted title_id 4E4D07D1`
   (twice, for two separate resolution passes), matching this project's
   sealed retail identity exactly.
2. VFS, kernel, guest scheduler, audio client, and networking all
   initialize successfully (`AudioSystem::RegisterClient: client 0
   registered successfully` — the exact step r156's native Canary build
   failed at).
3. Real Xenos shaders are translated and real Vulkan pipelines are built
   and used for actual draws (e.g. `Shader 472913F460D4B446 vertex
   translated successfully (17488 bytes)`, `Pipeline created for VS
   ...`), climbing past frame 2000 before capture ended.
4. **A real, correctly-rendered legal/trademark disclaimer screen**
   (BAE Systems, Boeing, Dassault Aviation, Lockheed Martin, Northrop
   Grumman, Japan Air Self-Defense Force logos, full legal text) —
   `01-trademark-screen.png`, `sha256sums.txt`. A profile-creation prompt
   ("There is no profile available... Create profile / Not now") blocked
   further auto-progression here; dismissed with a synthetic click
   (`xdotool`) at the "Not now" button, not a scripted input replay.
5. After dismissal, **a real, high-fidelity title-screen backdrop
   sequence** — a suspension bridge over a bay city
   (`02-title-backdrop-bridge.png`) and, moments later, a fighter jet in
   flight (`03-title-backdrop-jet.png`) — consistent with Ace Combat 6's
   known title-screen cinematic loop. Both are genuine in-engine renders,
   not static images: shader/pipeline creation continued throughout, and
   the visible content changed between captures.

No `DATA.TBL`-specific filename ever appears in the log at the logging
verbosity used (file opens are not logged by name at this level; only VFS
device-resolution events and the import-thunk address table are), so this
capture does **not** directly show a successful `DATA.TBL` open by name.
It does, however, demonstrate the game running for over 2000 emulated
frames — almost certainly well past the game's own campaign-data loading,
which on the native recompilation side (r130-r153) is the very first
disc-content read after boot, blocking before any menu or title rendering
occurs at all.

## What this establishes

**The `sub_821F7C80` crash chain this campaign has traced since r130 — the
uninitialized `[r1+88]` stack slot in `sub_82338568`'s frame feeding a
garbage allocation size, closed as "no further native-runtime lever" by
r144 and re-confirmed byte-for-byte by r153 — is now shown, for the first
time this campaign, to be specific to this project's own recompilation, not
a behavior the real retail title exhibits.** A faithful, independent
emulation of the exact same disc content proceeds to a fully rendered,
interactive-looking title screen; our own native recompilation's guest
thread never gets past the point where that stack slot is read. This is
the first piece of oracle evidence gathered in this campaign's entire
"no oracle used" history, and it changes the DATA.TBL sub-thread's status:
r144's "no further lever without an oracle" is no longer true in the
literal sense — an oracle now exists and shows the bug is
recompilation-local, meaning a real fix (correctly initializing or
otherwise not depending on that stack slot, rather than the game's own
logic being at fault) is now a defensible, evidence-backed thing to
attempt, not a guess.

## What this does not establish

- **Not campaign gameplay, not save/load, not any deeper mission state.**
  This is a title-screen boot, nothing beyond.
- **Not proof of the exact real value at `[r1+88]`** — Xenia Edge's own
  internal stack/memory management is a different implementation from both
  this project's recompilation and real Xenon hardware; it does not
  directly reveal what a real console would store there. It only proves
  the real game does not need whatever this project's recompilation
  currently produces at that slot to boot correctly, and does not itself
  reach an equivalent dead end.
- **Not a qualified, pinned oracle capture** per this project's own
  identity discipline — the Xenia Edge binary used is not the release
  `scripts/run_xenia_edge_native.sh` pins, and its own provenance was not
  established this cycle (see Qualification). A future cycle should locate
  or re-download the pinned `c2cac2a0...` release and re-run this same
  capture against it before citing this as fully qualified evidence in any
  contract or gate.
- **Not yet localized to a specific fix.** Knowing the bug is
  recompilation-local narrows the search (something in this project's own
  stack-frame handling, PPC ABI translation, or memory initialization
  differs from what real code paths expect) but does not by itself name
  the fix.

## Decision

This is recorded as the most significant single finding of this session's
DATA.TBL sub-thread: a real, if provisional, oracle result that overturns
this campaign's working assumption (native-Xenia-based oracle access is
unavailable/non-viable for this title) and gives the DATA.TBL crash chain a
genuinely new, actionable status. It is not treated as fully qualified
evidence — the pinned-release mismatch is flagged plainly — but it is
strong enough to justify naming "re-derive why `[r1+88]` differs from real
retail behavior, now that an oracle shows the real game does not hang here"
as a live, resourced next investigation, reversing r144's prior
cost-benefit closure of that specific question.

## Gates

No native code changed this cycle; the retail-native build/tests were not
touched (r155's last-known `ctest` 9/9 stands). `git status` unchanged
(only pre-existing, unrelated dirty state). The oracle run touched no
files under version control except this report and its evidence directory.

## Next

1. **High priority**: locate or acquire the pinned Xenia Edge release
   (`c2cac2a0...`) and repeat this exact capture against it, to convert
   this from a provisional to a qualified oracle result citable in
   contracts.
2. With a qualified oracle available, design a bounded, targeted
   comparison at the exact point this campaign's own recompilation reads
   `[r1+88]` — e.g., does Xenia Edge's own equivalent guest stack ever hold
   uninitialized content there, or does something upstream in real
   execution write it that this recompilation's codegen omits? This
   reopens r144's "no further lever" conclusion for the DATA.TBL
   sub-thread specifically.
3. `sub_82390880`/`sub_821F5630` caller question remains closed (r156).
   `IM_LOAD_IMMEDIATE`→SPIR-V remains policy-blocked pending Xenos fetch
   signature qualification — this cycle's oracle capture did not reach
   shader-microcode-level evidence and does not unblock that frontier by
   itself.
