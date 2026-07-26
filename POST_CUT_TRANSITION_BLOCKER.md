# AC6 post-CUT transition blocker

Date: 2026-07-15

## Closed native boundary

`ac6-scene-shell --campaign-selector 1 ... --play-to-completion` now has a
strictly bounded native terminal path:

1. campaign selector `1` resolves to `DPL::[0x9,0]`;
2. the direct low-id request selects physical `DATA.TBL` entry `9`;
3. the decoded entry-9 payload supplies the selected structural Scene group;
4. the group replay validates `CutStart`, per-frame lifecycle and
   `CutTerminate` before presentation;
5. the native Scene session moves from `scene_ready`, through `scene_playing`,
   to `scene_complete` only when its decoded player reaches the final camera
   sample.

The terminal state is deliberately a local CUT completion.  It is not called
mission completion, a following scene, player spawn, flight control, score or
campaign progress.

## Why no post-CUT game transition is implemented

There is no recovered consumer which maps the CUT's `CutTerminate` event, the
native player's final sample, or the completed DPL object to a campaign mode,
mission group, player object or flight receiver.

The exact evidence boundaries are:

| Available evidence | What it proves | What it does not prove |
| --- | --- | --- |
| `0x820a85e0 -> 0x821d1128` | selector `1` queues physical entry `9` | that a retail campaign UI chooses selector `1`, or that entry `9` activates Scene group `22.1.0` |
| NFIC event dictionary | a selected CUT has a terminated frame lifecycle | an event after `CutTerminate` that starts a mission or selects another CUT |
| `CModeTaskTitleMovie` at `0x821B9048` | auxiliary ready/skip/complete handling, owner state `+0x0c = 2`, and a publication to the shared mode owner | that the publication is campaign-specific, or that it carries a selector/mission payload |
| `0x8214C038` | selected-aircraft presentation-state update | a player-aircraft handoff, camera receiver, or flight control state |

The title publication is now bounded more precisely.  At expiry it writes
`owner + 0x18 = 1`, `owner + 0x1c = 3`, then, if `owner + 8` is non-null,
invokes that object's virtual slot `+0x20` with argument `3`.  The same exact
`{1,3}` write plus virtual call occurs in unrelated mode-task functions
`0x821ADE00`, `0x821B04D0`, and `0x8219A378`.  It is therefore a shared mode
signal, not evidence of title-to-mission progression.

The shared owner itself is initialized by `0x821BCA08`: its vtable is
`0x82065AC4`, its `+8` receiver field is initially null, and its runtime base
is stored in `DAT_8293BA10` at `0x821BCBC0`.  The static image does not yet
identify the later assignment to that runtime `+8` field, the receiver's
vtable/type, or the implementation of its `+0x20(3)` method.  No recovered
edge from any of those elements reaches `0x820A85E0`, DPL entry `9`, NFIC
`CutTerminate`, a mission group, or a flight/player object.

This pass also checked every recovered function with a direct reference to
`DAT_8293BA10` for `stw` stores at displacement `+8`.  The apparent candidates
at `0x821B5180` and `0x821B5330` are not writes through the global owner:
their base registers resolve to local `this + 0x20910` and a separately loaded
object respectively.  The global-owner load in the latter is used only to
read `owner + 0x84`.  Thus the candidate scan identifies no direct
`DAT_8293BA10 -> owner + 8` assignment.  This is a bounded negative result,
not a proof that no dynamic assignment exists: the remaining setter can pass
the owner through an argument, virtual call, or otherwise untyped pointer.

The candidate scan is retained as
`scripts/FindGlobalFieldWrites.java`, with its raw results in
`reports/receiver-global-owner-field8-candidates.log`; the two rejected
register traces are in `reports/receiver-field8-candidate-821b5180.log` and
`reports/receiver-field8-candidate-821b5330.log`.

## Follow-up pointer-flow audit (2026-07-15)

