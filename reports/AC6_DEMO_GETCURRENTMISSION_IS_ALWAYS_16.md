# `GetCurrentMission()` always returns 16, before and after START

## Qualification

AC6 demo PAL, same XEX SHA-256. Live trace: `probe --until frontend
--max-ticks 4000`, `AC6_DEMO_WATCH_MODE_STATE=1`, START pressed at
tick 3000-3001 (`651e7878`'s timing). Code change: `frontend_state_trace.hpp`'s
existing `AC6_GAMESTATE` block extended to also read `[sub112+8]` —
`sub_820E9300`'s first gate (`cmpwi cr6,r11,1`; failing it forces
`GetCurrentMission()` to return the sentinel `16`, per
`AC6_DEMO_THE_SCRIPT_VOCABULARY.md`). Demo `ctest` (26/26) passes with the
change.

## What THE_SCRIPT_VOCABULARY.md left open

"Les valeurs rendues à l'exécution [de GetCurrentMode/Mission/Level]... ne
figure[nt] pas ici." This report reads one of the three.

## The measurement

```
AC6_GAMESTATE tick=222 gs=0x82774B00 mode=0 sub112=0x8201DFDC
              sub116=0x82027D88 sub112f8=0x821DFC00
```

One line for the whole 4000-tick run — the key never changes, including
across the tick-3001 press. `[sub112+8] = 0x821DFC00`, not `1`, so
`sub_820E9300`'s first check fails immediately and every subsequent call
returns `0` without reading anything else. `GetCurrentMission()` is therefore
forced to its `16` fallback for the entire run.

## Reading

This does not discriminate pre- from post-START — it is constant. Same shape
as `5dc58584`'s finding for `GetCurrentMode` (also constant, also correctly
so: the title task never writes it). Whether `16` is *also* correct here
(a legitimate "no mission selected, on the title screen" sentinel) or a
genuine gap is not established by this measurement alone — but since it does
not change when the script's behavior changes, it cannot be what makes the
script decline to call `menu_endMode` after START. Recorded and closed;
`0x821DFC00`'s own meaning is not pursued further.

## Gates

Demo `ctest` 26/26, native gate JF=pass. Source change is a read-only trace
addition (opt-in, `AC6_DEMO_WATCH_MODE_STATE`), no behavior change.
