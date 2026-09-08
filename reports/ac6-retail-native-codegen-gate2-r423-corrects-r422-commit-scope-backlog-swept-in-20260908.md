# AC6 retail NTSC-U/J — r423 — CORRIGE r422 : son commit `2c07b1ec` a balayé un arriéré de cycles antérieurs non committés (r239-r286) sur trois fichiers, pas seulement le correctif IRQL décrit dans son message

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Ce qui s'est passé

Le message de commit de r422 (`2c07b1ec`) décrit un changement ciblé
(le correctif IRQL). En réalité, trois des fichiers commités portaient
des changements NON COMMITTÉS de nombreux cycles antérieurs, restés en
attente depuis la situation d'arbre non committé documentée depuis
r239 — `git add`/`git commit` les a inclus silencieusement avec le
changement de r422, sans que le message de commit ne le mentionne :

| fichier | lignes réellement ajoutées | cycles cités dans le diff, au-delà de r422 |
|---|---|---|
| `tools/materialize_native_import_stubs.py` | 868 (dont l'édit de r422 : ~15) | r239, r240, r276, r278, r280, r282, r286 |
| `native/src/ac6recomp_main.cpp` | 81 (dont l'édit de r422 : ~13) | r239, r240, r255 |
| `tests/test_materialize_native_import_stubs.py` | 224 (dont l'édit de r422 : ~10) | r280, r281, r282, r285, r286 |

`native/include/ac6/native_guest_threads.h` et
`native/src/native_guest_threads.cpp` sont, eux, un cas différent :
ils n'avaient JAMAIS été committés depuis leur création (r277) — leur
apparition en `A` (ajouté) dans le commit est correcte, c'est leur
tout premier commit, mais leur contenu porte lui aussi l'historique
complet de r277 à r421, pas seulement r422's ajout.

## Pourquoi ce n'est pas alarmant en soi

- **Contenu cohérent et déjà vérifié** : chaque ligne ajoutée cite un
  numéro de cycle et une justification, dans le style habituel du
  projet. Les suites de tests exécutées par r422
  (`ctest` natif 10/10, `pytest tests/` 222/1-skip,
  `pytest tests/test_materialize_native_import_stubs.py` 105/105,
  gates + `ctest` racine) ont toutes tourné contre CET état combiné du
  code — pas contre un état hypothétique plus restreint — donc rien de
  ce qui a été committé n'est non vérifié.
- **Précédent déjà établi** : `git log` montre un commit antérieur,
  `e0afccdd`, explicitement intitulé *"catch-up: commit the
  uncommitted retail-native Gate 0-2 backlog (r2-r76)"* — un
  rattrapage d'arriéré assumé comme tel existe déjà dans l'historique
  de ce dépôt.

## Ce qui est fautif

Le message de commit de r422 ne décrit QUE le correctif IRQL,
laissant croire à quiconque lit `git log` que ce commit est un
changement ciblé de quelques lignes, alors qu'il contient un arriéré
de neuf cycles antérieurs sur trois fichiers. C'est une inexactitude
de traçabilité, pas un défaut de fonctionnement.

## Décision prise

- **Ne pas `git revert`/`amend`** : le contenu est cohérent, testé, et
  défaire ce commit remettrait ces fichiers dans l'état d'arriéré non
  committé qu'ils avaient avant — un pas en arrière, pas une
  correction. Documenter l'écart plutôt que le défaire correspond à la
  discipline du projet (corriger prédécesseurs ET soi-même, par nom et
  numéro de cycle, précédent CLAUDE.md) plutôt qu'une opération
  destructive pour un problème de message, pas de contenu.
- Nommer explicitement les cycles balayés (`r239, r240, r255, r276,
  r278, r280, r281, r282, r285, r286`) pour que quiconque cherche leur
  trace dans `git log` la trouve ici plutôt que nulle part.

## Non établi

- **Le contenu détaillé de chacun des cycles balayés** (r239-r286) —
  non audité ligne par ligne ce cycle ; leur présence est confirmée
  cohérente par les suites de tests déjà vertes, pas par une relecture
  individuelle de chacun.
- **S'il reste d'autres fichiers dans le même état** (modifications
  non committées de cycles antérieurs, prêtes à être balayées par un
  futur commit ciblé) — l'état global de l'arbre (~200+ fichiers non
  suivis/modifiés selon les constats de début de session) reste la
  décision de l'utilisateur, non touchée ce cycle.

## Gate

Aucune source de production éditée ce cycle (constat et correction de
traçabilité uniquement — le contenu du commit `2c07b1ec` lui-même
n'est pas modifié).

## Files

Aucun.
