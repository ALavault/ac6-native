# Handoff main thread — PAC et shaders statiques de la démo PAL AC6

Date : 2026-08-16  
Cible exclusive : démo PAL `Default.xex`  
SHA-256 : `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`

## Résultat acquis

Le codec PAC est fermé et les quatre shaders atteints par le premier frame sont
désormais qualifiés sans exécution du guest.

- `DATA.TBL` contient 861 entrées : 821 brutes et 40 compressées.
- Le XOR fondé sur les mots de π/Blowfish utilise un ordinal **local au PAC** :
  `DATA00.PAC` emploie 0..411 et `DATA01.PAC` recommence à 0 pour les entrées
  globales 412..860.
- Le codec 1 est XOR puis DEFLATE brut (`wbits=-15`). Les 40/40 entrées donnent
  leur taille exacte et commencent par `FHM 01 01 00 10`.
- La fermeture récursive contient 2 583 nœuds uniques, dont 231 FHM et 2 352
  feuilles, sans erreur de parse. Un FHM vide valide mesure 20 octets.
- Les 49 occurrences NSXR contiennent 3 802 occurrences de shaders et 1 891
  microcodes uniques. Aucun des quatre shaders atteints n'y correspond, ni brut
  ni après swap endian par dword.
- Ce résultat PAC négatif n'implique pas une synthèse dynamique : les quatre
  microcodes sont des plages `.rdata` exactes du basefile PAL qualifié.

| Stage | Plage guest statique | Taille | SHA-256 microcode | SHA-256 SPIR-V |
|---|---|---:|---|---|
| VS | `0x82013E20..0x82013E7F` | 96 | `099625f3ea15a92e74e525503b3e41302fc268bc8845da6100c991f67321e4e3` | `944fd75222b6de743b9ce1cd18440b8497230e3813bb105c655cd6cfba123ce6` |
| PS | `0x82013E80..0x82013EA3` | 36 | `4913603d899eb3d5c8f5b3e2fa918ffb461320222f4748b233983ad8a2c98e25` | `f6422d60ff48b5ed43292db838655199322a6d439fea10d39302deda69ece9fe` |
| VS | `0x820140A0..0x8201410B` | 108 | `93488cb9a7bbbb2f0a8bc9cf9cc6b4111102ccaba9e76d0a16ef65184ea0402b` | `ba9b97cceb816059cd21ff6abfda6c59160363155d5b270a3e315b215adb0576` |
| VS | `0x82014140..0x8201417B` | 60 | `586168ec589613862294dae90f866303312abb8756318fa8d8633c8562a83cc0` | `4913cadb00aef0bba3f42c25e25919b6403e2de654e8165748337df331cdc920` |

Le basefile exigé est
`b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218`.
L'atlas canonique joint ces plages aux producteurs statiques `0x821B1D58`,
`0x821B6078` et `0x821B6FD0`. Le PS apparaît aussi dans deux NSXR embarqués au
XEX, à `0x8264B68C` et `0x8264BA8C`.

## Changements du working set

Modifiés :

- `tools/ac6_mode1_codec.py` : ordinal archive-local et décodage codec 1 ;
- `tools/ac6_fhm.py` : acceptation fail-closed du FHM vide de 20 octets ;
- `tools/extract_ac6_pac.py` : extraction/décompression déterministe ;
- `tools/tests/test_ac6_static_tooling.py` : tests codec, FHM et inventaire ;
- `recompilation/ace-combat-6-demo/tools/validate_qualified_vertex_sources.py` :
  validation hors ligne des quatre shaders, malgré son nom historique ;
- `recompilation/ace-combat-6-demo/tests/test_build_demo.py` : garde ciblée.

Ajoutés :

- `tools/inventory_ac6_pac_shaders.py` ;
- `analysis/demo/ac6-demo-codec1-pac-shaders-v1.json` ;
- `analysis/demo/ac6-demo-static-reached-shaders-v1.json` ;
- `reports/ac6-demo-codec1-pac-shaders.md` ;
- `reports/ac6-demo-static-shader-qualification.md`.

Le worktree était déjà dirty. Ne pas nettoyer, rebaser ou réinitialiser les
changements concurrents. Le sous-arbre `recompilation/ace-combat-6-demo` est
actuellement non suivi dans l'index parent ; cela ne signifie pas que ses
fichiers viennent tous de ce checkpoint.

## Reçus et hashes

- reçu PAC : `fae19818d4de1d52418278f0c3575f6cf4937eb357204b6efc1597e401a0319a` ;
- inventaire PAC frais, deux fois identique :
  `e9f90d32d50c1a1694dd26697642550b379fb3c6796a855cdff52d6025416f1e` ;
- rapport PAC : `939993e538b38466c3a70b72b448894ef2b6835eb7b8e98701dad7cda01e3a77` ;
- reçu shaders statiques :
  `32e359303a08e06f11f822b6a37adbf05b52d8aff1505d85817f11fbf10a4335` ;
- rapport shaders statiques :
  `5f80ad86074bb2c608767c06637d9590179e685b68c0695aac8e5c69118aaccd` ;
