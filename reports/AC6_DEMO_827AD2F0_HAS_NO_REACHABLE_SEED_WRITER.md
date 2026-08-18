# `[0x827AD2F0]`'s only seeding writer is unreached; its reader only resets or chains it

## Qualification

- Same target as `03179c5b`: AC6 demo PAL, `Default.xex`
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`,
  guest project `ghidra-projects/ace-combat-6-demo`.
- Static evidence: `codegen/generated/ppc_recomp.18.cpp` (`sub_821AD378`,
  `sub_820C2CC0` in `ppc_recomp.3.cpp`), exhaustive text search across all
  `ppc_recomp.*.cpp` for both address-composition idioms used on
  `0x827AD1C8`/`0x827AD2F0` (`lis -32133` + `addi -11832` two-step, and the
  single-step `addi +11080` from the same `lis`).
- Reachability control: `analysis/demo/ac6-demo-menu-consumer-reach-ab/...
  /buttons16/buttons16.atlas.json` (2286 functions, same XEX SHA-256,
  `--atlas` full direct+indirect instrument). Cross-checked against three
  functions already confirmed reached in `03179c5b` (`sub_821B9BC8`,
  `sub_821AD378`, `sub_821B94A8`, all present with plausible counts) to
  confirm the atlas is not stale for this route.

## What `03179c5b` left open

That report named `[0x827AD2F0]` as the field that must land in `11..19` for
`sub_821AD378` to write `device+21508`, unblocking the ring doorbell. It did
not check whether anything reachable can ever put it there.

## The exhaustive search

Two address-composition idioms reach `0x827AD2F0` anywhere in the generated
code:

- **Two-step** (`lis r11,-32133` / `addi r30,r11,-11832` / later `+296`):
  present in exactly one function, `sub_821AD378` — confirmed by grepping
  every `ppc_recomp.*.cpp` for the literal `-11832`, which returns only
  `ppc_recomp.18.cpp`.
- **One-step** (`lis r11,-32133` / `addi r4,r11,11080`): present in exactly
  one function, `sub_820C2CC0` (`ppc_recomp.3.cpp:8631-8641`), which tail-calls
  `sub_8219D4E8(r3, r4=&0x827AD2F0, r5)` — a registration-style stub, not a
  direct store.

`sub_820C2CC0` does not appear in the 2286-entry reachability atlas for the
`buttons16` route (nor does its callee `sub_8219D4E8`): **unreached**.

`sub_821AD378` is reached (confirmed in `03179c5b`), and does write
`0x827AD2F0` in 4 places — all inside itself, all downstream of already
having read a value in `11..19`:

- Two paths reset it to `0` (`loc_821AD3F0`, and twice more at
  `loc_821AD62C`/`loc_821AD7xx`-style blocks) once a case has been handled.
- One path (`loc_821AD608`-adjacent, case handling for selector `17`) writes
  it forward to `17` — chaining to a different case on the *next* call,
  not seeding a fresh cycle.

None of `sub_821AD378`'s own writes ever move the selector from `0` into
`11..19`; every write it performs is a continuation or a clear, never a seed.

## Conclusion

Within the current reachable set, **there is no code that can ever start**
this state machine. The one function that could (`sub_820C2CC0`, a
registration/console-command-style stub) is dead in this replay. This is
consistent with, not a new finding about, the standing diagnosis
([[demo-plan-approved-2026-08-17]]): the guest is still idling in
`CModeTaskStartUpDemoOffline`, waiting on XAM notifications that never
arrive, and never reaches whatever gameplay or menu code is meant to issue
this D3D command. This does not newly explain *why* — Phase 1 of the
approved plan (XAM notification FIFO) is what's expected to move the guest
past this state, not further static search on this address.

## What this search does not rule out

The two idioms above are the only ones found by literal-constant grep; a
writer that computes the same address through a **table of pointers**,
**r13-TLS-relative addressing**, or a **different immediate split** (e.g.
`lis` landing on a different page with a larger positive offset) would not
be caught by this search. This is a scoped, not exhaustive-over-all-possible-
codegen, negative result — recorded as such rather than asserted stronger
than it is.

## Gates

No source changed; `git status` after this report shows only the new file
plus the same pre-existing untracked/uncommitted items noted in `03179c5b`.
