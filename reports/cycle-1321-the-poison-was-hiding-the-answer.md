# Cycle 1321 — the poison was hiding the answer

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** No console emulator, no bridge, no game run.
- No product C++ changed.

## The correction, and it is to yesterday

Cycle 1320 reported that the consumer sets **"bit 5 of `record+0x0B` and no
other"**. It could not have known that.

The flag word is accumulated with `lwz` / `or` / `stw`. Under the `0xCD` poison a
byte the function OR-ed reads `0xCD | mask`, so **every mask bit `0xCD` already
carries — bits 0, 2, 3, 6 and 7 — is invisible**. The snapshot emitted only that
pass. Five of eight bits per byte were unobservable and the sentence claimed all
eight.

The harness now emits **`after_hex_b`**, the second poison pass, alongside
`after_hex`. It is purely additive; the digest in
`tools/emit_ac6_reader_digests.py` is defined on `after_hex`.

And the second pass is not merely a detection aid here — **it is the physically
correct one**. The frame stage `0x821CA908` clears `+0x00..+0x83` of every record
before the consumer runs, so a zero-filled record region *is* the consumer's real
precondition and `0xCD` is the artificial one. The campaign has been reading the
artificial pass for 138 committed snapshots and every cycle since.

The calibration was loosened to match, **deliberately and narrowly**: each write
run is compared on `{address, size, after_hex}` rather than as a whole dict,
because a field the specialised harness could not produce is not a semantic
difference. Address, size and content are still compared exactly, run for run, in
order. **138/138 equal, 138/138 digests identical.**

## The button map, measured

`tools/audit_input_button_map_microexec.py` runs `0x821CAA50` **101 times** in
one Ghidra startup — one case per device bit with that bit alone set, three
device words, four extra field candidates, and a null control. 23 seconds.

The consumer does not copy the button word. It remaps it:

| device bit | XInput label | record flag bit |
|---:|---|---:|
| 0–3 | DPAD up/down/left/right | 0–3 |
| 4 | START | 10 |
| 5 | BACK | 11 |
| 6 | LEFT_THUMB | 12 |
| 7 | RIGHT_THUMB | 13 |
| 8 | LEFT_SHOULDER | 8 |
| 9 | RIGHT_SHOULDER | 9 |
| 12 | A | **5** |
| 13 | B | **7** |
| 14 | X | **6** |
| 15 | Y | **4** |

The XInput column is a **label, not a measurement** — the run set a bit, not a
button.

Bit 12 in, bit 5 out. That is where cycle 1320's `0x20` came from, and the null
control reads `0x00000000`, so nothing else contributes it.

## Six negatives, each with a positive control in the same batch

The fourteen rows above lit up in the same run, so a blank result below is the
absence of an effect and not a broken experiment.

- **held bits 10, 11** — nothing. XInput leaves both unassigned.
- **held bits 16–31** — nothing. The device word is 32 bits and only its low 16
  reach a record.
- **`pressed`, all 32 bits** — nothing.
- **`released`, all 32 bits** — nothing. The edge words at `device+0x14` and
  `+0x18` are **not** folded into the record flag word.
- **`device+0x38`, `device+0x3A`** — nothing. Cycle 1318 could not place these
  two halfwords; they do not reach the flag word.
- **`device+0x4A`, `device+0x4B`** — nothing. **Cycle 1318 guessed the triggers
  fed record bits 14 and 15. That guess is refuted.** Both lie beyond
  `device+0x43`, outside the 0x40-byte snapshot copy, and driving them to `0xFF`
  changes nothing.

So bits 14 and 15 stay unexplained — with four candidates now eliminated by
execution instead of one standing as a guess.

## And no axis bit is in that word

A run with LY at 30000 fills the float slot at `+0x50` and leaves the flag word
at `0x00000020`. **Bit 17 is not set.** The masks `0x10000`…`0x80000` that cycle
1318 paired with the four axes select a float slot; they are not written into the
record's flag word. Cycle 1316's phrase "flag word … parallel float array" reads
as though one indexes the other, and after the consumer returns it does not.

## An inference the checker is allowed to make, once

A byte is undetected exactly when it equalled the poison in **both** passes — so
in the `0x00` pass it is zero. For a **bitmask** that is a value, not an absence,
and the flag-word reader defaults missing bytes to zero on that ground. The float
slots keep the strict reader, where a missing byte would otherwise read as `0.0`
and look like a written zero. The two readers sit side by side in the file with
the reason written between them.

## Not established

- What record flag bits 14 and 15 are.
- Which word the bit-scan loop scans to choose float slots, now that it is
  measured not to be the record's own flag word after the store.
- Whether the `pressed`/`released` words reach a record **anywhere else**. What
  is measured is that they do not reach `record+0x08`.
- The consumer is executed, not ported. No product C++, no contract entry.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none
ctest                                100% passed, 0 failed out of 28
microexec_harness_calibration        pass, 138/138 equal, 138/138 digests
contract_addresses                   pass, 144 cited, 144 supported
contract_derivations                 pass, 27 behaviours, 0 gaps
tools/tests                          Ran 72 tests, OK
instrument_discipline_index          pass, 19 shapes, 0 unindexed
contract_artifacts (live contracts)  pass, 50 cited, 50 match HEAD
```

`refresh_contract_evidence.py` refused again, correctly: the edited artefacts are
cited by no contract. The seven `contract_artifacts` failures over the full glob
are still the superseded `mission01-native-gate.json`, unchanged from cycle 1320.

## Next

The port. Everything the record needs is now measured rather than read — base,
stride, the four axis slots, the fourteen button bits, the two reciprocals, the
deadzone — except flag bits 14 and 15, which no input this cycle could reach.
