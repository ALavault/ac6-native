# Mission 01 — the live ladder

The single roadmap. Six older planning documents at the repo root describe
superseded ladders and are bannered as such; plan from this file.

## The product

A native Linux/Windows Ace Combat 6 with its own renderer and **no runtime
dependency** on Xbox libraries, RexGlue, XenonRecomp or Xenia. Those exist here
as evidence and observation tools and never ship.

Written by hand, behaviour by behaviour, each citing the retail function it
derives from — the rule the JF auditor enforces mechanically by reading the
native source and refusing any path marked generated or recompiled.

## Gates

| gate | what it certifies | state |
|---|---|---|
| **J0** | the product's own session loop: world visible, player visible, camera, input, deterministic replay | **passed** |
| **J1** | the retail domains — units, waves, objectives, HUD, debrief, radio — with retail semantics qualified | **passed** |
| **J2** | Mission 01 built from the payload alone, and every RTTI vtable named | **passed** |
| **JF** | eight behaviours of the mission path, each with static evidence, a native test and a **derivation citing its retail addresses in the native source** | **passed** |
| **JV** | the visible world: Mission 01 drawn from retail assets, no TSV manifest on the path | *not started* |
| **JP** | controller-playable: a human flies it to debrief, and a replay reproduces it | *not started* |
| **JG** | measured parity against Xenia at pre-registered tolerance | *not started* |

J0–JF were reached with **zero oracle passes spent**.

### Running the gates

```bash
python3 tools/audit_ac6_mission01_native_gate.py \
    analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_mission01_native_gate.py \
    analysis/contracts/mission01-native-gate-v2.json --artifact-root .
python3 tools/audit_ac6_class_map.py analysis/class-map.tsv \
    --rejects analysis/class-map-rejects.tsv --require J2
python3 tools/audit_ac6_contract_artifacts.py \
    analysis/contracts/mission01-final-gate-v3.json \
    analysis/contracts/mission01-visible-gate-v4.json
ctest --test-dir reconstruction/ace-combat-6/build
```

The third of those exists because the gate auditor hashes the **working tree**,
which is right for what it audits and leaves one gap it cannot see: an artefact
regenerated and never staged. The contract hash gets refreshed against the file
on disk, the gate passes locally, and a fresh clone fails on the stale committed
copy. That happened once and was caught by reading `git status`. It now has a
check.

Both auditors must exit 0 and ctest must pass before any commit. Anything
windowed runs as `SDL_AUDIODRIVER=dummy xvfb-run -a <cmd>`.

## Where the product stands

Two halves that do not yet meet:

- **The manifest path draws.** `--play-headless` renders real NDXR geometry —
  see `reports/mission01-native-captures/p7-current-main/`. But
  `tools/make_mission01_native_manifest.py` *synthesises* its transforms (every
  drawable at the origin), materials and textures, and it draws three drawables,
  not a world.
- **The retail path is the world.** `--retail-session` builds 230 units and the
  sub-mission script from the container with no manifest anywhere — and renders
  HUD only.

JV is the work of making those one thing.

## What the rendering code is

The names mislead, so plainly:

- `VulkanRenderer` (`src/native_renderer.cpp`) contains **no Vulkan**. It is a
  fail-closed validation gate over eleven databases plus a dispatch loop.
- The renderer is a **CPU scanline rasteriser**, `NativeRenderTarget`
  (`src/native_geometry_raster_target.cpp`).
- Vulkan exists only in `src/sdl_input.cpp`, as a present path that blits the CPU
  buffer to the swapchain — allocating a staging buffer every frame.
- **NDXR is the only retail format the product decodes** — and its header layout
  is **measured, not derived** (cycle 1189): `src/native_geometry_raster.cpp` and
  `include/ac6/native_geometry.h` cite zero retail addresses, and no contract
  names either as a derivation. It predates this session and has never been
  audited. That is a larger open item than any texture question. The `NTXR`, `MATE`
  and `MDLP` parsers described in the root structure reports are **not in this
  repository**; they lived in a prior workspace. One texture profile is
  pixel-decoded, in a Python diagnostic script, out of 7,993 wrappers.

