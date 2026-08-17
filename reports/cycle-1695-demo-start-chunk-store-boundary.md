# Cycle 1695 — absence de store guest hors pile dans le shim START

## Verdict

Une capture opt-in encadrée par l’appel qualifié `0x820E7E08 -> 0x820E1F78`
a enregistré 175 stores à tick 268. Tous appartiennent à la pile guest
`[0x7F040408,0x7F040770)` (prologues, cadres temporaires et sauvegardes de
registres). Aucun store ne touche l’objet `0x2E3D3D14`, sa vtable, ni une
plage guest hors pile. La transition START franchit donc le stub sans writer
guest-owned d’état persistant observable dans cette fenêtre.

## Identité

| élément | valeur |
|---|---|
| cible | `Default.xex` PAL démo, SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| basefile | SHA-256 `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| binaire instrumenté | `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp`, SHA `58cc4bad9296560a101b437d5c1e7a39a3cb4008eecf3293fe66539bcfe80e67` |
| hook | `AC6_DEMO_WATCH_CHUNK_TARGET_STORES=1`, actif uniquement pendant la cible |
| source hook | `src/guest_bridge.cpp`, SHA `327a3fcebf51534d8055525e85825aea3df0f6148056a150625f19e88d0b7ce1` |
| cap | 8 192 lignes; 175 lignes observées |

Le hook ne modifie pas la mémoire. Les reports/RTPLY de la route START restent
ceux du cycle 1694; seul le stderr ajoute les lignes de capture.

## Capture

| artefact | valeur |
|---|---|
| route | START tenu ticks 252–267, process/store frais |
| tick | 268 uniquement |
| RTPLY SHA-256 | `e1e99f68d7a3f04de87d8d4244eea137d9306265797b9f42ef0bc9c30185993e` |
| rapport SHA-256 | `dddbdc3d71169559449b5e69bc6dc5150a174fd3dcb4e0528b5089ce8e56d15b` |
| stderr SHA-256 | `acdeb87e1f2707e8643f6c571f546f550d9f0fc52989e3eb9f5149d834634c6d` |
| stores | 175 |
| plage minimale/maximale | `0x7F040408..0x7F040770` |
| hors pile | **0** |
| vers objet/vtable | **0** |

Les lignes contiennent les LR et fonctions générées de sauvegarde/restauration
(`__savegprlr_*`) ainsi que les cadres de `0x820E1F78` et de ses callees; elles
ne constituent pas une preuve de sémantique métier.

## Qualification

- `demo-qualified` : borne exacte du hook, tick/thread, 175 stores et absence
  de store hors pile dans la portée dynamique observée.
- `demo-observed` : le stub et ses callees exécutent des cadres temporaires,
  puis la route continue jusqu’à 600 ticks.
- `xenia-generic` : aucun élément.
- `unknown` : éventuels stores avant/après la portée du shim, état frontend,
  pixels, audio, mission et terminal.

## Garde et prochain test

Le hook reste désactivé par défaut et doit rejeter toute adresse hors de la
pile bornée si réutilisé. Le prochain test doit suivre les trois arêtes START
du tick 268 sur une fenêtre guest plus large (writer avant l’entrée et premier
consumer après le retour), plutôt que d’attribuer les stores de pile à un état
de menu. Aucun screencap ou fallback visuel n’est justifié.