The direct-reference scan has now been supplemented by
`scripts/FindGlobalPointerFieldStores.java`.  For every function that directly
references `DAT_8293BA10`, it seeds a candidate only at a `lwz` whose reference
targets that exact global, preserves only an explicit `or rD,rS,rS` register
copy, and reports a subsequent `stw ..., +0x8(candidate)`.  Its raw run is
`reports/receiver-global-owner-pointer-field8-candidates.log`; it completed
without a `CANDIDATE` record.

This is stronger negative evidence than searching for an arbitrary `stw +8`
in a referencing function: neither a direct owner-pointer load nor its simple
register copy reaches such a store in the recovered static image.  It is not a
proof that the field is immutable.  The pointer can still pass through a call,
stack/local memory, a virtual method, or an unanalyzed module before a setter
writes it.

The initializer caller at `0x821D9C54` was also rechecked.  It conditionally
constructs the owner at `0x821BCA08`, then calls `0x821BE718(owner)` and uses
owner field `+0x44`; it does not provide a `+8` receiver value or a
post-CUT/CUT-termination edge.  The decompilation and instruction evidence is
retained in `reports/receiver-owner-constructor-caller-821d9c54.log` and
`reports/receiver-owner-constructor-caller-821d9c54-asm.log`.

In particular, the first structural Scene group is archive order only. Making
it automatically precede a post-CUT group would fabricate campaign order.
Likewise, treating the shared title-mode signal as campaign selection would
fabricate its receiver type and payload.

The data-sidecar check reaches the same bounded conclusion independently:
every `NFICCUT`/resource-FHM/Scene-table triplet only joins local path record
`i` to local resource member `i`.  It has no successor-group or completion
consumer field.  The exact evidence is recorded in
`ENTRY9_SIDECAR_TRANSITION_BOUNDARY.md`.

## Shared receiver-dispatch audit (2026-07-15)

The direct `DAT_8293BA10 + 8` audit is now complemented by an exact
owner-to-receiver-to-vtable trace. Seven direct static sites load the owner,
load `owner + 8`, load the receiver vptr and virtual slot `+0x20`, then call
it with `r4 = 3`. Each site makes the same preceding `{owner + 0x18 = 1,
owner + 0x1c = 3}` publication. This strengthens the classification as a
shared mode signal rather than identifying a title-to-mission edge: the
receiver remains a nullable runtime pointer and no vtable or target method is
statically recovered.

The NFIC dispatcher follow-up likewise has only its local wrapper as a direct
static caller. That wrapper forwards three runtime values and supplies no
selector, resource request, title owner or mission object. Details and raw
addresses are retained in `SHARED_MODE_OWNER_DISPATCH_AUDIT.md`.

## Canonical owner-flow revalidation (2026-07-18)

The preceding receiver audit was re-run against the canonical PAL project,
not `ace-combat-6-corrected`.  The canonical image contains the owner
publication at `0x821b95c0`:

```text
stw r30,-0x45f0(r11)  ; DAT_8293BA10 <- r30
```

The surrounding instruction island beginning at `0x821b9408` initializes the
same owner vtable `0x82065ac4` and explicitly sets `owner+0x8 = 0` before the
global publication.  The exact constructor boundary is still not recovered
as a named Ghidra function.

`TraceGlobalReceiverDispatches.java` now finds 13 canonical sites, including
the seven previously seen in the historical project.  All load `owner+0x8`,
load the receiver vtable slot `+0x20`, and call it with `r4 = 3` after the
same `{owner+0x18 = 1, owner+0x1c = 3}` publication.  This confirms the
shared-mode idiom but does not identify the receiver type or a campaign edge.

The canonical `FindGlobalPointerFieldStores.java` and the broader
`TraceGlobalOwnerFlow.java` produce no direct `owner+0x8` store.  The apparent
`stw ...,0x8(r3)` sites remain writes through unrelated local/service
pointers.  `ReferencesTo.java` finds the owner publication as the only direct
write to `DAT_8293BA10`.

This is a stronger canonical negative result, not proof that a runtime setter
does not exist: the value may cross a call, virtual method, stack/local copy,
or an untyped instruction island.  Details are in
`reports/cycle-206-canonical-owner-receiver-flow.md`.

## Canonical selector-origin revalidation (2026-07-18)

