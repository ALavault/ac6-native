# Cycle 1803 — Edge ferme la jonction Q vers le renderer

Le HIR déjà capturé par Xenia Edge sur la démo PAL qualifiée résout l'artefact
de décompilation Ghidra autour de `0x8232710C`. Ce symbole est le prologue
`__savegprlr_29`; `0x822DA568` conserve directement son callback d'entrée.

Pour le record naturel type 1, la table sélectionne exactement
`0x821187A8`. Q est stocké à `node+0x0C`, puis remis inchangé à ce callback par
`0x822E35E8`. La frontière native n'est donc plus l'ordonnancement Q→callback,
mais le chemin postérieur vers un kickoff GPU effectif.

Les captures natives restent uniformément noires et ne constituent qu'une
preuve négative. `supported=false` reste inchangé.

Preuve : `artifacts/goal-playable/q-to-callback-static-20260824/RESULT.md`.
