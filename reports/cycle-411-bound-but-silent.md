# Cycle 411 — sonde reliée et vérifiée, pourtant muette : contradiction non résolue

## 1. Correction du cycle 410

Le cycle 410 concluait que le code n'avait pas été régénéré. **C'était faux** :
la vérification portait sur `.tools/recomp-eval/ac6/output/`, alors que la sortie
réelle est `generated/` (`out_directory_path = "generated"`, ligne 7 de la
configuration). Deux arborescences de code généré coexistent ; j'ai contrôlé la
mauvaise.

## 2. Vérifications de liaison, cette fois faites

| contrôle | résultat |
|---|---|
| `rexglue codegen` | terminé en 14,8 s |
| renommage présent dans `generated/` | ✔ `ac6recomp_recomp.38.cpp:23525` |
| objet de la sonde compilé | ✔ `ac6_ui_input_dispatch_probe.cpp.o` |
| entrée de table d'appel indirect | ✔ `ac6recomp_init.cpp:17446` → `rex_sub_8234D50C` |
| cvar dans le binaire | ✔ |

La table d'aiguillage indirect pointe donc sur l'alias faible que ma définition
forte remplace. Le montage est correct.

## 3. Le fait gênant

**Aucune ligne `[ac6-ui-dispatch]`**, ni sur les écrans qui fonctionnent, ni sur
l'écran bloqué (atteint à l'itération 11).

Cela contredit le cycle 408, où `sub_8234D3F0` est scrutée ~3000 fois et dont
l'unique site d'appel se trouve dans `sub_8234D50C`. Si l'appelant n'est jamais
exécuté, l'appelé ne peut pas l'être 3000 fois.

Je ne tranche pas cette contradiction ici, et je ne la maquille pas. Deux
explications au moins restent ouvertes :

1. l'analyse du cycle 409 portait sur `recomp-eval/ac6/output/`, l'arborescence
   périmée ; la structure d'appel réelle dans `generated/` peut différer, et
   `sub_8234D3F0` y être appelée d'ailleurs ;
2. la surcharge ne l'emporte pas sur l'alias faible dans ce cas précis, malgré
   un montage identique à celui, fonctionnel, de `ac6_fps_physics_fix.cpp`.

La première est la plus probable et la moins flatteuse : elle voudrait dire que
la chaîne d'appel des cycles 408-409 a été lue dans le mauvais arbre.

## 4. Vérification qui tranche, à faire en premier

Relire le site d'appel de `sub_8234D3F0` **dans `generated/`** et non dans
`recomp-eval/`. Si son appelant y est une autre fonction, les cycles 409 et 410
sont à refaire sur la bonne source, et la sonde à déplacer.

## 5. Leçon d'outillage, deuxième couche

Le cycle 410 retenait qu'une surcharge qui compile n'est pas une surcharge
reliée. Il faut y ajouter : **vérifier aussi que l'on inspecte l'arbre que la
compilation utilise réellement.** Les deux erreurs ont la même racine — la
confiance accordée à un chemin sans l'avoir confronté à la configuration.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
