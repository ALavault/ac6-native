# `CSwgCallback`'s constructor zeroes the byte that gates the title's per-tick step, and nothing else was seen to touch it

## Qualification

AC6 demo PAL, same XEX SHA-256. Static evidence: `.build/Default.xex.base.bin`
(RTTI walk) and `recompilation/ace-combat-6-demo/build-codegen-on/codegen/generated/ppc_recomp.4.cpp`
(control-flow evidence only, never copied). Live evidence: a new,
general-purpose watcher (`AC6_DEMO_WATCH_ADDR_LO`/`AC6_DEMO_WATCH_ADDR_HI`,
added this cycle, `src/guest_memory.cpp`), `probe --until frontend
--max-ticks 3600`, START at tick 3000 (correctly timed, per
`AC6_DEMO_START_DURING_TITLE.md`). No oracle.

## What this closes

`20fb6c48` ("The title screen waits on CSwgManager...") found that the
title's per-tick update calls one method on a sub-object at `mode+28`
(a `CSwgManager` instance, confirmed by RTTI), that method (`0x820CE368`)
bails immediately if `[that_instance+4]` is null, and left that value "not
established." A fresh 3600-tick run with `AC6_DEMO_WATCH_MODE_STATE=1`
(pre-existing instrumentation) shows it directly: `field4` (the trace's
name for `[CSwgManager+4]`) is `0x00000000` at tick 222 (construction not
finished yet) and becomes a valid pointer at tick 266 — non-null for the
rest of the run. **`20fb6c48`'s open question is answered: the field is
valid; the early-null-exit is a brief startup window, not the sustained
block.**

## The real gate, one level deeper

The code comment already in `frontend_state_trace.hpp` (from an earlier,
unpublished cycle) names the actual condition: `0x820CE368` "skips its
frame step unless byte `[+236]+9` is set." That sub-object at
`[CSwgManager+4]+236` has vtable `0x820061FC`. RTTI-walked directly against
`.build/Default.xex.base.bin` (`whose_vtable.py` doesn't apply here — its
`analysis/class-map.tsv` is a retail-build artifact with different
addresses; a direct walk of `[vtable-4]` → locator → `+0x0C` → type
descriptor confirms the name):

```
vtable-4 -> locator 0x8206D584 -> typedesc 0x82385890 -> ".?AVCSwgCallback@@"
```

**The object is a `CSwgCallback` instance**, one of the six-class swg family
already named in `demo-render-chain.md` but not previously resolved to a
concrete instance. Its own constructor, `sub_820CD990`
(`ppc_recomp.4.cpp:6111-6127`), is four instructions after the prologue:

```powerpc
lis  r11,-32256    ; r11 = 0x82000000
li   r10,1
addi r11,r11,25084 ; r11 = 0x820061FC (this class's own vtable)
li   r9,0
stb  r10,8(r3)     ; [this+8] = 1
stw  r11,0(r3)     ; install vtable
stb  r9,9(r3)      ; [this+9] = 0   <-- the step flag, unconditional
blr
```

There is no branch. The constructor does not conditionally clear the flag
— it always does, on every construction, with no code path that could set
it to anything else at construction time.

## Live confirmation, not just static reading

Added `watch_addr_range_host_write` (`src/guest_memory.cpp`), a
general-purpose write watcher parameterized by `AC6_DEMO_WATCH_ADDR_LO`/
`AC6_DEMO_WATCH_ADDR_HI` (any base `std::strtoul` accepts), hooked into all
four `store_uN` paths and `store_bytes`. Unlike the fixed IB/frontbuffer
watchers already in the file, this one exists so a one-off address (which
changes every run, since it depends on allocator state) doesn't need a new
hardcoded watcher each time.

Watching `[0x2E3DF0DC, 0x2E3DF0E0)` — the title's own `CSwgCallback`
instance's bytes 8-11, address confirmed live via the same run's
`AC6_SWG`/`AC6_SWGW` trace lines — across the full 3600-tick run:

```
AC6_ADDR_RANGE_HOST_WRITE address=0x2E3DF0DC size=4 value=0xFEFEFEFE ...   (allocator poison)
AC6_ADDR_RANGE_HOST_WRITE address=0x2E3DF0DC size=4 value=0x0        ...   (zero-init)
AC6_ADDR_RANGE_HOST_WRITE address=0x2E3DF0DC size=1 value=0x1        ...   ([this+8] = 1)
AC6_ADDR_RANGE_HOST_WRITE address=0x2E3DF0DD size=1 value=0x0        ...   ([this+9] = 0)
```

Four writes, all at construction (~tick 2429-2452), matching `sub_820CD990`
exactly. **Zero further writes to this byte range for the rest of the run —
including the 600+ ticks after the correctly-timed START press at tick
3000.** This is not a static absence-of-evidence claim; it is a live,
address-confirmed, whole-run negative.

## Conclusion

`0x820CE368`'s per-tick frame step is gated on a byte that this class's own
constructor sets to the disarmed value and that nothing was observed to
touch again, dynamically, across a full run including a correctly-timed
START. This is the same shape as `[0x827AD2F0]` (`03179c5b`/`8fba5b45`): a
single-byte/word gate that stays at its constructed value for the entire
observable run. Whether the two are the same underlying lock or two
independent ones downstream of the same root cause (`CModeTaskGameDemoOffline`
never entered) is not established.

## Not established

- Whether ANY reachable code ever writes a nonzero value to
  `CSwgCallback+9`. An exhaustive static grep for `stb rN,9(rM)` preceded by
  `li rN,1` within 15 lines found ~7 other sites in the codebase
  (`sub_8216C900`, `sub_8218CBD8`, `sub_8219CBF8`, `sub_8227ABD8`, and
  others with an unresolved preceding value), none matching this vtable's
  own slots. Whether any of them operate on a `CSwgCallback` instance, and
  whether any is reachable, was not checked — this needs the same
  atlas-based exhaustive reachability pass `[0x827AD2F0]` got, not done
  here.
- What the other five `CSwgCallback` vtable slots (`0x820CDF30`,
  `0x8220E428` ×2, `0x820CE078`, `0x822CCB30`) do; only checked each for a
  direct `stb ...,9(...)` in its own body, not traced further.
- Whether `CSwgCallback+8` (constant `1` the whole run, per every
  `AC6_SWGW cb8=0x01` line already in the trace) is load-bearing for
  anything, or just an unrelated flag living next to the one that matters.

## Gates

Demo ctest 26/26, native gate JF=pass, both contract audits pass. New code
(`watch_addr_range_host_write`) is opt-in (behind two env vars, both unset
by default) and adds no behavior change to any existing path.
