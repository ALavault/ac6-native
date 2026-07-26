# DATA.TBL entry 9 gameplay unit-owner chain

Date: 2026-07-15

## Result

The object that owns the entry-9 construction loop is no longer anonymous.
Microsoft RTTI identifies its concrete vtable as `X360UnitManager`, whose class
hierarchy includes `ACE6::CAce6UnitManager`. This is a typed gameplay/unit
owner rather than a title, menu, or graphics-state object.

The evidence also closes the first transform-bearing object boundary after
mission-resource loading: the manager's virtual factory returns a runtime
object, the loader clears three consecutive floats at object offsets
`+0x50/+0x54/+0x58`, initializes nearby vector blocks, and later attaches the
selected type-`0x98` MDLP resource at `+0x15c`.

This report deliberately does not rename address-named functions. It also does
not call the returned object an aircraft or its zero vector a spawn position:
the concrete returned-object RTTI and upstream field semantics remain open.

## Exact RTTI chain

In the retail/static address convention, the loader owner's vtable starts at
`0x82055190`. The preceding word at `0x8205518c` points to complete-object
locator `0x8206d024`. Its type descriptor is `0x8268f288`; the decorated type
name at descriptor `+8` is `.?AVCX360UnitManager@@`.

The hierarchy descriptor at `0x8206d038` contains three class descriptors. Its
self descriptor at `0x8206d058` refers to `0x8268f288`, while descriptor
`0x8206d34c` refers to type descriptor `0x8268f450`, whose decorated name is
`.?AVCAce6UnitManager@ACE6@@`. The hierarchy therefore proves that
`X360UnitManager` derives from `ACE6::CAce6UnitManager`.

The base vtable begins at `0x82054f14`. Function `0x82273880` installs that
vtable, clears 256 consecutive four-byte slots from object `+0x04` through
`+0x400`, clears `+0x404/+0x408/+0x40c`, then calls `0x82273430` and
`0x82297968`. Static initialization at `0x821a6cb8..0x821a6cd4` invokes this
base constructor and overwrites the vtable with `0x82055190`; the same pattern
constructs the adjacent instance at `0x821a6cd8..0x821a6ce0`.

The 256 cleared slots establish an exact manager-owned pointer/word table. Its
element subtype and every auxiliary field remain neutral pending their own
consumer analysis.

## Entry-9 resource-to-object path

Retail `0x820a7070` obtains DPL child 1 through owner virtual slot `+0x0c`,
whose concrete entry is `0x820a85e0`. It exposes the 94-element MDLP directory,
makes two complete type-`0x98` registration passes, then iterates mission unit
source records.

For each accepted source record, the loop invokes manager virtual slots
`+0x10` or `+0x14`. The returned pointer becomes the constructed runtime
object. At `0x820a77a0..0x820a77c8`, the retail code derives object `+0x10` and
stores the same zero float at object `+0x50`, `+0x54`, and `+0x58`. The
following VMX instruction block initializes or copies nearby vector-aligned
storage around `+0x10/+0x20/+0x60`; exact lane and matrix semantics are not yet
assigned.

Later source bytes `+0x61/+0x62` select one or two child-1 MDLP elements. The
type-`0x98` service result is stored at constructed-object `+0x15c`. Other
observed object fields include `+0xb4`, `+0xc4`, `+0x118`, `+0x11c`, and
`+0x170..+0x188`, but none receive speculative names here.

The corrected Ghidra project places the same loader body at `0x820aa670`, a
`+0x3600` code relocation relative to the retail convention. It independently
shows the virtual factory return at `0x820aad90`, the three stores at
`0x820aadbc/0x820aadc0/0x820aadc8`, and the later resource read from object
`+0x15c` at `0x820ab1b8`. Static vtable and RTTI addresses retain the retail
address convention, so reports must state which code mapping they use.

## Claim boundary and next front

Proved:

