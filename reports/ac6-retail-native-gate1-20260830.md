# AC6 retail NTSC-U/J — Gate 1 renderer natif (statique)

Date: 2026-08-30.

## Résultat

La tranche statique du Gate 1 est verte. Le profil `native` matérialise une
copie de `recompilation/ace-combat-6-retail/native` sans AC6_recomp, généré C++,
ReXGlue ou média retail. Le smoke build produit uniquement la bibliothèque et
le test `ac6_native_xenos_tests`; il n'est pas présenté comme `ac6recomp` de
release.

## Contrats fermés

- `MmioBus`: base/taille/pointeurs ring bornés, alignement, registres inconnus
  refusés, callback interruption.
- `VdBridge`: ring dwords, lecture avec wrap, taille MMIO cohérente, pointeur
  read publié seulement après décodage réussi; IB guest bornés, profondeur 8
  et cycles refusés.
- `Pm4Decoder`: TYPE0/1/2/3, validation complète avant effet, offsets d'erreur
  déterministes, WAIT limité à `0`/`7`, draw/resolve/present/IB typés.
- `XenosState`: registres bornés, génération monotone, EDRAM limité à 8 MiB.
- `VulkanBackend`: capability checks et refus explicite de l'IB sans résolveur
  guest; aucune conversion silencieuse d'état inconnu.
- `ShaderTranslator`: SPIR-V avec en-tête/bound valides accepté; microcode
  Xenos non qualifié et modules invalides refusés sans sortie.
- Services offline: réseau refuse sans socket, VFS confiné, save atomique,
  replay poll-exact et ring XMA double-buffer avec bornes plein/vide.
- Kernel boundary: handles invalidables, auto-reset event et timebase Xenon
  50 MHz/60 Hz déterministes.
- ABI PPC: registres 64-bit, VMX128, mémoire guest big-endian avec pointeurs
  32-bit, réservations `lwarx/stwcx` et `ldarx/stdcx`, dispatch indirect borné.
- Frontend contract: `ISO` or `assets/` input only; mutable storage resolves
  under XDG data root and cannot be redirected by path traversal.
- `NativeRuntime`: boot/submit/poll/save/load/shutdown lifecycle integrates
  these contracts; malformed media, ring or replay fails without synthetic
  gameplay state.
- `audit_native_release.py` est prêt pour l'installation finale; l'installation
  actuelle est volontairement l'oracle et est rejetée (`bin/ac6recomp` contient
  des chemins ReXGlue), preuve que l'audit ne laisse pas passer le mauvais
  profil.
- capsule `ac6.xenos-capsule.v1`: identité US, ring borné, événements MMIO,
  PM4/interruption/present dérivés, aucun champ de bytes retail accepté.
- `capture_xenos_capsule.py` convertit seulement un JSONL oracle read-only;
  `guest_write`, `socket`, `retail_bytes` et JSON invalide sont refusés.

## Validation reproductible

```text
pytest -q recompilation/ace-combat-6-retail/tests
106 passed
cmake -S recompilation/ace-combat-6-retail/native -B /tmp/ac6-retail-native-build -G Ninja
cmake --build /tmp/ac6-retail-native-build -j2
ctest --test-dir /tmp/ac6-retail-native-build --output-on-failure
5/5 passed (renderer + services/kernel + PPC ABI + frontend + runtime)
tools/validate.py --target ntsc-uj --runtime native
pass; `nm` forbidden-symbol audit empty; `release_ready=false`
```

`tools/validate.py --target ntsc-uj --runtime native --require-release` échoue
volontairement avec `native full release gate is still open`.

## Reste du gate

La capture read-only unique du backend ReXGlue sur
`mission01-qualified-96.steps` n'a pas été lancée dans cette tranche; aucune
preuve runtime du renderer, shader translation réel ou arrivée gameplay n'est
donc revendiquée. Prochaine action: instrumenter puis capturer une seule
capsule bornée, sans A/B ni écriture guest, et rejouer son contenu dans le
backend Vulkan natif.
