# Mission 01 native J0 evidence

The native headless gate passed with a qualified external slice manifest and
without loading a PAC, XEX, Xenia, RexGlue, PCSX or Wine at runtime.

Command:

```text
SDL_AUDIODRIVER=dummy xvfb-run -a \
  reconstruction/ace-combat-6/build/ac6-native \
  --play-headless /tmp/ac6-mission01-qualified-6lq59r 1 \
  /tmp/ac6-native-evidence/mission01.replay \
  /tmp/ac6-native-evidence/headless-v2
```

The run performs 1,800 fixed ticks, three identical replay executions, a
pause/resume check, and a save/resume check. Its native report is
`/tmp/ac6-native-evidence/headless-v2/native-session.json`, SHA-256
`7b703ffac27f53f03e4a6fcd6026c7206116203cfab1e878846a1b6253ff67b7`.

The run records player entity `4097`, three active units, ten submitted
geometry calls, 2,206 color/depth-covered pixels, a non-static follow camera,
and non-zero flight response. Deterministic replay, pause stability and
save/resume stability are all true.

Native readbacks:

- color PPM: 2,764,816 bytes, SHA-256
  `9fd8d3040f6535c8bd35a39a116e9fd3f109004ff39f2fe6427d51ff99edb196`;
- depth F32: 3,686,400 bytes, SHA-256
  `faa9ad8b7306ed64d26dd292855bc38b2ad50b480b003db78cf9f6632a1dd7db`.

The compact committed views are in
`reports/mission01-native-captures/`. They show the actual current output,
which is a sparse/dark wireframe-like scene rather than fine visual parity.
