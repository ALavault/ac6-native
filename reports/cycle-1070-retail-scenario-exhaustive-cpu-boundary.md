# Retail scenario exhaustive CPU scan — cycle 1070

The 926 DATA.TBL entries were extracted with the qualified PAC path and
scanned in a 32-process CPU pool. The scan covered ASCII, UTF-8-compatible
bytes and UTF-16LE/BE spellings for `SubMisTbl`, `SubMis`, `ComTbl`,
`Maneuver`, `Obj`, `Act`, `Order`, `Wave`, `Unit`, `Enemy`, `loadMission`,
`missionID`, `Destroy`, `Protect`, `objective`, `AWACS`, and related terms.

The expanded corpus was 5,424,368,676 bytes (2,914,429,232 bytes stored after
extraction). The hits classify as follows:

- entries 187/191 and 268–282 are UI/SWG families (`SubMis`, `loadMission`,
  `missionID`, result/statistics and loading-screen symbols);
- entry 210 is the retail briefing map/UI/audio family (`briefing_ms01`,
  `M500_briefing_MapOpen`, `WAVEfmt`), not a gameplay scenario table;
- entry 230 is debrief UI (`debrief`, `DESTROY`, `obj_0..obj_7`);
- entries 119–133 contain Mission map object names and binary NDXR float
  coincidences, not qualified `Obj`/`Act` records;
- entry 163 is the previously qualified shader closure, with coincidental
  `Order`/`Wave`/`Unit` bytes.

No exact printable `SubMisTbl`, `ComTbl` or `Maneuver` corpus, no
objective-condition table, and no unit/wave registry identity was found. The
result is a negative static boundary, not evidence that binary scenario data
does not exist. No payload is committed.

The native P6 acceptance test is deliberately separate: it qualifies the
native HUD, wave publication and success/failure/debrief mechanics using an
explicit non-retail fixture. It does not close `retail_objectives`.
