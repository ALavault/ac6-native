# Cycle 1135 — les 48 écrivains du port réel, classés

Date : 2026-08-08. Cycle autonome. La suite du renversement du cycle 1134.

## Qualification

- Image : Xbox 360 PAL `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Lecture dans `ghidra-projects-xenon/ac6-xenon`, **hors du projet canonique**.
- **Statique seul.** Aucun oracle.

## Les 48, classés

Le cycle 1134 a montré que le port d'écriture est `+0xA0` et non `+0x50`, et a
rendu 48 sites sans les classer. Ils le sont :

| classe | sites |
| --- | ---: |
| copie depuis la mémoire | 45 |
| calculé | 3 |

Des trois « calculés », deux (`0x8212653C`, `0x821F3D04`) remontent à un
`stvx128` antérieur du même registre — encore des copies. **Un seul est
réellement calculé** : `0x8225E4F0`, dans `0x8225E260`, par `vmulfp128`, c'est-à-
dire la mise à l'échelle d'un vecteur existant.

Le port réel donne donc **le même verdict que le faux** : tout est copie ou
composition.

## Sauf un, et il est vérifié

`0x8229C0E0` fait passer une position **dans les deux sens** entre un objet et un
enregistrement. Lu instruction par instruction :

```
; branche A - de l'objet vers l'enregistrement
8229c594  lfs f0,0xa0(r31) ; stfs f0,0x1c(r28)
8229c59c  lfs f0,0xa4(r31) ; stfs f0,0x20(r28)
8229c5a4  lfs f0,0xa8(r31) ; stfs f0,0x24(r28)

; branche B - de l'enregistrement vers l'objet
8229c5d0  lfs f0,0x1c(r28) ; stfs f0,0x50(r1)
8229c5e0  lfs f0,0x20(r28) ; stfs f0,0x54(r1)
8229c5ec  lfs f0,0x24(r28) ; stfs f0,0x58(r1)
8229c5f4  stfs f29,0x5c(r1)
8229c5f8  addi r10,r1,0x50
8229c5fc  lvx128 vr0,r0,r10
8229c600  stvx128 vr0,r31,r11   ; r11 = 0xA0 -> objet+0xA0
```

**C'est le premier chemin de cette série où un triplet lu dans un
enregistrement devient une transformation.** Les deux branches sont gardées par
les virtuelles `+0x54` et `+0x64` de l'objet.

C'est aussi pourquoi la classification ci-dessus le compte comme « copie » : le
vecteur est chargé depuis `r1+0x50`, une pile. Un classificateur qui ne distingue
pas « copié d'un autre objet » de « assemblé en pile depuis des données » se
trompe sur le seul site qui compte. **C'est le quatrième défaut d'instrument de
cette série** et il est noté comme les trois autres.

## Mais l'enregistrement n'est pas une table de départ

Son unique appelant, `0x8222B7E0` :

- lit `[global+0x29C80]`, **le pointeur de contexte de mission** que
  `0x8219BDD8` publie, et le passe à `0x82266CC0` ;
- teste le mot `+0x00` de l'enregistrement contre `0x403` et `0x404` — et
  `0x8229C0E0` le teste contre `0x401` ; ce sont des **opcodes** ;
- fait décroître `[enregistrement+0x0C]` par un facteur à chaque passage, avec
  saturation.

Un enregistrement à opcode, avec un champ qui s'amortit par trame et une position
qu'on sauve et restaure : c'est un **enregistrement d'événement**, pas une table
de naissance. Rien ici ne le relie à la charge utile de la mission.

## Ce que deux ports concordants disent

`+0x50` : 65 sites, tout est copie ou composition.
`+0xA0` : 48 sites, tout est copie ou composition, sauf un chemin depuis un
enregistrement d'événement.

Deux ports, deux verdicts identiques. Si la position initiale s'écrit par une
instruction unique, **elle n'emprunte ni l'un ni l'autre des deux idiomes
balayés**. Cela ramène à ce que le cycle 1133 a compté et laissé entier : les
**659 magasins indexés** dont l'index n'est pas une constante suivable.

## Décision de cycle

L'instrument n'est toujours pas étendu à la propagation de valeurs. Le cycle
1133 avait décidé de ne pas le bâcler ; deux cycles plus tard l'argument tient
encore, et cette fois il est renforcé : le classificateur vient de se tromper
sur le seul site intéressant faute d'une distinction de deux lignes. Ajouter de
la propagation avant d'avoir corrigé cela produirait des faux positifs plus
subtils, pas moins.

La correction minimale — distinguer une base de pile d'une base d'objet dans la
source d'un `lvx128` — est ce que le prochain cycle doit faire **avant** tout
nouveau balayage.

`ctest 24/24`, la porte JF reste verte.

## Ce que cela n'établit pas

- **La position initiale**, toujours pas.
- **Ce qu'est `0x82266CC0`**, appelé sur le contexte de mission par le
  consommateur de ces enregistrements.
- Le sens des opcodes `0x3EB`, `0x401` à `0x404`.
