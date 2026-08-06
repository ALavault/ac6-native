# Cycle 1047 — current-source bridge storage and window boundary

Date: 2026-08-06

## Result

The current external source reproduces the bridge-only save transition on a
fresh profile. The `stock` lane remains stopped at `state40=8,
selector44=4,type28=6`; the `bridge` lane reaches `selector44=7,type28=8`
and then `selector44=8,type28=10`. This closes the storage transition as a
bridge experiment boundary only. It does not qualify a native gate or retail
unit, wave, objective, HUD, or gameplay screenshot.

## Provenance

- PAL `default.xex` SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- External source commit: `b8b03c7a89dc7f23bcd7844d15aa5080d480bf11`
  (pre-existing dirty worktree; no source commit or reset was performed).
- Fresh bridge build: `AC6_EXPERIMENT_LANE=bridge`, Clang 21, Vulkan.
- Bridge binary SHA-256:
  `c82042b60b78d2e2b69733a70499eb1243a9c5fb6e47b3db0fd70dc1b814a30e`
- Host GPU: NVIDIA RTX PRO 4000 Blackwell.
- Audio: `SDL_AUDIODRIVER=dummy`.
- Recipe SHA-256:
  `df2dd2fb013b5f2c1a4b0b5a84de31c901e52d5ec87bcdac925493984cb01c65`.

The build used a separate external directory
`build-rt-bridge-1046-clang`; its `assets` link points to the qualified local
runtime assets. No retail payload was copied into this repository.

## Storage transition

Run output: `/tmp/ac6-cycle-1046-current-bridge-fresh-retry`.

The append-only follow log SHA-256 is
`d2302bd515f99197ea348fa0f63ee3d7c525da9ae319f0a409f61196dce62fbf` and the
launcher log SHA-256 is
`5bf58c272d0f175f8322b688c94cf0b16c0efbc25a176bab1be32e06fc8a1009`.

The decisive state sequence is:

```text
03:56:34.499  state40=8 selector44=4 response12=0 type28=6
03:56:42.449  state40=8 selector44=4 response12=1 type28=6
03:56:43.032  state40=8 selector44=7 response12=1 type28=8
03:56:43.049  state40=8 selector44=8 response12=0 type28=10
03:57:13.920  campaign transition state=1->2
```

The run reached the Mission 01 briefing/cinematic recipe and published 11,201
`PRESENT` records before the final focus failure. The bridge captures remain
external. Selected hashes are:

- `step-30-post-launch-type8.png`:
  `8f5f2f103dc5f9877deecf0ab7ec47f98fe81b26a9962423c4523452d1e67c36`
- `step-33-post-launch-type10.png`:
  `f161f73ede2fa84356797f5ba9d46a4d0c06993da201eee73c7747bc601eb211`

## Window-boundary control

Run output: `/tmp/ac6-cycle-1047-current-bridge-window-boundary`.

The recipe stopped injecting keys before the transition and observed 180 more
`PRESENT` records. It captured
`step-79-post-cinematic-observe-only.png`, SHA-256
`c0a5df6a4eb48113a1f7b524bfb7a8b9fcaa962c0e0076a290aa73242cb64a83`.
The follow log SHA-256 is
`4bd35f5dfc78aacf11424467ee998af4c32f0ec9440eae8e4c1583f69dc1ab65`.

The log contains 17,266 `PRESENT` records, 200 first-mission task records, and
583 canonical gameplay-input records. The first canonical input is at frame 3;
the last observed input is at frame 17,222. No retail unit or wave identity was
promoted: the read-only factory/constructor boundary produced no qualified
identity, and the screenshot is bridge evidence from the ReXGlue/Xenia
renderer. The visible briefing text (`Invasion of Gracemeria`, `Aerial Defence
(Air-to-Air)`) is retained only as an observation; it is not a native scenario
manifest.

The harness was interrupted after the observation capture, so the exit status
does not classify the guest as crashed. The next boundary is the gameplay
window/input handoff and its native replacement, not another storage probe.

## Gate classification

- `pause_save_restart`: unchanged native pass.
- `units_and_waves`: open.
- `retail_objectives`: open.
- `essential_hud`: open.
- `success_failure_debrief`: open.
- J0/J1: unchanged; bridge evidence is not accepted as native proof.