- validateur : `74bf0ce96433552bce1d4b635ea41ef720ec5b2afcb22d977a98ed9daa8b7641` ;
- `spirv-val` épinglé :
  `2cc19cddc1293518705467f41f55094800b319bd77b1eaf6e30bc7901d6e3406`.

## Validation à rejouer

Depuis `workspaces/ace-combat-6`, avec tous les temporaires sous le TMPDIR
qualifié :

```sh
export TMPDIR=/fastdata/lavaulta/tmp
python3 -m unittest tools.tests.test_ac6_static_tooling
python3 recompilation/ace-combat-6-demo/tools/validate_qualified_vertex_sources.py \
  .build/ac6-demo-codegen-xenon-51/xex-basefile.bin \
  recompilation/ace-combat-6-demo/build/ac6-demo-rexglue-runtime-probe \
  ../../.tools/spirv-tools-install/bin/spirv-val
python3 -m pytest -q recompilation/ace-combat-6-demo/tests/test_build_demo.py \
  -k vertex_shader_rr_provenance
```

Résultats observés : 77/77 tests statiques, 9/9 tests PAC ciblés,
`qualified_shader_sources=4/4`, `spirv_val=4/4`, et 1 test pytest passé
(65 désélectionnés). Ruff, validation JSON et `git diff --check` passent.

Pour régénérer la fermeture complète, extraire sous un `mktemp -d` dans
`$TMPDIR` les 40 indices suivants avec `tools/extract_ac6_pac.py --decompress`,
puis appeler `tools/build_ac6_asset_closure.py` et
`tools/inventory_ac6_pac_shaders.py` sur le manifeste :

```text
0 1 2 3 10 104 119 120 162 163 164 165 166 170 171 177 178 233
289 290 291 292 293 296 300 304 308 312 316 320 324 328 332 336
340 344 348 352 353 412
```

## Frontière et prochain checkpoint

Ce checkpoint prouve statiquement l'identité et la traductibilité des quatre
shaders. Il ne prouve ni leur sélection sur une route donnée, ni l'état complet
du draw, ni des pixels non noirs. Le runtime reste nécessaire uniquement pour
la reachability et la jointure au PM4/état de rendu.

Prochain checkpoint recommandé : appliquer le même gate métadonnées → traduction
ReXGlue/XenosRecomp → `spirv-val` aux 1 891 microcodes PAC uniques, conserver
tout microcode, désassemblage, IR et SPIR-V sous `TMPDIR`, et publier seulement
les identités, provenances, hashes et diagnostics. Toute instruction ou
traduction inconnue reste `unknown` et fail-closed.

Ne pas lancer une nouvelle importation Ghidra pour ce checkpoint. Ne modifier
ni Xenia/ReXGlue, ni le C++ généré, ni les microcodes. Aucun PAC, XEX, shader,
SPIR-V généré ou autre actif propriétaire ne doit être suivi.

## Addendum — qualification sémantique externe recoupée

L'archive `ac6-pal-shader-identification-20260816.tar.zst`, SHA-256
`7294a028408b5008236624cb46f6b108f3ed1c2ffd9961c5317634c47ae36a3c`,
a été vérifiée puis reproduite depuis une extraction fraîche de l'entrée PAC
163. Les 49 NSXR régénèrent un TSV canonique de 3 802 enregistrements et 1 891
microcodes uniques, octet-identique à celui de l'archive
(`32da44938…f5a9082c`). Les 1 876 chemins UPDB et les familles Map, Map_HDR,
Ocean et MPARTS sont donc des preuves statiques démo. Les rôles déduits des
noms restent `name-derived`.

Le reçu `analysis/demo/ac6-demo-static-shader-semantics-v1.json` corrige aussi
un faux négatif externe : `586168ec…a83cc0` est bien présent dans le basefile à
`0x14140`, avec hash swap32 `5c3f2841…7040483`. Le rapport durable est
`reports/ac6-demo-static-shader-semantics.md`. L'archive et ses 1 891
microcodes restent externes et ne doivent pas être suivis.

## Addendum — gate Map/terrain 72/78

Le gate `tools/qualify_ac6_map_shaders.py` repart d'une extraction fraîche de
l'entrée PAC 163 et ferme 28/28 PS via ReXGlue ainsi que 44/50 VS via le
`ShaderContainer` complet, XenosRecomp, DXC et `spirv-val`. Deux runs frais
produisent le même reçu complet SHA-256 `1aa151be…3297e`.

Huit microcodes supplémentaires sont fermés par une normalisation gardée : les
`VertexElement` d'adresses d'instruction distinctes partagent exactement
`TexCoord,1`, et seules les déclarations HLSL répétées byte-identiques sont
retirées. Les 6 VS restants exportent uniquement
`VSPointSizeEdgeFlagKillVertex.x`, que XenosRecomp ne prend pas en charge ; ils
restent fail-closed. Le microcode VS brut contient des formats de fetch non
patchés ; la déclaration vertex du conteneur est obligatoire et aucun format
ou builtin ne doit être inventé. Reçu durable :
`analysis/demo/ac6-demo-map-shader-offline-gate-v1.json`. Rapport :
`reports/ac6-demo-map-shader-offline-gate.md`.
