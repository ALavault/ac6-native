# AC6 complete native reconstruction plan

## Scope and completion definition

This plan covers `ac6-xbox360-pal`, module `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`,
image base `0x82000000`. It targets an independent native Linux AMD64 product;
Xbox 360 emulation, Xenia, firmware and retail containers remain external
evidence oracles and are not shipped dependencies.

"Complete" means that the product gates in
[`docs/native-port-acceptance.md`](../../docs/native-port-acceptance.md) have
executed, not merely that XenonRecomp emitted C++. In particular, the native
application must boot, select campaign entry 9 through the retail path, load a
mission, present a live frame, accept input, and continue without an emulator.
Generated functions remain `recompiler-generated` until their reachable
behavior is covered by deterministic or differential evidence.

## Stable sources of truth

1. The qualified PAL XEX and its SHA-256.
2. Canonical headless Ghidra project `ghidra-projects/ace-combat-6`.
3. `.tools/ac6-recomp-reference/ac6recomp_config.toml` plus a freshly
   regenerated XenonRecomp/ReXGlue tree; generated output is never edited.
4. Native tests and retail-data smokes under `reconstruction/ace-combat-6`.
5. Bounded Xenia traces only where native/static evidence cannot answer a
   named parity question.

The upstream `sal063/AC6_recomp` checkout is literal control-flow cross-match
evidence only. It cannot override the qualified XEX or headless boundaries.

## Current measured baseline

- The local ReXGlue runtime links on Linux and reaches recompiled PPC code.
- The generated corpus has 50 translation units and 23,321 implementations
  after the qualified boundary removal through cycle 301.
- The extended native AC6 corpus passes 48/48, including generated Xenon
  probes and the bounded Xenia shader-cache retail catalogue.
- The cycle-301 candidate `0x82345228` is confirmed internal and removed.
  The bounded retail smoke closes `0x8234522C -> 0x82345134` and advances to
  `0x8234530C -> 0x8234524C`; later configured entries remain preserved until
  a separate headless contract identifies the exact owner.
- The separate scene shell reaches `scene_complete` for its bounded CUT/Tcam
  slice but is not the full retail runtime.
- Renderer state capture, first material/texture joins and backend draw markers
  exist; retail observation of the host markers remains open.

## Workstreams and gates

| Phase | Workstream | Required output and exit gate |
| --- | --- | --- |
| 0 | Identity and reproducibility | XEX/config/tool revisions and all retail inputs are hash-qualified; codegen and build commands are reproducible; rollback is a TOML/source revert plus regeneration, never a generated-file edit. |
| 1 | Xenon PPC control flow | Audit false configured starts in bounded families, regenerate once per qualified batch, and advance the same retail smoke. Exit when every runtime-reachable direct branch has a generated target and startup has no unresolved-branch fatal. The residual global unresolved inventory is either eliminated or classified unreachable with explicit static evidence. |
| 2 | Xenon instruction and ABI fidelity | Exercise unaligned, byte-reversed, reservation/atomic, VMX128, floating-point, CR/XER, cache-publication and 32-bit guest-pointer behavior with generated-leaf probes. Exit when all instructions reached by the campaign path have an implementation and targeted tests. |
| 3 | xboxkrnl/XAM platform runtime | Inventory every imported ordinal reached by startup/campaign; implement memory, threads, synchronization, time, VFS, input, notifications, locale and overlapped I/O with ABI-qualified handlers. Exit when no reached service is a permissive stub and missing services fail with ordinal/context. |
| 4 | XMA and audio | Recover XMA voice/packet ownership and xboxkrnl/XAudio/XAM crossings; provide deterministic decode/stream scheduling and fixture tests. Exit with audible native mission/title output, stable packet timing, and no opaque emulator dependency. |
| 5 | Retail data and world construction | Complete DATA.TBL/PAC indexing, bounded decompression, resource graph, mission selector 1/entry 9, unit factories, scene/world population and preservation of unknown fields. Exit when the native runtime—not only the diagnostic shell—loads the first campaign world with non-empty proven state. |
| 6 | Xenos shaders and renderer | Inventory every shader reachable in the first mission; translate via XenosRecomp, preserve fetch/constant/endian semantics, and implement render targets, depth, vertex/index/stream state, materials, textures and command submission. Exit with zero missing reachable shader/state handlers and a deterministic frame manifest. |
| 7 | Gameplay runtime | Connect native input to the proven player owner, fixed-step simulation, camera, aircraft, weapons, AI, collision, mission scripts and pause/menu/save transitions. Exit when one first mission segment advances under native keyboard/controller input with stable state. |
| 8 | Differential parity | Define bounded checkpoints for startup, title, mission load and gameplay; compare registers/memory, resource identities, audio events and frame summaries against Xenia. Exit when every acceptance checkpoint is green or carries an explicit bounded deviation approved as a port change. |
| 9 | Product hardening | Linux normal and ASan/UBSan gates, deterministic missing/corrupt-data failures, root install, Windows AMD64 build and later executed startup. Exit only when all five product outcomes and executable gates are recorded. |

## Cross-workstream inventories

The following machine-readable inventories should be maintained rather than
reconstructed from prose:

- configured/generated/real PPC function starts and unresolved branch pairs;
- generated Xenon instruction opcodes with executed probe coverage;
- xboxkrnl/XAM/XMA imports with `implemented`, `fail-closed`, `unreached`, or
  `unknown` status;
- Xenos shader identities, translation results and runtime reachability;
- DATA.TBL/PAC resources with hashes, parent links and parser coverage;
- runtime checkpoints with native and oracle evidence identifiers.

No percentage may combine generated functions with semantically verified
functions. Report both denominators separately.

## Iteration policy

Each cycle must name one new input, one unresolved boundary and one measurable
exit. Boundary fixes are batched when they share a containing function or
failure family. After a qualified config change: regenerate, build with
`-j16`, run the bounded smoke, then run the relevant full CTest corpus. Do not
repeat the same fatal without changed inputs.

Human/VNC sessions remain deferred. A future request must name one target
action, expected artifact and time limit. Xenia may be run autonomously as a
bounded oracle, but is not required for static boundary repair.

## Active tranche

Cycle 302 qualifie `0x82345250` comme coupure interne par **28/28** assertions,
mais son retrait n'est pas accepté : la régénération a fini par
`std::bad_alloc` après avoir vidé partiellement `generated/`. La configuration,
les générés et le runtime du cycle 301 ont été restaurés et reliés. Le front
effectif reste donc `0x8234530C -> 0x8234524C`; le prochain cycle doit rejouer
ce seul retrait avec télémétrie mémoire, puis passer build, smoke et tests avant
toute promotion.

Cycle 301 closed configured `0x82345228`: 27 headless assertions classify it
as the internal third-loop backedge setup in `sub_82345100`. Removing only
that split closes `0x8234522C -> 0x82345134` and advances the retail runtime to
`0x8234530C -> 0x8234524C`.

1. Qualify the exact owner of source `0x8234530C` and internal target
   `0x8234524C`; preserve every configured entry beginning at `0x82345250`
   until its own contract passes.
2. Preserve every ambiguous address; remove only entries proven to be internal
   fallthroughs by headless CFG/prologue/frame evidence.
3. Regenerate once, build with `-j16`, and test whether
   `0x82345214 -> 0x82345144` disappears; preserve every independent hook at
   an internal instruction unless its own evidence warrants a separate change.
4. Run the same bounded Xvfb/GDB retail smoke and record exactly one new
   frontier.
5. Keep the runtime `candidate` until the product gates above execute.
