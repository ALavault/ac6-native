# `0x8217C4D8` is a `CModeTaskGame*` init-time callback, and it's unreached

## Qualification

AC6 demo PAL, `Default.xex` `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`. Static evidence: `codegen/generated/ppc_recomp.14.cpp` (`sub_8217C678`, `sub_82173DF0`), `tools/whose_vtable.py` against `codegen/xex-basefile.bin`. Reachability control: the same 2286-function `buttons16` atlas used in `03179c5b`/`8fba5b45`.

## What `AC6_DEMO_ATLAS_NAMES_THE_TWO_SLOTS.md` left open

That report traced the `CX360UnitManager` construction cone up to three functions with no direct caller and no vtable-slot literal found, `0x8217C4D8` among them, and logged it as "Non établi: table de saut ou entrée intérieure, non tranché."

## What it actually is

`0x8217C4D8` is neither. `sub_8217C678` composes it as data — `lis r10,-32232` / `addi r10,r10,-15144` reconstructs `0x8217C4D8` exactly — builds a 16-byte value `{0x8217C4D8, 0, 0, 0}` on its own stack, and passes it to `sub_82173DF0(this = arg+104, delegate)`. `sub_82173DF0` stores that 16-byte value into `this+8..+20` — a delegate/callback member assignment, not a call and not a jump-table entry. That is why neither a direct-call grep nor a vtable-literal grep found it: it is registered as data into an object field, one level removed from both.

`whose_vtable.py` on `sub_8217C678` itself:

```
0x8217C678  slot +0x0C, all eight of:
  CModeTaskGame, CModeTaskGameOnline, CModeTaskGameReplay,
  CModeTaskGameTutorial, CModeTaskGameDemoOffline,
  CModeTaskGameReplayMission, CModeTaskGameTutorial_FromMissionMenu,
  CModeTaskGameTutorial_CampaignMission
```

`sub_8217C678` is the shared slot-`+0x0C` method across the entire `CModeTaskGame*` family — the demo's own member is `CModeTaskGameDemoOffline`. Whatever slot `+0x0C` means (constructor-adjacent init, given it runs unconditionally and only stores fixed data), entering any `CModeTaskGame*` mode task calls it, and it registers `0x8217C4D8` as that instance's callback.

## The negative result this sharpens

Neither `sub_8217C678` nor `0x8217C4D8` appear in the reachability atlas. This is not new by itself — `CX360UnitManager`'s construction was already known unreached — but it is independent corroboration by a completely different code path (RTTI vtable membership of a *sibling* slot, not the mission-manager constructor chain this campaign has been following). Both point at the same fact from different directions: **the mode manager never enters `CModeTaskGameDemoOffline`** — the title never hands off to the "in mission" mode task family at all, consistent with `AC6_DEMO_START_SUPPRESSES_THE_ATTRACT_ADVANCE.md`'s finding that the film stops calling the title's own state-2 advance after START.

## Still open

- What slot `+0x0C` is named (constructor, `onEnter`, or something else) — not determined here, only that it is common to the whole family and unconditional.
- What `0x8217C4D8` itself would do if invoked — not read in this pass.
- The actual trigger that would enter `CModeTaskGameDemoOffline` in the first place, which is the same open question as everywhere else in this thread.

## Gates

No source changed; report-only commit.
