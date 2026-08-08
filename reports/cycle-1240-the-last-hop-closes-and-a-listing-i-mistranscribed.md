# Cycle 1240 — the last hop closes, and a listing I mistranscribed

## The chain is complete

The one hop cycles 1218 and 1234 left open — *what invokes
`CModeTaskLoading::vf11`* — is closed, and the vtable start is **anchored rather
than assumed**, which is what cycle 1226 could not do:

- `0x821A72C0` occurs as a data word at `0x82064A80`; `0x821A7A70` at
  `0x82064A9C`. The `.text` pointer run is bounded by two `.rdata` words, and
  `[0x82064A50]` read as a `RTTICompleteObjectLocator` names
  **`.?AVCModeTaskLoading@@`**.
- So the primary vtable starts at **`0x82064A54`**, `+0x2C` is **slot 11** and
  `+0x48` is **slot 18** — agreeing with cycle 1234, now for a reason.
- Independently confirmed by the constructor `0x821A7260`, which stores
  `0x82064A54` at `+0x00`.

**Slot 11 is the "mode entered" hook.** `CTaskModeManager` calls it at exactly two
sites, immediately after `Register`:

```
821ba974  lwz   r11,0x10(r31)   ; the creator
821ba97c  bctrl
821ba980  stw   r3,0x8(r31)     ; the new mode task
821ba998  bctrl                 ; CAce6TaskManager::Register
821ba9a4  lwz   r11,0x2c(r11)
821ba9ac  bctrl                 ; task->vf11()
```

Walked up — one conditional, both arms converge — and **down**: the only backward
branches in `0x821BA588`–`0x821BA9F8` target an early-return epilogue, so there
is no loop and vf11 runs at most once.

## What puts a `CModeTaskLoading` on the list

`CModeTaskAircraftSelect::vf18(1)`, at `0x8218D020`, whose own two-entry list is
based at `0x82691B7C` with index 1 holding `0x821BBEF8` = `new CModeTaskLoading`.

**The control is the best of the session**, because it attacks the exact shape
that killed cycle 1224. The creator table is a concatenation of per-class
sub-lists, so a *neighbouring* class whose window overruns index `0x82691B80`
would break the identification. Nine bases in the region were enumerated and each
read for its own guard:

| base | index needed | guard | verdict |
|---|---|---|---|
| `0x82691B7C` | **1** | `cmpwi r4,3` | **reaches it** |
| `0x82691B58` | 10 | `cmpwi r4,9` | impossible |
| `0x82691B50` | 12 | boolean, `cmpwi 2` | impossible |
| `0x82691B4C` | 13 | `cmpwi r4,1` | impossible |
| `0x82691B08` | 30 | 17 literal sites, max index 16 | impossible |
| `0x82691AF0` | 36 | `cmpwi r4,1` | impossible |
| `0x82691AE8` | 38 | `cmpwi r4,2` | impossible |
| `0x82691ADC` | 41 | `cmpwi r30,2` | impossible |
| `0x82691A00` | 96 | different array | wrong table |

**Five of the nine needed only a wider bound to make the answer non-unique. None
had one.** Zero survivors would have refuted the reading; several would have made
it useless. One survivor, and it is the class the semantics predict.

And the loop closes: `CModeTaskLoading`'s update arms the manager's transition
(`mgr+0x18 = 1`, `mgr+0x1C = 3`), three frames later the manager calls
`[mgr+0x10]` — which `vf11 → vf18(0)` set to `new CModeTaskGame` — and
`Register`'s `bctr` reaches `CFsm::SetInitialState`, cycle 1218's hop into
`0x82199F68` and on to `8219A1B0`.

## A listing I mistranscribed

Cycle 1234 printed `0x821A7A70` and **omitted `821a7a9c lis r10,-0x7d6c`**.
Reconstructing from that listing gives `r10 = k*4` feeding
`lwz r10,-0x45f0(r10)`, which is nonsense — the manager is the global
`*0x8293BA10`.

The conclusion was unaffected and the transcription was wrong. **Corrected in
place in cycle 1234**, with a note saying what was missing and why it matters,
rather than silently.

This is a different failure from the session's others. Cycles 1214, 1223 and 1224
stopped *reading* too early; this one read far enough and **copied badly**. No
rule about walking up or down catches it. What catches it is another pair of eyes
on the listing — which is what happened.

## Two other corrections carried

- Cycle 1218's **"110 slot-`+0x2C` call sites"** is **152** at byte level. Its
  uniqueness claim survives and strengthens: `0x8219A1B0` is still the only site
  loading its receiver from `[rX+0x288]`, now against 152 rather than 110.
- The seven Loading variants dispatch on `[this+0x288]`, and every constructor
  writes it with a **literal**. Since vf11 runs once, immediately after
  construction and a no-op registration hook, the field *is* that literal. The
  campaign path uses the base class, so **Mission 01 takes arm 0** — statically
  determined, no runtime value needed.

## Not established, stated plainly

- **135 of the 152 `+0x2C` sites were binned as unclassified**, having no simple
  receiver load two instructions above. "No third mode-task dispatcher exists" is
  **not** proven; only "no third site loads its receiver from `+0x8` in that
  idiom".
- The base-materialisation scan covers `lis`+`addi` only. A base formed
  `lis`+`ori`, or loaded from memory, is invisible — which is why the value
  census (`0x821BBEF8` exists at exactly one address and is formed by no
  instruction) is the primary leg and the neighbourhood table the secondary.
- What sets `[this+0x0C] = 4` on the AircraftSelect object rests on a
  `vt[+0x60]()` call that is beyond that class's 19-slot primary vtable, so it is
  on a sub-object that was not resolved. **The chain above the aircraft-select
  state is open.**
- `0x821BA980` does not null-check the creator's return before `Register` and
  `vf11`, and every `vf18` has a NULL arm. Whether that arm is reachable is
  undetermined.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
821a7a9c confirmed present in the image and absent from cycle 1234's listing
```

No product code changed.
