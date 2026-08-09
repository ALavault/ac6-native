# Cycle 1275 — the single open hop was a false negative

## Qualification

Flat image `analysis-input/ACE6_X360.exe`; instructions re-read in
`ghidra-projects-xenon/ac6-xenon`. `default.xex` SHA-256 `acc302c1…11bcde`.
**No oracle pass was spent.** No product code changed.

## The standing blocker

Cycle 1244 proved the unit placement is a parent-to-child push the Set leader
performs from inside its own order-execution FSM, and closed with one item:

> Single open hop: what starts the leader's FSM (`0x82297540` has zero
> instruction references).

That sentence has stood since, and it is what keeps 135 of Mission 01's 230
units without a load-time position.

## Established — the address is referenced six times

**It is materialised in two instructions**, which no scan in this repository
looked for. Re-read here:

```
822979fc  bl   0x82296e40
82297a00  lis  r11,-0x7dd7        ; 0x82290000
82297a08  addi r3,r31,0xf0
82297a10  addi r11,r11,0x7540     ; 0x82297540
82297a1c  stw  r11,0x50(r1)       ; stored, then passed on
```

and five more sites do the same: `0x82297AE0`, `0x82297D00`, `0x8229835C`,
`0x822983A4`, `0x82298480`.

**The target has the shape of a state handler.** Its first two tests are on a
negative message code:

```
82297540  mfspr  r12,LR
82297554  cmpwi  cr6,r4,-0x3
82297558  beq    cr6,0x82297928
8229755c  cmpwi  cr6,r4,-0x2
82297560  bne    cr6,0x82297930
```

which is the same convention cycle 1254 met at `0x82199F68`, whose `r4 == -3`
arm is on Mission 01's load path.

## Why the negative was false, measured three ways

The instruments available in 1244 answered a narrower question than the one
asked of them. With today's tooling, `0x82297540` is:

| scan | result |
|---|---|
| `bl` targets decoded from the image | **0** |
| unconditional `b` targets | **0** |
| as an aligned data word | **0** outside its own `.pdata` row |
| **`lis` + `addi` materialisation** | **6** |

The first three are all true and all beside the point. "Zero instruction
references" was a correct measurement of calls and a wrong claim about
references — the eighteenth shape again, and this time it cost a blocker that
stood for thirty cycles.

`INSTRUMENT_DISCIPLINE.md` has warned about register-held materialisation since
the `0x29c80` case, and the warning could only be followed by hand. It mostly
was not. `tools/find_materialised_address.py` now does it, and finds the
founding example too — `0x29C80` at `0x8231C618`, `0x8231E0F8`, `0x8231EC3C`,
all `lis` + `ori`.

## A second finding from the same scan

`0x8229C920` — which turns the `0x7D1` / `0x7D4` order codes into
`bl 0x8229adf8` — has **six real `.rdata` references** and one `bl`:

```
820078f4  8200967c  82009984  82009c4c  82009d84  82009ebc   (.rdata)
822fd0bc                                                     (bl)
```

`0x820078F4` is `galib::CGaObj`'s vtable `0x820078D0` plus `0x24`. So the order
handler is a **virtual slot** installed in six classes, not a free function —
which the earlier "6 aligned" count could not distinguish from noise until the
`.pdata` row was separated out in cycle 1266.

## Not established, and the limit is narrow

- **I read one of the six sites.** The other five are candidates matched by
  value, and the tool says so in its own output. What each does with the address
  is unread.
- **That this installs the leader's INITIAL state is not shown.** The address is
  built and stored; the call that consumes it, and whether it reaches
  `CFsm::SetInitialState` (`0x8219AAE8`, five call sites, all `CModeTaskGame`
  per cycle 1244), was not followed.
- So the hop is **no longer open at the address level and still open at the
  chain level**. What changed is that it is now a reading problem rather than a
  search problem.
