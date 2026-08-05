# AC6 Mission 01 — black world and partial HUD audit

Date: 2026-08-05. This is the selector-priority slice of the audit; it does
not claim termination of the complete black-world investigation.

## 1. Provenance

Canonical target: `ghidra-projects/ace-combat-6`, PAL `default.xex`,
SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
`DATA.TBL` SHA-256 is
`82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5`.
The current bridge source is external, commit
`b8b03c7a89dc7f23bcd7844d15aa5080d480bf11`, dirty; the executed bridge binary
is `9e0ef14cc07a6c1c1b584b73b30d989683a791790f15f75fb58f71cfe2e8bdd4`.
All runtime evidence below is `lane=bridge`; no bridge observation is promoted
to stock parity.

## 2. Baseline

The deterministic first-mission route reaches briefing, a textured aircraft
cinematic frame, then the black gameplay world with HUD lines/symbols. The
effective baseline configuration was performance off, debug logging, unlock
FPS off, render capture/frontier logs on, signature diagnostics on, 1x scales,
direct host resolve off, invalid fetch constants off, Vulkan RexGlue/Xenia
visible path. The marker confirms the values in the run log.

The late cinematic capture `step-80-post-cinematic-a-15s.png` is visibly
textured; the earlier historical white-aircraft observation is not requalified
by this selector slice and remains open as a separate aircraft/material gate.

## 3. Frame graph

The selector trace is ordered in
`analysis/render/campaign_render_routes.jsonl`. At the first post-cinematic
route observation, the manager briefly reports `exit_mask_zero`; by the first
camera tick it reports `full_3d`. C5 and C6 remain `full_3d` while the image is
black. The render graph after that branch is deliberately not changed here.

## 4. First black stage

The campaign selector is not the first black stage: at C5/C6 the route has bit
`0x10` and the context is non-null. The exact downstream stage—world draw,
render target, resolve, composite, swap, or presentation—remains open. The
next test must attribute a world draw to pixels before changing Vulkan.

## 5. Resource provenance

The current runtime decodes `DATA.TBL[119]` using the exact
`DATA00.PAC/offset/compressed/decompressed` tuple. Runtime and offline decoded
SHA-256 are both
`e57cbeeb8f97a7a607ee1315b11a822b6af2d32581dcb7cbd557f1a6280e6dbd`.
The current register/consumer join is still open; details are in
`reports/MISSION1_ENTRY119_RUNTIME_QUALIFICATION.md`.

## 6. White aircraft

The current late cinematic capture is not white. This is not a causal fix or a
complete historical-aircraft qualification. Material, shader and lighting
classification is reserved for a bounded positive-control draw pair.

## 7. HUD symbols

The UpHud inline boundary in `sub_8226D1C8` is reached. At C5/C6 update mask
bit `0x80` is set, `manager+0x29C` is `0xB0E8CE20`, and virtual slot `+0x38`
resolves to `0x8223B398`. Existing captures prove visible panel/symbol
primitives, but not every expected element.

## 8. HUD texts and numbers

The prior bounded text probe observes glyph quads submitted but absent at
pixels, while panel quads are visible. The exact atlas bind remains unknown.
`M70000_222` lookup returns 0 in the existing trace. The PAL `atoi` override
`0x82382480` has prior bounded tests, but no current HUD call join proves that
it explains the missing numbers. See
`reports/MISSION1_HUD_SUBMISSION_BOUNDARY.md`.

## 9. A/B matrix

No renderer toggle was run in this slice. The selector observation already
separates the prioritized branch without combining interventions, so no
`force_campaign_render_route` run was justified. Existing texture and D5B4
experiments remain historical evidence and are not reinterpreted as selector
proof.

## 10. Root-cause status

* A — gameplay view without bit `0x10`: `rejected` at C5/C6.
* B — null `manager+0x29C`: `rejected` at C5/C6; exact installation call
  execution remains open.
* C — absent cinematic-to-gameplay view transition: `rejected` behaviorally;
  exact store PC remains open because the frozen watcher reports `pc=0`.
* D — entry 119 decode mismatch: `rejected`.
* E — entry 119 correct but not registered/consumed: `open`, not yet causal.
* F — UpHud not executed: `rejected`.
* G — HUD globally never submitted: `rejected`; per-element atlas/draw cause
  remains open.
* H — all gates correct and defect downstream: `strongly_supported`, not yet
  qualified until the current register/consumer join and pixel attribution.

## 11. Correctifs

No behavioral correction was applied. The only changes in this slice are
read-only probes, a loaded-image fingerprint script, and durable evidence
artifacts. No generated C++ was edited; no shader, texture, light, resolve,
MRT, input, HSM, or renderer path was forced.

## 12. Non-régressions

The route reaches briefing, the textured late cinematic, gameplay ticks,
UpInput/UpObj/UpCam/UpRadio logging, continuous PRESENT, and the partial HUD.
The entry119 diagnostic run itself stopped at UI step 73 because its window was
not focusable; this is a harness/run limitation, not a gameplay regression.

## 13. Points ouverts

1. Join current entry119 registration and consumer to C5/C6 world draws.
2. Obtain exact guest PC for `manager+0x260` stores without treating `ctx.lr` as
   PC; this may require a corpus-level PC literal or a qualified parent hook.
3. Attribute world draw → RT → resolve → composite → swap pixels.
4. Join each missing text/numeric key to its atlas fetch and pixel region.
5. Requalify the historical white-aircraft frame with a textured cinematic
   positive control.

## 14. Reproduction commands

Static fingerprints, read-only:

```bash
GHIDRA_HOME=/fastdata/lavaulta/auto-re-agent/.tools/ghidra_12.1.2_PUBLIC \
JAVA_TOOL_OPTIONS=-Duser.home=/tmp/ac6-ghidra-home-selector-disasm \
/fastdata/lavaulta/auto-re-agent/.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  ghidra-projects ace-combat-6 -process default.xex -scriptPath scripts \
  -postScript QualifyCampaignRenderSelector.java -readOnly -noanalysis
```

Selector/HUD route, bridge lane, with `SDL_AUDIODRIVER=dummy`:

```bash
python3 tools/ac6-first-mission-render-experiment.py \
  --run-id cycle-1026-campaign-selector \
  --out /tmp/ac6-cycle1026-campaign-selector.hefpjj \
  --lane bridge --display :120 \
  --binary /fastdata/lavaulta/auto-re-agent/.tools/ac6-recomp-reference/.claude/worktrees/ac6-gapfill/out/build/linux-bridge-relwithdebinfo/ac6recomp \
  --step-file scripts/ac6-first-mission-bridge-airborne-probe.steps \
  --cvar ac6_log_campaign_render_route=true \
  --cvar ac6_log_campaign_context_29c=true \
  --cvar ac6_log_up_hud_submission=true \
  --cvar ac6_watch_campaign_view_stores=true
```

Entry 119 current-runtime dump, bounded and never committed:

```bash
AC6_DUMP_PAC_DECODED=1 \
AC6_PAC_DUMP_ENTRIES=119 \
AC6_PAC_DUMP_DIR=/tmp/ac6-entry119-runtime-dump \
python3 tools/ac6-first-mission-render-experiment.py ...
```

## Conclusion

**I — combinaison précisément localisée :** the selector and UpHud gates are
closed in the observed bridge run, entry119 decoding is byte-qualified, and
the remaining black-world/HUD defect is confined to downstream resource
registration/consumption, pixel attribution, and the per-element glyph path.
It is not yet legitimate to name the first Vulkan stage until those remaining
discriminating experiments are executed.
