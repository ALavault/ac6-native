# Cycle 1308 — the input classes are named, and the displacement scan is refused

## Qualification

- Ghidra project `ghidra-projects/ace-combat-6`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** No game code ran.
- No product C++ changed.

## The population, before the search

Cycle 1307 derived the input path's field layout and left the question *who reads
the split axes at `+0x28`…`+0x36`*. The discipline says establish the population
first, and this cycle is a demonstration of why.

`tools/whose_vtable.py` names the poll entry: `0x8234D510` is slot **`+0x10`** of
vtable `0x820124F8` = **`NU::Input::DriverController`**, from the audited class
map. It has two siblings sharing slot `+0x00` with it —
`NU::Input::Driver` (`0x820124E0`) and `NU::Input::DriverKeyboard`
(`0x82012544`) — and a container class, `NU::Input::DriverContext`
(`0x820110C8`).

## The vtable/table boundary, proved rather than assumed

Cycle 1307's axis table sits at `0x8201250C`, which is `vtable + 0x14` — close
enough to be more vtable. It is not:

```
0x820124F8  8234D940   slot +0x00
0x820124FC  8234CFE0   slot +0x04
0x82012500  8234D098   slot +0x08
0x82012504  8234D0A0   slot +0x0C
0x82012508  8234D510   slot +0x10   the poll entry
0x8201250C  0000000A   table
0x82012510  00000003   table
```

Five code pointers, then small integers, with the transition exactly at the
boundary the loop's start address implies. The vtable has five slots and the
table begins where the code pointers stop.

## The object model

`0x8234CFB0` is the `DriverController` constructor. **The substring scan for its
vtable found nothing** — a `lis`+`addi` pair is invisible to it, which is the
*rule that was written, correct, and unrunnable*; `tools/find_materialised_address.py`
finds the one site immediately. It writes the vtable at `+0x00` and zeroes
`+0x54`, `+0x70`, `+0x74`, `+0x78`, `+0x7C`, `+0x80`, `+0x84`.

**One caller**, `0x82343BB8` — the `DriverContext` constructor:

```
82343bd0 addi r29,r31,0x18     ; first controller
82343bd4 li   r30,0x3
82343bdc or   r3,r29,r29
82343be0 bl   0x8234cfb0       ; construct
82343be4 subic. r30,r30,0x1
82343be8 addi r29,r29,0x88     ; stride
82343bec bge  0x82343bdc
82343bf0 addi r3,r31,0x238
82343bf4 bl   0x8234d578       ; a fifth object, not a controller
```

`r30` runs 3, 2, 1, 0, so **four controllers**, at `context+0x18`, `+0xA0`,
`+0x128`, `+0x1B0`. The stride settles the class size: **`DriverController` is
`0x88` bytes**, which the constructor's last zeroed field (`+0x84`) corroborates
independently.

`0x82343C08` is a second constructor over the same vtable, and is not read here.

## And the displacement scan is refused

`tools/ghidra_scripts/Ac6FieldRead.java` over `+0x28` and `+0x2A` splits into
**577 field reads, 167 vtable dispatches, 1 stack slot**.

577 is not a candidate list. Every class in the image with something at `+0x28`
is in it, and nothing in the output says which of them is a
`DriverController` — this is the *displacement collision* exactly, and the same
scan run before naming the class would have produced a clean, complete, plausible
list of the wrong thing.

**So it is not used.** The join has to come from the owner side: a consumer needs
a `DriverController*`, and the four instances live at fixed offsets inside one
`DriverContext`. Finding that context's global is a bounded question; scanning a
displacement is not.

## Not established

- Who reads `+0x28`…`+0x36`. The scan that would answer it is unusable and the
  bounded route is not yet walked.
- Where the `DriverContext` instance lives.
- What the fifth object at `context+0x238` is. `0x8234D578` builds it and
  `NU::Input::DriverKeyboard` is the obvious guess — which is why it is recorded
  as a guess.
- What `0x8234D210` does, the trigger stage, and `0x8234D1B8`. Still open from
  cycle 1307.

## Gates

```
mission01_final_gate=audit-valid JF=pass open=none
ctest: 100% tests passed, 0 failed out of 27
contract_addresses=pass cited=103 supported=103 unsupported=0
tools/tests: Ran 72 tests, OK
```

## Next

Find the `DriverContext` instance. `0x82343BB8` is a constructor, so its caller
holds the storage — either a global or a member of a larger service. From there
the consumers of the split axes are reachable by following a pointer rather than
by matching a number, and the 577 become the handful that actually hold one.