- entry-9 child 1 feeds a typed `X360UnitManager` / `CAce6UnitManager` owner;
- its base constructor clears an exact 256-slot unit table and three trailing
  words;
- its factory-created runtime object contains a three-float vector initialized
  to zero and receives a selected MDLP resource at `+0x15c`;
- the path is gameplay/unit construction, not an interface screen.

Open:

- the concrete RTTI/vtable of each factory-returned subtype;
- which source type codes correspond specifically to player aircraft, AI
  aircraft, ground units, cameras, or other actors;
- whether `+0x50` is position, translation, velocity, or another vector;
- the exact VMX transform layout and the first mission spawn pose.

## Factory subtype reduction and native boundary

Owner slot `+0x10`, retail `0x820a7f48`, switches on selector values 1 through
6. The first two cases now have complete final-vtable RTTI:

- selector 1 calls `0x822a6560`, which installs vtable `0x820568d4`. Its COL
  at `0x8206e2a4` names `ACE6::CAce6UnitPlayer` and includes base
  `ACE6::CAce6Unit`;
- selector 2 calls the same constructor, then overwrites the vtable with
  `0x82056934`. Its COL at `0x8206e254` names
  `ACE6::CAce6UnitOtherPlayer` and includes the player/unit hierarchy.

Selectors 3 through 6 reach different constructors, but this tranche does not
yet have a complete final-vtable RTTI chain for them. Asset text elsewhere in
the image contains `MissionAircraft`, and user-facing strings describe other
players as aircraft, but neither text occurrence is an executable ownership
edge from these factories. Consequently the native API
`function_820a7f48_factory_evidence` reports only the two exact RTTI classes
and fails closed for all other selectors. Its `aircraft_proved` and
`spawn_object_proved` fields remain false.

The apparent record handoff at `0x820a7a10` is not a player spawn setter. It
calls vtable slot `+0x50`; for both proved final vtables, that slot resolves to
`0x822ddbe8`, a single `blr`. Thus the record callback is an exact no-op for
selectors 1 and 2. The native evidence reports this fact separately, while
leaving spawn and position unproved.

Selectors 3 through 6 are now bounded by their exact final vtables even though
they have no usable Microsoft COL immediately before those tables:

- selector 3 calls `0x822a8570`, ending at vtable `0x82009ab0`;
- selectors 4, 5 and 6 call `0x820a8e08` / `0x822986b0`, ending at vtable
  `0x82009440`;
- all four use slot-`+0x50` callback `0x82297a40`. That body does not consume
  its `r4` record argument; it inspects object `+0xe0`, conditionally sets a
  flag at `+0x60`, and initializes object `+0xf0`.

Thus owner slot `+0x10` is completely bounded for selectors 1 through 6, but
only selectors 1 and 2 have class RTTI. None proves `MissionAircraft`, a spawn
object, or a position write.

## Owner slot +0x14

Retail `0x820a8138` has direct selector cases and a 15-row first-match table.
Direct selector 2 constructs a `0x250`-byte object ending at vtable
`0x820094b8`, callback `0x822fd0c8`. Direct selector 3 constructs a
`0x3a0`-byte object ending at vtable `0x82009800`, callback `0x8228f678`.
Selector 1 has two runtime-global-dependent allocation paths and consequently
remains rejected by the single-result native API. Selector 4 searches keys
0 through 14 in the table initialized at `0x820a8164..0x820a8304`.

The keyed table resolves to these exact `(key, wrapper, constructor, size,
vtable, callback, metadata)` rows:

```text
0, 820a8e98, 8228f6b0, 340, 82008f58, 8228f678, 4
1, 820a8f28, 822a2f10, 230, 82009658, 822a30f0, 5
2, 820a8fb8, 822a67b8, 230, 82009960, 822a6c48, a
3, 820a8fb8, 822a67b8, 230, 82009960, 822a6c48, 6
4, 820a8fb8, 822a67b8, 230, 82009960, 822a6c48, 8
5, 820a8fb8, 822a67b8, 230, 82009960, 822a6c48, 7
6, 820a8fb8, 822a67b8, 230, 82009960, 822a6c48, 9
7, 820a9048, 822a9fe8, 230, 82009d60, 82225850, 2
8, 820a90d8, 8229a470, 200, 820078d0, 82225850, d
9, 820a90d8, 8229a470, 200, 820078d0, 82225850, e
a, 820a9168, 822aa558, 200, 82009e98, 82225850, f
b, 820a90d8, 8229a470, 200, 820078d0, 82225850, 10
c, 820a8fb8, 822a67b8, 230, 82009960, 822a6c48, 11
d, 820a91f8, 822a87c8, 3a0, 82009c28, 822a6c48, 6
e, 820a9288, 82294110, 360, 820092c8, 8228f678, 4
```

No listed vtable has a proved RTTI COL. Callback `0x82225850` only stores the
record pointer at object `+0x180`; `0x8228f678`, `0x822a30f0` and `0x822a6c48`
also bind `+0x180` and copy selected nested pointers. `0x822fd0c8` binds
`+0x180` and copies record `+8` to object `+0x210`. None of these callbacks
writes the transform block or establishes spawn semantics.

The native APIs reproduce the direct selectors 2/3 and all 15 keyed rows.
Every row has `rtti_proved`, `aircraft_proved`, `spawn_object_proved`, and
`position_proved` set false. Unknown keys, selector 1's dynamic branch and an
unkeyed selector 4 fail closed.

Likewise, `function_820a7070_initial_vector_evidence` exposes the exact zero
vector at object `+0x50`, but returns an `unknown` semantic with both
`position_proved` and `spawn_position_proved` false. This prevents the native
port from silently turning a layout observation into a spawn contract.

## Post-factory insertion and frame traversal

The first retail consumer after object construction is now closed. At
`0x820a7b00..0x820a7b08`, loader `0x820a7070` passes each non-null factory
result to `0x8226f050`. That function rejects null objects and counts above
`0x3ff`, appends the pointer at `collection + 0x8 + count * 4`, then increments
the count at collection `+0x4`. It also queries object virtual slots `+0x54`,
`+0x58`, `+0x64`, and `+0x40`: the first two may store the object at collection
`+0x1008/+0x100c`, while slot `+0x40` supplies an index into the 16-pointer
table beginning at collection `+0x1010`. These are exact control and storage
edges; the virtual-slot meanings remain unnamed.

The corresponding retail per-frame traversal is `0x8226ecb0`. Function
`0x8226a310` loads the same global collection and calls it at `0x8226a508`,
passing the frame scalar in `f1`. The traversal loops over collection count
`+0x4` and entries beginning at `+0x8`. For each non-null object it requires
either `(object[+0x118] & 0x22) == 0x22` or
`(object[+0x118] & 0x402) == 0x402`.

The guarded body queries virtual slot `+0x54`. On that branch it calls
`0x8222ccd0`, `0x8222b740`, and `0x82227378`, passing object-relative arguments
`+0x10`, `+0x24f8`, and `+0x254c`, respectively. The alternate branch queries
slots `+0x5c/+0x60` and can invoke slot `+0xcc` with the frame scalar. This
proves a frame consumer of the exact pointers inserted after entry-9 loading.
It does not prove that the objects are `MissionAircraft`, that any call is a
spawn operation, or that the four words read from object `+0x50..+0x5c` by
nearby traversal `0x8226f230` are a position vector. The `MissionAircraft`
occurrence remains text without an executable reference into this chain.

There is one exact position-semantic edge outside the traversal. Retail
`0x822f31e8` loads the object pointer at global collection `+0x1008` and passes
its floats `+0x50/+0x54/+0x58` to a formatter with string `0x8200eb54`,
`X=%g Y=%g Z=%g`. This proves XYZ for that selected global object. It does not
yet prove that a particular entry-9 factory result occupies `+0x1008`.