## JV — the visible world

**The v4 contract exists**: `analysis/contracts/mission01-visible-gate-v4.json`,
extending v3 with one behaviour per asset domain as the plan requires. It carries
`texture_decode` today and audits clean, every claimed retail address cited in
`include/ac6/ntxr_texture.h` and every artefact hashed.

There is deliberately **no `--require JV`** in the auditor. One domain of several
is not a gate, and adding the name before the domains exist would make the gate
assert something no evidence supports. It goes in when terrain, the binding and
the camera have entries beside the texture one.

```bash
python3 tools/audit_ac6_mission01_native_gate.py \
    analysis/contracts/mission01-visible-gate-v4.json --artifact-root . --require JF
```


1. ~~**Fuse the halves.**~~ **Done, cycle 1144.** `RetailSession` draws the world
   through the rasteriser's own projection. Diagnostic markers only — no
   geometry, and no capture containing one may be offered as visual parity.
2. ~~**The transform frame.**~~ **Done, cycle 1145**, and it settled the opposite
   question. The frame is `parent.translation + parent.basis * offset`, and
   `0x8229AF80` places nothing without a parent. Parents *are* assigned on the
   load path — cycle 1145 said otherwise and cycle 1147 corrected it — but only
   for 27 of 434 Obj records, and never for the ones whose triple would be used,
   so the Obj triple was never the units' position: 169 of 230 are `(0,0,0)`.
   Their load-time position is the first tag-2 order, resolved by `0x822953F0`:
   **95 of 230 placed, 135 with no load-time position in the container.**
3. **A flight camera — now the blocker.** Cycle 1146 corrected why the capture
   showed 4 markers and not 95: **not the camera**, but `project_point`
   normalising depth as `view_z / 4096`, so everything beyond 4,096 units
   saturated to 1.0 — the clear value — and the depth test dropped it. Mission
   01 spans 66,000 units. Callers plotting the world now pass their own far
   plane, and `world-overview.png` shows all 57 distinguishable positions.

   **A candidate spawn, and a reversal to check (cycle 1182).** The player's
   first order is tag 0 and its payload carries `(-2025, 1500, 1345)` — exactly
   Mission 1's `PLAD` row, in a second independent file, with mode 0 and `FF FF`
   anchors. Four controls hold across 230 units. It also suggests
   `initial_world_position` reads the wrong order: unit 9's first tag-2 triple is
   shared with sixteen ground units, so it is a destination, not a spawn. **The
   product is unchanged** — `0x822A23D8` has not been read (VMX128 halt), and
   reversing a ported behaviour on correlation is what this campaign refuses.
   Next read: `0x822A23D8` in the Xenon corpus.

   What remains is the player's spawn. Cycle 1177 exhausted `PLAD` — both its
   accessors have exactly three call sites each, paired, and every getter reads
   only word 3 — and cycle 1176 found the player also carries no model index,
   where 149 other units do. The mission describes neither the player's aircraft
   nor where it starts, which is consistent with the player choosing one before
   the mission. Three searches of the container have come back empty; the next
   one should be for the code that builds the player from the hangar's choice.

   It is **not** in the container:
   cycle 1146 dumped the five root slots the parser does not consume (3, 4, 6,
   7, 8, 9) and none holds a float triple in world range. The only qualified
   camera is the
   cinematic TCAM, which cycle 732 disqualified; the rasteriser's fallback is a
   hardcoded 60° / far 4096. This is blocked in turn on the **player's own
   spawn**, which cycle 1145 showed is neither in the player's behaviour program
   nor in `PLAD` — all three callers of `0x82249BC8` read only word 3, the route
   cursor, and none reads the record's floats.

   The open lead is that cursor. PLAD word 3 is stored to `+0xF0` of the unit at
   `manager+0x404` — the player — so the player's start is plausibly the first
   point of the route it selects. Two warnings for whoever picks this up:

   - `grep "0xf0(r"` does **not** find the readers, and cycle 1147 built the
     instrument that does: `tools/ghidra_scripts/Ac6FieldRead.java`. It splits
     a displacement load three ways using signatures the codegen guarantees —
     a **vtable dispatch** is consumed by `mtspr CTR` within four instructions,
     a **stack slot** has `r1` as its base, and what is left is a **field
     read**. On `+0xF0` the split is 43 field reads, 17 dispatches, 10 stack
     slots: a grep would have been 63% wrong, which is what cycle 1145 was.

     The result is a bounded negative. None of the 43 reads `+0xF0` as the
     route index PLAD wrote. They belong to a dozen unrelated classes that
     happen to share the offset — `0x82091130` copies it as a float,
     `0x82269530` dereferences it as an object pointer and checks `+0xB0`
     against `+0xF4`. Finding the right one needs the **class** of the object
     at `global+0x12BC34`, which this search does not establish. That is the
     next question for this thread, and it is a bigger one than a scan.

   - `+0x188` **is** assigned on the load path, by `0x820A7B2C` inside
     `0x820A7070`, from byte `+0x18` of the Obj record with `0xFF` as the
     sentinel. Cycle 1145 said nothing assigned it; cycle 1147 corrected that.
     27 of Mission 01's 434 Obj records name a parent and 407 do not, so
     parent-relative placement is a **bounded 27-record job**, not a missing
     mechanism. It is deliberately not implemented yet: the child needs the
     parent's staging transform populated when it is placed, and this port has
     no staging/commit ordering.
   - `+0x180`–`+0x194` are **floats on a different class** (`0x8229EAC0` clamps
     four of them between limits). They are not the unit class's `+0x184`
     pointer and `+0x188` parent, and cycle 1146 nearly read them as such.
