# Cycle 177 — ABI des slots du bloc vtable candidat

Date : 2026-07-18 (Europe/Paris)

## Cible

- target ID : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- projet Ghidra : `ace-combat-6`
- base : `0x82000000`
- architecture : Xenon PowerPC big-endian, pointeurs invités 32 bits

Passe headless en lecture seule. Aucun run humain ou Xenia n'a été utilisé.

## Table candidate et slots

Le bloc statique `0x8205c9a4` contient :

```text
address-point +0x04  -> 0x82101bf0
address-point +0xc8  -> 0x821005c8
address-point +0x140 -> 0x82102e70
```

Les sites qui chargent `*(table+0x36084)` et lisent ces mêmes slots sont
documentés au cycle 176. Cette passe vérifie les contrats observables des deux
premières cibles.

## Slot `+0x04` / `0x82101bf0`

Le préfixe borné `0x82101bf0..0x82101c8c` montre :

```text
r3 = receiver
r4 = pointeur d'entrée
lwz r3, 0x10(context)
lfs f0, 0x0(r4)
lfs f12, 0x8(r4)
calculs flottants et quantification vers des indices signés
```

La méthode lit les composantes `+0` et `+8` du pointeur `r4`, calcule des
valeurs bornées et poursuit vers une logique d'indexation. Cela est compatible
avec les call-sites qui passent `r4 = r31+0x50`, `r1+0x60`, `r30` ou
`r28+0x50`. Le registre `r5=1` préparé par ces call-sites n'est pas utilisé
dans le préfixe inspecté ; ne pas lui attribuer un rôle sans le corps complet.

## Slot `+0xc8` / `0x821005c8`

Le corps court est :

```text
lwz  r11,0x6840(r3)
stfs f1,0x62d0(r3)
stfs f1,0x160(r11)
blr
```

Il consomme donc le receiver `r3` et un scalaire flottant `f1`, exactement
comme le dispatch `0x8226bc04..0x8226bc1c` qui prépare `f1=f31`. La relation
de slot et l'ABI d'appel sont ainsi cohérentes, sans sémantique métier.

## Slot `+0x140` / `0x82102e70`

Les cycles 168, 173 et 174 ont déjà établi le contrat de la méthode : receiver,
trois sorties, deux records auxiliaires et deux indices. Le présent cycle ne
modifie pas ce classement.

## Décision de confiance

- `confirmed` : les formes ABI observées aux sites et dans les corps ciblés ;
  les mots du bloc statique candidat ; les loads depuis `table+0x36084`.
- `cross-match renforcé` : les trois correspondances de slots
  `+0x04/+0xc8/+0x140` entre les dispatchs et `0x8205c9a4`.
- `needs-dynamic-evidence` : la vtable effectivement stockée à
  `*(table+0x3607c)` au runtime.
- `unknown` : classe C++, noms métier, unités et rôle gameplay des données.

La concordance ABI ne justifie pas de renommer le receiver ni les records et
ne remplace pas une capture runtime ciblée. Elle suffit toutefois à conserver
le chemin statique comme prochaine étape de transcription/validation.

## Preuves exécutées

```text
DumpDataWords.java 0x8205c9a4 0x60
DumpRange.java 0x82101bf0 0x82101c90
DumpRange.java 0x821005c8 0x821005e4
InspectFunctionIsland.java 0x82100000 0x82102000
```

La prochaine action facultative est de rapprocher les sorties de `+0x04` du
contrat NDXR sans leur attribuer de type ; aucune intervention humaine n'est
requise pour continuer.