The slot-`+0x54` predicate is now instruction-exact. `0x8226f050` calls the
slot with the object pointer in `r3`, truncates the returned `r3` to its low
byte, and stores the object at `+0x1008` iff that byte is nonzero. For primary
selectors 1 and 2, slot `+0x54` targets `0x822ddbe8`, which is a single `blr`:
the return therefore preserves the object pointer. Their allocation requests
only `0x10` alignment from a dynamic arena, so static analysis cannot determine
whether `(object_address & 0xff)` is zero at runtime. The `0x100` object stride
preserves that unknown residue between successive allocations; it does not
resolve it.

For selectors 3 through 6, slot `+0x54` targets `0x822974c8`. With object flag
`+0x60 & 0x100` clear, the observed path likewise preserves the object pointer
and retains the same runtime-address blocker. Entry-9 callback `0x82297a40`
can arm that bit and install callback `0x82297540`; on this specifically proved
armed-record path, `0x822974c8` clears the bit, invokes the callback chain, and
the installed callback exits with `r3 = 0`, so the object is not selected at
`+0x1008` by that call. This result is deliberately limited to the known
callback state and is not generalized to every conceivable bit-set state.

Consequently, no static identity from a particular entry-9 result to the XYZ
object is proved. The entry-9 initial-zero vector and spawn-position claims
remain false; the debug formatter alone is never treated as spawn evidence.

### Complete selected-pointer writer inventory

An executable-wide retail scan of direct `0x1008/0x100c` operands, followed by
layout and value-flow filtering, finds no additional object-pointer writer for
this collection. The complete direct-store table is:

| Routine | Store site | Field | Stored value | Exact guard |
| --- | --- | --- | --- | --- |
| `0x8226eb88` | `0x8226eb98` | `+0x1008` | zero | unconditional initialization |
| `0x8226eb88` | `0x8226eb9c` | `+0x100c` | zero | unconditional initialization |
| `0x8226f050` | `0x8226f0a8` | `+0x1008` | input object | slot `+0x54` returned low byte nonzero |
| `0x8226f050` | `0x8226f0d0` | `+0x100c` | input object | slot `+0x54` low byte zero, then slot `+0x58` low byte nonzero |

The first routine also zeros count `+0x4`, all 1024 entries from `+0x8`, and
the 16-pointer table from `+0x1010`, which fixes its identity as the same
collection layout. Other literal stores reported by the raw offset scan belong
to different layouts: `0x8221e6c0` initializes 512 pointer/status pairs;
`0x82214750` and the range ending at `0x82215b94` update integer counters and
compact a separate structure; the `0x820902c0/0x82090320/0x82094140` stores at
`+0x100c` manage an allocation field. `0x82265f00` takes an address at
`param+0x1008` for its own spatial table and only reads the global collection.
None is an alternate writer of the selected-object fields.

### Native recompilation-assisted collection boundary

The native collection translation now includes the exact bounded
`Function_8226EDE8` table operation. When the `0x8226f050` auxiliary branch is
selected, the virtual-slot `+0x40` result supplies the index for collection
`+0x1010 + index * 4`; the native representation exposes exactly sixteen
pointer slots and rejects absent or out-of-range host indices rather than
creating an unbounded table. Constructor initialization clears that table with
the entries and selected-pointer fields. This is a runtime storage/control
translation only: it does not assign a unit subtype or gameplay meaning to the
slot predicate or table contents.

Thus the only nonzero writer remains `0x8226f050`, reached directly from the
entry-9 loader at `0x820a7b08`. This proves that its input is a factory result,
but the address-dependent virtual predicate still prevents proving that any
particular result reaches `+0x1008`. No other retail writer closes the
factory-result-to-XYZ identity, and none supplies spawn evidence.

