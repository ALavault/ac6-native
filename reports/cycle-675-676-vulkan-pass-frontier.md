# Cycles 675–676 — Vulkan pass and cutscene material frontier

Date: 2026-08-03

## Result

The visual boundary is narrower than the earlier renderer summary stated:

| checkpoint | observed result |
| --- | --- |
| Mission 01 hangar (`step-69`) | F-16C mesh and hangar materials are textured |
| overhead cutscene (`step-75`) | terrain is textured, but the aircraft meshes are white/untextured |
| flight (`step-78`) | HUD is visible over a black world |

The white cutscene aircraft are therefore a separate material/shader problem;
they must not be merged with the black gameplay-world problem. The statement in
the cycle-674 report that the “same scenes” became textured is too broad and is
corrected here: the signed-view change qualified the hangar aircraft and the
cutscene terrain, but did not close the cutscene aircraft material path.

## Deterministic draw evidence

Cycle 675 used the first (13-field) Vulkan pass catalog and reached all scheduled
Mission 1 captures. Its append-only log and derived manifest are retained:

```text
log:      reports/logs/cycle-675-vulkan-pass-catalog/ac6recomp-follow.log
log sha:  20dd583716420487b64e541e80c6d707b0a09e667e6c9fd786f7c9a3abb74cee
catalog:  reports/logs/cycle-675-vulkan-pass-catalog/pass-catalog.json
sha:      812f1d2a95deafeeff93d8fb0fdf4843174879027846f4ec848ff778d41ba7ec
records:  243 draws, 243 viewport records, 616 texture records
```

No `[ac6-bind] NULL` line and no fatal/assert/unresolved marker occurs in this
run. The cutscene aircraft do submit geometry and do receive non-null texture
descriptors:

```text
15:43:54.323  VS A1863AF658456A14  PS D5B4F4A878949938  53 vertices
             texture base 0x06B30000, format 20, 256x256

15:44:05.097  VS C1EE3147DFD5E624  PS D5B4F4A878949938  1331 vertices
             texture base 0x045FB000, format 20, 256x256
```

The same cutscene interval contains textured environment passes. The sky uses
guest base `0x1CC92000` (`format 20`, `2048x2048`), while the recurring
`0x1C461000` target is sampled as a `format 36`, `640x360` surface. In the
hangar, the aircraft pass uses a different material family (`PS C441CAB...`)
and samples bases `0x18105000`, `0x18107000`, `0x181DF000` and `0x181FF000`.
This is an executable distinction between the two material paths, not an LOD
absence claim.

The current evidence consequently rejects these explanations for the white
aircraft:

* missing aircraft geometry;
* a globally missing texture view;
* a PAC/resource load failure;
* the gameplay render-target/resolve defect.

It leaves a bounded set of renderer hypotheses: wrong signed/unsigned view for
the cutscene binding, incorrect format/endian or swizzle for the selected
`D5B4...` material, a UV/vertex-fetch mismatch in the cutscene LOD, or a shader
constant/lighting path that collapses the sampled colour to white. The catalog
does not yet distinguish those cases.

## Cycle 679 — bounded material-view diagnostic

One fresh route enabled only `ac6_log_material_views` and stopped after the
flight controls captures (step 87); it was intentionally interrupted before
the trace could continue. The route reached the cutscene and the HUD, with no
`[ac6-bind] NULL`, fatal, assert or unresolved marker in its logs:

```text
reports/logs/cycle-679-cutscene-material-views/
binary sha256: e62cbf5c2824e2f56122628747e482c2fcaeaa79750be74cc5edf1763a801883
ac6-view records: 240 raw (duplicate follow log), 64 unique D5B4 records,
                  56 unique C441 records
exit: 68 (capture interrupted deliberately after step 87)
```

The bounded records are also available as compact metadata manifests:
`material-view-catalog.json` (D5B4 log) and
`material-view-hangar-catalog.json` (C441 log), each retaining its source-log
SHA-256.

