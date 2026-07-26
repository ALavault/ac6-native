# AC6 Xenia entry-9 oracle audit

Date: 2026-07-15.  Scope: a bounded comparison attempt for the existing
native `campaign-selector 1 -> DPL 9 -> DATA.TBL entry 9` Scene/CUT slice.
Xenia is an observation tool only; neither the native executable nor its
resources are loaded by the delivered Linux executable.

## Inputs and isolation

| Item | Evidence |
| --- | --- |
| Retail program | `game-files/default.xex`, SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` |
| Oracle executable | `.tools/xenia-canary/build/bin/Linux/Release/xenia_canary`, SHA-256 `98559834c570d4be8ba5d532f000aadf8ea6cf4d495be34a02b7ae766134007c` |
| Native observation | `.build/ac6-xenia-oracle/native-entry9-play-to-completion.json`, SHA-256 `79af25ec51b48b0e7bcb88406dd08d626d0f035fbc3b1b54a25214b029ca70e4` |
| Oracle state | `HOME` was redirected below `.build/ac6-xenia-oracle/retail-run/home`; the Xenia log confirms its storage, content and host-cache roots are there. |
| Display | Xvfb, `1280x720x24`; Xenia selected the local NVIDIA Vulkan device. |

The Xenia log confirms that it loaded `GAME:\\default.xex`, module hash
`D4D6E80D983D5740`, and reached `KernelState: Launching module`. Thus this was
not a missing-executable or GTK-only result.

## Executed observations

Native deterministic observation:

```bash
SDL_VIDEODRIVER=dummy .build/ace-combat-6/native/ac6-scene-shell \
  --campaign-selector 1 \
  workspaces/ace-combat-6/game-files/DATA.TBL \
  workspaces/ace-combat-6/game-files/DATA00.PAC \
  workspaces/ace-combat-6/game-files/DATA01.PAC \
  --play-to-completion
```

It exited successfully and reported the exact bounded state:

- `campaign_dpl_resource_id: 9`, `campaign_data_table_index: 9`;
- Scene group `0`, archive path `22.1.0`;
- 120 serialized camera samples, final sample 119, and
  `native_campaign_scene_session_phase: "scene_complete"`;
- 16 current-frame objects (14 `Rigid`, 2 `AnimRigid`), with the 16 joined
  `Scene/dd01_01a/...` identities in the JSON;
- `mission_scene_group_activation_proved: false` and `spawn_proved: false`.

Retail oracle observation (15 seconds after launch under Xvfb) produced
[`xenia-canary-retail-15s.png`](../../artifacts/ac6-oracle/xenia-canary-retail-15s.png),
SHA-256 `d637b8e23395c7f21b4fd554b28bdbc802efe9211407f6b30a1c8468d48e0af9`.
It is a 1280x720 Xenia window with the Xenia menu bar and an otherwise black
content area (171 colors; normalized mean 0.0360151, standard deviation
0.183354). No retail title/menu or Scene/CUT frame is visible in this capture.

## Comparison result: not established

There is no valid native-vs-retail timing, resource, or render-difference
claim from this run. The native command deliberately bypasses retail campaign
activation and directly inspects an already-proven data route. Conversely, the
retail boot was launched from a fresh isolated Xenia profile, with no supplied
deterministic controller replay, profile/save state, or demonstrated menu path
that selects campaign state 1 and reaches entry 9. Its only captured output
never reached a visible game frame.

In particular, the black oracle window is **not** evidence that entry 9 is
black, that the native renderer differs, or that CUT timing is wrong. The
native inspector's 120 samples are decoded serialized CUT samples, not a
retail wall-clock timing assertion.

## Reproducible launch baseline

`scripts/run_xenia_ac6_oracle_baseline.sh` now owns a fresh Xvfb display and
isolated Xenia home per invocation, captures two timestamped root-window
frames, hashes the XEX, Xenia binary and captures, and writes `manifest.json`.
It is deliberately read-only and records `scene_parity_proved: false`.

The 2026-07-15 short baseline at
`.build/ac6-xenia-oracle/baseline-short-20260715/` captured identical black
frames at 8 and 20 seconds (SHA-256
`42fa11d06dad4ca1a1793a84882aa8a1fcb6eaeeb7373dbec443d2150e448a6b`). Its
log records the NVIDIA RTX PRO 4000 Blackwell Vulkan device and
`KernelState: Launching module`. This confirms an attributable retail launch
baseline only; it neither reaches nor identifies entry 9.

## Exact blocker and next admissible oracle

The missing bridge is a reproducible retail activation trace from `default.xex`
to the same entry-9/Scene-group/CUT identity. It must include the input/profile
preconditions, a captured retail frame (or debugger state) and an address,
resource path, or frame identifier tying that observation to entry 9. With
that evidence, compare only the matching serialized camera frame and its
explicit resource identities. Until then, no native behavior is changed from
this oracle attempt.
