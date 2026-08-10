# Cycle 1511 — PAL frontend closure and bounded Vulkan capture

## Delivered

- `ac6-native import --frontend` now selects DATA.TBL entries 2–8 in addition
  to the common camera, 15 campaign payloads and Mission 01 world entry.
- `RetailFrontendResources` walks those seven FHM font/glyph packs with the
  bounded native FHM reader, validates every NFH header and requires at least
  twenty font leaves per pack. No text or glyph bytes are synthesized.
- `FrontendController::configure(settings, resources)` now accepts each of
  the five PAL locale slots only when that sealed resource set is present;
  the legacy no-resource overload remains fail-closed to English.
- The public cache-bound `play` and `replay` paths refuse a cache without the
  complete frontend closure (`cache_incomplete/frontend_font_resources`).
- `FrontendController` now has explicit `Pause`, `Error`, resume and recovery
  transitions. Existing diagnostic commands and the English-only unqualified
  settings path remain unchanged.
- `play --capture PPM --frames N` writes the actual presented 1280×720 Vulkan
  target after a bounded fixed-60-Hz run. This is an evidence capture lane,
  not a JV/parity claim; the current frame still contains the documented
  diagnostic marker path.

## PAL validation

Against the qualified local PAL source (`default.xex` SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`):

```
ac6_import=pass records=24 bytes=647199600 frontend=true
index_sha256=ca25a10fc9dbf1987d34fd755c9d842c887273e5083e8b4d644339b5a156bbde
frontend_fonts=pass entries=7 locales=5
retail_cache=pass missions=15 blobs=24
```

The real Vulkan smoke passed with one simulated tick and produced a 1280×720
PPM (`sha256=8d3d2c5c019325b218d6690ea2e7db24f39c9e4a1e0f1c5225c2c827a5f39c37`).
A four-frame replay on the same sealed index reproduced twice with semantic
hash `0xa0125b5d0720e787`.

The CPack tarball also passes the corrected native-package audit
(`package_audit=pass entries=79`); the packaged audit script is the same
binary-only, word-boundary-aware version.

## Boundary retained

This closes resource qualification, not the unresolved live player pose,
retail camera producer, full scene composition or mission progression. JV/JP
remain unclaimed until those bindings are derived and the gate accepts them.
