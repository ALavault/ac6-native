# Cycle 1360 — the tick is complete

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass.** `0x82211B40` was micro-executed nine times.
- **Product C++ changed**, and the **fourteenth behaviour** is under contract.

## The last piece

`retail_slot_gather` is `0x82211B40`: an early return when the record's flag word
is zero, then one output-slot bit per player mask word that **intersects** it.

It is the join. Its input is `record+0x08`, which `retail_input_record` carries;
its 32 mask words are the ones `retail_input_binding` walks; its output is the
word `retail_slot_repeat` turns into edges. Three contracted behaviours that
previously met only in a report now meet in the product.

## The case that separates OR from assignment

Retail **ORs** into the caller's word. A port that assigned would pass every case
where that word starts at zero — which is every obvious case to write.

So two cases seed it non-zero, including one where the flag word is zero and the
function returns before touching anything. An assigning port returns `0` there
and loses the caller's `0xA5A5`. Nine of nine, first run.

## Any overlap, not a subset

A mask naming three record bits fires on **one** of them. A port testing for a
subset match reports nothing where retail reports the slot, and the
`partial-overlap` case is the one that says so.

## Where the input path stands

`0x821CA908` — the frame tick the ladder places on `0x821D7A90` — has six pieces,
and **five are now behaviours in the gate**:

```
0x821CB5F0 -> 0x821CAA50   the four records            retail_input_record
[0x823F6DB8]+0x1C          the frame's elapsed time    read, not ported
0x82211DF8 x5              per object                  read
  0x82211B40               the active-slot mask        retail_slot_gather   NEW
  0x82211C10 x4            the binding layer           retail_input_binding
  0x82211988               the auto-repeat bank        retail_slot_repeat
```

Plus `retail_input` beneath it all, for the device snapshot. The one unported
piece is the virtual that yields the elapsed time, whose class has not been read.

## Not established

- What class `[0x823F6DB8]` is.
- What consumes the three edge words.
- Where the flight integrator's time comes from — cycle 1358 established it is
  not this float.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 14 behaviours
ctest                                100% passed, 0 failed out of 33
contract_addresses                   pass
contract_derivations                 pass
slot_gather_microexec                pass, 9 cases, 0 divergences
tools/tests                          Ran 72 tests, OK
```

## Next

The input path is done to the edge words. What reads them is the next link, and
it is the first question in this thread whose answer is not already inside
`0x821CA908`.