Native evidence functions `function_8226f050_insertion_evidence` and
`function_8226ecb0_traversal_evidence` therefore expose only addresses,
offsets, masks, capacity, and virtual slots. Their MissionAircraft, aircraft,
spawn, and position claims remain false.
`function_8226f050_slot_54_evidence` records the exact per-selector return
behavior, low-byte test, dynamic alignment blocker, and known armed-callback
zero path while keeping selected-object identity and spawn-position false.
`function_8226_selected_pointer_writers_evidence` records the four complete
direct writer rows and their exact low-byte guards.
`function_822f31e8_xyz_evidence` separately records the proved XYZ label while
keeping entry-9 identity, MissionAircraft, and spawn-position false.

## Native bounded collection transcription

The collection reset portion of `0x8226eb88` and the directly visible control
flow of `0x8226f050` are now native code in
`reconstruction/ace-combat-6/src/unit_factory.cpp`. The host-neutral
`Function8226f050RuntimeCollection` retains the exact `0x400`-entry capacity,
the count-before-append return index, the two selected-object fields, and the
append order. `Function8226f050VirtualResults` explicitly supplies the two
separate slot-`+0x54` low-byte results plus slots `+0x58` and `+0x64`, because
their object-specific virtual dispatch is not yet a native runtime backend.

When the retail branch would invoke opaque `Function_8226EDE8`, the native
result only reports that auxiliary registration was requested; it does not
invent that callee. Unit tests cover reset, primary and secondary selection,
the independent second slot-`+0x54` branch, null rejection, and the exact
`0x400` capacity rejection. This is a collection/control-flow transcription,
not an aircraft, spawn, transform, or frame-simulation claim.

The next useful static tranche is the meaning of the frame traversal's virtual
slots and its three dispatch cases, correlated with source-record type and the
known MDLP aircraft indices. The first case now has an exact bounded state
write at `+0x830..+0x86c`, while the other two cases are a runtime service
forward and a nested ten-entry enumerator; none has a proved flight semantic.
Because the record callback is a no-op for the two player RTTI cases, their
actual initial pose must still be proved in a later activation/write path
rather than assigned to `0x820a7a10`. See
`reports/cycle-100-unit-frame-state-cases.md`.

## Reusable factory-object method family

The qualified vtables now have one compact, target-qualified method dossier
usable by later transcription work:

| selector | final vtable | slot `+0x50` | slot `+0x54` | slot `+0x54` behavior |
|---:|---:|---:|---:|---|
| 1 | `0x820568d4` (`CAce6UnitPlayer`) | `0x822ddbe8` | `0x822ddbe8` | preserves incoming object pointer |
| 2 | `0x82056934` (`CAce6UnitOtherPlayer`) | `0x822ddbe8` | `0x822ddbe8` | preserves incoming object pointer |
| 3 | `0x82009ab0` | `0x82297a40` | `0x822974c8` | flag-gated preserve-or-zero |
| 4–6 | `0x82009440` | `0x82297a40` | `0x822974c8` | flag-gated preserve-or-zero |

Slot `+0x50` is the post-factory record callback. For selectors 1 and 2 its
target is the proven `blr`; for selectors 3–6 the callback ignores the record
argument while binding the object-specific state described above. Slot `+0x54`
is the predicate consumed by `0x8226f050` and is therefore kept separate from
the record callback even when both slots share a target.

This is a reusable method family, not a complete C++ class. The native
`function_820a7f48_virtual_method_evidence` API records these exact targets
without assigning aircraft, spawn or position semantics. Its tests also fail
closed for selectors outside 1–6.

## Evidence

- `reports/820a7070-range.log`: retail loader body;
- `reports/820aa760-type98-consumers.log`: corrected-project relocated body;
- `reports/logs/entry9-gameplay-owner-rtti.log`: RTTI, vtables, constructor and
  static initialization;
- `reports/logs/entry9-unit-object-transform.log`: corrected factory return,
  scalar-vector stores and object resource field.
- `reports/logs/entry9-frame-consumer.log`: retail insertion and frame
  traversal excerpts.
