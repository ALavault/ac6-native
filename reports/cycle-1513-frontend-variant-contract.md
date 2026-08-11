# Cycle 1513 — frontend variants are no longer collapsed to English/Normal

## Delivered

`FrontendSettings::valid()` now accepts the qualified PAL enum domain:
Normal/Easy/Hard, Normal/Expert controls, and English/French/German/Italian/
Spanish. The resource-backed `FrontendController::configure()` validates that
domain and requires the selected locale slot in the sealed seven-pack
frontend closure. The legacy no-resource overload remains English-only, so a
diagnostic fixture cannot claim a translated frontend without the retail font
resources.

The resource test now exercises all 3 difficulties × 2 control schemes × 5
locale slots. This is configuration coverage only; NFH glyph decoding,
translated text lookup, and menu rendering remain explicit boundaries.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j16       pass
ac6-frontend-runtime-tests                                  pass
ac6-retail-frontend-resources (without opt-in cache)        skipped
```

No retail bytes, generated output, or language fallback were added.
