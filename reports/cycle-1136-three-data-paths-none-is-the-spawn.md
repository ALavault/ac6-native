# Cycle 1136 — l'instrument corrigé : trois chemins de données, aucun n'est la naissance

Date : 2026-08-08. Cycle autonome. Il fait ce que le cycle 1135 s'était engagé à
faire avant tout nouveau balayage.

## Qualification

- Image : Xbox 360 PAL `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Lecture dans `ghidra-projects-xenon/ac6-xenon`, **hors du projet canonique**.
- **Statique seul.** Aucun oracle.

## La correction d'instrument

Les cycles 1132 et 1135 classaient `0x8229C0E0` en « copie » parce que son
vecteur arrive par `lvx128` — depuis `r1+0x50`, une case de pile qu'il venait de
remplir depuis un enregistrement. **Copier la ligne d'un autre objet et en
assembler une depuis des données sont deux choses opposées**, et aucun des
balayages ne les séparait.

`tools/ghidra_scripts/Ac6StagedWrite.java` les sépare : il suit les registres
dérivés de `r1`, marque tout vecteur chargé depuis l'un d'eux, et ne retient que
les écritures de ligne de transformation dont le vecteur a été **assemblé sur
place**.

```
transform_row_writes=108  assembled_on_stack=34
```

**34 sur 108.** Un tiers des écritures de transformation ne copient pas : elles
construisent. C'est la liste que dix cycles cherchaient, et elle n'existait pas
avant que le classificateur sache faire cette distinction.

## Ce que les 34 lisent

Une fenêtre sur chacune, et les `lfs` qui remplissent la case de pile. Trois
seulement lisent un **enregistrement** — une base qui n'est ni la pile ni
l'objet lui-même :

| site | source | verdict |
| --- | --- | --- |
| `0x8229C600` dans `0x8229C0E0` | `+0x1C/+0x20/+0x24` de `r28` | enregistrement à **opcode** (`0x401`, `0x403`, `0x404`), champ `+0x0C` amorti par trame, position sauvée **et** restaurée — un enregistrement d'**événement** (cycle 1135) |
| `0x82255AB0` dans `0x822557C8` | `+0x14/+0x18/+0x1C` de `r28` | **le flux de rejeu** — voir ci-dessous |
| `0x822F5744` dans `0x822F5668` | `+0x30/+0x34/+0x38` de `r11` | enregistrement de pool `0x40` octets, **conditionné** à un objet dont le type vaut `0x2CF4` : un cas particulier |

Les 31 autres partent de constantes, de champs de l'objet lui-même
(`+0x114`, `+0xC8`) ou de résultats vectoriels.

## Le troisième chemin est le rejeu

`0x822557C8` prend son enregistrement dans `this+0x150`, le range en `this+0x144`
et en lit le flottant `+0x04`. Son appelant `0x822560C8` construit toute la
chaîne — `+0x148`, `+0x14C`, `+0x150`, `+0x154` — et `0x82255C18` **compare
`[enregistrement+0x04]` à `[this+0x154]+0x04`**, deux flottants : un **temps**.

Un flux d'enregistrements horodatés qui repositionne les objets, et une carte de
classes qui porte `ACE6::CAce6ReplayManager`, `CX360ReplayManager`,
`CQueueBase<SReplayData>` : **c'est la lecture du rejeu**. Elle place des objets,
et elle les place depuis un enregistrement de rejeu, pas depuis la mission.

Détail qui ferme la boucle : `0x822560C8` est appelé par **`0x8226A018`**, le même
installateur que le contrat JF cite déjà pour `mission_area`.

## Le résultat

Sur les deux ports, `+0x50` et `+0xA0`, l'énumération est complète pour l'idiome
`stvx128` à index suivable :

> **108 écritures de ligne de transformation. 74 copient. 34 assemblent. Sur ces
> 34, trois lisent un enregistrement, et les trois sont identifiés : un flux
> d'événements, un flux de rejeu, un cas particulier de pool. Aucun ne place les
> unités d'une mission au chargement.**

Ce n'est plus « je n'ai pas trouvé » : c'est une énumération close, avec sa
frontière écrite.

## La frontière, inchangée et chiffrée

L'instrument voit `stvx128` avec un index constant ou un biais formé par `addi`.
Il ne voit toujours pas les **659 magasins indexés** dont l'index n'est pas une
constante suivable (cycle 1133). Si la naissance s'écrit par une instruction
unique, c'est là qu'elle est.

## Décision de cycle

Rien n'est porté. Le produit natif n'a ni rejeu, ni flux d'événements, ni pool
`0x2CF4`, et aucun des trois chemins ne concerne la Mission 01 au chargement.

`ctest 24/24`, la porte JF reste verte.
