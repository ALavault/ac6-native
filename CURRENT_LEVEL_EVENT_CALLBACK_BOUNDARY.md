# AC6 current-level event `0x259` callback boundary

Date: 2026-07-15 (Europe/Paris)

## Result

The event writer at `Function_821B6258` is installed as a callback in three
homologous XEX data tables.  This establishes an event-dispatch integration,
but it does **not** identify a menu, a controller action, a campaign resource,
or a Scene group.  There is no direct caller edge to the callback in either
the exported call graph or the corrected Ghidra project.

Consequently no selector-to-Scene-group route, new campaign selector, or
automatic Scene selection is added to the Linux shell.

## Direct callback evidence

`ReferencesTo.java 0x821b6258` in the corrected `default.xex` project reports
exactly these three DATA references:

| Callback table | Slot containing `0x821b6258` | Structural observation |
| --- | ---: | --- |
| `0x82065720` | `+0x1c` (`0x8206573c`) | sibling callback slots around the same event slot |
| `0x82067280` | `+0x1c` (`0x8206729c`) | same slot layout as the first table |
| `0x82067330` | `+0x1c` (`0x8206734c`) | same slot layout as the first two tables |

The three tables also share callback entries at `+0x00`, `+0x04`, `+0x08`,
`+0x14`, and `+0x38`; their table-local entry blocks differ.  This supports
only the narrow conclusion that the function is registered in three related
dispatcher profiles.  The table data carries code pointers, not UI text,
resource paths, DPL IDs, or serialized `Scene` names.

The direct function body remains bounded:

```text
event id == 0x259 && 0 <= value < 8
  -> paired current-level setter(value)
  -> if value is 1..7, update a per-profile record from 1 to 2
```

The `0x259` value is therefore an event filter.  It is not a recovered
mission-menu item nor a level-to-Scene mapping.

## Mode-1 increment writer

The exporter body for `Function_821A6048` contains the known mode-1
read/increment-or-wrap path through the same paired setter.  Its exported
`callers` list is empty.  A fresh `ReferencesTo.java 0x821a6048` query of the
corrected project also reports no direct references.  The function contains
virtual calls on its object and calls to `Function_821D2FC0` /
`Function_821D27F0`, but their names, object fields, and the event callback
tables above provide no direct UI or resource identity.

Thus it proves state progression can write selector 1 after stored level 0;
it does not identify the screen that advances it and does not select a
decoded entry-9 Scene group.

## Negative selector-to-Scene check

Neither the three callback tables, their table-local blocks, the event writer,
nor the increment writer contains a direct DPL id, `DATA.TBL` index, FHM path,
or `Scene` group identity.  The independently proved chain remains the only
valid native campaign resource route:

```text
current-level selector 1
  -> Function_821B6E58 (mode 1)
  -> DPL id 9
  -> DPL::[0x9,0]
  -> DATA.TBL entry 9
```

Entry 9's decoded groups remain inspectable only through the explicit
`--scene-group` native option.  Selecting group 0 (or any group) from event
value 1 would be an invented progression rule.

## Evidence

- `reports/current-level-event-821b6258-refs.log`: three exact DATA callback
  references;
- `reports/current-level-table-82065720.log`,
  `reports/current-level-table-82067280.log`, and
  `reports/current-level-table-82067330.log`: table words and identical slot
  placement;
- `exports/821b6258.json`: event/value bounds, paired setter, and profile
  record transition;
- `exports/821a6048.json` plus
  `reports/current-level-increment-821a6048-refs.log`: increment body and no
  direct caller/reference;
- `CAMPAIGN_SELECTOR_ONE_ORIGIN_BOUNDARY.md` and
  `DPL_ARCHIVE_HANDLE_CHAIN.md`: the separate, proved selector-1 resource
  chain.

## Next exact frontier

Recover a concrete dispatcher owner that binds one of these callback tables to
a named UI/control, or recover a post-DPL activation consumer that reads a
specific entry-9 Scene path.  Until either direct edge exists, no further
native campaign routing is justified.
