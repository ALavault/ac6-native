# Cycle 184 — matérialisation des tables statiques du caller NDXR

Date : 2026-07-18 (Europe/Paris)

## Cible

- target ID : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- projet Ghidra : `ace-combat-6`
- base : `0x82000000`

Passe headless statique, lecture seule, sans intervention humaine.

## Question

Les deux tables contenant `0x82234040` (`0x82007a30` et `0x82009150`) sont-elles
matérialisées comme adresses par le code PPC, ce qui permettrait de les relier
à un consommateur runtime ?

## Résultat

Le script `FindPpcAddressMaterialization.java` cherche les paires contiguës
`lis/addi` et `lis/ori` dans les instructions définies. Pour les cibles :

```text
0x82007a30
0x82009150
```

aucune matérialisation n'est trouvée.

Le seul résultat voisin est :

```text
0x820f8e50 lis  r11,-0x7d65
0x820f8e54 addi r11,r11,0x7a30  => 0x829b7a30
```

Cette adresse est différente des tables statiques `0x82007a30`/`0x82009150`.
Dans le même corps, `0x829b7a30` est additionnée à un index puis passée en
`r6` à un dispatch indirect sur `owner->vtable + 0x6c`; elle ne constitue pas
une preuve d'utilisation des deux tables précédentes.

## Qualification

- `confirmed` : les deux tables statiques contiennent `0x82234040` (cycle 183).
- `confirmed` : aucune paire simple de matérialisation PPC vers
  `0x82007a30`/`0x82009150` n'est présente dans les instructions définies du
  projet canonique.
- `confirmed` : le motif `0x829b7a30` appartient à une autre adresse globale et
  à un autre contrat d'argument.
- `cross-match` : les deux séquences de pointeurs restent apparentées, mais
  sans consommateur code statiquement relié.
- `unknown` : mécanisme éventuel de relocation ou d'accès non contigu aux
  tables, nature exacte des tables et adresse-point runtime.
- `needs-dynamic-evidence` : vtable effective du receiver partagé et payload
  métier.

Cette passe empêche de transformer abusivement la répétition des pointeurs en
preuve de vtable. Aucun binaire, projet Ghidra ou fichier généré n'a été
modifié.

## Preuves exécutées

```text
FindPpcAddressMaterialization.java 0x82007a30 0x82009150 0x829b7a30
DumpRange.java 0x820f8d80 0x820f8f30
```

