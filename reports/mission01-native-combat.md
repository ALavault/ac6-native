# Mission 01 native combat mechanics probe

Date: 2026-08-05

The product executable now exposes a bounded, headless combat probe:

```text
ac6-native --combat-headless MANIFEST_DIR 1 OUTPUT_JSON
```

It loads the same native manifest/runtime path as `--play-headless`, selects a
unit with a different faction from the player, locks it, fires the first
qualified weapon, advances fixed 250 ms ticks until the projectile resolves,
and requires the target to be destroyed. No PAC, XEX, Xenia or RexGlue path is
opened by this command.

The executed native artifact is `/tmp/ac6-native-evidence/combat-v1.json`:

```text
SHA-256  cca1e77db38cc8096bfd1a89d3f47a75c0831507107cd81d9f93072ea3c013a3
size     394 bytes
```

The executable used for the probe is
`reconstruction/ace-combat-6/build/ac6-native`, SHA-256
`bc78c837126f4170a0be602057fc10ce06a91e2560997f6c95f1cd5f2c873111`.

The artifact records player `4097`, hostile target `4098`, lock success,
weapon `7`, two shots, health `100 -> 0`, two damage events, and active units
`3 -> 2`.

This is native mechanics evidence for targeting, weapons, and damage/destruction
only. The temporary hostile-faction/weapon launch row is a controlled combat
fixture; it is not promoted as retail Mission 01 wave or objective semantics.
Those retail associations remain open and must be qualified separately.
