# Cycle 681 — D5B4 sample-content A/B

Date: 2026-08-03 (Europe/Paris)

## Result

The evidence adapter's bounded sample override was exercised on the same
bridge lane and fresh-loadout route as cycle 675. It replaced only the
`D5B4F4A878949938` pixel shader's `tf0` result with a zero `float4`, before
Xenos swizzle/sign handling and before the shader's lighting constants.

The route reached the Mission 01 launch screen, the overhead cinematic, the
flight HUD and all four control captures. The aircraft in the cinematic stayed
white and the gameplay world stayed black. The sample-content hypothesis is
therefore not sufficient to explain either visible defect; it is closed as the
next renderer split, not as proof that every D5B4 texture upload is correct.

```text
binary:  out/build/linux-bridge-relwithdebinfo/ac6recomp
binary SHA-256: 9620c0ca50ea28ecbf20c854737aa9d878ff149c67953c7eb949e46525853317
cvar:    --ac6_d5b4_texture_sample_override=1
route:   scripts/ac6-first-mission-fresh-loadout.steps
output:  reports/logs/cycle-681d-d5b4-sample-zero/
sample log: df3aeb2ea3a3c22d0be8aa3240fc7e0a21c9077d4be3bf99595658404fc71719
sample marker: one `[ac6-d5b4-sample] override=1 ... tf0` record
NULL binds: none
fatal/assert/unresolved in follow log: none
```

The corrected run pre-created its `user-data` root. Earlier cycle-681 attempts
were harness-only failures (stock lane or missing root) and are not graphics
evidence.

## Image comparison

The zero override does not alter the mission briefing (`step-72`) at all. The
cutscene image (`step-75`) remains the same white-aircraft class as cycle 675;
its small pixel delta is normal replay timing/camera variation, not a material
transition. The flight HUD remains black-world/HUD-only.

```text
capture                         changed image sum vs cycle 675
step-72 mission briefing        0
step-75 overhead cutscene       14,608,719 absolute channel units
step-78 flight HUD              464 absolute channel units
```

The relevant captures and source hashes remain local and are not committed as
retail assets. The next named diagnostic should target runtime D5B4 constants
or final pixel output, not another generic BC3 or signed-view A/B.

## Native consequence

The portable tree now contains `ac6/vulkan_material.h` and
`src/vulkan_material.cpp`. `resolve_vulkan_material_binding` is a fail-closed,
renderer-neutral contract joining:

```text
MATE first texture id == NDXR first texture id == NTXR GIDX
  + decoded BC1/BC3 extent and resident mip range
  + qualified interpolated-UV / texture-fetch facts
  + explicit signed/unsigned view availability and fallback permission
  -> VulkanMaterialBinding
```

It contains no D5B4 special case and no RexGlue/Xenia dependency; a future
AC6-owned Vulkan backend can map its format/view enums to `VkFormat` and bind
only accepted records.

The follow-up constant hook is documented separately in
`reports/cycle-682-d5b4-constants-harness.md`; its attempts stopped before a
D5B4 draw and do not weaken this cycle's sample-zero classification.
