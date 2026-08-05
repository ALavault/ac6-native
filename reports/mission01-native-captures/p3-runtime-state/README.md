# P3 native runtime state

This report is a native headless state proof for Mission 01. It is not a full
J1 mission proof: the P1 manifest still has no qualified retail objectives,
waves, radio definitions, or debrief transitions.

Command:

```text
SDL_AUDIODRIVER=dummy xvfb-run -a reconstruction/ace-combat-6/build/ac6-native \
  --play-headless /tmp/ac6-mission01-native-p1-camera/manifest.tsv 1 \
  /tmp/ac6-native-evidence/mission01.replay \
  /tmp/ac6-native-evidence/headless-p3-runtime
```

The run uses the same 1,800 fixed ticks, replay, manifest, and 1280x720
resolution as the native J0/P1/P2 captures. The committed
`native-session.json` uses schema `ac6.native-session.v3`, is 1,392 bytes, and
has SHA-256
`0b509194669eccd022c6e27637b44f3d1df98d383a49ad037b45a63cb47ffeae`.

The artifact records `deterministic_replay=true`, `pause_stable=true`,
`save_resume_stable=true`, and `restart_stable=true`, with semantic hash
`db6cfa8c0aff25f3`. This closes the native `pause_save_restart` requirement.

Large PPM/F32 readbacks remain external under `/tmp/ac6-native-evidence/`; the
P2 PNG captures are already committed. No retail payload is committed here.
`success_failure_debrief`, retail objectives/waves/radio, and full essential
HUD acceptance remain open.
