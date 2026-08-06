# Cycle 697 — Vulkan readback et première qualification locale de Mission 2

Date : 2026-08-03  
Périmètre : reconstruction native AC6, sans oracle et sans force flag.

## Résultat

Le backend Vulkan AC6 possède maintenant une lecture headless vérifiable d'une
cible RGBA8 rendue. `readback_render_target()` effectue les transitions
`COLOR_ATTACHMENT_OPTIMAL → TRANSFER_SRC_OPTIMAL → COLOR_ATTACHMENT_OPTIMAL`,
copie l'image dans un staging buffer hôte, attend la fin de la queue et renvoie
les octets RGBA8. Le test crée une cible 8×8, la remplit en rouge pur et
vérifie les quatre octets du premier pixel. Cela ferme la frontière
clear→readback, mais ne prétend toujours ni pipeline shader, ni mesh, ni
présentation/swapchain.

En parallèle, la fonction de diagnostic PAC a été rendue générique. Le nouvel
exécutable `ac6-entry-diagnostic` accepte un index DATA.TBL explicite (le nom
historique `ac6-entry9-diagnostic` reste conservé). La lecture de l'entrée 10
du corpus PAL réel est bornée à sa plage PAC et produit une payload FHM/MDLP
valide.

## Preuve locale Mission 2 candidate

La chaîne statique qualifiée par le XEX donne :

```text
campaign selector 2 → DPL resource id 10 → DPL::[0xa,0] → DATA.TBL[10]
```

La commande `ac6-mission-diagnostic 2` retourne
`physical_data_table_entry=10` et `direct_data_table_route_proven`.
Cette preuve reste une qualification de route statique : aucune sauvegarde
retail remplie, transition interactive, déverrouillage ou exécution de
Mission 2 n'est revendiquée.

Pour le corpus PAL local (`DATA.TBL` SHA-256
`82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5`),
`DATA.TBL[10]` est dans `DATA00.PAC` à `0x01cc8000`, classe de stockage 1,
avec 2 860 059 octets stockés et 10 232 304 octets décodés. Les représentations
locales, hors dépôt, sont :

```text
entry_0010.stored.bin       SHA-256 ec6bbe672a3e33efd6b70bb1c54551837463aa942777998a0d85285c16b6490f
entry_0010.decompressed.bin SHA-256 e894db3b36956562fa888511c3a0ce352cd690186f96eeb0d7a772f3b1a59675
```

Le diagnostic borné observe :

```text
root FHM, 112 lignes récursives
child 1 : MDLP à 0x150000, taille 0x693000, 27 éléments
40 NTXR, 71 NDXR, 127 MATE
0 Scene, 0 NFIC, 0 GYZ résolu
```

Le résultat est reproductible sans téléverser les PAC complets :

```bash
./ac6-mission-diagnostic 2
./ac6-entry-diagnostic entry_0010.decompressed.bin \
  entry_0010.mdlp.csv entry_0010.scene.csv \
  entry_0010.scene-resolution.csv 10
```

## Implémentation et validations

Fichiers principaux :

```text
include/ac6/vulkan_backend.h
src/vulkan_backend.cpp
tests/vulkan_backend_tests.cpp
tools/entry9_diagnostic_tool.cpp
CMakeLists.txt
```

Hashes source :

```text
vulkan_backend.h             6b4e75c069c9740febd976534cb49b2c391686a0fc8507612dc0900a4ab137ff
vulkan_backend.cpp           43dd4b5103ae47d8c35c33fd3c7c3891915a813b2fd09c4b0b2d0c8a59234489
vulkan_backend_tests.cpp     aac8a5107532493b79a5a750fb6387c1b22a0507f7c9ea004f80d58528056da7
entry9_diagnostic_tool.cpp   711ae8a6eda831bd699fdc306d704e2a1e51a5bf2077dbc263df8003b1774cfc
```

Le smoke test Vulkan `ac6-vulkan-backend-tests` passe. `git diff --check` est
propre. Le test reste headless et est sauté proprement si aucun device Vulkan
n'est disponible.

## Frontière suivante

Le readback rend maintenant mesurable un futur pipeline shader/mesh sans
swapchain. La prochaine étape doit ajouter un contrat de pipeline SPIR-V et un
draw minimal vérifiable par readback, puis brancher les vues BC1/BC3 et les
descripteurs qualifiés. Le mapping selector 2→entry 10 doit ensuite être
confronté à une sauvegarde retail remplie ; la route synthétique Mission 2 ne
doit pas être promue en preuve runtime.
