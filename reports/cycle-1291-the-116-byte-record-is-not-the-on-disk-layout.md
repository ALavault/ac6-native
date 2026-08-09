# Cycle 1291 — the 116-byte record is not the on-disk layout

## Qualification

Payload `reports/logs/cycle-739-pac-mission-gate/fhm/idx_0009/000_00_00_00_10.bin`,
**3,477,248 bytes**. `default.xex` SHA-256 `acc302c1…11bcde`. **No oracle pass
was spent.** No product code changed.

## What was tested

Task #17's step 2 was: find Mission 01's script records and look for opcode 30,
the command that starts the Set leader's FSM and places its children.

Cycle 1285 established the runtime shape: records of **116 (0x74) bytes**, based
at `*(this+0x3A4)`, with the opcode read as `8225a734 lwz r11,0x4(r23)` and
tested `subi r11,r11,2` / `cmplwi cr6,r11,0x21` — so a word at `record+0x04`
holding **2 through 35**.

Every 4-byte-aligned base in the whole payload was tried, counting how many
consecutive 116-byte records have an in-range opcode:

| opcode field | longest run |
|---|---:|
| u32 BE at `+0x04` — the runtime form | **2** |
| u16 BE at `+0x04` | 6 |
| u16 BE at `+0x06` | 3 |
| u8 at `+0x04` | 2 |
| u8 at `+0x07` | 3 |

**Nothing.** A mission script is hundreds of commands; the longest run found
anywhere, under any of five readings of the opcode field, is six — which is what
chance produces in 3.4 MB.

## Established

**The 116-byte record is a runtime structure, not a file layout.** Whatever the
container holds, the interpreter's records are built from it rather than mapped
onto it.

So task #17's step 2 was the wrong search, and the question becomes: **what
builds the array at `*(this+0x3A4)`, and from what?** That is a code question,
not a data one, and it is where the next attempt should start.

## The boundary, stated narrowly

This negative covers: **one payload**, a **0x74** stride, **4-byte-aligned**
bases, and **five** readings of the opcode field. It does not exclude

- a different stride — the 0x74 is the runtime record's size and the on-disk
  record could be anything;
- unaligned bases, or a base inside a compressed or relocated sub-block;
- the script living in a different container entirely. Mission 01's boot touched
  PAC entries 9, 119, 165, 199 and 210, and only entry 9's payload was scanned.

Each of those is cheap to test and none was, because the result already answers
the question that mattered: **do not look for 0x74 records on disk.**

## Not established

- What the on-disk script format is.
- Whether opcode 30 appears in Mission 01 at all. That is exactly as open as it
  was before this cycle; what has changed is that one way of answering it is
  ruled out rather than left to be tried again by the next reader.
