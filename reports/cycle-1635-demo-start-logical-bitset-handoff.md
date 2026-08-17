# Cycle 1635 — handoff logical START et borne menu

## Résultat

Le même replay START sous le gate A/B `rr` joint la sortie normalisée au
bitset logique guest :

| Étape | PC / symbole | Adresse | Valeur logique |
|---|---|---|---|
| normalisation | `0x82198DD0` | `0x827B37E0` | `0x400` |
| mapper/edge | `sub_821DE990` → `sub_821DE6E0` | — | — |
| pressed tick 252 | `0x821DE6F8`, `91 03 0E 4C` | `0x82798488` | `0x10` |
| pressed tick 253 | même writer | `0x82798488` | `0` |

Le mapping statique exact relie le normalized START `0x400` au binding index 4
et au masque logique `0x10`. La preuve dynamique confirme l'écriture et son
effacement au poll suivant. La chaîne input est donc fermée jusqu'au bitset
logique, mais pas jusqu'au menu.

Un probe GDB sous le même `rr` pose des breakpoints aux entrées
`0x82170F58` et `0x82185198` (tests `0x82170FCC`/`0x82185210`). Aucun hit n'est
observé jusqu'au tick 254 avant le trap contrôlé de finalisation XAM. Aucun
effet de tâche, écriture de scène ou transition visuelle n'est attribué ; cette
absence ne couvre pas les ticks ultérieurs.

## Qualification

- `demo-qualified` : normalisation, mapping, writer pressed, valeurs ticks
  252/253 et bytes PAL ;
- `static-exact` : deux branches menu candidates ;
- `demo-observed-no-hit-through-tick-254` : probe rr des entrées de fonctions ;
- `unknown` : construction/dispatch des tâches menu, écriture guest causale,
  readback et résultat de mission.

Reçu enrichi : `analysis/demo/ac6-demo-start-newpress-rr-provenance-v1.json`.

Politique inchangée : cible PAL démo SHA `de917873…5da8`, rr local commit
`7352eb80…dbfb0`, aucune preuve retail ni actif propriétaire suivi.

## Prochain checkpoint

Instrumenter uniquement les deux frontières candidates `0x82170FCC` et
`0x82185210` sur un replay START frais. Un hit doit fournir PC/LR/thread/tick,
objet tâche et première écriture guest ; l'absence de hit doit rester une
borne dynamique explicite et renvoyer au dispatcher de tâches.
