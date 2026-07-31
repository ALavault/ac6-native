# Cycle 390 — les deux témoins passent au vert, et révèlent mon erreur d'arithmétique

## 1. Le second témoin trouve la cause immédiatement

Ajouté au démarrage : journalisation de la valeur **effectivement lue** du cvar.

```
ac6_skip_texture_base = 0x03504400
```

Or je visais `0x03514000`. **Ma conversion en décimal était fausse** :
`0x03514000` vaut **55 656 448**, et non 55 591 936 — lequel vaut `0x03504400`,
exactement ce que le runtime a lu.

Trois cycles (387, 388, 389) ont donc omis des dessins liant une adresse
inexistante. Le cvar fonctionnait, l'analyseur acceptait le décimal, la sonde
était correcte : **seul le nombre que j'ai calculé était faux.**

Aucune des hypothèses envisagées — cvar non lu, hexadécimal rejeté, état non
atteint — n'était la bonne. Le témoin les a toutes écartées d'une ligne.

## 2. Avec la valeur correcte

```
ac6_skip_texture_base = 0x03514000   <- paramètre arrivé
skip fired : 6                       <- sonde déclenchée, 6 dessins omis
```

**Les deux témoins sont verts.** Le mécanisme fonctionne de bout en bout :
le paramètre arrive, la sonde reconnaît la texture, les dessins sont omis.

## 3. Ce qui bloque encore la lecture

```
deltas image : 22969, 19095, 20968, 21742, 20232, 26146, 19883
```

Cette exécution s'est fixée sur un **écran animé**, pas sur l'écran de
sauvegarde. L'effet de l'omission — au plus quelques milliers de pixels — est
noyé dans une animation qui en modifie vingt mille par seconde.

Il ne manque donc plus qu'**un état statique** pour lire le résultat.

## 4. Leçon

Deux témoins étaient nécessaires et je n'en avais posé qu'un. Le premier (cycle
388) prouvait que la sonde tournait ; il ne disait rien de ce qu'elle recevait.
Le second l'a dit, et a désigné une faute d'arithmétique que ni le code ni le
raisonnement ne pouvaient révéler.

**Un paramètre est un canal comme un autre : il doit être prouvé arrivé, pas
supposé.**

## 5. État

Outil correct, deux témoins verts, mesure toujours non lue faute d'état statique.
L'attribution du cycle 385 reste non vérifiée.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