Cycle 207 revalidated the selector path against the same canonical PAL image:

- `0x821a6048` increments current-level through `0x82196508` when runtime state
  is `1`; a stored level of `0` therefore produces selector `1`.  It also emits
  event `0x109` and publishes a service at `owner+0x10`, but has no FHM, DPL,
  Scene-group or flight-object identifier.
- `0x821b6258` handles event `0x259`, forwards values `0..7` to the current-
  level setter, and advances a record state in a `0xa7e0`-stride table.  Its
  data references do not establish a campaign mapping.
- `0x820a85e0` consumes current-level through `0x821b6e58`, giving the bounded
  route `selector 1 -> DPL id 9 -> DATA.TBL entry 9`.  No static edge to a
  particular Scene group, `CutTerminate`, or a flight object was recovered.

This closes selector origin and DPL consumption only.  It does not close the
post-CUT transition; keep the native boundary at `scene_complete` until a
canonical `CutTerminate` consumer or a post-DPL Scene activation is proven.
Details are in `reports/cycle-207-canonical-selector-origin.md`.

## Canonical NFIC termination revalidation (2026-07-18)

Cycle 208 rechecked the NFIC terminator on the canonical PAL image.  The code
tests tag `0x8004` at `0x8236a510` and `0x8236a550`; the helper at `0x8236a548`
has direct callers `0x8236a5dc`, `0x8236a600` and `0x8236a6c4`, plus a raw
branch from `0x8236a708`.  These sites detect and iterate the end of the NFIC
record stream, but carry no typed mission selector, DPL request or player/flight
object.

The canonical chunk constructor around `0x8236ad70..0x8236adc4` composes the
`0x3000/0x3021/0x3022/0x3031/0x3040` parsers.  Its `0x8236a720` parser uses a
dynamic virtual slot `+0x04`; no edge to current-level, `DAT_8293BA10`, a
Scene successor or flight was recovered.  A whole-text structural scan found
no `lwz [vptr] / lwz +0x20 / mtspr CTR` triple in the NFIC zone, so this path
must not be identified with the shared mode receiver without further proof.

The exact consumer of `CutTerminate` remains open.  Keep the native boundary
at `scene_complete`; no human action is needed for this static limitation.
Details are in `reports/cycle-208-canonical-nfic-termination-flow.md`.

## Exact next recovery targets

One of the following joins is required before extending the executable beyond
`scene_complete`:

1. trace the later assignment to the shared owner receiver at `DAT_8293BA10 +
   8`, recover its concrete vtable and its virtual `+0x20(3)` implementation,
   then determine whether it emits a campaign selector;
2. trace the caller/consumer that creates selector `1` and prove its campaign
   mission identifier and selected Scene-group identity;
3. recover a consumer of NFIC `CutTerminate` that proves a post-CUT mode or
   resource request.

Until one edge closes, the executable must remain on its completed CUT rather
than silently starting a second group or a synthetic flight loop.

## Canonical DPL resource-loader boundary (2026-07-18)

Cycle 209 followed the canonical route immediately after current-level
resolution. `0x820a85e0` reads the current level, maps selector `1` to DPL id
`9`, formats `DPL:[#x,#x]` through `0x821d1060`, resolves the resource through
`0x821d2fc0`, and attaches the result with `0x8228e988`.

The next dispatcher is a Resource Manager method: `0x821d1128` is referenced
from the method table at `0x82067b90`, adjacent to other resource methods, and
the same data zone contains `Resource Manager:%s`. Its state machine indexes
resource tables and writes resource-object fields; it does not consume NFIC
`CutTerminate`, select a Scene group, or publish a flight/player object.
The resolver `0x821d2fc0` itself walks a keyed provider chain and
`0x82234dd0` only reads a bounded table entry, reinforcing the resource-only
classification.
`0x8228e988` only binds relative pointers at offsets `0`, `4`, and `8`.