The cutscene aircraft family (`PS D5B4F4A878949938`) consistently selected a
non-null unsigned image view for both shader-result sign requests:

```text
guest format:       20 (Xenos DXT4_5 / BC3)
host unsigned:      137 (VK_FORMAT_BC3_UNORM_BLOCK)
shader_signed:      0 and 1, both selected_signed=0
signed_available:   0
unsigned_available: 1
selected_view:      1
swizzled signs:     00
```

The known-good hangar family (`PS C441CAB465EA3DBF`) shows the same binding
contract with guest format 18 (DXT1 / BC1), host unsigned format 133
(`VK_FORMAT_BC1_RGBA_UNORM_BLOCK`) and a non-null unsigned view. The diagnostic
therefore closes the signed/unsigned-view hypothesis for the white cutscene
aircraft. It also shows that the problem is not an absent descriptor or a
failed texture-cache residency decision.

The earlier cycle-675 `ac6-tex2` records already provide the guest-side upload
metadata, so a second oracle is not needed just to recover it:

```text
D5B4 base 0x06B30000: 256x256, swizzle 0x688, num_format 0,
                     endian 1, tiled 1, pitch 8, mip_max 2
D5B4 base 0x045FB000: 256x256, swizzle 0x688, num_format 0,
                     endian 1, tiled 1, pitch 8, mip_max 8
C441 base 0x181DF000: 512x256, swizzle 0x688, num_format 0,
                     endian 1, tiled 1, pitch 16, mip_max 0 (known-good BC3)
```

BC3 is therefore not globally broken in this renderer: the hangar path uses a
textured BC3 surface (`0x181DF000`), and the same pass catalog contains
textured BC3 sky/terrain surfaces with mip chains. The cutscene split is now
more specifically a material-content/UV/constant or mip-selection issue, with
the generic BC3 view/upload path retained only as a bounded comparison.

The latest evidence build adds bounded dimensions, pitch, tiling, endianness,
fetch swizzle and host swizzle to this diagnostic for any later A/B; it does
not change the product renderer policy. The remaining cutscene branch is now
material content/mip selection, UV or shader-semantic/lighting behaviour, with
format/endian/tile retained only as a bounded regression comparison. The latest
rebuilt binary is
`547ebec7ac07c811aa6e91442b8bfa97f092b48734d403056e79e034deda9acd` and the
asset-tool unit suite remains 7/7. Do not apply a view-selection workaround to
this material family.

## Cycle 680 — static shader contract

The runtime's existing `dump_shaders` facility was enabled for one bounded
replay. The route stalled before the initial `type28=30` predicate and was
stopped; this is not a new gameplay result. It nevertheless emitted 244 ucode
disassemblies, including the shader families already observed in the cycle-675
cutscene pass. The manifest is local and hashed:

```text
reports/logs/cycle-680-cutscene-shader-dumps/shader-manifest.json
sha256: 6b697e12aef2ca6372dd2aa78a5258e4ee91d5c77ba08af3e0c4f2df82bc5814
```

The static contracts are now explicit:

```text
D5B4 pixel: tfetch2D r3 <- r0.xy, tf0; one texture fetch; export oC0
C441 pixel: tfetch2D r2 <- r0.xy, tf0 and tfetch2D r1 <- r1.xy, tf1

D5B4 VS A1863: position vf0 stride 8; UV FMT_32_32_FLOAT at offset 6 -> o0.xy
D5B4 VS C1EE:  position vf0 stride 8; UV FMT_32_32_FLOAT at offset 6 -> o0.xy
C441 VS 109446: UV offsets 5/7 -> o0/o1
C441 VS 527E1A: UV offsets 4/6 -> o0/o1
```

The D5B4 path therefore has a real, single `tf0` sample fed by an explicit
interpolated UV; it is not a mesh with no texture instruction. The remaining
unknown is the runtime value/content of that sample (including mip choice) or
the D5B4 lighting/constant path after the sample. A future A/B should target
that contract, not texture-view selection or generic BC3 support.

