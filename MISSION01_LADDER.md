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
- **NDXR is the only retail format the product decodes** — and the *product's*
  header layout is still **measured, not derived** (cycle 1189):
  `src/native_geometry_raster.cpp` and `include/ac6/native_geometry.h` cite zero
  retail addresses, and no contract names either as a derivation. It predates
  this session and has never been audited.

  **The derivation now exists to replace it (cycles 1194–1199), in eight stages
  and with no measured format imported:**

  | stage | address | what it does |
  |---|---|---|
  | recognise | `0x8234CA28` | type code = `u16` at `+0x08`; GIDX is a `0x10`-byte header in front of an NDXR |
  | dispatch | `0x8234CB58` | `0x200` → `0x82350CA0` / `0x82350C50` (all 537 retail files are `0x200`) |
  | construct | `0x82350C50/CA0` | vtable `0x8201283C` / `0x820128B4`; size at `this+0x08` |
  | sequence | `0x82352B88` | vtable slots `+0x18`, `+0x10`, `+0x20`; two of the three are `blr` |
  | header | `0x82350F08` | four section extents at `buf+0x10..+0x1C`; body base `buf + [buf+0x10] + 0x30` |
  | records | `0x823556E0` → `0x823555D0` | `u16 [+0x0A]` records of fixed `0x30` at `file+0x30`; relocated in place |
  | streams | `0x82355468` | four stream pointers at `sub+0x10..+0x1C`; stride table `0x82012C40`; `[obj+0x98]` counts |
  | names | `0x82355318` | linked list relocated by `[obj+0x90]`, a string table |

  Controls, each able to fail: `[+0x04] == filesize` and `[+0x08] == 0x200` on
  **537/537**; printable bytes at the derived body end on **537/537** against
  **0/537** for the rival without the `+0x30`; the relocation guard bit clear on
  disk and `sub+0x28` zero on disk on **13,014/13,014**; and every record's
  `rec+0x20` resolving to a printable C string on **13,014/13,014** — the names
  being `mapparts_m01_*`, Mission 01's own map parts.

  **The geometry is located, and the product's old reader was right (cycles
  1212–1213).** `0x82362190`, which `0x823556E0` calls unconditionally as its
  first act, binds **section 1 as a `D3DFMT_INDEX16` index buffer** (Length
  `[buf+0x14]`, base `obj+0x84`) and **section 2 as a vertex buffer** (Length
  `[buf+0x18]`, base `obj+0x88`). Five cycles said "nothing addresses the
  vertices" because the object is **aliased** — everything below `0x82350F08`
  gets `ctx = this + 0x10`, so the bases read as `0x74/0x78(ctx)`.

  The draw at `0x82364518` reads `desc+0x00 >> 1` as StartIndex, `desc+0x04` as a
  byte offset into the vertex block, `desc+0x20` as a count of 16-bit indices,
  and draws triangle strips — **exactly the mapping
  `native_geometry_raster.cpp` has carried unaudited.** The extent control
  settles it: over 179 distinct files and 4,338 descriptors the product's
  `0x0613→32 / 0x0611→28` gives `max(desc+0x04 + stride·count) == [buf+0x18]` in
  **178/179**, against **0/179** for both rivals (20 and 16). Cycle 1198's `0x14`
  was never a vertex stride: `0x82012C40` is materialised once in the image and
  its only use writes `desc+0x28 = table[idx]·count`, a field zero on disk
  everywhere.

  **What remains is runtime state, not file content.** The stride comes from an
  8-byte table at `0x828711F0`, BSS-resident and built at run time, so the
  product's constants are corroborated rather than read.

  **And the path is proven to execute, end to end (cycles 1213, 1218, 1234).**

  | hop | address | what selects it |
  |---|---|---|
  | XEX entry | `0x821F5E90` | — |
  | `main` | `0x821D7D90` | one call site |
  | boot resource mount | `0x821D5EF8` | one call site, unconditional |
  | frame update | `0x821D7A90` | `821d7e34`, every frame |
  | task pump | `0x822AAFC8` | `CAce6TaskManager` vtable `+0x04` |
  | mode manager tick | `0x821B99B8` | registered at `821D6B4C` |
  | mode switch | `0x821BA588` | counter at `this+0x1C` |
  | **creator select** | **`0x821A7A70`** | `CModeTaskLoading::vf18(0)` → `[0x82691B8C]` = `new CModeTaskGame` |
  | register | `0x822AB0F0` | **tail-calls** `vt[+0x0C]` via `bcctr` |
  | FSM enter | `0x82199F68` | `SetInitialState(−3)` |
  | **the load call** | **`8219A1B0`** | `lwz r3,0x288(r30)` / `lwz r11,0x2c(r11)` / `bctrl` |

  The loader itself is chosen by `mode = *([0x826E4EB4] + 0x78)`: **4** → Online
  `0x82097560`, **5** → Replay `0x8219BDD8`, **else** → Campaign `0x8219F8C0`.
  Modes 1, 2 and 3 share the `else` arm, so Campaign and Tutorial are not
  distinguished **by this selector** — a real ambiguity in the binary. Note
  (cycle 1239) that `BeginLoad`'s selector is a **different partition of the same
  word**: it tests 4 then **3**, not 4 then 5, so modes 3 and 5 land on different
  arms of the two. For Mission 01 the mode is normalised to `{1, 2}` by
  `CModeTaskGame`'s base constructor `0x82199BD8`, so both selectors take their
  `else` arm.

  The NDXR loader is reached through vtable slot `+0xEC` of `0x8205C9A4` into
  `0x820FA9C0`, pinned by receiver-offset matching on `+0x29520`, `+0x29130` and
  `+0x35C00`. `0x820FA9C0` **sweeps container members 0…0xFF** (cycle 1215) and
  feeds every one that exists to the loader and to an NTXR mount; Mission 01's
  entry carries 26 members, and members 4–10 are the texture-bearing FHM bundles.

  Two corrections worth carrying: `0x82691AD8` is **not** one indexable creator
  table but a concatenation of per-class successor lists (cycle 1234), and
  `CModeTaskTitle::vf18` accepts only `k ∈ {0,1}` — cycle 1224's "index 44" is
  impossible. **Not established:** what invokes `CModeTaskLoading::vf11`, one
  level above the creator select.

  Two earlier calls, `0x8233EE40` and `0x8233EF88`, are read: they resolve
  texture and shader ids (cycles 1200–1209). The `NTXR`, `MATE`
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
   shared with sixteen ground units, so it is a destination, not a spawn.

   **`0x822A23D8` has since been read, and the picture is settled (cycles 1206,
   1244).** It writes a 4×4 at `leader+0x80` — rotations at `+0x90/A0/B0` via
   `0x822A1E80`, translation at `+0xC0` — and the per-unit `+0x184` record is a
   **formation offset** rotated by that matrix and added, not a position. So
   `world = SetMatrix · desc[+0x184] + SetTranslation`, and `8229ae7c` is the
   only instruction in the image that ever writes `unit+0xA0`.

   **But it does not happen at load, and that is proved rather than suspected.**
   The placement is a parent-to-child push a Set leader performs from inside its
   own order-execution FSM: `0x822A23D8` loops over `[leader+0xD8]` sending
   `0x7D1` or `0x7D4` per child, and `0x8229C920` turns those into
   `bl 0x8229adf8`. The census is complete rather than a lower bound — three
   `li 0x7d1` and three `li 0x7d4` in 851,718 instructions, and **both codes
   occur zero times as data across 11,117,714 bytes** with `0x8229C920` at 7 as
   the control. `0x820A7070` wires every input the push needs, on the unit
   (`+0x184`, `+0x170`, `+0x118`, `+0x188`) and on the leader (`+0xD8`, `+0xDC`,
   `+0xE0`, `+0xE4`), and never fires it.

   **So `initial_world_position` must not be "fixed" to apply this at load.** The
   honest change is *applied at first update*, in the session loop — larger than
   cycle 1182 contemplated. **The product stays unchanged**, now for a proved
   reason rather than a cautious one. Single open hop: what starts the leader's
   FSM (`0x82297540` has zero instruction references).

   What remains is the player's spawn. Cycle 1177 exhausted `PLAD` — both its
   accessors have exactly three call sites each, paired, and every getter reads
   only word 3 — and cycle 1176 found the player also carries no model index,
   where 149 other units do. The mission describes neither the player's aircraft
   nor where it starts, which is consistent with the player choosing one before
   the mission. Three searches of the container have come back empty; the next
   one should be for the code that builds the player from the hangar's choice.

   **Cycle 1254 ran that search and it closed.** The player's unit is selected
   by the Set's **class byte**, `820a72c0 lbz r11,0x8(r11)` on the resolved
   payload, bounded `cmplwi r11,0x4` and dispatched through the table at
   `0x820A72EC`. Class 0 sets `r15 = 1`, which selects factory slot `+0x10` on
   `CX360UnitManager` (`820a7630`/`820a7638`) → `0x820A7F48` → `0x822A6560`,
   which installs vtable `0x820568D4` — RTTI `.?AVCAce6UnitPlayer@ACE6@@`. The
   aircraft comes from the profile via `0x820A8678` (`820a8820 stw r3,0x15c(r23)`),
   never from the container, which is why three container searches found nothing.

   **The Set index is an output of that identification, not an input.** Over
   912 of 912 instructions of `0x820A7070`, `r21` occurs thirteen times and is
   compared twice: once *after* `820a76e8 subi r21,r15,0x1` has overwritten it,
   and once against the loop bound. It is written out at `820a7648
   stw r21,0xd0(r16)` and `820a7a28 stw r10,0x170(r31)`.

   So **"Set 0 is the player's Set" is not the code's rule** — *class byte 0*
   is, and Set 0 satisfies it in 15 of 15 campaign containers. Over 38 scenario
   containers and 4,591 Set records the class byte never leaves the switch's
   `0..4` domain, and four containers carry class-0 records at non-contiguous
   indices, which is the control an index rule cannot survive.
   `(-2025, 1500, 1345)` now rests on a read instruction. **JV 2b unblocks, and
   the change is still "applied at first update", never a load-time write.**

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
5. **Terrain. The Scene/CUT edge is REFUTED (cycle 1205) — the rule stays, and
   now for a stronger reason.** `CDemoDDMapObj` is reached only by
   `82129b24 cmpwi 0x2005`; its vtable `0x8205D774` has one reference in the
   whole image and zero data pointers; and across **all 44 Mission 01 Scene
   groups — 575 records, 2.68 MB — there is not one record of type `0x2005`**.
   The singleton control makes that zero mean something: type `0x3001` occurs
   exactly once in the same data and the parser found it.

   Nor would that branch own a mesh: `0x82129C10` allocates `0x50` bytes, looks
   up an **already-existing** world object via `0x8226F9B8` and toggles a flag on
   its `+0x15C` — the handle `0x820A7070` fills from the two `0x8228E9B8` MDLP
   lookups. And the namespace it could reach holds no geometry: entry 9 carries
   **1,106 `.mop` and zero `.nud`/`.nut`/`.hir`/`.bin`**, while the same scan
   finds 292 NDXR. The 553 named records match the 553 resolved Scene paths with
   no remainder.

   So Mission 01's Scene/CUT system owns `.mop` tracks and nothing else. **The
   four `mapobj_m01` NDXRs stay uninjected** — injecting them would be a
   fabrication with no retail edge behind it. If terrain comes from anywhere it
   is the scenario Obj→MDLP path, which is untouched. Only reopener: the source
   of `*0x8291889C + 0x5174`, published at `0x82185C84`.
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
   rest on 94 of 94 bundles parsing cleanly.

   **CORRECTED (cycle 1248).** The bytes `46 48 4D` do occur **zero times
   anywhere in the loaded image**, code or data — that measurement stands,
   verified with a scanner that finds `NDXR` at `0x8200A24C`, `GIDX` at
   `0x82067EC8` and `NTXR` at `0x82067EC0`. **What was wrong is the conclusion
   drawn from it.** Retail walks the FHM layout through **`0x82234C18`**, a
   directory reader that takes a version byte at `+0x04`, an endian byte at
   `+0x05` and a table offset at `+0x06`, and **never compares a magic**. A
   format is parsed; its name is never looked at, which is exactly why the tag
   scan returns zero.

   Control: `u16[+0x06]` is `0x10` in all **439** extracted `.fhm`, against 1 in
   all 1052 `.ntxr` and 94 in all 3 `.mdlp` — and `0x10` puts the count and the
   offset table exactly where `tools/ac6_fhm.py` reads them. So the layout cycle
   1192 called *measured, not derived* **is derivable**, and two independent
   readings of the same header meet.

   There is still no type registry: `0x82337BD8` is
   `ResourceManager::init(1 MiB, 0x400 resources)` and dispatch at `0x8234CA28` /
   `0x8234CB58` is compiled in over three codes. And cycle 1181's "registry
   entries are twelve bytes each" remains wrong — `0x82342D70` is a byte-size
   query, `n * 0xDC`.

   **And cycle 1246 decides what that means for the product, not just for the
   derivation.** The temptation was to port the measured FHM walk anyway, or to
   ship an extraction-side mapping — the first puts a measured format in the
   product, the second is a manifest, and JF exists because manifests were
   eliminated. Both are wrong for the same reason: **porting an FHM reader would
   be porting something the shipped game does not do.**

   What retail does instead: `0x820A7070` stores the directory result at
   `object+0x15C` as a **handle**, held and flag-toggled (`0x82129D00`,
   `0x82222F80`) and never walked, and resources are resolved **by integer id**
   through the registries — textures in `0x828C8100` keyed by `GIDX+0x08`,
   shaders in `0x828CCB80` — which `0x821D5EF8` fills at boot from the pack
   mounts.

   **So the product's boundary is NDXR bytes in, geometry out**, which is where
   `NdxrContainer` already sits and which is the boundary retail itself has.
   `ModelDirectory` stays: it resolves the index retail resolves, and must not go
   further.

   The real remaining work is therefore the **id path** — and cycle 1248 measured
   it **easier than cycle 1246 said**, not harder. The rebasing bias
   `[0x828C9700+0x08]` is **zero**, written once as a literal in the constructor
   `0x82340A60`, its address materialised at `82335f2c` rather than inferred from
   stride arithmetic. Every extracted `.ntxr` is a **self-describing pack of
   count 1**, and the six `mode = 0` mount sites take each entry's own GIDX id —
   so on that path **pack grouping is irrelevant** and an offline index keys
   exactly as retail does.

   Still open, and the list is **two items, not three**: **duplicate-mount
   policy** (847 duplicates over 205 ids, and a flat extraction does not record
   order) and the `mode = 1` packs whose ids are `base + index`.

   Two corrections to the sentence that stood here:

   - It said the pixels need `0x821FCA48`, the X360 tiler, "which is unported".
     **False, and it was false when written.** `src/ntxr_texture.cpp` already
     untiles Xenos `Tiled2D` (`xenos_tiled_2d_offset`, `pad_to_tile` at 32
     blocks) and decodes BC1/BC2/BC3, under contract behaviour `texture_decode`:
     692 wrappers, 668 decoded (656 BC3, 10 BC1, 2 BC2), 22 refused
     not-block-format, 2 refused cube-map, corpus pixel hash `8a7b59cbf13ba39b`,
     endianness control 468/170/30. The claim came from a delegated
     investigation, was true of what that agent had been given, and was carried
     into the roadmap unchecked. One `grep` refuted it. It is the twentieth
     shape in `INSTRUMENT_DISCIPLINE.md`, and it survived one round of
     correction here because the task list was fixed and this file was not.
   - It said "first mount of an id wins" **and** listed duplicate-mount policy as
     open, in the same sentence. Both cannot be true. The policy is **not
     derived**; the parenthesis was a guess wearing a result's grammar. It is
     removed until an instruction says otherwise.

   Cycle 1181's "registry entries are twelve bytes each" is also wrong:
   `0x82342D70` is a byte-size query, `n * 0xDC`, carved by `0x82342F68` into a
   `0xC0` pool, a `0x10` map and a `0xC` free list — the twelve bytes are one
   array of three.

   What retail walks instead is the **DPL archive layer** — and **cycle 1193
   corrects the shape cycle 1192 gave for it.** `0x8293BA38` / `0x8293BA3C` and
   their `0x44`-byte records are the **dead branch** of a format switch: the byte
   at `0x8293BA18` is set to **2** by `0x821D61F4` just before the table is
   mounted, and all three functions that test it (`0x821CC250` `!= 0`,
   `0x821CC4D0` `== 1`, `0x821CBFD0` `!= 0`) route away from that code.

   The live path: `0x821CC250` reads `sim:DATA.TBL` and publishes
   `*0x8293BA2C = file + 8` as the record base, `[file+0x00]` to `0x8293BA30` and
   `[file+0x04]` to `0x8293BA34`. `0x821CBFD0` indexes it at `821cc1f0` with
   `rlwinm r11,r11,0x4` — **records are 0x10 bytes** — reading `+0x0C` as a size
   rounded up to 2048 and `+0x01` as a u8 selector. The file agrees on its own:
   14,824 bytes, `[+0x00]` = 926, `8 + 926 * 0x10 = 14824` exactly, and `[+0x04]`
   = 2 is the number of PACs `0x821D5EF8` mounts next.

   Open: whether 926 is read as a bound, and the record's `+0x00`/`+0x04`/`+0x08`,
   which `0x821CBFD0` does not touch.

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
