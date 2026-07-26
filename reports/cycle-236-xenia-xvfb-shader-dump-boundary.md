# AC6 cycle 236 — autonomous Xenia/Wine shader-dump boundary

Date: 2026-07-18

## Goal

Capture active shader identities without human input by running the qualified
Windows Xenia Canary oracle under an isolated Xvfb display with
`dump_shaders` enabled. This was a diagnostic follow-up to the static XXH3
catalogue, not a gameplay or parity run.

## Preflight

`scripts/launch_xenia_ac6_wine.sh check` reports:

```text
status=ready
release=16e1eb8
renderer=vulkan
service=ac6-xenia-wine-gui.service
```

The configured binary SHA-256, PAL XEX, local profile, Vulkan renderer and
AZERTY keyboard bindings all pass the existing preflight. No launcher service
was active. `vulkaninfo --summary` from Xvfb can enumerate the NVIDIA RTX PRO
4000 Blackwell and llvmpipe, so lack of a discoverable Vulkan device is not a
sufficient explanation for the failure below.

## Bounded attempts

Four short, input-free runs were retained under
`dynamic/runs/*-xenia-*-xvfb` or `*-xenia-shader-dump`:

1. command-line storage/cache/dump overrides after preparatory `winepath`
   calls;
2. the same overrides with manual `Z:\\...` paths after stopping the prefix's
   wineserver;
3. an isolated portable application copy with only the XEX argument;
4. the isolated copy through Wine `start /wait`.

The first attempt exposed a concrete setup error: `winepath` had started Wine
without a DISPLAY, and the later Xvfb process reused that server, producing
`nodrv_CreateWindow`. The next three attempts removed that cause. They still
returned before Xenia initialization:

- no new `xenia.log` was created or modified;
- no shader file was emitted;
- no Xenia process remained;
- no input was injected;
- each attempt stayed below 25 seconds.

Exit status zero is not success here because the required log and shader-dump
postconditions both failed.

## Decision

Do not repeat this Xvfb/Wine launch family with different sleeps, coordinates
or keyboard events. The qualified Canary binary remains usable through its
existing real graphical-session launcher, but no current graphical session is
attached to this agent. Per the active user instruction, human/VNC sessions
will be started only on request.

The AC6 static-to-runtime hash catalogue remains valid and complete. The open
MATE-to-permutation relation is classified `needs-dynamic-evidence`; it is not
filled from filenames or flags. When a graphical session is explicitly
requested, enable an isolated `dump_shaders` directory and require a new Xenia
log plus nonzero shader count before accepting the run. Until then, continue
autonomous work on another portfolio frontier.
