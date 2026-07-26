# AC6 cycle 204 — helper de reset de collection et propagation de flags

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- image base : `0x82000000`

Passe statique headless en lecture seule sur le projet Ghidra canonique. Aucun
état Ghidra, binaire ou sortie générée n'a été modifié.

## Helper `0x8226b290`

Le helper reçoit un objet en `r3` et conserve le scalaire flottant de `f1`.
Son corps effectue quatre opérations bornées :

1. appeler `0x82269ac8`, qui parcourt la collection globale et invoque le slot
   virtuel `+0x124` de chaque entrée non nulle ;
2. charger l'objet de table au slot `0x03006054` et appeler son slot virtuel
   `+0xd4` ;
3. charger l'objet de table au slot `0x03006084`, rappeler son slot `+0x11c`
   avec le scalaire conservé dans `f1` ;
4. parcourir la collection du slot `0x02009fc8` (`count` en `+0x04`, pointeurs
   à partir de `+0x08`) et appeler `0x8224f3c0(entry, 0)` pour chaque entrée
   non nulle.

Le dernier appel ne modifie pas les données du jeu de façon arbitraire :
`0x8224f3c0` nettoie une famille de bits de `entry+0x118` lorsque son second
   argument vaut zéro. Les branches et les masques sont confirmés ; le nom
   métier du flag ne l'est pas.

## Convergence et réutilisation

Les branches PPC brutes vers `0x8226b290` sont observées depuis :

```text
0x82255904  0x8226b508  0x8226bb34  0x8226bbd8
0x8226e740  0x8226ea30  0x822e4d90  0x822e549c
0x822ed254
```

Le chemin de reset documenté aux cycles 203 et 202 passe par `0x8226b508`.
Le helper est donc partagé par plusieurs transitions et ne doit pas être
réduit à un unique scénario de reset.

`0x82269ac8` possède l'appelant direct `0x8226b2a4`. Les appels bruts à
`0x8224f3c0` incluent `0x8226b33c` (dans le helper) ainsi que
`0x8225562c`, `0x8226eaac`, `0x8229c9ac` et `0x8229cae8`.

## Contrat sûr pour la transcription

```text
collection_state_reset(owner, scalar)
  -> notify collection entries through virtual slots
  -> update table services at +0xd4 and +0x11c
  -> clear an offset-qualified flag family at entry +0x118
```

Ce contrat peut être transcrit avec des pointeurs et des offsets qualifiés.
Il ne justifie pas les noms `aircraft`, `camera`, `spawn`, `velocity` ou
`mission state`.

## Qualification

- `confirmed` : ABI partiel, collection `+0x04/+0x08`, slots `+0x124/+0xd4/+0x11c`,
  appel `0x8224f3c0(entry,0)`, et liste des appelants PPC ;
- `cross-match` : participation au même cycle de transitions/éléments que les
  cycles 201–203 ;
- `unknown` / `needs-dynamic-evidence` : signification des flags et identité
  gameplay des objets.

Aucune intervention humaine n'est requise.

## Validation

- `DumpRange.java 0x8226b240 0x8226b420` ;
- `DumpRange.java 0x82269ac8 0x82269b80` ;
- `DumpRange.java 0x8224f360 0x8224f480` ;
- `FindPpcRawBranchesTo.java 0x8226b290` ;
- `FindPpcRawBranchesTo.java 0x82269ac8 0x8224f3c0` ;
- projet : `workspaces/ace-combat-6/ghidra-projects/ace-combat-6` ;
- mode : `-readOnly -noanalysis`.

