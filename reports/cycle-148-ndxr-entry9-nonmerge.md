# Cycle 148 — séparation NDXR / entry-9 unit-manager

## Cible et portée

- Target : `ac6-xbox360-pal-default-xex`
- Module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Méthode : Ghidra `analyzeHeadless`, lecture seule, `-noanalysis`.

Cette passe vérifie si les écritures de champs observées dans le chemin
gameplay entry-9 pouvaient être fusionnées avec les champs du sous-objet NDXR
qualifié aux cycles 143–147. La réponse est négative : les preuves de vtable
et de construction appartiennent à deux objets distincts.

## Preuves de séparation

Le chemin entry-9 est possédé par `X360UnitManager` /
`ACE6::CAce6UnitManager`. Son vtable final commence à `0x82055190`; son
constructeur de base est `0x82273880`. Le rapport entry-9 documente notamment
les écritures `object+0x50/+0x54/+0x58` et la ressource `object+0x15c`.

Le chemin NDXR construit un sous-objet à `outer+0x14` (et dans un second parent
à `parent+0x1844`) avec le constructeur `0x820f9dc8`. Ce constructeur écrit
`0x8205c980` au début du sous-objet. La recherche de références headless vers
`0x8205c980` ne retourne que l'écriture du constructeur à `0x820f9dfc`, tandis
que la vtable extérieure `0x8205d6c0` n'est référencée que par la construction
du parent à `0x8212a2a4`.

Les constantes de vtable et les chemins d'initialisation sont donc disjoints :

```text
entry-9 manager       : vtable 0x82055190, constructeur 0x82273880
NDXR sous-objet       : vtable 0x8205c980, constructeur 0x820f9dc8
objet extérieur NDXR  : vtable 0x8205d6c0, construction 0x8212a2a8
```

La coïncidence d'un offset numérique tel que `+0x28` ou `+0x5c` ne fournit
aucune identité d'objet. Les écritures entry-9 `object+0x28/+0x5c` restent
dans le modèle `X360UnitManager` et ne sont pas des writers du sous-objet NDXR.

## Décision

- `KEEP` : ne pas fusionner les champs entry-9 avec les champs NDXR.
- `KEEP_WITH_CLARIFICATION` : les rapports peuvent mentionner les deux chemins
  comme frontières gameplay distinctes, mais doivent qualifier l'objet et la
  vtable avant de comparer un offset.
- `OPEN` : pour NDXR, la valeur dynamique de `owner+0x28`, la vtable effective
  de chaque instance et la contradiction du champ 9 bits dans le worker restent
  non résolues.
- `needs-dynamic-evidence` : la relation finale du dispatch NDXR avec une
  ressource NDXR ou une soumission graphique.

Aucune action humaine n'est requise pour cette frontière. Une trace Xenia ne
sera demandée qu'après épuisement des recherches statiques sur la valeur du
handle et les remplacements de vtable.

## Validation

Commandes headless exécutées :

```text
ReferencesTo.java 0x8205c980  -> 0x820f9dfc
ReferencesTo.java 0x82055190  -> 0x8209405c
ReferencesTo.java 0x8205d6c0  -> 0x8212a2a4
DumpRange.java 0x820f9dc8 0x820fa060
```

Les trois requêtes de références et le dump ont réussi en lecture seule. Aucun
XEX, projet Ghidra, export généré ou code natif n'a été modifié dans cette
passe.