4. **Textures — the descriptor is now derived (cycle 1150).** The consumer is
   `0x8233EA78`. `0x8234B360` decodes the descriptor: `lhz +0x14` = **width**,
   `lhz +0x16` = **height**, `lbz +0x13` = **format code** bounds-checked `< 47`
   and indexing an 8-byte table at `0x826767C0` whose word 0 carries the Xenos
   `TextureFormat` in its low 6 bits. `0x8234B128` reads the **mip count** at
   `+0x11`, `0x8234B118` the **cube flag** at bit 9 of `+0x1C`. The factory
   `0x8234AED8` picks `TextureContextXenon` / `…MipMapXenon` / `…CubeMapXenon`
   from those two, and `0x8234AA68` fixes the field mapping. The table has
   exactly 47 entries, which is the bound the code checks.

   Cycle 1149 refused to name these fields from payload correlation (83% vs a
   71% shuffled null) and was right to; the method was simply the wrong one.
   `NTXR_STRUCTURE_REPORT.md`'s refusal is lifted for these four fields.

   **The correction this forced:** `probe_ntxr_bc.py` decodes the one visually
   validated wrapper as BC3/DXT5, but its descriptor says format code 1 → low6
   `0x13` → **k_DXT2_3, BC2/DXT3**. BC2 and BC3 share their colour half exactly
   and differ only in alpha, so the probe's RGB was right — which is why the
   atlas looked intelligible — and its alpha has always been wrong. Derived
   census over 692 wrappers: **656 BC3, 22 ARGB8888, 12 BC1, 2 BC2**.

   **The surface rule is derived and the decoder is written (cycles 1151-1156).**
   `0x821FBE30` is `XGSetTextureHeader`: it allocates nothing and returns the
   size to allocate. `0x821DF838` aligns X to 32 blocks and Y to 32 block-rows
   and rounds every level to 4096; for BC1 and BC3 alike the untiled 256-byte
   pitch floor also lands on 32, so `pad32 x pad32 x bytes_per_block` is the
   rule either way. Of `0x821DF958`'s four base-size formulas the corpus takes
   the **tiled** one, `roundUp(pitch x alignedH, 4096)` - confirmed on four
   shapes where the tiled and untiled answers differ by 6-20%.

   The mip geometry is **declared, not modelled**: file `+0x40` and `+0x44`
   (absent on single-level headers, which hold `eXt` there) satisfy
   `payload == word[0x40] + word[0x44]` for **360 of 360** multi-level wrappers,
   and `word[0x40]` equals the rule measured from single-level payloads for all
   360 - two derivations sharing nothing and agreeing.

   `decode_ntxr_base_level` decodes **668 of 692** wrappers (324 distinct, 41
   shapes, 26 non-power-of-two); 22 are not block formats and 2 are cube maps.
   Levels above zero are not decoded and nothing consumes them yet.

   Still open: 
   MATE batch→material→texture→NTXR in the runtime.
