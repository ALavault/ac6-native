# E_EFFMOVE MOP metadata boundary

Date: 2026-07-15

## Corpus result

The local Scene/CUT export contains 247 resources whose joined Scene path has
the `/E_EFFMOVE_` form. Every one validates as a bounded GYZ MOP through the
existing wrapper parser and has exactly two `0x30`-byte GYZ records:

| Record index | `field_08` | Count observed |
| ---: | ---: | ---: |
| 0 | `0x00000003` | 247 / 247 |
| 1 | `0x00010002` | 247 / 247 |

Outer resource sizes range from 288 to 20,176 bytes. The two record element
counts are not constant: single-element records occur alongside hundreds of
elements, and the two counts may differ. `content_id` is likewise not a
unique per-effect handle (224 distinct values across the 247 MOPs).

## Native metadata API

`extract_mop_effect_resource_metadata` accepts only this exact two-record
shape and exposes the content ID plus each record's raw `field_08`,
`element_count`, `data_offset_0`, and `data_offset_1`. Offsets remain bounded
by `MopGyzView`; their inner payload format is deliberately not interpreted.

The API does **not** reuse `MopTransformTrackView` for E_EFFMOVE resources.
Although the two field values match the generic transform form, no static
consumer or effect-specific contract proves that the values are an effect
position/orientation pair, an effect duration, or render data.

## Validation

`ac6-mop-tests` verifies accepted metadata and rejects a malformed second
record type. The full isolated AC6 suite passed 36/36 after the change. No
effect rendering, transform sampling, or visual change is added.
