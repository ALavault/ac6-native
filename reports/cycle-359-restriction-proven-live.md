# Cycle 359 — la restriction par hachage est prouvée vivante ; le test reste à conclure

## 1. Contrôle de vivacité : concluant

Une ligne journalisée à l'intérieur de la branche restreinte :

```
[ac6-forcetex] forcing white sample in ps=8F1C48BA92C8E43E
```

Elle apparaît **une fois** — la traduction d'un shader n'a lieu qu'une fois — et
nomme **exactement** le shader visé.

**La restriction par hachage fonctionne.** L'anomalie du cycle 358 — sortie
identique au forçage global — n'était donc pas une restriction morte : c'était
une exécution ratée dont l'image ne rendait rien d'exploitable. Le contrôle a
tranché en une ligne ce que le raisonnement ne pouvait pas.

Preuve corroborante : avec la restriction active, l'image n'est plus le lavis
blanc du cycle 357 mais une image réelle — moyenne RVB `[36.3, 54.7, 54.9]`,
**42 183 couleurs** contre 26.

## 2. Le test lui-même : toujours pas conclu

L'exécution a atteint la **cinématique d'attrait** (un chasseur en vol), pas
l'écran de sauvegarde. Cause : j'ai fusionné chauffe et test en une seule
exécution de 115 s, et l'écran-titre sans entrée bascule dans sa démo — le piège
signalé par l'opérateur au cycle 356 et déjà documenté.

Le protocole correct existe et a fonctionné au cycle 356 ; il ne doit pas être
raccourci :

```bash
rm -rf build-rt/cache
# 1) exécution de CHAUFFE, ~115 s, puis on la tue
# 2) exécution de TEST, cache chaud, entrée à 33 s
```

## 3. État exact de la question

| | statut |
|---|---|
| restriction par hachage | **prouvée vivante (ici)** |
| couleur de sommet nulle | réfutée par test contrôlé (356) |
| échantillon de texture = facteur nul | établi par déduction (356) |
| forçage de l'échantillon à 1.0 sur le shader visé | **outil prêt, résultat non lu** |

Il ne manque qu'une exécution en deux temps pour lire le résultat : si la couche
apparaît en aplats blancs, la faute est dans l'échantillonnage ou le décodage
`k_DXT4_5` ; sinon, la perte n'est pas dans le produit et il faut remonter à la
géométrie de la passe.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