5. **Terrain.** Currently **fail-closed by policy**:
   `MISSION_VISUAL_BOOTSTRAP_REPORT.md` requires a proved Scene/CUT ownership
   edge before any static environment is drawn. Prove it; do not lift it.
6. **Derived binding — the data is Mission 01's own MDLP (cycle 1157).**
   `idx_0009/001_MDLP.mdlp` is 29 MB indexing **94 `FHM ` bundles** — header
   `+0x04` count, `+0x08` total size (equals the file size), `+0x0C` table at
   `0x1000`, `+0x10` base at `0x2000`; 94/94 signatures, monotonic offsets. It
   holds **292 NDXR, 381 MATE, 86 NTXR, 522 GIDX** for this mission, and it sits
   in the same directory as the scenario container this product has parsed since
   J2. Read it with `tools/ac6_mdlp_index.py`.

   Cycle 1148 said the binding needed "the external definition table identified
   and parsed first" and looked for it in the archives at large. It was in the
   mission's own bundle the whole time. **The index space is derived (cycle 1158).** `0x8228E9B8` is the entry
   getter — count at header `+0x04`, offset table stride 4, `entry = base +
   table[index]`, bound-checked — which is the MDLP layout exactly, established
   independently by disassembly and by parsing the file. `0x820A7070` walks the
   whole container registering every entry, then indexes **the same container**
   with bytes `+0x61`/`+0x62`, so those bytes are MDLP entry indices. All four
   calls to the getter in the image are in that function and all use the same
   container.

   **The container is an MDLP, derived (cycle 1163).** `0x820A7070`'s first
   argument is a `CX360UnitManager` (vptr `0x82055190`, two instances embedded in
   `CX360MissionManager<ACE6::CAce6MissionManagerReplay>`); its vtable `+0x0C` is
   `0x820A85E0`, ending in the writer `0x8228E988` which sets word0 = blob,
   word1 = blob + `*(blob+0x0C)`, word2 = blob + `*(blob+0x10)`, count =
   `*(blob+0x04)` — the MDLP's three header fields exactly. The blob is
   payload-resident: `0x820A85E0` re-derives the loader's own node and descends
   by the hashed name `"DPL::[%#x,%#x]"` to chunk index 1.

   **Ported, cycle 1172.** `ScenarioModelBinding` carries the pair per Obj
   record; the parser test asserts 434 bindings, 123 sentinels, 309 secondaries,
   281 consecutive pairs, 38 distinct primaries and a highest of 74 against the
   directory's 94, plus the structural rule that a record without a primary
   never carries a secondary. **No table was written** — the chain is cited
   instruction by instruction in the header. It is the join, not the loader:
   nothing loads a model yet.

   `ModelDirectory` (cycle 1174) ports the container and both accessors and
   closes the join — 94 entries all beginning `FHM `, 311 model indices from the
   scenario, 0 refused. It stops at the entry boundary, because the **FHM layout
   is measured, not derived** (cycle 1175): no instruction builds the `FHM `
   magic, the reader has not been found, and `tools/ac6_fhm.py`'s field offsets
   rest on 94 of 94 bundles parsing cleanly. Importing a whole format on
   measurement is how a port stops being a derivation, so the walker stays out
   until its reader is followed out of the typed-resource dispatch at
   `0x82343010` / `0x8234CB58`.

   **The gap is closed (cycle 1171), and cycle 1148 was wrong.** Byte `+0x61` is
   in the scenario container after all — on the Obj entry's **child[0]** data
   block, one node below where cycle 1148 measured. Over the 434 records it takes
   38 non-sentinel values (0, 2, 4 … 74) with `0xFF` in 123, `+0x62 == +0x61+1`
   in 281, and every value below the MDLP's 94 entries. The two bytes are a pair
   of consecutive MDLP indices; what the second one is remains unestablished. Also still to derive: MATE material → texture id → GIDX, and the
   NTXR pack directory (measured in cycle 1163: 522 records of stride `0x50`,
   518 with `eXt`+`GIDX`, 494 decoding).
   Walked with `tools/ac6_fhm.py` rather than regex-scanned: **94/94 parse with
   zero parser notes**; 47 entries carry geometry, 46 of those pair NDXR 1:1
   with MATE and entry 88 does not (9 NDXR, 5 MATE); 82 entries hold exactly one
   NTXR, 2 hold two, 10 hold none.

   **Also present (cycle 1156 corrects 1155).**
   179 distinct NDXR models and Mission 01's own 29 MB `001_MDLP.mdlp` (94
   entries) are extracted, in `idx_0009` beside the scenario container the
   session already reads. Cycle 1155 reported them absent from a `find` capped
   at depth 3; the assets are four to six levels deep.

   **Still needs a second data source (cycle 1148).** Unit class
   byte → object category → model must come from retail data, not a hand-written
   table. `0x820A7070` fills `+0x15C` from two `0x8228E9B8` lookups keyed by
   bytes `+0x61` and `+0x62` of the record at word 0 of the 0x20-byte array
   (`r28`), with `0xFF` as the sentinel.

   **That record is not in the scenario container.** Both candidate structures
   were measured at those exact offsets — 230 unit record data blocks and 434
   Obj node data blocks — and `+0x48`, `+0x56`, `+0x61`, `+0x62` are zero in
   every one, never the `0xFF` the code tests. The reads were inside their
   blocks: the smallest gap between consecutive Obj data blocks in the mission
   is 352 bytes.

   So the derived route needs the external definition table identified and
   parsed first. The container supplies only the join key — class byte at data
   `+0x08` (four distinct values on Mission 01) and faction at `+0x0D`. **This
   is still where JV would quietly become J1 again, and more so now**: the
   honest route just got longer, which makes a hand-written table more
   tempting, not less.

