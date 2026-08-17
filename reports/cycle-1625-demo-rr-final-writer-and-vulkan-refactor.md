# Cycle 1625 — writer final rr et refactor Vulkan

## Résultat

Le reçu `rr` distingue désormais l'écriture initiale de la fenêtre
`0x1274A000` de son producteur final. Le dword capturé `0x00000D02` est écrit
par `sub_821B0D20`, PC guest `0x821B0D70`, instruction
`stwu r10,4(r11)`, bytes PAL `95 4B 00 04`. L'ancien PC `0x821BAE5C` reste une
preuve valide de première écriture de fenêtre, mais n'est plus attribué comme
producteur final.

Le dernier dword reste joint à `sub_821B9F70`, PC `0x821BA01C`, bytes
`94 CA 00 04`. La publication ring reste jointe à `sub_821B9BC8`, premier
store PC `0x821B9D24`, bytes `7D 2A C1 2E`. Les trois instructions sont
recoupées dans le basefile PAL SHA-256 `b98a9ac1…4218` du même XEX.

Le helper des quatre descripteurs shared-memory a été extrait de `main.cpp`
vers `vulkan_shared_memory.cpp`. La création, l'initialisation intégrale à zéro,
la copie guest bornée et le commit des descripteurs restent transactionnels.
`main.cpp` respecte de nouveau sa limite exacte de 1200 lignes.

Deux neutral Vulkan depuis des stores frais restent byte-identiques après le
refactor : RTPLY `c5357c6d…c5794`, rapport `04116bf6…de35`, quatre descripteurs.
START n'a pas été exécuté.

## Validation

- builds codegen OFF/ON : PASS ;
- CTest OFF 18/18 et ON 17/17 sous Xvfb avec audio dummy : PASS ;
- deux imports/replays neutral Vulkan frais : byte-identiques ;
- JSON, audit source et budget de complexité : PASS.

Aucune preuve retail n'est fusionnée. Aucun checkout oracle, projet Ghidra,
C++ généré, microcode ou actif propriétaire n'est modifié ou suivi.

## Prochain checkpoint

Construire les cinq payloads de constantes pour chacun des deux rectangles.
Les buffers bool/loop et fetch sont des copies exactes de registres selon
ReXGlue générique ; le buffer système reste fail-closed jusqu'à qualification
de chaque champ effectivement lu par les quatre SPIR-V atteints.