## Cycle 681 — sample-content A/B

The bounded `--ac6_d5b4_texture_sample_override=1` diagnostic replaced only
the D5B4/tf0 result with zero before post-sample shader operations. A corrected
fresh-loadout bridge run reached the same Mission 01 captures as cycle 675:

```text
output: reports/logs/cycle-681d-d5b4-sample-zero/
binary SHA-256: 9620c0ca50ea28ecbf20c854737aa9d878ff149c67953c7eb949e46525853317
override marker: one `[ac6-d5b4-sample] override=1 ... tf0` record
NULL binds: none; fatal/assert/unresolved: none
```

The cinematic aircraft remain white and the flight world remains black. The
cutscene image has the same visual class as cycle 675, while the Mission 01
briefing is byte-identical. Thus a bad D5B4 sample value or mip alone is not a
sufficient explanation. The next split is the D5B4 lighting/constant or final
pixel-output path (and, independently, the gameplay target/world pass).

The adapter remains evidence-only and off by default. Do not promote the
override to a renderer fix or spend a second white-sample run until a named
constant/output diagnostic can distinguish those paths.

## Cycle 676 instrumentation limit

The enriched 15-field catalog (adding host vertex/index volume) was compiled,
but its fresh route stalled at 943 presents before `type28=30`; it never reached
the campaign selector or a cutscene. The log is retained as a tooling limit,
not graphics evidence:

```text
reports/logs/cycle-676-vulkan-pass-volume-catalog/ac6recomp-follow.log
sha: 64461364f62fbe1928fb8a8494ed02af303553d6271837840ac70e8abfed85c7
exit: 67 (harness timeout; no guest fatal signal)
```

Do not repeat that full route with the same instrumentation. Keep the proven
cycle-675 catalog as the accepted pass evidence until the volume fields are
made cheaper or collected only inside a bounded transition window.

## Cycle 683 — existing-save gate is not a populated checkpoint

The only copied profile available from cycle 675 has a valid container/header
but an empty campaign presentation: all three save slots render `MISSION ----`
and `DIFFICULTY LEVEL ----`. The bounded route reaches the browser (`selector44=3`,
then `type28=6`) but cannot transition into a loaded campaign save. It records
zero `[ac6-d5b4-const]`, zero NULL binds and no fatal/assert/unresolved marker.
The route and exact hashes are documented in
`reports/cycle-683-d5b4-existing-save-window.md` and must be classified as
save/harness evidence, not graphics evidence.

The runner's state gates now match only lines after an append-only
`ac6recomp-follow.log` baseline; this closes a generic stale-state false
positive observed during the attempted save route.

## Next low-oracle checkpoint

Do not repeat a fresh route or this empty profile. The next oracle requires a
manifested, non-empty campaign save (or a controlled save-state/scene window)
and should capture only the two D5B4 draws, then classify the named constants
and final pixel output. Keep the cycle-675 pass catalog as the accepted
renderer baseline and do not spend an oracle run on the black gameplay-world
branch until this material split is closed. The eventual fix remains an
AC6-owned Vulkan `MaterialBinding` contract backed by qualified NTXR/NDXR
metadata; the RexGlue view log is evidence tooling only.

The eventual fix must be expressed as an AC6-owned Vulkan `MaterialBinding`
contract backed by the qualified NTXR/NDXR metadata; the RexGlue view log is
evidence tooling and not a product dependency.

Cycle 682 adds the bounded D5B4 constant snapshot hook, but the fresh and
launch-route attempts stopped at the campaign/window transition before a D5B4
draw, with zero constant records. Cycle 683 then demonstrated that the
available copied profile is empty rather than a reusable campaign checkpoint.
Both are harness/save-corpus evidence only. The runner retries SDL focus for
at most 10 seconds and now rejects stale state lines using the follow-log
baseline; wait for a populated save manifest before the next oracle.
