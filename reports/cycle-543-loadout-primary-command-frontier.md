# Cycle 543 — loadout primary-command frontier

> Superseded by cycle 544. The numerical overlap between events 200–208 and
> the ids passed by `sub_822FA098`/`sub_822FA748` was misleading: RTTI and the
> unique owner write qualify `level_root+0x36054` as `CX360EffectManager`.
> Those functions are effect-side and are not the aircraft-selection event
> producer. The state-machine and temporal corrections in this report remain
> valid; its “Primary command path” interpretation does not.

Date: 2026-08-02

## Qualification

- Target: Xbox 360 PAL `default.xex`.
- XEX SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Canonical Ghidra project: `ghidra-projects/ace-combat-6`.
- Native worktree HEAD: `6c417820fd6a89aade3cb02c53ddf4f222b81991`.
- Generated C++ was read only as revision-pinned literal control-flow/ABI
  cross-match evidence; generated output was not edited.
- Cycle-543 profile is isolated and both experimental loadout force options
  are disabled.

## Correction to cycles 539–542

The cycle-539 vtable interpretation is invalidated.

- `0x82064264` is the primary vtable of
  `CAce6MissionManagerCampaign`.
- `0x8206432C` is its secondary vtable at complete-object offset `0x348`, but
  it contains only one entry before the next complete-object locator. It does
  not extend to `sub_82199D08`.
- `sub_82199D08` is virtual slot 3 of vtable `0x82064384`, whose qualified
  TypeDescriptor is `CModeTaskGame`. `sub_82199BD8` constructs that task and
  `sub_82199D08` registers callback `sub_82199F68` on its `+0x268` registry.
- `CModeTaskGame`, its campaign MissionManager and embedded ArmsManagers are
  downstream of loadout acceptance. Their absence cannot explain the current
  loadout panel or the stalled state-1 screen.

Canonical factory selection confirms the order:

```text
sub_8218C238 state 1
  -> state 4 after accepted loadout
  -> virtual slot +0x48 selector 1
  -> sub_8218D020 selects sub_821BBEF8
  -> CModeTaskLoading
  -> sub_821A7A70 selects sub_821BBF98
  -> CModeTaskGame
  -> campaign MissionManager and ArmsManagers
```

Cycles 540–542 observe none of those downstream constructors. This is the
expected consequence of the loadout state machine remaining in state 1, not
an upstream MissionManager-registration defect.

## Qualified active state-1 contract

The state object is `0xB8E70000`. Its inline subobject at decimal offset 624
(`+0x270`) is exactly the active loadout manager `0xB8E70270`, with vtable
`0x8205DAEC`.

Canonical program-endian vtable words and instruction dumps establish:

```text
vslot +0x60 -> sub_82144F98 -> manager+35984 (u32 state)
vslot +0x6C -> sub_82144FC8 -> manager+35994 (u8 ready)
vslot +0x70 -> sub_82144FD8 -> manager+35995 (u8 status)
vslot +0x8C -> sub_82146DB8 -> loadout update
```

Literal instructions at `0x8218C634..0x8218C74C` execute this sequence:

1. update through slot `+0x8C`;
2. read ready through `+0x6C` and choose which UI object receives slot
   `+0x20`;
3. read status through `+0x70`; when nonzero, require event-list entry 49,
   call the outer state object's slot `+0x48` with selector 0 and enter state
   2;
4. otherwise read the state word through `+0x60`; value 5 enters outer state
   4.

The old force path writes arbitrary byte value 5 to `manager+35995`. Native
code only writes value 1 on the qualified negative resource-result arm. The
state-word setter `sub_8214B5F0` remains the native owner of `manager+35984`;
cycle 543 again observes only requests for value 0.

## Primary command path

The command-object family has two adjacent qualified vtables:

- `0x82007310`, slot `+0x7C` -> `sub_822FA098`;
- `0x820073B0`, slot `+0x7C` -> `sub_822FA748`.

`sub_82221830` constructs the first `0x600`-byte object and installs vtable
`0x82007310`. It is present at slot 26 (`+0x68`) of both qualified
`CAce6ArmsManager`/`CX360ArmsManager` vtables. The already qualified
`sub_82221A28` is their shared slot 38 and constructs the second object.

The distinction matters: `sub_822FA748` emits events 202–208, while
`sub_822FA098` can emit events 200/201. Manager event arm 200
(`sub_8214D44C`) is the qualified arm that, from manager state 0 and with
`manager+35990 == 0`, calls manager vslot `+0xAC` (`sub_8214B978`) with
argument 1. That path can request state 4 when the active selection reaches
its terminal condition.

Cycle 543 adds bounded logging-only probes for `sub_82221830` and
`sub_822FA098`. Neither executes. The older slot-38 constructor and
202–208 translator also remain absent. This proves that no qualified command
object publishes event 200 on the observed route; it does not yet prove which
upstream UI owner should instantiate or replace that path.

## Runtime control and artifacts

Accepted recipe:
`scripts/ac6-first-mission-loadout-native-ready-probe.steps`.
The runtime was bounded to 215 seconds and terminated by the harness after the
final capture; this is not a runtime crash.

Observed control:

- outer state object remains `state=1`, `ticks=3`;
- manager state, ready and status remain zero;
- `sub_8214B5F0` receives two value-0 requests;
- `sub_8214D390` receives event 22 twice and no event 200–208;
- no primary/secondary command constructor or translator log occurs;
- the capability axes and aircraft model render, while the value polygon and
  weapon quantities remain empty.

Artifacts under `reports/logs/cycle-543-primary-command-frontier/`:

- binary: `ae2d515a7a77eda1194600ba756ba9c52618f15e2d67a9c207167ea7f624adcb`;
- logs: `db5c4b4b...`, `26b6cc88...`, `516ba964...`, `80edd182...`;
- aircraft capture: `585d655a3454d9ad3eaa04d051d7918262bfb659d9836b1ecaf949c9cb8f1218`;
- final capture: `3951c9bf9226a2634e62e7404659059e1175bfc66540aef61febe4a82d4b1543`.

The reference build succeeds, targeted AC6 CTest passes 6/6 and
`git diff --check` is required before the next replay.

## Next bounded checkpoint

Trace the live aircraft/loadout UI object that consumes the final A edge and
identify its qualified command/event target. Determine whether it should emit
event 200 directly or instantiate the `0x82007310` command object through a
different pre-game owner. Join that producer to `manager+35984` before any
repair. Acceptance is a natural nonzero manager transition plus populated
capability/weapon data with both force options disabled. Do not synthesize
event 200, construct an ArmsManager from a probe or activate the downstream
`CModeTaskGame` chain early.
