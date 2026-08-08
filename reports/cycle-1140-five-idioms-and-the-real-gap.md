# Cycle 1140 — cinq idiomes fermés, et l'angle mort qui était sous les yeux

Date : 2026-08-08. Cycle autonome. Il clôt la série et nomme ce qui reste.

## Qualification

- Image : Xbox 360 PAL `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Lecture dans `ghidra-projects-xenon/ac6-xenon`, **hors du projet canonique**.
- **Statique seul.** Aucun oracle.

## Le cinquième idiome

Le cycle 1138 filtrait les copies **par leur taille** et n'en trouvait aucune qui
écrive la transformation d'une unité. Une copie de taille variable y échappe.
Ce cycle filtre donc **par la destination** : tout appel à `memcpy`
(`0x82382F70`) dont `r3` est formé par `addi r3,rX,imm` avec `imm` sur une ligne
de transformation, et `rX` hors pile.

```
memcpy_calls=394  into_a_transform_row=7
```

Sept, sur 394 appels. `0x821C31E0`, `0x821C78D8`, `0x8220AE28`, `0x8237C8A8`,
`0x82393670`, `0x823937E8`, `0x82398CD0`. **Aucun dans le groupe de mission
`0x822[2-F]xxxx`.**

## L'énumération complète

| idiome | sites | dans le code de mission |
| --- | ---: | --- |
| `stvx128`, ports `+0x50` et `+0xA0` | 108 | 28 — toutes copies ou compositions |
| `stfs` à déplacement littéral | 12 | 0 |
| `stfsx` indexé, signature de triplet | 21 | 0 |
| `memcpy` filtré par taille | 38 | 1, qui écrit `this+0x04` |
| `memcpy` filtré par destination | 7 | 0 |

**Cinq idiomes, et aucun n'écrit la position d'une unité depuis des données de
mission.**

## L'angle mort réel

Le cycle 1139 a tué la seule lecture qui rendait ce vide acceptable. Il reste
donc quelque chose que les balayages ne voient pas, et ce n'est probablement pas
un sixième idiome — c'est une propriété du corpus, mesurée au cycle 1122 et
jamais reliée à cette recherche :

```
.text  82090000..823d772b   3 438 380 octets
       décodé  2 964 896     86,23 %
       6 144 trous, 473 484 octets
```

**Tous les balayages de cette série ne lisent que des instructions décodées.**
Treize virgule sept sept pour cent du code exécutable n'a jamais été examiné, par
aucun d'entre eux. Le cycle 1122 avait déjà rencontré exactement ce problème sur
`0x822953F0` : sa table de sauts et les corps qu'elle vise, 2 416 octets, étaient
en données ; désassemblés, ils rendaient l'appelant qu'un balayage de 755 392
instructions n'avait pas trouvé.

Le même piège, à l'échelle de 473 484 octets.

## Décision de cycle

La prise suivante n'est pas un sixième balayage : c'est **de réduire les trous**.
Deux voies, dans cet ordre :

1. désassembler les cibles des tables de sauts non résolues, comme le cycle 1122
   l'a fait à la main pour une seule ;
2. relancer les cinq balayages sur le corpus ainsi complété.

C'est mécanique, c'est chiffré, et cela n'ajoute aucune hypothèse.

## Ce que la série laisse

Seize cycles, cinq instruments corrigés, trois lectures corrigées dont deux de
moi, une hypothèse tuée par mesure. La position initiale d'une unité de la
Mission 01 **n'est toujours pas trouvée**, et l'espace où elle peut encore se
cacher est maintenant décrit exactement.

`ctest 24/24`, la porte JF reste verte.