## Evidence discipline

Operational companion: `INSTRUMENT_DISCIPLINE.md`, written from eight
instrument-scope failures in one session. All eight produced a *negative*, which
is the direction nothing downstream catches. The technique that caught the only
one spotted immediately: **before believing a zero, run the same search against a
case whose answer you already know is not zero.**


In `CLAUDE.md`, and it is the point of the whole campaign: never assert a value
that was not read; refuse a plausible rule with no control; correct predecessors
**and yourself**, by cycle number; measure the instrument before trusting it.

The oracles, and which question each answers:

- **Xenia** — what a frame should look like. For JG.
- **The AC6_recomp bridge** — what the game code does. **Not in this
  workspace** (cycle 1146). There is no `AC6_recomp` binary or source tree
  anywhere under `/fastdata/lavaulta`; only `patches/` survives —
  `ac6_boundary_probe.cpp`, the hook TOMLs, the boundary-probe patches. Earlier
  reports describe it reaching airborne Mission 01 with live input and no fatal,
  its world black because RexGlue's *presentation* fails after the logic. That
  description was carried forward into this ladder's first draft as verified
  state; it was inherited, not checked. Rebuilding it is a prerequisite, not a
  given. When it exists: read a value from it, then derive the native code from
  the Ghidra listing. It is never the source.

  This is the second inherited workspace claim to fall, in the opposite
  direction from the first — cycles 1130–1131 said the retail archives were
  absent when they were present. **A "verified state" table is only verified on
  the day someone runs `ls`.**
