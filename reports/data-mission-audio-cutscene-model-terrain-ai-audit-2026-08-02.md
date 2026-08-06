# AC6 data systems audit — 2026-08-02

Read-only Terra/max audit. No source or retail asset was modified.

## Qualification

- Target: PAL `default.xex`.
- XEX SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Canonical project: `ghidra-projects/ace-combat-6`.
- Archived disc source SHA-256:
  `db67238afbe1ec0e5978314a8f0f011851ea969d1786b8fd4146f3b124398dd6`.
- Generated C++ was used only as literal control-flow cross-match evidence.

## Missions and cutscenes

Established:

- Campaign selector `1` resolves through the DPL table to DPL `9`, then to
  physical `DATA.TBL` entry `9`. This does not by itself activate a Scene
  group.
- Entry 9 is 42,446,032 bytes, SHA-256 prefix `cd81e0`; child 1 is an MDLP
  inventory of 94 elements. Children 22/23 expose 44 `NFIC CUT`/FHM/Scene
  triples and 553 resolved MOP/GYZ paths.
- Every resolved Scene record `i` maps to FHM member `i`. The exported
  `cut/NNNN` order is FHM traversal order, not a mission chronology.
- CUT0000–0015 contain `dd01_01a`; CUT0016–0031 are post-effects;
  CUT0032–0037 contain `dd01_02a`; CUT0038–0043 are post-effects.
- In all 16 `dd01_01a` CUTs, `Tcam` is record 0 and
  `Tunit__PLAYER__01` is record 1/id 2. Every `GazeCamera` (`0x2001`)
  event names raw id 2. CUT0011–0015 also contain
  `Tunit__WINGMAN__01` as record 2/id 3, with no GazeCamera id 3.

The PLAYER/GazeCamera association is strong serialized evidence, not proof of
the runtime player actor or its aircraft model. The consumer at `0x823695C0`
still performs only a generic lookup.

## Aircraft and scene models

Established:

- The first CUT has 16 `Rigid/AnimRigid` objects (ids 3–18) with valid
  MOP→literal-key→MDLP/NDXR joins.
- Published complete diffuse chains include `r_f16c` at MDLP 76/77 and
  `r_f18f` at MDLP 78/79.
- `scene_shell.cpp::scene_asset_key()` accepts only names shaped as
  `__<key>_t`. `Tunit__PLAYER__01` and `Tunit__WINGMAN__01` do not match that
  contract, so the current join cannot assign F-16/F-18 geometry to either
  track.

Unknown:

- Player, allied and enemy model ownership.
- Whether all 16 objects have complete material/texture coverage. Current
  smoke expectations count 16 bound objects, while detailed reports publish
  only the two diffuse chains above.

## Terrain

No exact terrain/ground/landscape token or proven Scene→environment-mesh join
was found in the 553 resolved paths or bounded MOP/NFIC exports. A useful
bounded candidate is `Tlod__s_dstr_t1__01.mop` at decoded entry-9 offset
38,588,112, length 5,840, but it must acquire an exact MDLP key and a Scene
consumer before being called terrain.

## Audio and XMA

Established:

- `reports/fhm-asset-manifest.csv` contains 546 RIFF leaves among 926 entries.
- DATA 210 contains BRDB member 0, BMAP member 1 and eight RIFF leaves under
  `4.*`/`5.*`.
- Canonical `0x8218F358` computes
  `FUN_820943B0(DAT_826E4EB4+0x70) + 0xD1`, with a `0xE1` fallback via
  `0x820F6228`. Therefore bridge selector 1 chooses entry 210 on this path;
  this still does not map any RIFF to a line or cutscene.
- Qualified members `210/4.1`, `210/5.1` and `232/0.1` have
  `RIFF/WAVE/fmt(0x20)`, format tag `0x0165`, then `ALIG`. In these three
  bounded samples, RIFF `+4` equals the containing FHM member size.
- `bgmpack.bin` starts with `RIFF … WAVE XMA2`. Voice packs start with
  `RIFF/WAVE/fmt`, fmt size `0x20`, tag `0x0165`.
