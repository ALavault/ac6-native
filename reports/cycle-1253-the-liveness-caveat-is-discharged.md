# Cycle 1253 — the liveness caveat is discharged

Cycles 1250, 1251 and 1252 each closed by saying the placement's last steps were
**structure, not reachability**, and that *a true positive from dead code would
look exactly like this*. Naming a caveat three times is the pattern this session
has repeatedly had to correct. It is discharged.

## The missing link, read here

The initial state `0x82297B20` dispatches three ways:

```
82297b30  cmpwi cr6,r4,-0x3
82297b34  beq   cr6,0x82297d34    ; ENTER
82297b38  cmpwi cr6,r4,-0x2
82297b3c  beq   cr6,0x82297b7c    ; UPDATE
82297b40  cmpwi cr6,r4,-0x1       ; EXIT, inline
```

and `bl 0x82296e40` — the order executor — sits at **`0x82297CFC`**, which is
between `0x82297B7C` and `0x82297D34`. **It is in the UPDATE arm.**

## The chain is live

| step | status | established by |
|---|---|---|
| FSM installed on each leader with children | **reachability** | 1247 |
| the tick delivers `−2` to the current state | **reachability** | 1247 |
| the `−2` arm calls `0x82296E40` | **read here** | 1253 |
| `0x82296E40` calls `0x822A23D8` at `82296fac`, tag < 1 | read | 1206 |
| `0x822A23D8` pushes `0x7D1`/`0x7D4` over the child array | read | 1244 |
| `0x8229C920`'s `0x7D4` case calls `0x8229ADF8` **unconditionally** | read | 1244 |

Steps 8 and 9 of cycle 1251's table are therefore reached from a step 7 that
cycle 1247 proved live. **The placement runs**, and the caveat three cycles
carried was one instruction's reading away from being lifted.

## The guards, which are what "live" actually means here

Not unconditional — each hop carries a condition already read:

- `[leader+0xDC] > 0` — no children, no FSM (`822980f8`, cycle 1247);
- the order's tag byte `< 1` for the `0x822A23D8` arm (cycle 1206);
- `[child+0x118]` bit `0x2` selects `0x7D1` versus `0x7D4`, and **both reach the
  placement** (cycle 1244), so that bit changes which message, not whether.

So: **for a Set leader with children, whose first order is tag 0, the placement
executes on the first tick after the mission load.** That is the whole claim, and
every clause in it is read.

## What remains, and it is smaller than it was

- **Set 0 as the player's Set** — convergent, not derived, unchanged since cycle
  1206. This is now the *only* thing between the derivation and applying the
  transform in the product.
- The leaf class among the six `0x138`-byte vtables.
- `[unit+0x118] & 0x2` at construction, which decides first-push versus repeat.

## For the product

Cycle 1244's instruction stands and its reason is now complete: **do not apply
the transform at load** — but the honest alternative, *applied at first update*,
is no longer a hypothesis about how retail might work. It is what retail does,
with every hop read and the two reachability hops proved.

Task 13 is updated. The change itself still waits on Set 0.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
0x82297B20's three arms and the call at 0x82297CFC read here
```

No product code changed.
