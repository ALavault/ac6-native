# NFIC CUT XEX event-consumer trace

Date: 2026-07-15

## Exact parser chain

The XEX consumer of the serialized `NFIC CUT` chunk/event structure is now
identified. The recovered address-based chain is:

1. `0x8236eda0` receives a buffer pointer in `r4` and a 64-bit size in `r5`,
   starts at offset `0x10`, walks `8 + payload_size`, counts top-level chunks,
   and builds an index of their addresses.
2. `0x8236e2e0` initializes the complete view, calls `0x8236eda0`, then invokes
   per-chunk initializers for `0x3000` through `0x3040`.
3. `0x8236da78` looks up chunk `0x3040`, counts its variable-size TLV records,
   stores the first/current record pointer at object offsets `+0x78/+0x80`, and
   scans for tag `0x8004`, storing that terminal pointer at `+0x7c`.
4. `0x8236dc70` is the exact bounded event accessor used by the runtime: from
   the current record at `+0x80`, it returns the 16-bit tag, payload pointer
   (`record + 8`), and 32-bit payload size.
5. `0x8236db48` advances the current record by `8 + payload_size`, stopping at
   tag `0x8004` or the counted boundary.

The comparisons in `0x8236b880` independently confirm the control tags:
`0x8001`, `0x8002`, and `0x8003`. The routine uses `0x8236dc70` and, after a
`CutStart`, can inspect the following event before returning a frame boundary.

## Dispatch boundary

`0x8236b920` obtains each current tag/payload/size through `0x8236dc70` and
dispatches it through receiver vtable slot `+0x20`. It continues until
`0x8003`, then handles the following `0x8004` boundary. `0x8236bba0` calls this
dispatcher after converting its floating input to an integral frame value.

The native replay now treats that final `0x8004` record as a required
presentation boundary.  It rejects a second `CutStart`, a `FrameStart` while a
frame is open, and a stream that merely ends after `FrameTerminate`.  Unknown
in-frame event receivers remain outside the reconstructed semantic surface;
this check only prevents the Linux presenter from showing a partial CUT as a
completed cinematic sequence.

The pointer chain is now more precise: `this+4` is a runtime dispatch context;
its first pointer leads to the concrete event target stored at offset `+0x10`,
and that target owns slot `+0x20`. The target class/vtable address is supplied
dynamically and remains unavailable from this static call site, so it retains
an address/offset description rather than a speculative class name.

## GYZ runtime boundary

The same XEX region contains the fixed-record runtime path. `0x8236c9d8`
searches an indexed table for identifier `0x1001` and stores a pointer derived
from the matching entry. `0x8236eab0` selects an integral time/key interval,
derives a fractional part, and calls `0x8236bac0` through a callback object.
This is consistent with runtime evaluation of the recovered GYZ tables, but
the serialized identity is now independently closed by the CUT/Scene join.

## First native verifiable state

`NficCutReplayState` reproduces the dictionary-proven state transitions.
For the supplied first CUT it executes:

- `CutStart`: cut active, no frame selected;
- `FrameStart` payload `1`: cut active, frame 1;
- `MoveCamera` payload `0001000000010000`: cut active, frame 1, camera event
  present, selecting one-based Scene object 1 and Tcam key/frame 1.

Scene object 1 resolves exactly to resource/path index 0,
`Tcam__c01.mop`. Its three GYZ tracks now produce a bounded position,
orientation, FOV and explicit XYZ-convention native transform. The retail
rotation order and dynamically supplied receiver class remain open; see
`AC6_LINUX_SCENE_SHELL_REPORT.md`.

## Evidence and claim boundary

The executed Ghidra evidence is retained in:

- `reports/nfic-3040-parser.log`;
- `reports/nfic-parser-upstream.log`;
- `reports/nfic-runtime-setup-update.log`;
- `reports/nfic-top-level-constructor.log`;
- `reports/nfic-chunk-index-parser.log`;
- `reports/gyz-runtime-state.log`.
- `reports/nfic-camera-callback-receiver.log`;
- `reports/nfic-driver-followup.log`.

No re-agent generation was used. The newly recovered leaves are simple exact
accessors already represented directly by the bounded native parser; the
remaining receiver is virtual and not suitable for speculative generation.