- `0x821249C8` chooses `voicepack_{jpn,eng}.bin` and
  `demopack_{jpn,eng}.bin`; its callbacks are in table `0x8205D3A4`.
- Named command entries map `PlayBGM` to `0x820F7448`, BGM fade-in to
  `0x820F74B8`, stop to `0x820F7540`, fade-out to `0x820F7568`, and
  `PlayVoice` to `0x820F7638`. The BGM commands call slots `+0x10..+0x1C` of
  `DAT_8293B840`.
- `0x82374490` (`nuSound2Stream::_setBufferSize`) has a distinct XMA2 branch
  when `stream+0x90 == 4`, sizing from `+0x144 * +0x26C`; other input is
  diagnosed as WAV or XMA1. `0x823AD298` submits the render-driver frame.
- RTTI establishes `CX360RadioManager` vtable `0x82056850`. Slot `+4`
  (`0x820A6720`) creates campaign resource/play managers at `this+0x10/+0x14`
  when its parameter is zero. RTTI also names `CAce6RadioMissionData` and
  `CAce6RadioPackTable` plus Arrival/Request tables.

Unknown:

- BRDB/BMAP→RadioMissionData/PackTable→RIFF-index consumption.
- `DAT_8293B840`→nuSound2 stream ownership and packet submission/decoding.
- The exact `PlayVoice` body; its current Ghidra boundary crosses the
  `0x82382EF8` save-register island and must be corrected first.

## AI and ownership

Established:

- Factory `0x820A7F48` maps selector 1 to `ACE6::CAce6UnitPlayer` vtable
  `0x820568D4`, and selector 2 to `ACE6::CAce6UnitOtherPlayer` vtable
  `0x82056934`. Selectors 3–6 have no qualified RTTI identity.
- `CDemoDDWingman` vtable `0x8205D750`/constructor `0x82127648` is a demo
  property, not mission-allied AI evidence.
- Campaign mission orchestration has typed HSMs:
  `CAce6MissionManagerCampaign` (`0x8206432C`, `0x82064264`) and
  `CX360MissionManager<…>` (`0x82064648`, `0x8206457C`). Slot `0x821A6348`
  reads state `this+0xC`; states 0/2 take a global path, otherwise it
  dispatches children at `+0x1C` and conditionally `+0x274`, virtual slot
  `+0x24`.
- Strings `Init/Update_DestroyTarget_FollowEnemy` are camera choreography at
  `0x8217B508`, not aircraft behavior AI.
- `MissionAircraft` at `0x8218C238` remains a registry/normalization key, not
  actor ownership or spawn evidence.

Unknown:

- Active HSM owner and mission→player/allied/enemy mapping.
- Enemy behavior states, spawn ownership and flight-controller dispatch.
- Whether `OtherPlayer` is allied, enemy or another network/player role.

## Highest-value next experiments

1. Trace the `GazeCamera` receiver after `0x823695C0` until id 2 resolves to a
   proven Scene track.
2. Trace the first runtime `Tunit`→`Tlod`/MDLP factory join; never infer role
   from record order, adjacent ids or filename prefixes.
3. After entry 210 selection, capture `CX360RadioManager+0x10/+0x14` and the
   first consumers of FHM members 0/1.
4. For one BGM command and one voice command, capture the vptr of
   `DAT_8293B840`, invoked slot and stream fields `+0x90/+0x144/+0x26C`.
5. During first-mission task creation, capture the active HSM vptr, `this+0xC`
   and child vptrs at `+0x38/+0x44..+0x60`.

## Source evidence

- `DPL_ARCHIVE_HANDLE_CHAIN.md`
- `ENTRY9_CHILD1_CONSUMER_REPORT.md`
- `AC6_LINUX_SCENE_SHELL_REPORT.md`
- `AC6_MATERIAL_TEXTURE_LINK_REPORT.md`
- `MISSION_VISUAL_BOOTSTRAP_REPORT.md`
- `reports/entry9-scene-path-resolution.csv` (SHA-256 prefix `899161`)
- `reports/first-world-material-texture-links.csv`
- `remaster-export/relations.csv` (SHA-256 prefix `d4b52b`)
