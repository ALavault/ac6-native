# Cycle 1036 — frontière du corpus retail scénario

Date : 2026-08-06.

## Provenance

- XEX PAL : `game-files/default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Index : `game-files/DATA.TBL`, 14 824 octets, 926 entrées, SHA-256
  `82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5`.
- Méthode : extraction officielle `tools/extract_ac6_pac.py --decompress`,
  lecture bornée des 926 entrées, fermeture FHM et scan ASCII/UTF-8/
  UTF-16LE/UTF-16BE. Le scan a utilisé 32 workers CPU. Aucun payload retail
  n’est ajouté au dépôt.

## Résultat exhaustif

- Les 926 racines décodées sont `FHM `; aucun root non-FHM n’a été promu.
- Aucun token exact `SubMisTbl`, `ComTbl` ou `Maneuver` n’est qualifié dans
  le corpus, quelle que soit l’encodage testé.
- Les occurrences pertinentes de `SubMis` conduisent aux entrées 187 et 191.
  L’entrée 187 est une fermeture UI/free-mission (`FHM` imbriqués, 45 `NTXR`,
  1 `SWG`), avec `freeMission`, `objNum`, `objRank` et des getters de score;
  elle ne contient pas une table d’ordres ou de vagues. L’entrée 191 reste
  également une fermeture UI déjà inspectée, sans propriétaire scénario.
- L’entrée 163 (payload décompressé, 7 836 352 octets, SHA-256
  `5025480f9ed4fc157b7c679b340d7b7069705385d0e0688982275aff6228dea4`) est
  une fermeture `FHM + 50 NSXR` de base shader, avec des noms comme
  `ImpactShockWave`; `Wave` n’y est pas une définition de vague.
- L’entrée 230 (2 482 176 octets, SHA-256
  `7160dc3d05c988aa5bfb098fde667c9c7ab09e2b4af1dc3fbe69e60f759ebd5a`) est
  une fermeture UI debrief (`FHM + 13 NTXR + 1 SWG`) contenant
  `DEB_*` et les getters de score/destructions; elle ne fournit pas les
  objectifs de Mission 01.
- Les marqueurs isolés `Obj`/`Act`, les noms génériques `Mission`/`Wave` et
  les packs RIFF/XMA ne sont pas promus sans propriétaire FHM et décodage
  lossless des champs.

## Contrôle statique complémentaire

Dans le projet Ghidra canonique `ghidra-projects/ace-combat-6`, la chaîne
d’erreur `SubMisTblBin` à `0x8200F5A8` n’a aucune référence et aucune
matérialisation PPC split dans une fenêtre de 64 octets. Cela qualifie
uniquement cette chaîne comme non-reliée dans le projet canonique; ce n’est
pas une preuve que toutes les fonctions de parsing sont mortes.

La grammaire de nœuds little-endian qualifiée pour AC5 a aussi été exécutée
comme contrôle négatif sur les entries AC6 0, 9 et 119 : zéro candidat. Elle
ne doit donc pas être transférée à AC6 par analogie.

## Décision

Cette expérience ferme la piste « noms textuels ou grammaire AC5 donnent
directement les vagues/objectifs AC6 ». Elle ne ferme aucun gate J1. La
prochaine expérience utile est dynamique et étroite : capturer le créateur ou
le registre d’unités au moment d’une publication de vague, avec identité et
transition d’objectif. Le census générique `0x822707C8` du cycle 1035 ne doit
pas être relancé sans ce nouveau point d’observation.
