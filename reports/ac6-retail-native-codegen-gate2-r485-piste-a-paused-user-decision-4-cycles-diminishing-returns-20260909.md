# AC6 retail NTSC-U/J — r485 — Piste A mise en pause sur décision utilisateur explicite : 4 cycles (r481-r484) de progrès réels mais négatifs, aucune nouvelle capture de cible depuis r477

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle lancé ce cycle (décision de pause, pas d'investigation).

## Contexte

Nommé par r484 : Piste A est maintenant à 4 cycles consécutifs
(r481-r484) sans nouvelle capture des 3 états cibles de r478
(`09dd1c7cddae1141`→primitive_type=0x01,
`57b8e5f14b93cff4`→primitive_type=0x08, `4dd456c4ea0923c1` absent
sous toute modification). r484 a explicitement recommandé un point de
décision utilisateur plutôt que de continuer seul, même raisonnement
que la pause de r480. Question posée : pause, ou une session Ghidra
ciblée sur l'attente « movie worker », ou autre chose. **Réponse de
l'utilisateur : pause.**

## Établi — bilan de la chaîne r481-r484

- r481 (`5b056f30`) : `--mission-dump-shaders` découplé de la route
  scellée longue, confirmation indépendante du motif r477/r478 (les 2
  nuanceurs connus existent sous la mauvaise modification, le
  troisième reste introuvable).
- r482 (`53625ef5`) : hypothèse du minutage de préambule réfutée par
  une preuve directe (préambule identique à une route qui réussit,
  échec identique).
- r483 (`b85b99ca`) : un motif de stabilisation différent (celui que
  `run_gate.py` utilise lui-même en interne) lève le blocage « movie
  worker » une fois.
- r484 (`a3eb826e`) : re-vérification sur 3 exécutions connues (r483 +
  2 nouvelles) — 2 réussites, 1 échec. Motif confirmé **flaky**, pas
  fiable. Toujours aucune des 3 cibles capturées.

Chaque cycle a produit une preuve directe et vérifiable (pas une
supposition non testée), et chacun a respecté sans exception la
contrainte de coordination : zéro écriture sous
`recompilation/ace-combat-6-retail/native/`, `git status` vérifié
inchangé sous ce répertoire avant et après chaque cycle.

## Établi — état de la session concurrente au moment de la pause

`native/` est calme depuis **3h30+** au moment de ce rapport (dernière
écriture 2026-09-09 05:07:55, ce rapport rédigé après 08:38) — très
au-delà de tout écart observé dans le cadencement de cette autre
chaîne (3-15 min entre r482 et r489). Ceci suggère fortement (sans le
confirmer formellement — aucun contact direct établi avec la session
propriétaire malgré deux tentatives de coordination lors de la reprise
de piste A) que cette session est terminée ou en pause stable. Ce fait
est consigné pour la prochaine décision ; il n'est PAS agi ce cycle
(aucun merge, aucune modification sous `native/`).

## Décisions prises

- **Piste A mise en pause**, sur décision utilisateur explicite,
  répondant à la question posée par ce cycle. Ne pas relancer sans
  nouvelle décision utilisateur.
- Ne pas fusionner les traductions déjà obtenues (12 paires par r481,
  aucune des 3 cibles) dans le registre épinglé — bénéfice marginal
  déjà noté par r482, et `native/` reste la propriété d'une autre
  session tant qu'un commit ou une pause stable confirmée n'est pas
  établi formellement.
- Consigner l'observation de calme prolongé (3h30+) sans agir dessus —
  une décision distincte (reprendre piste A, tenter le merge différé,
  ou toute autre piste sous `native/`) reste à prendre par
  l'utilisateur.

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts, inchangés — aucune source de contrat touchée. `ctest`
natif délibérément non relancé (`native/` toujours propriété de
l'autre session). `git status --porcelain` sous
`recompilation/ace-combat-6-retail/native/` confirmé identique aux 4
cycles précédents (mêmes 7 fichiers modifiés par l'autre session,
aucun changement introduit par cette chaîne).

## Named for r486

**Piste A en pause, sans date de reprise fixée.** Candidats pour une
reprise future, aucun tenté :
1. Une session Ghidra ciblée sur le désassemblage invité autour de
   `0x82916E2C`/`0x82916E3C`/`0x82916E08` (le mécanisme d'attente
   « movie worker ») — seule piste qui attaquerait la cause
   plutôt que de mesurer le symptôme, nécessite un budget hors cycle
   oracle.
2. Une fois `native/` confirmé stablement libre (commit ou pause
   confirmée par contact direct avec l'autre session), envisager le
   merge différé des 12 traductions de r481 dans le registre épinglé —
   bénéfice marginal mais sans coût une fois `native/` libre.
3. La piste `fetch_const` du HUD de vol, nommée par r475 mais jamais
   suivie, indépendante du blocage « movie worker ».

## Files

Committé : ce rapport, `NEXT.md` (nouvelle entrée ajoutée sans
perturber le pointeur actif de la session concurrente ni la section
historique). `reports/handoff/CURRENT.json` délibérément non touché
(pointeur actif de l'autre session). Aucun fichier sous
`recompilation/ace-combat-6-retail/native/` modifié. Scratch
(`/fastdata/lavaulta/tmp/r484-verify-*`) nettoyé, non conservé.
