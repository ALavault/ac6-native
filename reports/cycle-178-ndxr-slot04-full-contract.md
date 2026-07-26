# Cycle 178 — contrat borné du slot `+0x04`

Date : 2026-07-18 (Europe/Paris)

## Cible

- target ID : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- projet Ghidra : `ace-combat-6`
- base : `0x82000000`
- architecture : Xenon PowerPC big-endian

Analyse headless en lecture seule. Aucune session humaine, VNC ou exécution
Xenia n'a été utilisée.

## Corps observé

Le chemin `0x82101bf0..0x82101ee0` se termine par des branches vers
l'épilogue commun `0x82382f44`; l'analyse Ghidra fragmente cette zone, donc ce
rapport ne prétend pas remplacer une frontière `.pdata` complète.

Au début du corps :

```text
r3 = receiver
bl  0x82382ef4
context = *(context + 0x10)
r4 = pointeur d'entrée
```

Si les pointeurs de contexte `+0xc` ou `+0x10` sont nuls, le chemin retourne
une constante flottante via `f1` (`0x82101edc`).

## Contrat d'entrée et de sortie

Le corps lit :

```text
r4+0x00  float
r4+0x04  float (voie finale conditionnelle)
r4+0x08  float
r5       indicateur/paramètre consommé par son octet bas
```

Il convertit deux composantes en indices entiers, les borne dans `0..15`, puis
forme des offsets de tables. Le chemin charge des valeurs dans une zone
indexée par un stride de `0x4204` et combine plusieurs flottants par
soustractions, divisions et `fmadds`. La forme est compatible avec une
interpolation bornée ; aucune unité ni sémantique gameplay n'est attribuée.

Après le calcul principal, `r5` est réduit à son octet bas à `0x82101eac`.
Lorsque ce paramètre est actif, `r4+0x04` et une valeur contextuelle servent à
une comparaison/clamp finale. Le résultat est retourné dans `f1` par
`0x82101ed0`.

## Comparaison aux call-sites

Les sites qui chargent `*(table+0x36084)` puis le slot `+0x04` préparent tous
`r5=1` et un pointeur `r4` :

```text
0x82228de8 : r4 = r31+0x50
0x822334c8 : r4 = r1+0x60
0x82233824 : r4 = r30
0x8231c684 : r4 = r28+0x50
```

Le contrat du corps est donc compatible avec les quatre dispatchs. Cette
concordance renforce la correspondance du mot `0x8205c9a8 -> 0x82101bf0` dans
le bloc `0x8205c9a4`.

## Limites de confiance

- `confirmed` : lectures `r4+0/+4/+8`, utilisation de `r5` comme paramètre,
  bornes d'indices `0..15`, sortie flottante `f1`, et ABI des call-sites.
- `cross-match renforcé` : slot `+0x04` du bloc `0x8205c9a4`.
- `needs-dynamic-evidence` : vtable réellement installée dans
  `*(table+0x3607c)`.
- `unknown` : type de la table, unité des flottants, nom de méthode et rôle
  gameplay.

Cette preuve ne permet pas de renommer la méthode en interpolation de position,
de grille ou de trajectoire. Elle fournit en revanche un contrat exploitable
pour un harness différentiel, en préservant les flottants, le mode de clamp et
les valeurs NaN/comparaisons.

## Preuve exécutée

```text
DumpRange.java 0x82101bf0 0x82101ee8
```

La prochaine étape peut comparer ce contrat flottant aux entrées r7/r8 du
parent `0x82102e70` sans nécessiter d'action humaine.
