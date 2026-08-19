# Closing `7513c9dc`: the single remaining frame-table entry is type 4,
# dispatching once per tick to `sub_82326420` through the parallel layer —
# not a no-op, and fully explaining the enqueue stop

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. Five short live probes (`--until frontend
--max-ticks 3600`, `634cff33`'s START tuple, neutral store,
`AC6_DEMO_WATCH_ADDR_LO/HI` brackets as listed below) plus static reads of
the generated dispatch code (`sub_82323BB8`
`ppc_recomp.43.cpp:16291-16720`, `sub_823239F0` `:16019-16088`,
`sub_82326608` `ppc_recomp.44.cpp:4211-4278`, `sub_823266F8`
`:4351-4470+`, `sub_82326420` `:3910-4134`) and the four jump tables
(outer `0x8264CE6C`, `0x8264CE54`, `0x8264D07C`, `0x8264D094`, all
cross-checked between `.build/Default.xex.base.bin` and fresh
`Ac6Bytes.java` Ghidra dumps).

## The question `7513c9dc`/`2ce5c350` left open

After the START press, the owner's frame-element table is swapped to a
single entry at `0x2DD6A854` (`00 0B 96 20 00 00 00 01`), and the enqueue
handler (`sub_82322A80`, outer-table slot 6) stops firing. `2ce5c350`
corrected the entry's `+4` word to a loop count (`1`) and pinned the type
selector to `[cursor+0]` where `cursor = entry.word0 + [[owner+32]]`,
leaving open: what the type value is, which table slot it resolves to,
and whether the single per-tick dispatch does anything meaningful. `r26`
was then unidentified; this report identifies it statically as
`owner+32` ( `addi r26,r31,32` at `sub_82323BB8` `loc_82323D60`) and
closes the whole chain with live-captured values.

## The full resolved chain, each hop live-captured

1. **Entry** `0x2DD6A854`: word0 `0x000B9620`, word4 `0x00000001`.
   Loader `sub_82278F78`, tick 2436, thread 9 (bracket
   `[0x2DD6A840, 0x2DD6A85C)`).
2. **Cursor**: `cursor = word0 + [[owner+32]]`. Live: `[owner+32] =
   0x2E3E3ADC` (bracket `[0x2E4035F0, 0x2E4035F8)`, written tick 2571,
   re-confirmed tick 3001), `[[owner+32]] = [0x2E3E3ADC] = 0x2DCB1220`
   (derived; consistent with `bfc927e1`'s `table_base`). `cursor =
   0x000B9620 + 0x2DCB1220 = 0x2DD6A840` — live-observed as
   `[owner+248] = 0x2DD6A840` at tick 3599 in the owner bracket run.
3. **Type selector** `[cursor+0] = [0x2DD6A840] = 0x00000004`
   (bytes `00 00 00 04`, bracket `[0x2DD6A840, 0x2DD6A844)`).
4. **Outer dispatch**: table `0x8264CE6C` slot 4 = `0x823239F0`
   (`sub_823239F0`).
5. **`sub_823239F0`'s second-level lookup** (code `:16040-16057`):
   `type2 = [[[owner+32]+56] + [cursor+8]*8]`.
   - `[cursor+8] = [0x2DD6A848] = 0x0000000F` (bytes `00 00 00 0F`).
   - `[[owner+32]+56] = [0x2E3E3ADC+0x38] = [0x2E3E3B14] = 0x2DD7963C`
     (bracket `[0x2E3E3B14, 0x2E3E3B18)`, tick 2451).
   - `type2 = [0x2DD7963C + 0xF*8] = [0x2DD796B4] = 0x00000000`
     (bytes `00 00 00 00`, bracket `[0x2DD796B4, 0x2DD796B8)`).
   - Dispatch `[0x8264CE54 + 0] = 0x00000000` → **NULL** → `beq`, no
     call. Cursor advances by 20 (`[owner+248] = 0x2DD6A854`,
     live at tick 3599).
6. **Parallel layer** (`sub_823266F8`, flag-gated on `[owner+215]`,
   still set post-press): its own cursor2 = same formula = `0x2DD6A840`,
   count `1` from the same entry, type `4` → its table `0x8264D094` slot
   4 = `0x82326608` → `sub_82326608` runs.
7. **`sub_82326608`'s second-level lookup** (same shape, code
   `:4236-4247`): same `type2 = 0` → dispatch `[0x8264D07C + 0] =
   0x82326420` — **non-NULL** → calls `sub_82326420(owner, cursor, 0)`
   (`:4252-4260`), then cursor `+= 20`.
8. **`sub_82326420`** (`:3910-4134`): real per-tick transform work —
   builds its own sub-cursor `r30 = [0x2DD796B8] + [[owner+32]] =
   0x00001C04 + 0x2DCB1220 = 0x2DCB2E24` (bytes `00 00 1C 04` live),
   runs `sub_82323798`/`sub_823227F8`/`sub_823237B8`, applies the
   owner's 320-348 transform matrix (`fmuls`/`fmadds` over
   `[r3+0..28]`), then walks its own sub-list: count `[0x2DCB2E24] = 1`
   (bytes `00 00 00 01`, bracket `[0x2DCB2E24, 0x2DCB2E28)`), first
   element at `0x2DCB2E28`, dispatching each through table `0x8264D074`.

## Reading

The single remaining entry is **type 4** (live), loop count **1**
(live), dispatching **once per tick**. In the outer loop it resolves
through `sub_823239F0`'s own table to a **NULL slot** (cursor-advance
only — this is the "apparent no-op" `7513c9dc` hypothesized), but in the
parallel layer the *same* element resolves through `sub_82326608` to
**`sub_82326420`, which performs real per-tick transform work with a
1-element sub-list**. So the new frame is a genuine content change, not
a dead end: it stops the outer loop's enqueue statement (`type 6 →
`sub_82322A80` never selected) while continuing a real, parallel
transform application each tick. The enqueue stop is explained exactly:
the new frame's only element has type 4, and no table in either layer
maps type 4 to `sub_82322A80`.

## Not established

- `sub_82326420`'s own inner table `0x8264D074` slot contents and what
  its single sub-element (`[0x2DCB2E28]`) dispatches to — a new,
  deeper frontier below the frame-table swap.
- What `sub_82323798`/`sub_823237B8` compute (the transform inputs) and
  where `sub_82326420`'s results go (no live store to a bracketed
  address was observed in this campaign's brackets).
- Whether the `0x8264D074` inner dispatch resolves to a null slot or a
  real call — unread, next step if pursued.

## Gates

Native gate JF, demo `ctest` 26/26, both contract audits: run clean
before this commit. No source change — pure static + probe evidence.