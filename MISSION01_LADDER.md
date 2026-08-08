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

1. **Fuse the halves.** `RetailSession` drives the rasteriser and the present
   path instead of writing a HUD-only PPM.
2. **The transform frame.** Cycle 1142 found the placement chain for the 434
   `Obj` entities; the frame it is relative to (`entity+0x188`) and the placement
   of the 230 units are open. Without this every unit renders at the origin.
3. **A flight camera.** The only qualified camera is the cinematic TCAM, which
   cycle 732 disqualified. The rasteriser's fallback is a hardcoded 60° / far 4096.
4. **Textures.** Port BC3 + Xenos `Tiled2D` + 8-in-16 into C++, then close
   MATE batch→material→texture→NTXR in the runtime.
5. **Terrain.** Currently **fail-closed by policy**:
   `MISSION_VISUAL_BOOTSTRAP_REPORT.md` requires a proved Scene/CUT ownership
   edge before any static environment is drawn. Prove it; do not lift it.
6. **Derived binding.** Unit class byte → object category → model must come from
   retail data — the `+0x15C` MDLP resource pointer each object carries — not a
   hand-written table. This is where JV would quietly become J1 again.

## Evidence discipline

In `CLAUDE.md`, and it is the point of the whole campaign: never assert a value
that was not read; refuse a plausible rule with no control; correct predecessors
**and yourself**, by cycle number; measure the instrument before trusting it.

The oracles, and which question each answers:

- **Xenia** — what a frame should look like. For JG.
- **The AC6_recomp bridge** — what the game code does. It reaches airborne
  Mission 01 with live input and no fatal; its own world is black because
  RexGlue's *presentation* fails after the logic, so its logic and draw
  submissions are observable. Read a value from it, then derive the native code
  from the Ghidra listing. It is never the source.
