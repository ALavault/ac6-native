# Cycle 1636 — A/B START étendu et borne menu tick 300

## Résultat

Un A/B distinct direct/sous `rr` est maintenant acquis jusqu'au tick 300,
depuis des stores neufs, avec le même binaire codegen ON et le même movie XAM.

| Artefact | Direct | `rr` |
|---|---|---|
| RTPLY | `0b5ffbbd…85182` | `0b5ffbbd…85182` |
| rapport | `e3f6b8e2…9785` | `e3f6b8e2…9785` |
| PRESENT | 163 | 163 |
| frontend | false | false |
| mission | false | false |

Le probe GDB utilise exclusivement le `rr` local épinglé. Les breakpoints aux
entrées `0x82170F58` (`CModeTaskDemoBase`) et `0x82185198`
(`CModeTaskMissionTitle`) produisent zéro hit jusqu'à la fin contrôlée
`max-ticks=300`. Le log du probe est sous `TMPDIR`; aucune instruction runtime
ou donnée propriétaire n'est ajoutée au dépôt.

Cette borne renforce le constat précédent : l'entrée START est bien injectée,
normalisée et mappée au bit logique, mais aucune des deux frontières menu
statiques n'est construite/atteinte dans cette route. La transition visuelle,
le premier frame gameplay et la mission restent ouverts.

## Reçu et politique

Reçu enrichi : `analysis/demo/ac6-demo-start-newpress-rr-provenance-v1.json`.
La preuve A/B neutral existante reste inchangée. Aucun retail, Xenia, Ghidra,
C++ généré, microcode ou actif propriétaire n'est fusionné ou suivi.

## Prochain checkpoint

Remonter le dispatcher exact qui devrait construire ces tâches, ou qualifier un
nouveau appel de tâche à partir de l'atlas reachability. Toute nouvelle route
doit conserver un A/B frais avant d'être jointe au renderer.
