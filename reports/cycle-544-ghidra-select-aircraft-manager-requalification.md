# Cycle 544 — Ghidra requalification of aircraft selection

Date: 2026-08-02

## Qualification

- Target: Xbox 360 PAL `default.xex`.
- XEX SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Canonical project: `ghidra-projects/ace-combat-6`.
- Ghidra 12.1.2 was run read-only with analysis disabled; generated C++ was
  used only for revision-pinned literal ABI/control-flow cross-checks.
- Native worktree HEAD: `6c417820fd6a89aade3cb02c53ddf4f222b81991`.

## Corrected object identities

The pointer published at `level_root+0x36054` is not a loadout event target.
`ClassifyPpcOffsetUses.java` finds its unique writer in the function beginning
at `0x82194A10`: it allocates `0x1125420` bytes, calls constructor
`sub_820B1DF0`, then publishes the result. That constructor installs vtable
`0x8205834C`; its qualified TypeDescriptor is exactly
`.?AVCX360EffectManager@@`.

Consequently, `sub_822FA098` and `sub_822FA748` send effect ids through
`CX360EffectManager`. Their values 200–208 only overlap numerically with the
aircraft-selection event table. They do not prove an ArmsManager or loadout
command path. The two cycle-543 probes derived from that interpretation were
removed; the older effect-side probes were relabelled.

The active object at `0xB8E70270` is exact:

```text
object                         0xB8E70270
vtable                         0x8205DAEC
CompleteObjectLocator          0x82074164
TypeDescriptor                 0x826E8D0C
type                           CSelectAircraftManager
base descriptor               0x82071A88
base TypeDescriptor            0x826E74DC
base type                      CSwgListener
CSwgListener standalone vtable 0x8205A1CC
```

`sub_8214D390` is vtable slot `+0x04`, therefore the observed event callback is
the real `CSelectAircraftManager::CSwgListener` override.

## Owners and construction

Raw PPC address-materialization scanning recovers both constructor sites that
Ghidra's listing references missed:

- `sub_82144FE8` installs vtable `0x8205DAEC` and constructs
  `CSelectAircraftManager`;
- `Function_8218BE70` constructs it at `this+0x270` inside
  `CModeTaskAircraftSelect` (RTTI TypeDescriptor `0x826E9DD8`);
- factory `Function_821BBD08` allocates `0x1D9B0` bytes before calling that
  owner constructor;
- `Function_821A56E8` is the alternate `CModeTaskHangar` owner and constructs
  another selector at `this+0x20790`; its factory allocates `0x3DE90` bytes.

The active task address `0xB8E70000` plus `0x270` equals the observed manager
address. This closes the runtime/static identity join without inferred names.

## Registration and real message path

The manager is not missing from the listener registry. Literal flow inside
`Function_8218C238` registers two listeners in the 16-entry table at
`0x8293B800`:

```text
0x8218C3E8  candidate = task + 0x268
0x8218C41C  first-free-slot <- candidate
0x8218C448  candidate = task + 0x270
0x8218C450  next-free-slot <- candidate
```

The same function immediately calls manager vslot `+0x84`
(`sub_821461C0`) and then vslot `+0x68` (`sub_82144FB8`) with `task+0x1C`.
Destructor/leave method `sub_8218CD80` removes both registrations. Thus the
manager lifecycle and its initial setup execute before the stalled panel.

Cycle-544 added only the caller LR to the existing bounded event probe. An
isolated replay with both force options false observed twice:

```text
lr=0x820F63B4 manager=0xB8E70270 event=22
state=0 ready=0 status=0 -> unchanged
```

Ghidra shows `0x820F63B4` immediately after the indirect call in
`Function_820F6330`. This function iterates `0x8293B800` and calls vslot
`+0x04` on every registered listener. Its pointer at `0x82694AB4` belongs to
the named export table `SendMsgV`. Helper `sub_820F62B0` accepts a four-byte
message beginning with `M`, copies the following three characters and converts
them to the integer event id. Event 22 therefore comes from the `Mddd` message
contract (equivalent numeric payload `M022`), not from an effect manager.

Exact ASCII searches find the seven `SendMsgV*` export names in the XEX but no
`M200`, `M201`, `M202` or `M022` literal. This is consistent with message text
being supplied by external SWG/UI content; it is an inference, not yet a
qualified asset mapping.

## Runtime control and artifacts

The accepted native-readiness recipe completed its final capture and the
harness then terminated the owned process; exit 130 is not a guest crash.

- force readiness: false;
- force launch: false;
- manager state/ready/status: `0/0/0`;
- delivered messages: event 22 twice, both from `0x820F63B4`;
- binary SHA-256:
  `5b37f799e9e1e146be8a26d82f67cf0073d1af45a61d949b2cfe74e6ddb2baea`;
- primary log SHA-256:
  `354bfd503773e5c76f3b0fb31a02a7271185a5117feb429ef5903787f8be6633`;
- aircraft capture SHA-256:
  `b6a3aa3e2c6caf0c3d124f9c236d101e5cb45e6a42172c48cecccaff09318a60`;
- final capture SHA-256:
  `832dbc42889c196f7f9f4b948a06d9196b6b719aa62a4d629f20b584fc062309`.

Artifacts: `reports/logs/cycle-544-select-aircraft-listener/`.

Validation:

- `cmake --build build-rt -j2`: pass;
- targeted AC6 CTest: 6/6 pass;
- native and workspace `git diff --check`: pass.

## New bounded frontier

The missing boundary is upstream of `CSelectAircraftManager` event handling,
but it is not listener registration, `CModeTaskGame`, ArmsManager or
`CX360EffectManager`. Trace the SWG/UI invocation that supplies `SendMsgV` and
explain why the aircraft-selection route emits only event 22 instead of the
manager's selection/acceptance messages. In parallel, join the already executed
`sub_821461C0` setup to the aircraft-stat records used by the capability
polygon and weapon quantities. Accept a repair only when natural publication
populates those values and produces a nonzero manager transition with both
force options disabled.

Do not synthesize event 200, force a readiness/status byte, construct an
ArmsManager early or treat effect ids as UI events.
