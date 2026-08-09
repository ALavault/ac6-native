# Cycle 1286 — the count was window-dependent, and said so nowhere

## Qualification

Flat image `analysis-input/ACE6_X360.exe`. `default.xex` SHA-256
`acc302c1…11bcde`. **No oracle pass was spent.** No product code changed.

## The discrepancy that had no wrong side

Cycle 1285 fixed an operand-field bug in `find_materialised_address.py` and got
**144** sites for the unit-registry offset `0x0002D3B4`. The agent that found the
bug reported **138**. Neither of us was wrong.

The scanner pairs a `lis` with a later `ori`/`addi` within a fixed window, and
the window was a constant nobody printed. Measured:

| lookahead | `0x0002D3B4` | `0x82297540` |
|---:|---:|---:|
| 4 | **138** | 6 |
| 8 | 142 | 6 |
| 12 | **144** | 6 |
| 32 | 151 | 6 |
| 64 | 157 | **8** |

138 is the answer at a four-instruction window; 144 at twelve. A wider window
pairs a `lis` with an `ori` further away, which may be one materialisation split
by instruction scheduling or two unrelated uses of a register — the tool cannot
tell, and its output says `CANDIDATE` for exactly that reason.

**So the number was never a count. It was a count-per-window, and the window was
invisible.** It is now printed with every result and settable with
`--lookahead N`.

## The negative, restated properly

A windowed scan's zero is only as good as its window, so the load-bearing one
was re-run across the range:

```
0x4D415445  'MATE'  0 sites WITHIN  4 instructions
0x4D415445  'MATE'  0 sites WITHIN 12 instructions
0x4D415445  'MATE'  0 sites WITHIN 64 instructions
```

**Zero at every window tested**, where `0x0002D3B4` moves from 138 to 157 over
the same range. That is a materially stronger claim than cycle 1276's
single-window zero: it does not rest on a parameter, and the parameter is now
shown to matter elsewhere.

Note also that `0x82297540` — the FSM state whose six materialisations cycle
1275 published — becomes **eight** at a 64-instruction window. Those two extra
pairs are unread. The six remain the ones actually read; the report should have
said "six within twelve instructions", and now the tool makes that unavoidable.

## The shape

This is the denominator rule, which `Ac6XenonForceScan` has printed since it was
written and which two later tools did not: **a scan states the parameter its
answer depends on, or its answer is not a measurement.** The cost here was small
— two people reporting different true numbers — and the same omission in a
negative would have been a false one.
