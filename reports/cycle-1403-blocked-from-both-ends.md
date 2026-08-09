# Cycle 1403 — blocked from both ends

## Qualification

- **No Ghidra run and no oracle pass.** The corpus and the image.
- No product C++ changed; ctest stays 47. **No contract entry.**
- `analysis/flight/command-caller-search.tsv` extended and closed.

## The last strong candidate is not decidable

`0x822911E8` dispatches slot 14 with `f1 = 1.0` and `f2 = [[r30+672] + 36]` —
genuinely data-driven, unlike the three auto-level functions. Its constants are
**±π/6 and ±π/12** — thirty and fifteen degrees — with 0.97, 0.9, 400.0, 150.0
and 7200.0.

Commanded attitude angles from a table. An AI or navigation repertoire is at
least as good a reading as a stick, and **nothing in the function distinguishes
them**.

## So the search was run from the input side

The contracted input record array is `0x826EDB98`, materialised at nine sites in
nine functions. Forward from all nine, following direct calls, four hops, **128
functions explored: no path to any of the eleven candidates**.

## And that negative is weak, for a reason worth stating

Cycle 1218 measured that `bl` plus non-local `b` reaches **2,144 of 8,135
functions — about a quarter of this program**. The thirteenth shape. Everything
else is reached through vtables, and the input path's own consumer `0x82211C10`
is called through one.

So a four-hop call-graph negative is evidence about the call graph, not about the
program. Reporting it as "not connected" would be the same error as cycle 1370's
"reached through a stored pointer" — reading an empty result from a search that
cannot see the answer.

## The shape of the block

**Both ends are blocked by the same property.** The setters are dispatched
virtually, at slot numbers 78–151 unrelated functions share; the input side
reaches its consumers virtually too. Neither end offers a handle a call-graph
walk can follow, and neither offers a slot number that means anything on its own.

That is why five cycles of search have produced five refutations and no caller.
It is not that the searches were bad — the argument-count refutation at 1401 and
the class-map refutation at 1402 were both cheap and correct. It is that the
question is on the wrong side of the binary's 27% RTTI coverage, in a subsystem
where every handle this campaign has built depends on one of the two.

## The decision

**The search stops here.** The demo's link stays invented and its header says so.
Recorded in the artefact, in order of cost, is what would resume it — and the one
I would bet on is the one nobody has run:

> look for a **second writer** of `[+36]`, `[+40]`, `[+44]` **outside the class
> family**, which cycle 1393's search deliberately excluded.

That search excluded 478 of 487 functions on the grounds that they were not
flight models. If the player's commands are written by something that is not a
flight model — a controller, an input adapter — it is in the 478.

Naming that now, rather than after another five cycles, is the point of stopping.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.3 | 7 (1395, 1396, 1399–1403) |
| implementation/integration spent on A3.3 | 2 (1397, 1398) |

Five of the seven went to this search. That is the honest cost of a question that
turned out to be on the wrong side of the binary's naming, and it is recorded
rather than amortised into a vaguer total.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 25 behaviours
ctest                                 100% passed, 0 failed out of 47
tools/tests                           Ran 77 tests, OK
```

## Next

The 478. `stfs` into `+36`, `+40` or `+44` from any function, filtered by whether
the same function also touches anything the contracted input path produces —
which is a filter on *data*, not on the call graph, and therefore not blocked by
the property that stopped this cycle.
