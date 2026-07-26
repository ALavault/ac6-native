# AC6 — correction de provenance de `0x8207c180` (cycle 163)

Date : 2026-07-17 (Europe/Paris)

## Correction

La passe précédente décrivait provisoirement `0x8207c180` comme une table de
dispatch. La vérification de la mémoire Ghidra montre que cette adresse se
trouve dans le bloc non exécutable `.pdata` :

```text
0x8207c180  block=.pdata  0x82079e00..0x82089faf
0x82102148  block=.text   0x82090000..0x823d772b
0x8205c9a4  block=.rdata  0x82000400..0x82079dd3
```

La paire observée est donc un enregistrement de fonction et de métadonnée de
déroulement, pas une preuve d'appel indirect :

```text
0x8207c1c8 -> 0x82102148
0x8207c1cc -> 0x40010706
0x8207c1d0 -> 0x82102568
0x8207c1d4 -> 0x40024105
```

La fonction `0x82102148` est ainsi statiquement bornée par l'entrée `.pdata`
suivante, soit `0x82102148..0x82102567`. Les recherches de branches et d'appels
directs n'ont trouvé aucune arrivée directe vers `0x82102148`; `.pdata` est la
seule référence retrouvée dans cette passe.

## État de preuve corrigé

`confirmed` :

- `0x82102148` est un corps `.text` complet et borné par `.pdata` ;
- le corps charge `receiver+0x28`, `receiver+0x30`, `receiver+0x5c`,
  `receiver+0x74` et `receiver+0x78` avec `receiver=r31` ;
- le champ `+0x30` est donc lu par un consommateur statique réel.

`cross-match` :

- ces offsets coïncident avec le receiver NDXR préparé autour de
  `0x820fa258`, mais aucune entrée de la vtable `0x8205c9a4` ne pointe vers
  `0x82102148`.

`unknown` :

- l'appelant ou la table de dispatch réelle ;
- le constructeur qui associe ce corps à l'instance ;
- le nom C++ et le rôle métier ;
- le propriétaire et le libérateur final de la zone `+0x30`.

Cette correction réduit l'hypothèse précédente sans retirer le résultat utile :
le lecteur existe, mais son routage reste à qualifier. Aucune intervention
humaine ou session Xenia n'est nécessaire.

## Commandes exécutées

- `MemoryBlockAt.java 0x8207c180 0x82102148 0x8205c9a4` : PASS ;
- `FindDirectCallsTo.java 0x82102148` : aucune arrivée directe ;
- `FindPpcBranchesTo.java 0x82102148` : aucune arrivée PPC directe ;
- aucune écriture dans Ghidra, XEX ou sortie générée.
