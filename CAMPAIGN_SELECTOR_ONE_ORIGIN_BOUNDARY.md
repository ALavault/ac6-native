# AC6 campaign selector 1 origin and Scene identity boundary

Date: 2026-07-15 (Europe/Paris)

## Result

The retail source consumed by `Function_820A85E0` is a mode-dependent
current-level field, not an archive traversal index and not a Scene-group
index.  Selector `1` is a valid value in that state domain and reaches the
already proved direct route:

```text
current-level selector 1
  -> Function_821B6E58(mode 1) -> DPL id 9
  -> DPL::[0x9,0] -> direct DATA.TBL index 9
```

This is sufficient for the existing, deliberately bounded native
`--campaign-selector 1` route.  It does **not** prove that selector 1 activates
`22.1.0`, or any other serialized Scene group inside decoded entry 9.  No
campaign-selector extension or automatic group selection was added.

## Retail state accessor and writers

`FUN_820943B0` reads the current level from the object at global
`PTR_DAT_826e4eb4 + 0x70`.

- mode 2 reads `+0x10`;
- mode 3 reads `+0x24`;
- mode 4 reads `+0x20`;
- mode 5 reads `+0x18`;
- all other modes select one of three `0xa7e0`-stride records and read
  `+0x4c60`.

The paired `FUN_82196508` writes exactly the same mode-dependent locations.
This is an accessor/setter pair for a current-level value; it does not inspect
an FHM, `Scene` path, CUT payload, or archive child.

Three directly exported writer paths establish the limited provenance of the
value without inventing a menu label:

1. `Function_821A6048`, when runtime mode at global `+0x78` is `1`, reads the
   current level.  Values below 15 are incremented through `FUN_82196508`; a
   value of 15 or more is replaced with 0 and marks a local completion byte.
   Therefore retail progression from stored level 0 produces selector 1, but
   the callee does not select an archive Scene group.
2. `Function_821B6258` accepts event id `0x259` and a value in `[0, 7]`, then
   writes that value through `FUN_82196508`.  The exporter contains no caller
   edge or UI/control identity for this event, so calling it a mission-menu
   selection would be unsupported.  It nevertheless proves that value 1 can
   be written directly into the same current-level state.
3. `Function_821B3ED0` sets runtime mode 4 and writes the output of
   `FUN_821B84E8` through the same setter.  The latter maps a category and
   index to arithmetic level values.  Its exported call edge does not identify
   a title/menu screen, a campaign save slot, or a Scene group.

`Function_82212688` independently bounds a consumer's returned level to
unsigned `[1, 15]`, falling back to 1.  That proves selector 1 is the first
accepted campaign-level value, not a first archive-child or first Scene-group
identity.

## Why the native route remains selector-1 only

For runtime mode 1, `Function_821B6E58` maps selector 1 to DPL id 9.  The
direct DPL request chain subsequently proves that id 9 queues physical
`DATA.TBL` index 9.  This closes the resource selection edge and validates the
current native selector-1 loader.

The only Scene-group relation currently recovered is structural: decoded entry
9 contains replayable groups such as `22.1.0`, `22.1.1`, `22.1.5`, `22.1.10`,
`23.1.2`, and `23.1.4`.  The state getter/setter and all three writer paths
above have no call, data, string, or field edge to any of those FHM paths.
Selecting group 0 because selector 1 is first would therefore be a synthetic
campaign rule.  The existing `--scene-group` option remains an explicit native
inspector/export selection, and campaign playback remains bounded to the
proved selector-1 resource route.

## Evidence inspected

- `exports/820943b0.json`: exact mode-dependent current-level accessor;
- `exports/82196508.json`: paired mode-dependent setter;
- `exports/821a6048.json`: mode-1 increment/wrap writer;
- `exports/821b6258.json`: bounded event `0x259` direct writer;
- `exports/821b3ed0.json` and `exports/821b84e8.json`: mode-4 arithmetic
  writer;
- `exports/82212688.json`: `[1, 15]` consumer bound;
- `exports/820a85e0.json`, `CAMPAIGN_MISSION_LOAD_PATH_REDUCTION.md`, and
  `DPL_ARCHIVE_HANDLE_CHAIN.md`: selector-1 resource and physical-entry
  chain.

## Next exact frontier

Recover a call/data edge from the current-level setter or its state owner to a
specific entry-9 Scene path, or recover the retail activation code that reads a
Scene record after the DPL request completes.  Until then, neither a
selector-to-group table nor a selector greater than 1 native scene route is
justified.
