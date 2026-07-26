# AC6 — balayage des références littérales des vtables NDXR (cycle 149)

## Cible et méthode

- target : `ac6-xbox360-pal-default-xex` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- projet de référence : `ghidra-projects/ace-combat-6` ;
- mode : `analyzeHeadless -readOnly -noanalysis` ;
- aucune modification du XEX, du projet Ghidra, des exports ou du runtime.

Cette passe vérifie s’il existe une autre table ou un remplacement statique des
vtable candidates déjà associées au sous-objet NDXR. Elle ne cherche pas à
déduire la vtable effective à partir d’un seul offset numérique.

## Résultats headless

Les scripts exécutés ont été :

```text
FindU32Any.java 0x8205c980
FindU32Any.java 0x8205d6c0
ReferencesTo.java 0x8205c980
ReferencesTo.java 0x8205d6c0
```

`FindU32Any` ne rapporte aucune occurrence littérale supplémentaire des deux
adresses dans les blocs mémoire inspectés. Les seules références Ghidra
classées `DATA` sont :

```text
0x820f9dfc -> 0x8205c980
0x8212a2a4 -> 0x8205d6c0
```

Elles correspondent aux faits déjà établis :

- `0x820f9dfc` écrit la vtable du sous-objet construit par `0x820f9dc8` ;
- `0x8212a2a4` écrit la vtable de l’objet extérieur avant l’appel du
  constructeur sur `outer+0x14`.

Le balayage ne révèle donc pas de troisième emplacement statique qui
remplacerait directement l’une de ces tables par une autre adresse littérale.

## Ce que cette preuve ferme

La recherche statique permet de conserver les décisions suivantes :

- la table `0x8205c980` reste une vtable candidate du sous-objet, pas de
  l’objet extérieur ;
- `0x8205d6c0` reste la table de l’objet extérieur dans le chemin observé ;
- aucune fusion avec le chemin entry-9 `X360UnitManager` n’est justifiée par
  une occurrence littérale supplémentaire ;
- les offsets `+0x28` et `+0x5c` doivent toujours être qualifiés par l’owner et
  la table d’appels avant toute comparaison.

## Limites et ABI `r4`

L’absence d’un autre pointeur littéral ne prouve pas que toutes les instances
dynamiques chargent exactement `0x8205c980`. Une vtable peut être copiée,
calculée par morceaux, héritée ou modifiée par une écriture indirecte qui ne
contient pas l’adresse complète sous forme contiguë.

La contradiction centrale reste inchangée :

```text
worker : r4 = (r31 >> 16) & 0x1ff
feuille candidate 0x82101be0 : lhz r11,0x1c(r4)
```

Il est donc toujours interdit de qualifier `r4` de pointeur de record, d’index
de table ou de handle NDXR sans preuve de provenance supplémentaire. Cette
passe réduit l’espace des remplacements statiques directs, mais ne convertit
pas la question ABI en résultat dynamique.

## Statut

- références aux deux constructions observées : `confirmed` ;
- absence d’une autre occurrence littérale contiguë : `confirmed` pour ce
  balayage mémoire ;
- vtable effective des owners du worker : `unknown`/`cross-match` ;
- interprétation de `r4` et relation du dispatch avec NDXR/draw/vol :
  `needs-dynamic-evidence`.

Aucune action humaine n’est requise pour cette passe. La prochaine étape sans
interaction est de suivre les écritures indirectes du premier mot des owners
du worker et les chemins de copie de vtable. Si cette recherche reste vide,
une capture Xenia ciblée deviendra la première demande humaine justifiée, mais
elle n’est pas encore nécessaire ici.

## Validation documentaire

- balayage `FindU32Any` sur les deux adresses : PASS ;
- références `ReferencesTo` sur le projet canonique : PASS ;
- aucun fichier généré ou binaire modifié : PASS.
