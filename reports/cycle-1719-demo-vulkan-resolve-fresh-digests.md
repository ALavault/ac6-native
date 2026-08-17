# Cycle 1719 — oracle Vulkan resolve frais et digests tiled

## Verdict

Le harness test-only du resolve atteint est maintenant déterministe sur les
deux représentations du résultat. Il compare l'intégralité du buffer tiled à
l'oracle CPU (y compris les gaps `0xA5`), calcule un SHA tiled puis un SHA après
untile, et vérifie un motif asymétrique non nul. Le binaire crée deux
instances/device/buffers frais; le wrapper CTest relance ensuite deux processus
frais et compare leur stdout complet.

Ce résultat valide le chemin générique du shader de resolve pour les constantes
PAL déjà qualifiées. Il ne constitue pas une screencap ni une preuve de pixels
guest-owned : le contenu EDRAM réel de la démo reste inconnu.

## Identité et provenance

| élément | valeur |
|---|---|
| cible | `Default.xex`, démo PAL, Xenon big-endian/Xenos |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| test source | `recompilation/ace-combat-6-demo/tests/ac6-demo-vulkan-resolve-tests.cpp` |
| test source SHA-256 | `adf1a740c3db56829105f73ce47d4c1c5fd3178eb1773a51d290b199b829528` |
| repeat wrapper | `recompilation/ace-combat-6-demo/tools/run_vulkan_resolve_repeat.py` |
| wrapper SHA-256 | `6b8ccbb0098bcc6b507560364bbb97b27456d5d8b66595d64de275ac3460bcd6` |
| ReXGlue | commit `cb58065c793429aa92895d778af58d12e9d26d8f` |
| SPIR-V header SHA-256 | `de24d6b23367da6b2fa3b5d1d843d920cbdf4a5170cf49544412c4bebcb1eb11` |
| SPIR-V blob SHA-256 | `e8cfb0d6981476118cefbf797d33092ccf09281f728d34220c1514dd79487b32` |
| `spirv-val` | target Vulkan 1.1 PASS; pinned binary SHA `2cc19cddc1293518705467f41f55094800b319bd77b1eaf6e30bc7901d6e3406` |

Le binaire de production `ac6-demo-recomp` reste inchangé (SHA
`30429ddecb4154d2c09f4f68055bc69f937f38850afcfd8cb3212043313ed2bb`).

## Sorties exactes

Constantes poussées :
`{0x00000010,0x00091400,0x01000300,0x00005C28,0x00000000}`;
dispatch `20x90x1`; destination tiled `0x398000` octets avec gardes alignées.

| motif destination | SHA tiled (gaps inclus) | SHA linéaire après untile |
|---|---|---|
| `00 00 00 00` | `94831d4c398252020f792d92f546c5122ad522c4270b73be9e8619fde1db641f` | `0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f` |
| `33 22 11 44` | `0bf69cf42fd6c3ac73b30c438a4db6d1664eaafa9c716b9ba330a9886c976786` | `66dde082635ccc6b24abba5b372ceb10173bc2b062faa2d93de7c4548bb60dc8` |

La commande CTest verbose confirme :

```text
fresh_processes=2 deterministic=true spirv_sha256=e8cfb0d6981476118cefbf797d33092ccf09281f728d34220c1514dd79487b32
```

## Classification et limites

- **demo-qualified** : constantes, dimensions, format/destination et ordre du
  resolve provenant des IB PAL déjà qualifiés; invariance attendue des gardes
  et de l'oracle de destination.
- **xenia-generic** : SPIR-V ReXGlue, formule tiled/untile et sémantique du
  shader de copie.
- **demo-observed** : aucune nouvelle observation runtime guest dans ce cycle.
- **unknown** : contenu EDRAM PAL, draw/rasterisation source, pixels
  guest-owned, frontend, audio et résultat de mission.

## Validation et garde

- codegen ON : CTest `17/17`;
- codegen OFF canonique : CTest `18/18`;
- `spirv-val --target-env vulkan1.1` : PASS;
- aucun changement de Xenia/ReXGlue, Ghidra, C++ généré, microcode ou actif
  propriétaire;
- aucun fallback visuel ajouté à `play`, aucune screencap publiée.

Capsule : `analysis/demo/ac6-demo-vulkan-resolve-fresh-v1.json`.
