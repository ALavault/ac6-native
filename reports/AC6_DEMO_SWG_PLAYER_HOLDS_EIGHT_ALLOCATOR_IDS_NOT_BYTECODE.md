# The swg "player" object holds 8 allocator-assigned IDs, not compiled bytecode

## Qualification

AC6 demo PAL, `Default.xex` `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`. Live evidence: one-shot header dump added to `frontend_state_trace.hpp` (`AC6_SWG_W224_HEADER`, gated by the existing `AC6_DEMO_WATCH_MODE_STATE`), `probe --until frontend --max-ticks 400`. Static evidence: `codegen/generated/ppc_recomp.17.cpp` (`sub_8219E580`), independently found in the demo's own generated code — not carried over from another build.

## Why this thread was opened

`demo-render-chain.md`'s scoping note left "find where the compiled swg movie/clip data lives" as the open step, after establishing the six `CSwg*` RTTI classes give no obvious bytecode-container candidate.

## The measurement

```
AC6_SWG_W224_HEADER tick=266 addr=0x2E3BFA94:
  82007D0C 2E3C3AD4 FFFFFFFF 00000000
  0E000057 0E000058 0E000059 0E00005A
  0E00005B 0E00005C 0E00005D 0E00005E
  00000000 00000000 00000000 00000000
```

`w224` (the "player" field already traced by the existing `AC6_SWGW`/`AC6_SWG` output) opens with `0x82007D0C` — the `CSwgRenderer` vtable pointer already identified — confirming `w224` *is* the player/renderer instance, not a separate buffer. At `+16` through `+44` sit eight consecutive values, each one more than the last: `0x0E000057` .. `0x0E00005E`. Not bytecode — a small table of sequentially-issued IDs.

## What the ID shape is, verified in this XEX

`sub_8219E580` (`ppc_recomp.17.cpp:28977`) is a rolling-counter allocator: it increments a field at `this+26308`, and when that counter exceeds a threshold in `r9`, resets it to `0x0E000000` (`lis r11,3584` = `234881024` = `0x0E000000`) before continuing. This is the same shape independently, and this allocator's presence and mechanics are confirmed in the **demo's own generated code**, not inferred from another build.

## What the label might be — flagged as a cross-build hypothesis, not proven here

`reports/cycle-1260-the-mode-1-id-and-a-hypothesis-refuted.md` and
`reports/cycle-1268-the-115-copies-are-not-a-collision.md`, from the
reconstruction campaign's retail Mission01 investigation on a *different* XEX,
name a sibling rolling allocator seeded `0x0F000000` as "ids for dynamically
created textures" (function `0x821AEB08` there), and refer to a second
rolling allocator seeded `0x0E000000` in the same breath without naming its
category as precisely. If the same pairing holds in the demo — unverified —
these eight IDs would be handles to eight dynamically created resources the
title screen's renderer registered, most plausibly textures/sprites for its
visual elements, not a compiled animation timeline. This is a hypothesis
carried across builds and is explicitly not qualified evidence for the demo
XEX; only the allocator mechanism above is.

## What this changes

The original framing — "find the swg bytecode format and read it" — looks
like the wrong shape for this specific object. `w224`/player holds a short,
plain resource-ID table, not a data blob a VM would interpret. If the
resource(s) behind these eight IDs are what the title's script is actually
waiting on (e.g. a texture load that never completes), that is a resource-
readiness question, not a bytecode-reading one — a materially smaller and
more tractable investigation than reverse-engineering an in-house bytecode
format from scratch.

## Not established

- Whether `0x0E000000` really means "texture/resource id" in the demo (see
  above — cross-build hypothesis only).
- What consumes these 8 IDs, and whether each one resolves to a valid,
  loaded resource or a dangling one.
- Whether this table is even on the path the film's post-START decision
  depends on, or just describes the renderer's already-working steady state.

## Gates

Demo build succeeds; the header dump is a one-shot, read-only, opt-in trace
addition (gated by the existing `AC6_DEMO_WATCH_MODE_STATE`) with no
behavior change. Full ctest run pending before commit.
