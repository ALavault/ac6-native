# Cycle 1142 — le placement initial, trouvé — et le cycle 1125 était faux

Date : 2026-08-08. Cycle autonome.

## Qualification

- Image : Xbox 360 PAL `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Lecture dans `ghidra-projects-xenon/ac6-xenon`, **hors du projet canonique**.
- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- **Statique seul.** Aucun oracle.

## La chaîne, complète

```
1.  la charge utile        le bloc de données d'un nœud ObjBin, ses trois
                           premiers flottants en +0x00 +0x04 +0x08

2.  0x8232F380 / 0x8232F198   la liste Obj construit deux tableaux parallèles :
       8232f3d4  stw r5,0x4(r29)      +0x04 = tableau d'éléments de 8 octets
       8232f3e8  stw r10,0x8(r29)     +0x08 = tableau d'ObjBin de 0x20 octets
    et pour chaque enfant, d'un seul appel et à la même cadence
    (8232f4ac +8, 8232f4b0 +0x20) :
       8232f1dc  stw r11,0x0(r27)     élément+0x00 = le pointeur de données du nœud

3.  0x820A7070            à la construction de l'entité :
       820a779c  lwz r10,0x4(r25)     le tableau de 8 octets
       820a77b8  lwzx r27,r10,r23     l'élément de ce rang
       820a7a1c  stw r27,0x184(r31)   entité+0x184 = ce pointeur

4.  0x8229AF80            plus tard :
       8229afbc  lwz r9,0x184(r3)
       8229afd0  lfs f13,0x0(r9)  -> pile
       8229afe0  lfs f13,0x4(r9)  -> pile
       8229afe8  lfs f13,0x8(r9)  -> pile
       8229b090  stvx128 vr0,r3,r6    r6 = 0xA0 : entité+0xA0, la translation d'attente

5.  0x8229BE98            la validation du cycle 1134 :
       8229bf18  lvx128  +0xA0
       8229bf1c  stvx128 -> +0x50, la translation vivante
```

**C'est le placement initial.** De la charge utile à la transformation vivante,
cinq maillons, chacun lu instruction par instruction.

## Le cycle 1125 était faux

Il concluait, de ces mêmes trois mots :

> « Ce n'en est pas un [triplet de position]. Chaque route qui les atteint passe
> par l'enregistrement classé en `objet+0x180`, et sur chaque route les trois
> sont lus **séparément**. »

Les trois consommateurs séparés qu'il citait existent bel et bien — `+0x00`
derrière l'octet `+0x51`, `+0x04` près de `+0x52`, `+0x08` rechargé dans un
compte à rebours. **Mais il n'a suivi que `+0x180`, et le quatrième consommateur
passe par `+0x184`** — un champ voisin, jamais examiné, qui les lit **ensemble**,
comme un vecteur.

La leçon est précise et vaut d'être écrite : *« aucune route ne les lit
ensemble » n'était vrai que des routes que j'avais parcourues.* Un balayage
exhaustif sur un champ ne dit rien du champ d'à côté.

## Ce que cela change pour le produit natif

`build_retail_world` place chaque unité au premier triplet `Obj` de son
enregistrement. **La valeur était juste depuis le début** ; c'est le
raisonnement qui était faux, et deux cycles de commentaires d'en-tête l'ont
décrite comme « un nombre que retail ne consulte pas sur ce chemin ». Retail le
consulte, par `+0x184`.

Le champ redevient donc `objects` — un triplet de position — et le commentaire
dit la chaîne au lieu de s'excuser.

## Ce que cela n'établit pas

- **Par rapport à quoi.** Les valeurs mesurées sont petites — `(-50, -6,25, 50)`,
  `(0, -200, -1000)` — donc relatives. `0x8229AF80` teste `[ce+0x188]` et un bit
  de son `+0x118` avant d'écrire : il y a un **parent**, et ce cycle ne dit pas
  ce qu'il est ni comment sa transformation entre dans le calcul.
- **Quand `0x8229AF80` s'exécute** : ses quatre appelants sont `0x822551D0`,
  `0x8229C920`, `0x8229CD78`, `0x8230B030`, non analysés ici.
- **Les unités**, par opposition aux entités `Obj`. `+0x184` est un champ
  d'entité `Obj` — une unité fait `0x100` octets et n'en a pas. Le placement des
  230 unités reste distinct de celui des 434 entités.

## Décision de cycle

Corriger le commentaire natif et le rapport de dette, sans changer une valeur :
le produit place déjà les entités là où retail les place. Aucun test ne bouge,
aucune empreinte de la porte JF ne bouge.

`ctest 24/24`, la porte JF reste verte.
