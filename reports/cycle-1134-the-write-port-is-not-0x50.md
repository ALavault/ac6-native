# Cycle 1134 — le port d'écriture n'est pas `+0x50` : il est `+0xA0`

Date : 2026-08-08. Cycle autonome, avec cinq sondes parallèles. Il retourne dix
cycles de recherche.

## Qualification

- Image : Xbox 360 PAL `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Lecture dans `ghidra-projects-xenon/ac6-xenon`, **hors du projet canonique**.
- **Statique seul.** Aucun oracle. Cinq sondes indépendantes sur le corpus
  canonique `exports/`, puis vérification de chaque affirmation portante par
  désassemblage VMX128 — les sondes n'avaient pas accès à Ghidra.

## Le retournement

Les cycles 1128 à 1133 ont cherché qui écrit la ligne de translation en `+0x50`,
et ont conclu, exhaustivement pour leurs idiomes, que **personne ne l'écrit
depuis des données** : tout est copie ou composition. La conclusion était juste
et la question était mal posée.

`0x8229BE98`, vérifié instruction par instruction :

```
8229bee0  addi r10,r31,0x60     ; la base de la transformation d'attente
8229bef0  addi r30,r31,0x10     ; la base de la transformation vivante
8229bef4  li   r29,0x40
8229bef8  lvx128 vr0,r0,r11     ; +0x70  \
8229befc  lvx128 vr13,r11,r27   ; +0x80   >  les trois lignes de base
8229bf00  lvx128 vr12,r11,r28   ; +0x90  /
8229bf08  stvx128 vr0,r0,r11    ; -> +0x20 \
8229bf0c  stvx128 vr13,r11,r27  ; -> +0x30  >
8229bf10  stvx128 vr12,r11,r28  ; -> +0x40 /
8229bf18  lvx128 vr0,r10,r29    ; +0xA0   la translation d'attente
8229bf1c  stvx128 vr…,r30,r29   ; -> +0x50
```

Et, avant cela, `8229bed4/bed8/bedc` mettent à zéro `+0x100/+0x104/+0x108`.

**Ce n'est pas un transfert entre deux objets : c'est la validation, dans un même
objet, d'une transformation préparée en `+0x70..+0xA0` vers la transformation
vivante en `+0x20..+0x50`.** Le bloc que le cycle 1124 avait pris pour « la copie
de trame précédente » est en réalité — au moins pour cet usage — **la zone de
préparation**.

Le balayage du cycle 1132 avait bien vu ce site. Il l'avait classé `copy`, ce qui
est exact au niveau de l'instruction et faux au niveau du sens. **Toutes les
copies que ce cycle a énumérées sont, pour partie, des validations.** Une
position authorée n'apparaît jamais en `+0x50` parce qu'elle n'y est jamais
écrite : elle est écrite en `+0xA0`, puis validée.

## Ce que le port réel donne

Le même instrument, pointé sur `0xA0` : **48 sites**, dont beaucoup dans des
zones que dix cycles n'avaient jamais visitées — `0x822C....`, `0x822D....`,
`0x822F....`, `0x8230....`.

Une sonde a par ailleurs produit, et j'ai vérifié le principe, un écrivain qui
part de **données** : `0x8229C0E0`, dont l'enregistrement porte un opcode en
`+0x00` et des coordonnées en `+0x1C/+0x20/+0x24`, écrit `objet+0xA0` puis
appelle `0x8229BE98`. C'est la première fois de cette série qu'un triplet lu
depuis un enregistrement atteint une transformation.

## Le piège que le nouveau port porte aussi

`0x822A6710`, dans le code même de la classe d'unité, figure parmi les 48. Lu :

```
822a6758  lwz r11,0xd8(r31)     ; le tableau d'entités Obj de cette unité
822a675c  addi r9,r31,0x80
822a6774  addi r11,r9,0x10      ; = unité+0x90
822a6788  stvx128 vr0,r0,r11    ; -> +0x90
822a678c  stvx128 vr13,r11,r5   ; -> +0xA0
822a6790  stvx128 vr12,r11,r6   ; -> +0xB0
822a6798  stvx128 vr0,r9,r7     ; -> +0xC0
```

C'est une copie de quatre lignes vers `+0x90..+0xC0` — **un troisième bloc**, dont
`+0xA0` n'est que la deuxième ligne. Le balayage `0xA0` le compte comme un
écrivain de translation ; il ne l'est pas.

Autrement dit : **le port `0xA0` a exactement le défaut que le port `0x50`
avait**, et les 48 sites demandent le même travail de classification que les 65.
Ce cycle ne le fait pas ; il le nomme.

## Ce que cela corrige

- Le cycle 1132 : sa conclusion vaut pour `+0x50`, et `+0x50` n'est pas le port
  d'écriture. La phrase « aucune position n'est écrite depuis des données » doit
  se lire « … en `+0x50` », ce qui est vrai et sans portée.
- Le cycle 1124 : `+0x70..+0xA0` n'est pas seulement une copie de trame
  précédente ; c'est aussi la zone de préparation qu'une validation recopie.

## Ce que cela n'établit pas

- **La position initiale des 230 unités.** `0x8229C0E0` lit un enregistrement,
  mais rien ne relie encore cet enregistrement à la charge utile de la mission,
  et aucune sonde n'a trouvé de chemin de `contexte+0x264` vers un triplet.
- **Que `+0xA0` soit le seul port.** Il est *un* port, vérifié.
- Les 48 sites, non classés.

## Décision de cycle

Rien n'est porté. Le gain est un renversement de la question et un artefact à
refaire : `analysis/translation-writes.tsv` décrit le mauvais port et le dit
désormais en tête.

`ctest 24/24`, la porte JF reste verte.
