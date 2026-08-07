
# Cycle 1116 — le gate J2 est vert, et ce qu'il ne couvre pas

Date : 2026-08-09.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Charge utile Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- **Statique et produit natif seuls.** Aucun oracle : la politique du goal J1,
  reconduite, n'a toujours pas dépensé un seul passage N3.

## La condition de fin

```
ctest --test-dir reconstruction/ace-combat-6/build-core \
      -R "ac6-retail-(playable|classmap)"
→ 100% tests passed, 0 tests failed out of 2
```

Les deux tests sont **présents et non sautés**. Suite complète : **19/19**.

## Piste jouable

`tests/retail_playable_tests.cpp` n'a qu'une entrée : le conteneur de scénario.
Il construit le monde par `build_retail_world`, qui produit une unité de combat
par enregistrement — faction et classe comprises —, un objectif `Manual` par
sous-mission, et la table de compteurs dimensionnée par le slot 1. Puis il fait
tourner 1 800 pas, **deux fois, depuis deux constructions indépendantes**, et
exige une empreinte sémantique identique.

```
retail_playable units=230 ticks=1800 hash=4efb07a0a1d370a6
```

Il vérifie aussi ce que le goal interdisait : aucune unité détruite par le
temps qui passe, aucun objectif complété tout seul, et l'existence d'un
manifeste ne change **rien** au monde construit.

## Piste carte

`analysis/class-map.tsv` — 811 vtables, 721 classes — et
`tools/audit_ac6_class_map.py` qui refuse une carte laxiste : nom décoré
obligatoire, gabarit gardant son nom manglé plutôt qu'un joli nom faux, au
moins une base par ligne, et six classes d'ancrage dont l'absence fait échouer.

## Les deux écarts, que la commande ne voit pas

Le gate est une commande ; la phrase du goal est plus large. Je note les deux
endroits où elle l'est, plutôt que de laisser le vert parler à ma place.

**1. « se joue » est prouvé au sens du contenu et du déterminisme, pas de la
session.** Le test construit et fait tourner le monde ; il ne passe pas par la
boucle de session du produit, ni par l'entrée, la caméra ou le HUD — ceux-là
sont couverts par J0, mais sur manifestes. Ce que J2 démontre, c'est que le
**contenu** vient de la charge utile et que l'exécution est reproductible ; pas
qu'on tienne une manette.

**2. « toute vtable » est en réalité « toute vtable qui résout ».** Le balayage
nomme 811 tables et **rejette 1 619 candidats** qui échouent à une étape. Ils
sont comptés et non nommés — c'est la discipline demandée — mais la phrase
« toute vtable du binaire » n'est pas établie tant que ces 1 619 ne sont pas
expliqués.

**Et une limite de fond** : les **positions monde** restent des décalages
relatifs d'`Obj`. Le verrou VMX128 a été levé au cycle 1115, le corpus Xenon se
construit, mais `0x822953F0` n'a pas encore été lu. Le monde du test est donc
peuplé et déterministe, pas encore spatialement fidèle.

## Ce qui reste, si le jalon doit être resserré

1. Lire `0x822953F0` dans le corpus Xenon et décider si l'ordre d'étiquette 2
   porte des coordonnées monde ; si oui, les câbler à la place des décalages.
2. Faire consommer `build_retail_world` par la commande de session de
   `ac6-native`, pour que « se joue » couvre entrée, caméra et HUD.
3. Expliquer les 1 619 rejets du balayage, ou resserrer le critère.
