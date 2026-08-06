# Cycle 683 — existing-save D5B4 window: profile gate, no graphics evidence

Date: 2026-08-03 (Europe/Paris)

## Decision

The bounded scene-window route did **not** reach a D5B4 draw.  The run is a
harness/save-corpus result, not a renderer comparison and must not be used to
claim anything about the white cutscene aircraft or the black gameplay world.

The copied profile came from the accepted cycle-675 route, but it is only the
new-game template.  Its save browser visibly reports `FILE 01`, `FILE 02` and
`FILE 03` with `MISSION ----`, `DIFFICULTY LEVEL ----` and empty flight times.
The file is structurally a SAVE container (`SAVE` in UTF-16-like form followed
by the `0xFEFE` sentinel), not a completed Mission 1 checkpoint.

## Reproducible corpus

```text
source profile:
  reports/logs/cycle-675-vulkan-pass-catalog/user-data/
  B13EBABEBABEBABE/4E4D07D1/00000001/

save.dat                  129112 bytes
  sha256 37740dda30cd5dbc829d5342b0880f707fa4b4937b400ec068b4a4735455925c
not_00000000.dat          524296 bytes
  sha256 7999583abfa6bf03f346a4258a59e9bf122a1a58cabce61583e3ce30fb174e3e
save header                   328 bytes
  sha256 61cef61c40c18c26dd938d2dc2cedb047f60897e189175bc189771be30f7354a

binary: out/build/linux-bridge-relwithdebinfo/ac6recomp
  sha256 f2eb1fa6a569ecfa1feef0e871549622364461876b144392946e2b49b57f450f
runner: .tools/.../ac6-gapfill/tools/ac6-run.sh
  sha256 b9af13a68f8b61b3ca9f4e2de0e47cbf243149c8707cd6f524ab5a3216b7430a
recipe: scripts/ac6-d5b4-existing-save-scene-window.steps
  sha256 4cd7e7cdd528312184980d72d9e003b88b4e57a32ec3abd54f1eb60a4b68bd82
accepted log: reports/logs/cycle-683g-d5b4-existing-save-scene-window/
  follow-log sha256 bef6b0435085c4be707e8fe5da0d78fe41cc72b039534a356256e8929f83549d
```

The run reached `selector44=3` and `type28=6`, then stopped before
`type28=8`; it emitted zero `[ac6-d5b4-const]`, zero NULL texture binds,
zero fatal/assert/unresolved markers.  The two bounded captures are retained
as save-browser evidence:

```text
step-01-existing-save-browser.png
  sha256 3798ba20b41b7bca985b00e8385d8f109e0577c3a8bd8e4ca7e1d1eae6206bfb
step-04-existing-save-type6.png
  sha256 d95bc2633b778c20e31701c958da3d471cc6d898a0bad587b4234d244f675994
```

## Runner correction

The state-driven runner previously kept a counter per regex.  A first
`wait type28=6` therefore accepted a line written before the preceding input.
The recipe now uses the append-only `ac6recomp-follow.log` line baseline for
all `wait` and `wait-pulse` gates.  This makes stale dialog states impossible
to satisfy a later stage and is a generic harness invariant, not an AC6
force path.  `bash -n tools/ac6-run.sh` passes.

## Consequence

Do not spend another oracle run on this profile.  The next runtime request
needs a genuinely populated campaign save (or a controlled save-state/scene
window) whose manifest proves a non-empty mission record.  Until then, keep
cycle 675 as the accepted renderer baseline and continue the native Vulkan
contract work without promoting cycle 683 to graphics evidence.
