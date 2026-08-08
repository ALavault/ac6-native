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
ctest --test-dir reconstruction/ace-combat-6/build
```

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
- **NDXR is the only retail format the product decodes.** The `NTXR`, `MATE`
  and `MDLP` parsers described in the root structure reports are **not in this
  repository**; they lived in a prior workspace. One texture profile is
  pixel-decoded, in a Python diagnostic script, out of 7,993 wrappers.

## JV — the visible world

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

   What remains is the player's spawn, and it is **not** in the container:
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
4. **Textures — not a porting job (cycle 1149).** `scripts/probe_ntxr_bc.py`
   reads word 5 of the descriptor as `width = hi16, height = lo16` and word 8 as
   the data offset. `NTXR_STRUCTURE_REPORT.md` explicitly refuses to name those
   fields, and the script's own docstring calls itself diagnostic-only, gated on
   visual proof. Promoting that interpretation into the product is anti-goal 2.

   Cycle 1149 tried to earn the names by measurement over 692 wrappers and
   **failed honestly**. If word 5 held the dimensions, `payload / (hi*lo)` would
   be a clean multiple: it is, for 83%. But shuffling payloads against dimensions
   scores **70.9% mean, 73.3% max over 200 trials** — the baseline is that high
   because both are powers of two. 83% over a 71% floor is signal, not a name.
   (Control: the same measure on word 0 scores 0%.) A second, non-visual
   orientation test — wrong pitch should push tiled addresses past the payload —
   returned **completely null: 148 of 148 non-square wrappers fit both ways**.

   What it needs is the retail consumer. Concrete start: `0x8233EF48` tests
   `0x4E445852` (`NDXR`) and `0x8233EF68` tests `0x47494458` (`GIDX`), same
   shape. **No instruction in the image builds `0x4E54`**, the high half of
   `NTXR` — so NTXR is recognised by container type code, not by signature, and
   the descriptor consumer must be found through the resource system.

   Then, once the descriptor is named: port BC3 + Xenos `Tiled2D` + 8-in-16 into
   C++, and close
   MATE batch→material→texture→NTXR in the runtime.
5. **Terrain.** Currently **fail-closed by policy**:
   `MISSION_VISUAL_BOOTSTRAP_REPORT.md` requires a proved Scene/CUT ownership
   edge before any static environment is drawn. Prove it; do not lift it.
6. **Derived binding — needs a second data source (cycle 1148).** Unit class
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
