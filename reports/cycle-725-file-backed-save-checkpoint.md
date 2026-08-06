# Cycle 725 — sauvegarde `AC6S` persistante et reprise PAL

Date : 2026-08-04  
Périmètre : transformer le codec de progression en checkpoint fichier borné,
le relire dans le test retail PAL, puis conserver le déverrouillage de Mission
2 après restauration.

## Plan exécuté

1. Ajouter une I/O native indépendante de SDL/Vulkan autour du format `AC6S`.
2. Écrire dans un fichier voisin temporaire puis le remplacer par
   `rename` POSIX ou `MoveFileExW(...REPLACE_EXISTING|WRITE_THROUGH)` Windows.
3. Borner la lecture à 16 MiB et distinguer ouverture, lecture, corruption et
   dépassement de taille sans allouer un payload non borné.
4. Faire utiliser ce chemin fichier par la fixture PAL : terminer Mission 1,
   écrire/lire le snapshot, restaurer un runtime neuf et sélectionner Mission
   2 via le même manifest/PAC loader.

## Résultats PAL

```text
selector 1 / DATA.TBL entry 9:
  AC6S snapshot size                           28 octets
  file-backed write/read/restore               1
  restored Mission 2 available                1
  active scene_changed                         3 pixels à 128x128
  active HUD readback                          303 pixels verts

selector 2 / DATA.TBL entry 10:
  même manifest/runtime/frontend               true
  active scene_changed                         166
  active HUD readback                          300 pixels verts
```

Le chemin de remplacement est vérifié deux fois par le test d'I/O et par la
fixture PAL. La lecture d'une charge de quatre octets est classée
`decode_failure/truncated`; une limite explicite de quatre octets est classée
`file_too_large`. La preuve porte sur la sauvegarde native `AC6S`, pas sur le
format de sauvegarde retail Xbox 360.

## Validation

```text
targeted CTest : 7/7 pass, 2.37 s
full CTest PAL : 62/62 pass, 63.58 s
```

Les sept tests ciblés couvrent Vulkan, le frame retail, la progression, le
runtime, le vol, le codec et l'I/O fichier. Le full CTest est passé après la
recompilation du nouveau module.

## Ce qui reste ouvert

- La durabilité après panne électrique (fsync du fichier et du répertoire) et
  les verrous interprocessus ne sont pas revendiqués.
- Les axes ne sont pas encore alimentés par la boucle SDL/gamepad ni présentés
  dans une vraie swapchain; le backend headless reste le seul chemin démontré.
- Mission 2 est déverrouillée et sélectionnable avec entry 10, mais son vol,
  rendu complet et sa complétion ne sont pas encore qualifiés.
- Le fallback géométrique solide ne prouve ni matériaux, ni profondeur, ni
  shaders Xenos; les avions blancs des cutscenes restent une frontière
  matériaux/texture à traiter.

## Hashes

```text
include/ac6/campaign_save_io.h                   3622448d6d0898ef86679322fa2de64cb338d714088146b2b54498477e628d99
src/campaign_save_io.cpp                          5cda47484e98c54b1dd68128332214da5627c2d61db303cbe0ae4a01295f66ed
tests/campaign_save_io_tests.cpp                  505abd6e26d30aaabd298487ac9243cd298f8303f95911345927428f7e4f015e
tests/campaign_vulkan_retail_frame_tests.cpp      82dc9782b7b66942d1d73b614caf6590b6baddff754cc662f17cc499a4325880
CMakeLists.txt                                    884e7ad1a49931146562c46951b8d0c62b0a68b5dde8a439a36d457f1318cc5a
```
