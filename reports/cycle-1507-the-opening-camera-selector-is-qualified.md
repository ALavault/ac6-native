# Cycle 1507 — the opening camera selector is qualified

## Evidence

- Target `ace-combat-6-pal`, canonical project `ghidra-projects/ace-combat-6`,
  PAL `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- The canonical read-only Ghidra reference graph reports five calls to
  `0x82226D80` (`0x82230AF8`, `0x82269B40`, two from `0x8226B618`, and
  `0x8226D1C8`) and three calls to `0x8225D9F0` (the three camera handlers).
- `0x82230AF8` sets its selector register to zero before reading the current
  per-aircraft word at table offset `+19560` and passing it to `0x82226D80`.
  In the canonical PAL image, `[0x826E4EB4] = 0x829E6720`; the three records
  at `+0x70 + 0xA7E0*i` have `+19560 == 0` for `i = 0,1,2`.
- `0x82226D80` stores the raw word at player `+2176` (`+0x880`) and then
  dispatches the view branch through `0x82223AC0`. The recovered branch is
  raw `2 -> view 2`, raw `3 -> view 3`, and the supported initial zero/one
  values -> view 1. Unsupported raw values are not silently coerced by the
  native product.
- `0x8226D1C8` is the later view-cycle/reapply path: it reads player `+0x880`,
  maps `1 -> 2`, `2 -> 3`, otherwise `1`, and writes the next table word. It
  does not replace the opening constructor evidence.

## Product change

`RetailCameraModeSelection` and `resolve_retail_camera_mode()` now carry the
raw selector and its mapped table view. `retail_opening_camera_mode()` records
the qualified campaign value `0 -> 1`.

`RetailSessionConfig` accepts that raw word (default zero), rejects an
unsupported selector before parsing the payload, and publishes the selection
on every `RetailSessionFrame`. The session and store-backed path therefore
keep camera selection beside the retail payload/loadout identity.

`Mission01CpuFrameRequest` can carry a qualified selector. The compositor
validates the raw-to-view mapping, derives view 1 when the request leaves the
view zero, and refuses a mismatch. The frame report records the raw word and
moves `camera_mode_selection` into `closed_domains` for selected frames. The
live camera-manager/player producer, complete pose, and all other JV domains
remain open; `jv_eligible` remains false.

No retail bytes, generated recompilation output or synthetic camera pose were
added to the product.

## Validation

```text
Release build                                      pass
CTest, SDL_AUDIODRIVER=dummy + Xvfb                 67/67
tools/tests                                          87/87
sealed-cache audit                                  15 missions / 17 blobs
mission01-final-gate-v3 --require JF               pass
mission01-playable-gate-v1 --require JF             pass
contract addresses                                  321/321
contract derivations                                52, gaps 0
C++ complexity                                      191 files
contract artefacts                                  pass after commit
```

The session report hash and the three live contract records were refreshed
atomically after the deterministic frame digest gained the raw/view selector.
The cache audit regenerated the campaign matrix byte-for-byte
(`62a8d2199baa0d8811946eace99f3367338c795af30eb034a7871f2a1273ab87`),
so no unrelated retail identity changed.

The tracked session report's semantic hash changes because the deterministic
frame digest now includes the qualified raw/view selector. The next complete
cycle reruns CTest, Python, sealed-cache, JF, address, derivation, complexity
and artefact audits before commit.

## Remaining boundary

The producer of player `+0x88C/+0x890/+0x894/+0x898/+0x8A0`, consumed by
`0x8225D9F0`, still needs a retail-backed port into the live Mission 01 player.
Until that state and the complete camera pose are joined, this cycle is
evidence and a reusable selector contract, not JV/JP acceptance.
