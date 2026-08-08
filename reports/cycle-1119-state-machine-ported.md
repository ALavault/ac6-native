
# Cycle 1119 — la machine à états, portée

Date : 2026-08-09. Le plus gros des trois comportements ouverts de JF.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- **Statique et produit natif seuls.** Aucun oracle.

## Ce qui est porté

`include/ac6/retail_state_machine.h`, `src/retail_state_machine.cpp`.

**L'alphabet de signaux**, aux valeurs lues à leur position dans `0x8219AD20`
et confirmées sur le gestionnaire `0x822E8660` au cycle 1110 : `-1` sortie,
`-3` entrée, `-4` interrogation du sur-état, `-5` transition initiale. Le
signal `-2`, observé et jamais modélisé, est nommé comme tel dans l'en-tête.

**La topologie** — les 36 états, chacun avec son adresse de gestionnaire, son
sur-état et son code d'entrée. Elle est **compilée dans la source**, pas lue à
l'exécution : c'est une structure de code récupérée du binaire, pas du contenu
de mission ; la charge utile n'a pas voix au chapitre. Le fichier porte donc les
36 adresses, ce qui satisfait le contrôle de dérivation de JF par construction
plutôt que par décoration.

**L'algorithme** de `0x8219AD20` : remonter les deux chaînes, trouver l'ancêtre
commun, sortir en remontant la branche courante, entrer en descendant la
branche cible.

## Ce que le test vérifie, et pourquoi ainsi

Un test qui ne comparerait que l'état final passerait sur une machine qui
n'exécute aucun gestionnaire. Celui-ci compare **l'ordre des étapes** :

| transition | attendu |
| --- | --- |
| frères `0x822EACC0` → `0x822EB090` | une sortie, une entrée, **rien sur le parent commun** |
| deux niveaux de part et d'autre | sortie `0x822E6E98`, sortie `0x822EACC0`, entrée `0x822ED070`, entrée `0x822E6CC0` |
| vers un descendant | aucune sortie, une entrée par niveau franchi |
| vers un ancêtre | des sorties seulement |
| vers soi-même | rien |
| gestionnaire inconnu | aucun plan, pas une supposition |

Et les invariants du cycle 1112 sont **re-mesurés depuis la table compilée**, de
sorte qu'une table éditée à la main ne puisse pas contredire silencieusement le
cycle qui l'a établie :

```
retail_state_machine states=36 depth=6 coded=11
```

acyclique, une seule racine `0x822E39A8`, et `0x822E7760` publiant le code 8 —
le fait lu directement sur sa branche d'entrée.

## Ce que cela n'établit pas — deux limites à ne pas laisser passer

1. **Les gestionnaires ne sont pas portés.** Ce cycle porte le *moteur* et la
   *topologie* ; les 36 états retail ont des effets propres — écritures de
   champs, appels de sous-systèmes — dont rien n'est reproduit. `plan_transition`
   rend la **suite d'invocations** qu'il faudrait faire, pas leurs effets. C'est
   honnête et c'est incomplet.
2. **La descente initiale `-5` n'est pas implémentée.** La table n'a pas de
   colonne « sous-état initial » : l'extracteur du cycle 1112 n'a jamais résolu
   les écritures vers `+0x350`, et j'ai préféré une colonne absente à une
   colonne devinée.

S'y ajoute que **25 des 36 codes d'entrée restent non attribués par branche** —
le cycle 1112 le disait déjà, la table le porte tel quel avec `-1`.

## Effet sur JF

```
avant : open=mission_state_machine,playable_session,mission_completion
après : open=playable_session,mission_completion
```

Six comportements dérivés sur huit. `ctest 20/20`.
