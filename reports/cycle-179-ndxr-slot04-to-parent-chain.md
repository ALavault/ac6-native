# Cycle 179 — chaîne slot `+0x04` vers le parent NDXR

Date : 2026-07-18 (Europe/Paris)

## Cible et méthode

Cible canonique `ac6-xbox360-pal`, module `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`, projet
Ghidra `ace-combat-6`, base `0x82000000`.

Inspection headless en lecture seule. Aucun run humain ou Xenia n'a été
nécessaire.

## Chaîne observée autour de `0x82233404..0x82233550`

Le caller prépare d'abord un buffer de 16 octets :

```text
r10 = r27 + 0x40
r11 = r1 + 0x60
lwz/stw  r10[0],  r11[0]
lwz/stw  r10[4],  r11[4]
lwz/stw  r10[8],  r11[8]
lwz/stw  r10[12], r11[12]
```

Le même buffer est ensuite passé au slot `+0x04` du receiver partagé :

```text
r5 = 1
r4 = r1 + 0x60
r3 = *(table + 0x36084)
vtable = *(r3 + 0)
target = *(vtable + 0x04)
bctrl
```

Le retour flottant `f1` est immédiatement conservé dans `f0` à
`0x822334e8`. Le caller recharge ensuite des composantes du buffer et
prépare les zones locales `r1+0x80`, `r1+0x88`, `r1+0x8c` ainsi que d'autres
records/scratch.

Enfin, il appelle directement `sub_822131d0` à `0x82233550` en réutilisant
`r4 = r1+0x60` comme entrée. Le pointeur produit depuis `r27+0x40` traverse
donc :

```text
record/entrée amont
    -> buffer local r1+0x60
    -> méthode virtuelle +0x04
    -> f1/f0 et scratch dérivés
    -> sub_822131d0
    -> dispatch virtuel +0x140 / parent 0x82102e70
```

Cette chaîne est une preuve de provenance de buffer et de contrôle de flux,
pas une identification du record comme position, cellule ou donnée d'avion.

## Portée de la preuve

- `confirmed` : copie des quatre mots, dispatch `+0x04`, retour `f1`, réemploi
  du même pointeur `r1+0x60` dans l'appel direct à `0x822131d0`.
- `confirmed` : le parent reçoit ensuite des zones locales distinctes pour ses
  sorties et ses records auxiliaires, conformément aux cycles 173/174.
- `cross-match renforcé` : le slot `0x8205c9a8 -> 0x82101bf0` appartient à la
  même famille de receiver que le slot `+0x140 -> 0x82102e70`.
- `unknown` : type C++ du buffer, unité du résultat `f1`, rôle des zones
  `+0x80/+0x88/+0x8c`, et vtable réellement présente au runtime.

Ne pas transformer cette chaîne en nom gameplay. Elle est suffisante pour
préparer un harness différentiel qui vérifie le buffer, `f1` et le résultat du
parent avec les mêmes règles de flottants et de signedness.

## Preuves exécutées

```text
DumpRange.java 0x822333e0 0x82233490
DumpRange.java 0x822334a8 0x82233520
DumpRange.java 0x82233540 0x82233578
DumpRange.java 0x82233710 0x82233790
```

La qualification reste entièrement statique ; aucune intervention humaine
n'est demandée pour poursuivre.
