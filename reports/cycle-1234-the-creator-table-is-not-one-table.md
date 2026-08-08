# Cycle 1234 — the creator table is not one table, and three of my cycles were wrong about it

## Cycle 1226's negative was an artefact of an assumed start

Cycle 1226 declined to publish a slot number because the RTTI check failed:
`[0x820654DC]` and `[0x8206558C]` were `.text` pointers, not `.rdata` locators.
**The check failed because I read it at the wrong address.** I assumed the runs
began at `0x820654E0` and `0x82065590` — the first word of the *pure-`.text`*
stretch — and the real starts are `0x24` and `0x0C` earlier.

| | | |
|---|---|---|
| `CModeTaskStartUp` | vtable `0x820654D4`, COL at `0x820654D0` | `0x821B54C0` at start + `0x48` |
| `CModeTaskTitle` | vtable `0x82065584`, COL at `0x82065580` | `0x821B5808` at start + `0x48` |

**Slot 18.** And the control that settles it could have failed: scanning 100% of
`.text` for every `lis rX,0x8206 / addi rX,rX,0x54xx–0x57xx` pair, the program
materialises eleven vtable bases in that window and **never `0x820654E0` or
`0x82065590`**. My assumed starts are addresses no instruction ever forms.

Cycle 1226 was right to withhold the number and wrong about why. It said "either
RTTI is off for these classes or these are not vtable starts" — the second
disjunct was true, and I treated the pair as a reason to stop rather than a fork
to resolve.

## Cycle 1224's index 44 is impossible

```
821b58d8  cmpwi cr6,r30,0x2
821b58dc  blt   cr6,0x821b5844    ; k < 2 -> table lookup
821b58e0  li    r10,0x0
821b58e4  b     0x821b5854        ; k >= 2 -> creator = NULL
```

Read here. **`CModeTaskTitle::vf18` accepts `k ∈ {0,1}` and nothing else.** Cycle
1224 computed that index 44 reaches `0x82691B8C` and called that the mission
path; 44 sets the creator to null.

I read `821b5840`–`821b5854` and stopped one basic block short of the guard.
That is the *walk up to the conditional* rule, half-applied — the same failure
as cycle 1214, which stopped short of a back-edge. **Twice now the missing
instruction was within twenty bytes of where I stopped reading.**

## The table is not one table

The consequence is larger than the correction. `0x82691ADC` is not "the global
base off by one" — it is **`CModeTaskTitle`'s own two-entry successor list**.
`0x82691AD8` is a **concatenation of small per-class lists**, not one indexable
array of ~175 creators.

Seven Loading variants carry the identical stub back to back at `0x821A7A70`,
`0x821A7AB0`, `0x821A7AF0`, `0x821A7B30`, `0x821A7B70`, `0x821A7BB0`,
`0x821A7BF0` — each with **its own base**: `0x82691B8C`, `0x82691CF8`,
`0x82691C6C`, `0x82691C74`, `0x82691C7C`, `0x82691D6C`, `0x82691C30`.

That also explains, and retires, the `{shared, distinct}` alternation cycle 1224
noticed in the table dump and explicitly declined to name. It is per-class
sub-lists.

## The Mission 01 chain, verified here

```
821a7a70  cmpwi  cr6,r4,0x2
821a7a74  blt    cr6,0x821a7a8c
821a7a8c  lis    r11,-0x7d97
821a7a90  rlwinm r10,r4,0x2,0x0,0x1d
821a7a94  addi   r11,r11,0x1b8c     ; 0x82691B8C — THIS CLASS'S OWN LIST
821a7a98  lwzx   r11,r10,r11
821a7a9c  lis    r10,-0x7d6c        ; CORRECTED (cycle 1240): this line was
                                    ; omitted from the listing as first
                                    ; published. Without it a reader
                                    ; reconstructs r10 = k*4 feeding the load
                                    ; below, which is nonsense. The manager is
                                    ; the global *0x8293BA10.
821a7aa0  lwz    r10,-0x45f0(r10)   ; the CTaskModeManager
821a7aa4  stw    r11,0x10(r10)
```

`CModeTaskLoading::vf11` (`0x821A72C0`, shared by all seven variants) calls
`this->vf18(0)`; `vf18(0)` reads `[0x82691B8C] = 0x821BBF98` = `new
CModeTaskGame`.

**Cycle 1225's confirmation survives and its framing does not.** Three routes did
agree that `0x82691B8C` holds `0x821BBF98`; calling it "entry 45 of a 175-entry
table" was inherited from cycle 1218 and is wrong. It is index 0 of a two-entry
list belonging to one class.

## Why every earlier scan missed it, for two independent reasons

- The address is formed `addi r11,r11,0x1b8c` then `lwzx` — **not** `lwz
  0x1b8c(rX)`. A scan for the load form finds 41 other entries and not this one.
- The function is reachable only through a vtable slot, and cycle 1224 established
  that both setters appear zero times in `.text` as text.

Either alone would have hidden it.

## Not established, stated plainly

- **Which class introduces slot 18.** The tidy answer — `CSwgModeTaskBase` adds
  it — **fails its own control**: of the 9 `CModeTask` classes with a null at
  `+0x48`, **8** derive from `CSwgModeTaskBase`, not 9. Whether that null is slot
  18 or trailing padding is undetermined, so no declaring class is published.
- What calls `CSwgListener::vf21` with which value, for the Title path. 314
  `+0x54` dispatch sites image-wide, overwhelmingly unrelated classes; the value
  comes from the UI layer at runtime and only its admissible range `{0,1}` is
  static.
- What calls `CModeTaskLoading::vf11`. It is itself a vtable slot and the level
  above is untraced.
- `CModeTaskTitle`'s two successors, `0x821BB618` and `0x821BB5C8`, undisassembled.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
the guard at 821b58d8 and the stub at 821a7a70 re-read here
```

No product code changed.
