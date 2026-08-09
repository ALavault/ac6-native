# Cycle 1402 — the check before the work

## Qualification

- **No Ghidra run and no oracle pass.** The class map and the corpus.
- No product C++ changed; ctest stays 47. **No contract entry.**
- `analysis/flight/command-caller-search.tsv` extended.

## One command instead of a cycle

`0x821405F8` — 281 instructions, no callers, a target from `[+3032]`, and the
candidate cycle 1401 called "the first whose shape suggests the right side of the
boundary".

It is slot **+0x68 of `CDebriefingManager`**. The debriefing screen. `[+3032]` is
debriefing data and its offset-48 dispatch is on some other object entirely.

**Refuted before reading a line**, by running `whose_vtable.py` first.

That is worth marking. Cycle 1383 was caught not checking what the repository
already answers; cycle 1385 found a named class by finally checking, and called
it the thread's first anchor. This is the first time in the thread the check came
**before** the work rather than after the mistake — and it cost one command
against the 281-instruction read I had planned.

## The same lookup across all eight

| function | class |
|---|---|
| `0x821222B0` | `CNuSound` slot +0x20 — refuted at 1399 |
| `0x821405F8` | `CDebriefingManager` slot +0x68 — refuted here |
| the other six | **not in any vtable** — direct-call functions |

So **both candidates the class map can name are refuted**, and the six it cannot
are direct-call functions in `0x8229xxxx` and `0x8223xxxx`. Three of those six
are the auto-level subsystem.

There is a pattern in that, and it is not encouraging for the search: the two
that RTTI could identify were the two that had nothing to do with flight. The
steering code, like everything else in this thread, is in the unnamed part of the
binary.

## Where the search stands

**Unread: two.** `0x822911E8` (538 instructions, four callers, `f2` from a field
and from a computed value) and `0x822389D8` (589, mixed). Those are the search.

Four cycles of searching, no caller found, five refutations, and a population
narrowed from 78/104/127 dispatches to two functions. That is progress by
elimination and it is slower than the flight thread's, for the reason cycle 1401
named: every filter has to be built from something other than the dispatch.

The demo's invented link is unchanged and its header still says so.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.3 | 6 (1395, 1396, 1399–1402) |
| implementation/integration spent on A3.3 | 2 (1397, 1398) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 25 behaviours
ctest                                 100% passed, 0 failed out of 47
tools/tests                           Ran 77 tests, OK
```

## Next

`0x822911E8`: 538 instructions, four callers, and the only remaining candidate
whose `f2` is ever a **computed** value rather than a field or a constant. A
stick-derived command is computed. If it is not the caller, `0x822389D8` is the
last of the eight, and the honest conclusion would then be that the caller is
not reachable from the setters — that the search has to start from the input side
instead, where the contracted binding layer's outputs go.
