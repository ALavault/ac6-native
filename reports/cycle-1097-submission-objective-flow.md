# Cycle 1097 — le flux d'objectifs : les sous-missions et leur script

Date : 2026-08-08. Le second domaine ouvert, `retail_objectives`, en statique.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- **Statique seul.**

## L'ancrage : `contexte + 0x264`

Le chargeur publie l'enregistrement de scénario dans le contexte :

```c
piVar14 = piVar9 + 0x495037;   // contexte + 0x12540DC, l'enregistrement racine
piVar9[0x99] = (int)piVar14;   // contexte + 0x264
```

Tous les consommateurs passent par là. Un balayage de chaînes de deux charges
(`lwz rX,0x264(rY)` puis `lwz rZ,disp(rX)`) rend 65 sites et six déplacements
seulement : `0x00`, `0x04`, `0x08`, `0x0C`, `0x18`, `0x20`, `0x28`. Ce sont les
slots de la racine, et ils suffisent à cartographier le chargeur :

| déplacement | slot | contenu Mission 01 | dérivé alloué |
| --- | ---: | --- | --- |
| `+0x08` | 1 | 339 entrées, compte `u16` | `contexte+0x5C` = compte × `0x14` |
| `+0x0C` | 2 | **4 sous-missions** | `contexte+0x2C` = compte × 4 |
| `+0x18` | 5 | 4 factions | `contexte+0x58` = compte × `0x44` |
| `+0x20` | 7 | 3 entrées | passé à `0x82266EF0` |

Les trois allocations sont contiguës dans `0x8219BDD8` et chacune lit son
compte dans l'octet ou le mot de tête du slot correspondant. **Le tableau des
sous-missions et le tableau des compteurs de mission sont donc dimensionnés par
les données analysées elles-mêmes.**

## Sélectionner une sous-mission — `0x8226E908`

```c
undefined8 FUN_8226e908(int contexte, int index) {
  *(int *)(contexte + 0x10) = index;    // la sous-mission courante
  *(int *)(contexte + 0x14) = 0;        // le pas courant, remis à zéro
  compte = *(byte *)*(*(contexte + 0x264) + 0xc);   // le compte analysé
  if (compte <= index) return 1;        // plus de sous-mission : fin
  return Function_8226E158();
}
```

La borne est **l'octet de compte du slot 2**, c'est-à-dire `4` pour la
Mission 01. `contexte+0x10` est l'index de sous-mission ; `contexte+0x14` est
l'index de pas.

## Exécuter un pas — `0x8226E158`

```c
liste = *(int *)(*(int *)(*(int *)(slot2 + 4) + contexte[0x10] * 0x10 + 8) + 4);
pas   = liste + contexte[0x14] * 0x28;
contexte[0x9a] = pas;                    // contexte + 0x268
switch (*(u8 *)*pas) { ... }             // l'étiquette du pas
```

La foulée `0x28` et l'union à dix étiquettes sont **exactement la liste sans
nom** que le cycle 1092 avait dû modéliser sans pouvoir la nommer, faute de
chaîne d'erreur dans tout son sous-arbre. Elle a maintenant un nom fondé sur
son consommateur : **c'est le script d'une sous-mission**, une suite de pas
étiquetés exécutés dans l'ordre.

Ce que fait l'étiquette 0, dans l'ordre du code :

```c
*(int *)(contexte[0xb] + contexte[0x10] * 4) = contexte[0x19];  // horodatage
pfVar3 = **(float ***)(pas + 4);
FUN_82268b28(pfVar3[0], pfVar3[2], pfVar3[1], pfVar3[3], contexte);
contexte[0x10..0x11] = classement de l'octet pfVar3+0x43;
contexte[0xC..0xF]   = pfVar3[4..7];      // deux paires de bornes
si pfVar3[8] > 0     → FUN_822667C8;
FUN_822562B0(pfVar3[9], ...);             // une durée
drapeaux depuis (uint)pfVar3[0xC] bits 0,1,2,4,5,6,10;
```

La première ligne est le point qui compte : **le pas écrit l'instant courant
(`contexte+0x64`) dans `contexte+0x2C[index de sous-mission]`**, le tableau
dimensionné par le compte de sous-missions. Et `0x82267008` le relit :

