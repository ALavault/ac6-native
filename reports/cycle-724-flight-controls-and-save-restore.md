# Cycle 724 — contrôles de vol natifs, snapshot et déverrouillage Mission 2

Date : 2026-08-04  
Périmètre : fermer le raccord natif STANDBY→vol→progression sans force guest,
et vérifier qu'un snapshot de Mission 1 peut déverrouiller Mission 2 par le
même loader de ressources.

## Plan exécuté

1. Définir un contrat d'axes normalisés
   `pitch/roll/yaw/throttle/brake`, indépendant de SDL et de Vulkan.
2. Faire évoluer une pose native bornée et la reprojeter avec la caméra TCAM
   PAL déjà qualifiée.
3. Ajouter un format de sauvegarde `AC6S` versionné, big-endian et strict,
   puis une restauration transactionnelle de la progression.
4. Rejouer le chemin réel `DATA.TBL[9] → STANDBY → A`, terminer les deux
   objectifs de Mission 1, encoder/décoder/restaurer, puis sélectionner
   `DATA.TBL[10]` via le même manifest/PAC loader.

## Résultats PAL

```text
selector 1 / DATA.TBL entry 9:
  camera_path                                  Scene/dd01_01a/dd01_01a_01/Tcam__c01.mop
  world_objects / transforms                   16
  textured Scene parts / draw groups            115 / 2
  solid geometry parts / draw groups            91 / 1
  standby HUD readback                         300 pixels verts
  active scene_changed                         3 pixels à 128x128
  active HUD readback                          303 pixels verts
  native flight projection changed             1
  AC6S snapshot size                           28 octets
  restored Mission 2 available                1

selector 2 / DATA.TBL entry 10:
  même manifest/runtime/frontend               true
  active scene_changed                         166
  active HUD readback                          300 pixels verts
```

Le mouvement observé est la preuve du contrat natif
`axes → état → caméra → projection`. Il ne constitue pas une identification
des constantes ou de la physique de vol retail. Le snapshot conserve les
missions terminées et les masques d'objectifs; il ne prétend pas être le format
de sauvegarde Xbox 360.

## Validation

```text
targeted CTest : 6/6 pass, 2.31 s
full CTest PAL : 61/61 pass, 63.82 s
```

Les tests couvrent le codec (`AC6S`), les erreurs de restauration, la
progression générique, le runtime, le contrat de vol et la soumission Vulkan
retail. Le full CTest a été exécuté après la correction de formatage de la
restauration.

## Ce qui reste ouvert

- Les axes ne sont pas encore alimentés par la boucle SDL/gamepad ni présentés
  dans une vraie swapchain; le backend headless reste le seul chemin démontré.
- La mission 1 n'a pas encore de boucle générique de dégâts/objectifs/fin de
  vol interactive; la fixture valide des événements de progression déterministes.
- La sauvegarde `AC6S` est encodée en mémoire. Une I/O atomique et une reprise
  depuis disque restent à ajouter; aucune compatibilité avec un fichier retail
  n'est revendiquée.
- Mission 2 est déverrouillée et sélectionnable avec entry 10, mais son vol,
  rendu complet et sa complétion ne sont pas encore qualifiés.
- Le fallback géométrique solide ne prouve ni matériaux, ni profondeur, ni
  shaders Xenos; les avions blancs des cutscenes restent une frontière
  matériaux/texture à traiter.

## Hashes

```text
include/ac6/campaign_flight.h                   6a44a2ce0d8ca523a7fdb7363878503e91807f5c6213b6f39f5dc07de4d743f2
src/campaign_flight.cpp                          4a2f308a95dfcff2920d958293ac0411bb2bfc48181699b0b6f780e0bb18251d
tests/campaign_flight_tests.cpp                  7ef402d95525e3006961afc92745cd4301b152fc4b64ef64e0830a2e0a5ad9e4
include/ac6/campaign_save.h                      ddfc1d6a412fc44725acd621312dfbcd04e2de8a6b1b393ba25730103798aed1
src/campaign_save.cpp                             3090a188f59340c8a4eae0bee378663caabde176bc21141073299cee035130af
tests/campaign_save_tests.cpp                     4a0136d9bc5acf7d6f106d212dadf0031f792318b9a2ebf599ed1539bd9167a8
include/ac6/campaign_progression.h                9b56f9fe02e0ae1b873b816b80f9925727823067b7c66cdba607a055c27680fe
src/campaign_progression.cpp                     0d350ef2a70f62623fb15dc7dda769de6aec14485a0423bd02a81ec233013887
include/ac6/campaign_runtime.h                    b9c8146e85a670e6220c051ab9b49bb20a124921ff0642a255786e22e330fc85
src/campaign_runtime.cpp                           67144e13a1be73acfc1f2a24654faf30a92d673a877b1e5462902ff38d77b9d9
tests/campaign_progression_tests.cpp              dae8a88e45b7228250e475f9d6d1be0cd56822d17a12da36f90584b68c45b165
tests/campaign_runtime_tests.cpp                  a282180f332b5bfc83031e164b24ffed06b83ed5d283071ebf87a8af6122aa2c
tests/campaign_vulkan_retail_frame_tests.cpp      6b16b1bf6f3bd958ee55f28875f1ce07a8848ee54ca5224dc013bf7baaf8dd0a
CMakeLists.txt                                    117fd2fe35d85f0e500d5f12773f2d92c161e6a1d59050eef6a58ace006e3714
```
