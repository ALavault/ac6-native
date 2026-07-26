# AC6 — base de référence pour l'objectif « natif Linux, assets exposés, backend moderne »

Écrit le 26 juillet 2026. Mesures, pas revendications. Aucune sortie générée
n'a été éditée ; le clone de référence a été restauré à son état cycle 301
après chaque expérience, vérifié par hash.

## 1. Transcription native — où en est la colonne vertébrale

| Mesure | Valeur |
| --- | ---: |
| Fonctions PPC générées | 12 671 |
| Unités de traduction | 52 |
| Compilation du corpus (C++23) | **52/52**, 0 échec |
| Édition de liens `ac6recomp` | **réussie**, 168 Mo |
| Suite native `reconstruction/ace-combat-6` | **44/44**, 0 avertissement |

Le retrait qualifié de `0x82345250` passe désormais codegen, compilation **et**
édition de liens. Le motif `runtime_blocked` du cycle 302 est réfuté (voir
`cycle-303-rexglue-bad-alloc-not-reproducible.md`).

Norme requise : **C++23**. Le SDK utilise `std::byteswap` et
`std::move_only_function` ; toute tentative en C++20 échoue sur les 52 unités.

## 2. Backend graphique moderne — déjà présent, déjà activé

Le SDK ReXGlue embarque un backend **Vulkan complet** :

```
src/graphics/vulkan/  graphics_system, command_processor, pipeline_cache,
                      primitive_processor, render_target_cache, shader,
                      deferred_command_buffer
src/graphics/pipeline/shader/  traducteur Xenos -> SPIR-V
```

Configuration du build AC6 : `REXGLUE_USE_VULKAN:BOOL=ON`. Le binaire lié
contient **13 967** chaînes Vulkan et référence 7 bibliothèques graphiques.

Hôte de validation : NVIDIA RTX PRO 4000 Blackwell, Vulkan **1.4.329**, plus
`llvmpipe` en repli logiciel. `/dev/dri/card1` et `renderD128` présents.

Conclusion : le backend moderne n'est **pas** à écrire. Il est à valider. Un
backend D3D12 coexiste dans le SDK mais n'est pas la cible Linux.

## 3. Assets exposés — première mesure de couverture

Le manifeste natif `ac6-asset-manifest` décode les **926 entrées** de la table
retail en **56 514 lignes**. Répartition par format reconnu :

| Format | Lignes | Part | Nature |
| --- | ---: | ---: | --- |
| `ntxr` | 8 006 | 14,2 % | textures |
| `fhm` | 5 435 | 9,6 % | archives imbriquées |
| `ndxr` | 2 228 | 3,9 % | modèles |
| `nfic` | 1 549 | 2,7 % | cinématiques |
| `scene` | 1 293 | 2,3 % | scènes |
| `nfh` | 1 029 | 1,8 % | — |
| `mate` | 733 | 1,3 % | matériaux |
| `riff` | 546 | 1,0 % | audio |
| `capt` | 301 | 0,5 % | — |
| `swg` | 150 | 0,3 % | — |
| `ace6` | 128 | 0,2 % | — |
| `nsxr` | 51 | 0,1 % | shaders |
| **identifié** | **21 449** | **38,0 %** | |
| `binary` | 23 861 | 42,2 % | **non identifié** |
| `empty` | 11 204 | 19,8 % | **non identifié** |
| **non identifié** | **35 065** | **62,0 %** | |

**62 % des lignes du manifeste n'ont pas de format reconnu.** C'est l'écart
exact que « tous les assets exposés » doit combler, et il n'avait jamais été
chiffré.

Les décodeurs existants couvrent déjà `data_table`, `fhm`, `ndxr`, `ntxr`,
`nsxr`, `mate`, `mop`, `nfic_cut`, `motion_record`, `scene`,
`mission_resource` et `resource_archive`. L'effort restant porte donc sur les
`binary` et `empty`, pas sur de nouveaux formats nommés.

## 4. Ce qui n'est pas prouvé

Le smoke runtime **échoue** : `ac6recomp` s'interrompt sur `SIGABRT` juste
après le constructeur `Ac6recompApp`, sans message, sous `xvfb-run`. L'arrêt
se produit dans le code hôte, **avant** toute exécution de code PPC recompilé.

Cette observation n'est pas encore attribuée : une comparaison contre le
binaire de référence cycle 301, reconstruit à l'identique, est nécessaire pour
distinguer une régression d'une limite d'environnement (`xvfb` sans intégration
Vulkan NVIDIA). Tant que cette comparaison n'est pas faite, **aucune
revendication de comportement runtime n'est formulée**.

## 5. Prochaines actions, dans l'ordre

1. Attribuer le `SIGABRT` par différentiel contre le binaire cycle 301.
2. Exécuter le smoke sur un affichage réel plutôt que `xvfb`, le backend Vulkan
   NVIDIA n'étant pas disponible sous serveur X logiciel.
3. Classer les 35 065 lignes `binary`/`empty` : en-têtes, tailles, entropie,
   voisinage dans l'archive, afin de séparer données réelles et remplissage.
4. Ne promouvoir aucun format nouveau sans décodeur testé et cas de rejet.
