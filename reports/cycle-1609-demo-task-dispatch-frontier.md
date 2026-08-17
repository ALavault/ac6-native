# Cycle 1609 — dispatcher de tâches et mode actif

## Résultat

Le callsite `0x82259D74` est qualifié par le code Xenon littéral :
`r3 = object`, `r11 = *(object+0)`, cible `*(r11+0x10)`, donc slot 4 exact.
Le runtime vérifie fail-closed que l'objet est mappé, que sa vtable est mappée
et que ce slot contient l'unique cible observée avant d'enregistrer le tuple.
Il ne modifie aucune mémoire guest.

Au tick 252, les trois objets parcourus par `0x82259D10` sont :

| objet | vtable | RTTI local | slot 4 |
|---|---|---|---|
| `0x2E7F0080` | `0x8201130C` | `CModeTaskStartUpDemoOffline` | `0x8218A4A0` |
| `0x18BA2BF4` | `0x8200F388` | `CTaskLoading` | `0x8218CE20` |
| `0x18980000` | `0x82011694` | `CTaskModeManager` | `0x821929A8` |

Les noms RTTI proviennent des descripteurs exacts de l'image qualifiée ; les
adresses, slots et graphes proviennent de l'atlas démo. Aucun nom ou byte du
C++ généré n'est promu comme sémantique.

La conclusion est négative mais discriminante : START est pollé pendant
`CModeTaskStartUpDemoOffline`. Les constructeurs/vtables de
`CModeTaskDemoBase` (`0x8200C904`, update `0x82170F58`) et
`CModeTaskMissionTitle` (`0x8200E5C4`, update `0x82185198`) ne sont pas encore
atteints. Le consumer manquant n'est donc pas un trou du hook d'entrée.

## Frontier scheduler suivant

Après le dispatch, le thread primaire entre dans `0x820FF8D8` et attend que
les indices de la file de commandes guest à base `0x82386CC0` convergent. Son
appel virtuel slot 12 cible `0x820FF710`; le worker dédié a bien été créé avec
le même paramètre et est entré dans `0x820FFCA0`. Le prochain test doit
capturer les deux indices `+0x60D0/+0x60D4`, le writer et le premier reader à
chaque préemption, puis déterminer si la file progresse ou si une primitive de
synchronisation est incorrecte. Aucun forçage de compteur n'est admis.

## Validation

- probe START frais : 253 ticks, 115 PRESENT, frontend faux ;
- movie XAM strict : `6ff80573…e95` ;
- rapport avec objets/vtables/slots qualifiés : `35b980fc…f07b2` ;
- CTest codegen OFF : 14/14 ;
- CTest codegen ON : 13/13 avant le dernier enrichissement, build ON réussi
  après celui-ci ;
- hypothèse rejetée : deux callsites `0x821A61F0/0x821A6268` chargeaient le
  slot 4 d'une interface globale mais ne passaient pas son objet en `r3`; ils
  ont été retirés du qualifieur.

La vidéo fournie par l'utilisateur est consignée dans
`analysis/demo/ac6-demo-external-video-reference.json`. Elle sera utilisée
uniquement pour l'ordre/timing visuel des jalons ; son contenu n'est pas une
preuve guest et n'est pas importé dans le projet.