```c
bool FUN_82267008(double seuil, int contexte, int index) {
  ... bornes sur l'index contre le compte analysé ...
  return seuil <= (contexte->float(0x64) - *(float *)(*(int *)(contexte+0x2c) + index*4));
}
```

soit « le temps écoulé depuis le démarrage de la sous-mission `index`
dépasse-t-il `seuil` ». **Une sous-mission a donc un instant de démarrage
horodaté et un test de durée.**

## La condition d'objectif — l'étiquette 7

```c
puVar4 = *(ushort **)pas[8];
id = *puVar4;
if (id == 0 || id == 0xffff) → pas de condition
compteur = (int *)(id * 0x14 + contexte[0x17]);   // contexte + 0x5C
switch (*(u8 *)(puVar4 + 2)) {                    // l'opérateur
  0 : satisfait = (compteur[0] == (s16)puVar4[1]);
  1 : satisfait = (compteur[0] <= (s16)puVar4[1]);
  2 : satisfait = (compteur[0] >= (s16)puVar4[1]);
}
if (satisfait) return FUN_8226e908(contexte, *(u8 *)(puVar4 + 5));  // saut
else           return Function_82267370(contexte);
```

C'est une condition d'objectif au sens propre : **comparer un compteur de
mission, indexé dans le tableau de 339 entrées de foulée `0x14` que le slot 1
dimensionne, à un seuil de 16 bits, selon trois opérateurs, et sauter vers une
sous-mission nommée par son index.** Aucune de ces quantités n'est devinée :
l'index vient du champ, le tableau vient de l'allocation du chargeur, la borne
vient du compte analysé.

## La Mission 01, mesurée

| sous-mission | pas | étiquettes |
| ---: | ---: | --- |
| 0 | 2 | 0, 1 |
| 1 | 1 | 0 |
| 2 | 2 | 1, 0 |
| 3 | 1 | 0 |

Quatre sous-missions, six pas, deux étiquettes seulement — ce qui reproduit
exactement le relevé du cycle 1092 (« étiquettes 0 ×4 et 1 ×2 ») par une voie
indépendante, celle du consommateur.

**La Mission 01 ne contient aucun pas d'étiquette 7.** Sa progression n'est
donc pas conditionnée par un compteur : les sous-missions s'enchaînent par le
chemin des étiquettes 1, 4, 5, 6 et 8, qui délèguent à `0x82267370` selon le
mode de jeu. C'est un fait sur cette charge utile, et c'est la raison pour
laquelle le manifeste d'objectifs natif est émis avec quatre colonnes :
la condition `Manual` est la traduction fidèle d'« avancé par son propre
script », et **rien n'y complète un objectif tout seul**.

## Ce que cela établit

Une chaîne propriétaire → consommateur complète pour le domaine des objectifs :

- le **compte** de sous-missions, lu par `0x8226E908` sur le slot 2 analysé,
  borne l'index `contexte+0x10` ;
- chaque sous-mission a une **liste de pas** de foulée `0x28`, indexée par
  `contexte+0x14` — la liste que le cycle 1092 ne pouvait pas nommer ;
- chaque démarrage est **horodaté** dans `contexte+0x2C[index]`, un tableau
  dimensionné par ce même compte, et relu par un test de durée ;
- l'étiquette 7 compare un **compteur de mission** indexé dans le tableau
  dimensionné par le slot 1, avec trois opérateurs et un saut vers un index de
  sous-mission.

## Ce que cela n'établit pas

- Ce que **désignent** les 339 compteurs du slot 1. Leur tableau est
  dimensionné et indexé ; ce qui les incrémente n'est pas suivi ici.
- Le contenu des étiquettes 2, 3 et 9, absentes de la Mission 01.
- La signification des quatre bornes flottantes et des drapeaux de l'étiquette 0
  — une zone et une durée sont l'hypothèse évidente, et elle n'est pas testée.
- Aucun texte, aucun libellé d'objectif : les identifiants stables du manifeste
  (`mission01-submission-N`) sont des étiquettes de position, pas des noms
  retail.
