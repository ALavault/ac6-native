# Cycle 1251 — the placement chain in one piece, and the seam in it

The chain from the console powering on to a unit's world position now spans
cycles 1213, 1218, 1240, 1244, 1247 and 1250, and **no single document states
it**. Written out, it exposes one seam that none of those cycles had to look at.

## The chain

| # | where | what | cycle |
|---|---|---|---|
| 1 | `0x821F5E90` → `0x821D7D90` → frame loop | one call site per link | 1213 |
| 2 | task pump → `CTaskModeManager` → `CModeTaskAircraftSelect::vf18(1)` → `new CModeTaskLoading` → `vf11` → `vf18(0)` → `new CModeTaskGame` | creator lists, per class | 1240 |
| 3 | `CFsm::SetInitialState` → `0x82199F68` (load-**wait**) → `0x8219A140` | −3 on entry | 1218, 1235 |
| 4 | `0x8219A140` ENTER, `vt[+0x2C]` → `0x8219F8C0` → `0x820A7070` ×3 | **both class families built** | 1218, 1250 |
| 5 | same ENTER arm, `vt[+0x34]` → HSM phase 1 → `0x8226FE30` → `u->vt[+0x38]` = `0x822980C8` | installs state `0x82297B20` when `[leader+0xDC] > 0` | 1247 |
| 6 | per frame: UPDATE → `0x82267450` **tail branch** → `0x822707C8` → `u->vt[+0x3C]` → `0x82294660` | the state gets −2 | 1247 |
| 7 | `0x82297B20` UPDATE walks `[this+0xE0]`, calls `0x82296E40` → `0x822A23D8` | the order executor | 1244, 1247 |
| 8 | `0x822A23D8` writes the Set matrix, then pushes `0x7D1`/`0x7D4` over `[leader+0xD8][i]` | first vs repeat | 1244 |
| 9 | `0x8229C920` case `0x7D4` → `0x8229ADF8` | writes `unit+0xA0` | 1244, 1250 |

Steps 4 and 5 being **two calls in the same ENTER arm, in that order**, is why
the placement is not a load-time write — the finding cycle 1244 proved and cycle
1247 gave a mechanism.

## The seam

Cycle 1250 separated two hierarchies that four reports had called "the unit":

- `r16`, the **`ACE6::CAce6Unit`**, receives `[+0xD8]` (child array), `[+0xDC]`
  (count), `[+0xE0]` (order list) and the order FSM;
- the loop's `r31`, the **`galib::CGaObj`**, receives `[+0x184]`, `[+0x170]`,
  `[+0x118]`, `[+0x188]` and is what `0x8229ADF8` places.

So step 8's leader is a `CAce6Unit` and step 9's placed object is a `CGaObj`.
**The array at `[leader+0xD8]` therefore holds `CGaObj` pointers while its owner
is a `CAce6Unit`** — which is consistent with what was read (`822a28f0` tests
`[r29+0x118]`, and `+0x118` is a `CGaObj` field), but **no cycle established it
as such**, because until cycle 1250 the two were one word.

`820a7c74` stores `&unitPtrArray[idx+2]` into `[r16+0xD8]`, and `820a7b28` reads
the same array for the parent pointer. **One array, two owners, and which family
its elements belong to is now a question that can be asked and has not been.**

I am not answering it here. It is named, it is checkable — the elements' `+0x00`
vtable settles it in one dump — and pretending the chain is seamless because
every individual link was read would be exactly the failure cycle 1250 found in
the prose.

## What the chain does and does not establish

**Established:** every hop above, each with what selects it, and the negative that
matters — the placement does not run at load.

**Not established, and this is the load-bearing caveat:** cycle 1250's separation
is an **identity** finding. It says which class each field belongs to. It does not
say that `0x8229ADF8` runs in Mission 01, and cycle 1250 warned in its own words
that *a true positive from dead code would look exactly like this*. Steps 1–7 are
reachability results; steps 8–9 are structure. **The chain is not proved live end
to end.**

## Not established, stated plainly

- Which family `[leader+0xDC]`'s elements are, above.
- Which of the six `0x138`-byte vtables the loop's `r31` receives — the family is
  established, the leaf class is not.
- Set 0 as the player's Set: still convergent, not derived (cycle 1206).
- Whether `[unit+0x118] & 0x2` is clear at construction, which decides first-push
  versus repeat but not whether the push happens (cycle 1244).

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
```

No product code changed. This cycle adds no reading; its value is that the chain
is in one place and its seam is named rather than smoothed.
