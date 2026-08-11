# Checkpoint 2 — common reader corpus (missions 01–15)

## Qualification

Source: PAL `default.xex`, SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`, canonical project `ghidra-projects/ace-combat-6`.  The input cache is the qualified content index `cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85`.

For each campaign entry 9–23, the first scenario child was opened from the cache and passed through the same native `MissionScenario` and `BinReaders` paths used by the product boundary.  No PAC was read after import.

## Results

| mission | unit records | object records | sub-missions | flag orders | reader runs |
|---:|---:|---:|---:|---:|---:|
| 01 | 230 | 434 | 4 | 232 | 666 |
| 02 | 127 | 211 | 1 | 11 | 340 |
| 03 | 98 | 262 | 2 | 4 | 362 |
| 04 | 179 | 418 | 2 | 13 | 599 |
| 05 | 254 | 705 | 2 | 198 | 961 |
| 06 | 214 | 621 | 2 | 10 | 837 |
| 07 | 190 | 350 | 6 | 246 | 542 |
| 08 | 247 | 634 | 2 | 257 | 883 |
| 09 | 105 | 248 | 4 | 16 | 355 |
| 10 | 191 | 521 | 2 | 6 | 714 |
| 11 | 246 | 572 | 4 | 128 | 820 |
| 12 | 121 | 189 | 4 | 10 | 312 |
| 13 | 253 | 892 | 5 | 213 | 1147 |
| 14 | 251 | 390 | 3 | 2135 | 643 |
| 15 | 247 | 337 | 8 | 171 | 586 |

All 15 probes exited zero with `reader_failure=null`, counters and faction ids in range, and a successfully built campaign unit table.  The reader corpus closes common container traversal and field bounds; it does not claim that the unimplemented tag meanings, AI producers, renderer, media video path, or frontend are complete.

## Reproduction

The existing probe is `reconstruction/ace-combat-6/build/ac6-retail-scenario-probe PAYLOAD`.  The payloads are intentionally not committed; the table above was generated from the qualified cache after import.