This closes the classification of the DPL edge as resource loading, not the
post-CUT transition. The remaining joins are unchanged: recover the later
owner receiver assignment/implementation, prove the campaign mapping of
selector `1`, or recover a typed `CutTerminate` consumer. Keep the native
boundary at `scene_complete`; no human action is needed for this static result.
Details: `reports/cycle-209-canonical-dpl-resource-loader.md`.

## Canonical unit-manager resource/object pipeline (cycle 210)

La revalidation du projet PAL canonique confirme le manager RTTI à
`0x82055190`/`0x8268f288` (`.?AVCX360UnitManager@@`) et son chemin
`0x820a7070 -> 0x820a85e0`. Après le chargement du premier enfant DPL, le
manager parcourt 94 éléments et appelle les services `+0x18/+0x1c` avec le
type `0x98`. Le chemin `0x820a7a08 -> 0x820a8678` résout ensuite des
ressources DPL secondaires et écrit notamment `objet+0x15c`; les branches
`0x820a793c -> 0x820a8bb8` et `0x820a8120 -> 0x820a8e08` restent des chemins
de construction sans type métier récupéré.

Cette preuve resserre la frontière sur l'assemblage de ressources/unités. Elle
ne relie pas encore le pipeline à une mission, à un groupe Scene, à
`CutTerminate`, au receiver `DAT_8293BA10+8` ou à un objet de vol. Le prochain
travail peut rester statique; aucune action humaine n'est requise. AC6 reste
`native-partial`, frontière `scene_complete`.

Détails :
`reports/cycle-210-canonical-unit-manager-resource-pipeline.md`.

## Canonical motion/resource separation (cycle 211)

Cycle 211 separates two plausible but distinct routes on the canonical PAL
image. The vtable at `0x8205cd90` is the `CX360MotionRequestManager` family;
its slot `+0x08` (`0x8211bcd8`) dispatches record tags `0x11` and `0x8181`, and
`0x82136100`/`0x821371d8` forward object fields `+0x1a4/+0x1a8` into it. A raw
producer block at `0x82127eb0..0x82127f30` fills those fields and calls
`0x82136168`, but its containing function and concrete object type remain
unrecovered.

The separate consumers of `+0x15c` perform resource lifecycle, record scanning,
property lookup or array aggregation (`0x82226c20`, `0x8228e9e8`,
`0x8228fc80`, `0x82293d08`, `0x82374590`, `0x82374978`). Offset reuse does not
establish an identity with the unit manager, a mission object or an aircraft.

No edge was recovered from either family to the shared receiver
`DAT_8293BA10+8`, the campaign mapping for selector `1`, a typed `CutTerminate`
consumer, or a flight/camera owner. Keep the native boundary at
`scene_complete`; this is a static evidence boundary and does not currently
require a human action or runtime session.

## Canonical motion-record producer boundary (cycle 212)

Cycle 212 traced the direct callers of the record normalizers. `0x82118a50`
accepts tagged records: `0x11` delegates to `0x82339718`, while `0x8181`
materializes a relative table at `record+0x1c`, normalizes its stride-`0x20`
entries and sets the high-bit marker at `record+0x18`. `0x8211bd50` reads the
normalized table or delegates tag `0x11` to `0x82339508`.

The four raw call sites `0x82128c90`, `0x8212a100`, `0x8212a23c` and
`0x8212a598` first resolve `entry+0xd0` through `0x8212c020`, store the
result into local object fields `+0x14`, `+0x1c`, `+0x3c` or `+0x24`, then call
`0x82118a50` and set `0x826948c0 = 1`. They are resource-record assembly
sites; no mission or aircraft identity is present in the recovered contract.

The raw producer `0x82127f30 -> 0x82136168` remains a separate unknown object
initialization path. It writes `+0x18c/+0x190/+0x194/+0x1a0/+0x1a4/+0x1a8`,
then invokes `0x82136168`; the containing function, dynamic vtables and type
are not recovered. No edge to the owner receiver, selector-1 campaign
mapping, typed `CutTerminate` consumer or flight/camera owner was found.

Keep `scene_complete` as the native boundary. This is still a static
`needs-types` limitation; no human action or runtime session is required.
Details: `reports/cycle-212-motion-record-producer-boundary.md`.
