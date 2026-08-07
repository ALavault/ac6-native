
# Cycle 1110 — délimiter le cluster, et ce que cela révèle

Date : 2026-08-09. Le blocage nommé au cycle 1109 : plusieurs des 51
gestionnaires n'étaient pas des fonctions pour Ghidra.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- `.pdata` byte-qualifiée avant écriture, SHA-256
  `740f31d530dcfca2fcddab6ac6f93e1ab55d36106a9a015e41f074d5e6d73034`,
  8 246 entrées.
- Sauvegarde avant écriture : `ghidra-projects/ace-combat-6.rep.bak-pre-attach`.
- **Statique seul.**

## Le diagnostic, qui n'était pas celui attendu

Sur les 51 sites d'appel : **33 dans une fonction, 18 hors fonction, et zéro non
désassemblé**. Chacun des 18 portait pourtant une instruction, et chacun tombait
dans une entrée `.pdata` dont la fonction **existait**, avec l'extent exact.

| entrée `.pdata` | fonction Ghidra | site orphelin |
| --- | --- | --- |
| `0x822EA978 + 0x348` | `822EA978-822EACBF` | `0x822EABE8` |
| `0x822EC4D0 + 0x4C4` | `822EC4D0-822EC993` | `0x822EC814` |
| `0x822EE2C8 + 0x2F8` | `822EE2C8-822EE5BF` | `0x822EE368` |

Le site est **à l'intérieur** de l'extent et **à l'intérieur** des bornes de la
fonction. Ce n'est donc ni une fonction manquante ni une troncature : le **corps
est troué**. Ghidra a désassemblé les octets sans jamais les rattacher, faute
d'y voir un flux — typiquement une cible de branchement calculé.

Un trou de corps répond « aucune fonction ne contient cette adresse », ce qui
fait passer pour orpheline une instruction au milieu d'une fonction bien formée.

## La réparation

`scripts/AttachPdataOrphanBlocks.java`, sur la règle que S0 avait déjà employée
et qui vient du linker, pas d'une inférence : *si une instruction est dans
l'extent enregistré d'une fonction et qu'aucune autre fonction ne la possède,
elle appartient à cette fonction.*

Politique : n'attache que des adresses **déjà désassemblées** — aucun octet
n'est décodé ici ; ne prend jamais une adresse possédée par une autre fonction ;
saute toute entrée qui chevaucherait la suivante ; préserve le corps existant.

```
AC6_ATTACH mode=apply functions=905 attached_bytes=619180
           owned_elsewhere=1240 overlapping_entries_skipped=0
```

Idempotent : le second passage rend `functions=0`.

## Le résultat

| mesure | avant | après |
| --- | ---: | ---: |
| sites d'appel dans une fonction | 33 / 51 | **51 / 51** |
| couverture des corps `.pdata` | 76,2 % | **96,0 %** |
| octets couverts | 2 391 946 | **3 011 126** |
| fonctions au corps complet | 7 201 | 7 233 |

La réparation n'est pas locale au cluster : **905 fonctions** du binaire y
gagnent 619 KB de corps. Le cluster `0x822Exxxx` compte désormais **140
fonctions délimitées**.

## Ce que la délimitation révèle aussitôt

`0x822E8840`, qui n'appartenait à aucune fonction, appartient maintenant à
`0x822E8660`. Sa décompilation :

```c
Function_822E8660(param_1, this, signal, argument) {
  if (signal == -3) {                       // entrée
      *(int *)(this + 0x3CC) = argument;
      *(int *)(this + 0x260) = 3;
      Function_8226A018(this, argument, 0);
      ...
  }
  if (signal == -2) { ... }
  if (signal == -1) {                       // sortie
      *(this + 0x810..0x81C) = ...;
      Function_8226A310(this);
      *(int *)(this + 0x260) = 0;
  }
}
```

Trois choses en tombent :

1. **Les signaux `-3` et `-1` sont confirmés depuis un gestionnaire**, et non
   plus déduits de la position dans l'algorithme. Le cycle 1109 les lisait par
   structure ; ils se lisent maintenant directement. Un signal **`-2`** existe,
   que le cycle 1109 n'avait pas listé.
2. **L'objet de la machine à états est le contexte de mission.** Le gestionnaire
   écrit `this+0x260`, exactement le champ dont le cycle 1093 avait montré que
   `mission_manager_update` le teste (`état+0x260 == 8`). L'entrée pose 3, la
   sortie pose 0.
3. **L'installation de la zone de mission est une action d'entrée d'état** :
   le gestionnaire appelle `0x8226A018`, la fonction qui installe le rectangle
   depuis le slot 6 (cycle 1105).

Les cycles 1093, 1105, 1107 et 1109 décrivaient donc quatre morceaux d'un même
objet : le contexte de mission porte une machine à états hiérarchique en
`+0x348`, son état courant en `+0x350`, son code d'état en `+0x260`, et ses
gestionnaires d'entrée installent la zone.

## Ce que cela n'établit pas

- **Quels états.** Un gestionnaire lu ne fait pas une carte. Les 140 fonctions
  du cluster ne sont pas classées, et rien ne dit lesquelles sont des états ni
  comment elles s'emboîtent.
- Le signal `-2`, observé et non modélisé.
- La base Ghidra n'est pas versionnée : la preuve reproductible est le script,
  byte-qualifié et idempotent, pas la base.
- La couverture reste à 96 %, pas 100 % : 1 013 fonctions gardent un corps plus
  court que leur extent, et 1 240 octets appartiennent à une autre fonction que
  celle que `.pdata` désigne — non expliqué.
