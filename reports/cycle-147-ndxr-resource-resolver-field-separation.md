# Cycle 147 — séparation statique des champs de ressources `+0x28` et `+0x5c`

## Cible et méthode

- Target : `ac6-xbox360-pal-default-xex`
- Module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- méthode : Ghidra `analyzeHeadless`, `-readOnly -noanalysis`, dumps PPC
  ciblés
- aucune modification du XEX, du projet Ghidra, des exports générés ou du
  runtime natif

## Résolveur de ressources

Le dump brut de `0x82101a18` corrige la frontière d'export qui le présentait
comme un simple thunk. Le contrat local observé est :

```text
entrée : r3 = owner, r4 = descripteur/chemin, r5 = adresse de sortie
action : *r5 = 0 ; normalisation du chemin dans owner+0x6c5c/+0x6c7c ;
         résolution via les helpers de registre/ressource
sortie : handle ou pointeur retourné en r3 et, selon le chemin, copié dans *r5
échec  : résultat nul après libération/retour contrôlé
```

Les instructions `0x82101a24..0x82101a8c` copient et normalisent la chaîne
avant l'appel de résolution. Les branches `0x82101aa4..0x82101b20` gèrent le
résultat nul, le stockage de la valeur retournée et une voie de validation ou
de réessai. Les noms métier du registre et des ressources restent inconnus.

`0x82101b28` est distinct : il construit une chaîne normalisée avec séparateur
final dans le buffer `owner+0x6c7c` à partir d'un descripteur. Il ne doit pas
être fusionné avec `0x82101a18` ni avec le lookup borné `0x82234e08`.

## Initialisation du sous-objet

Le corps de `0x820fbc28`, présent dans la même famille de méthodes que
`0x820fa9c0`, initialise un ensemble de champs avant résolution :

```text
zero : owner+0x08, +0x0c, +0x14, +0x18, +0x1c, +0x20,
       +0x24, +0x28, +0x2c
resolve : owner+0x0c, +0x10, +0x14, +0x18, +0x1c, +0x20,
          +0x24, +0x28, +0x2c
```

Chaque appel réutilise le même owner en `r3`, un descripteur local en `r4` et
une adresse de sortie en `r5`. Les constantes de descripteur sont distinctes
pour chaque champ (`0x4250`, `0x4240`, `0x4238`, `0x4230`, `0x4228`, `0x4220`,
`0x4218`, `0x4210`, `0x4208`). Cela établit une série de résolutions de
ressources, pas une simple copie d'un tableau déjà peuplé.

Pour `owner+0x28`, le chemin est particulier :

```text
r18 = owner + 0x5c
call 0x82101a18(owner, descriptor_0x4210, r18)
owner+0x28 = returned_r3
```

Une branche postérieure vérifie `*r18 == 4`; elle libère alors
`owner+0x28`, remet ce champ à zéro et efface `owner+0x5c`. Cette branche
explique le cycle de vie observé sans attribuer de sémantique NDXR au contenu.

## Clarification critique sur `+0x5c`

Deux notions précédemment proches doivent rester séparées :

1. le **champ mémoire** `owner+0x5c`, utilisé ici comme adresse de sortie et
   contrôleur de type/état pour la résolution de `+0x28` ;
2. le **slot vtable** à l'offset `0x5c`, qui est chargé depuis le premier mot
   de l'objet avant un dispatch indirect dans le worker.

La coïncidence numérique des offsets ne prouve pas que le contenu du champ
mémoire soit la vtable, ni que `0x82101be0` soit le résolveur utilisé par
`0x82101a18`. Le worker conserve donc la contradiction précédente : il forme
un champ de 9 bits dans `r4`, tandis que la feuille candidate lit `r4+0x1c`.

La nouvelle preuve permet seulement de dire :

```text
owner+0x28 = résultat d'une résolution de ressource, éventuellement invalidé
             selon owner+0x5c
owner-vtable[0x5c] = cible d'un dispatch indépendant, encore non reliée
```

Cette distinction évite de transformer un offset partagé en preuve de type ou
de classe.

## Niveau de confiance et décision

- `confirmed` : frontière corrigée de `0x82101a18` ; initialisation zéro des
  champs ; résolutions multiples ; `+0x28` reçoit le retour du résolveur ;
  branche de remise à zéro conditionnelle sur `*owner+0x5c == 4`.
- `cross-match` : appartenance de `0x820fbc28` et `0x820fa9c0` à la famille du
  sous-objet `0x8205c980`.
- `unknown` : identité des ressources, contenu exact de la table parcourue
  par le worker, vtable dynamique effective et interprétation du champ 9 bits.
- `needs-dynamic-evidence` : relation de la feuille `0x82101be0` avec les
  valeurs produites à l'exécution et avec NDXR/draw/vol.

Aucune session humaine n'est demandée. La prochaine étape statique rentable
est de comparer les writers de `owner+0x5c` et les chemins qui consomment le
handle de `owner+0x28`, sans les renommer en ressources graphiques avant une
preuve de provenance.

## Validation documentaire

- `DumpRange.java 0x820fbc28..0x820fc080` : PASS ;
- `DumpRange.java 0x821019c0..0x82101b40` : PASS ;
- dumps des deux callers `0x820fbbd4` et `0x820fcf3c` : PASS ;
- aucune écriture dans les projets Ghidra/XEX : PASS.
- build `cmake --build .build/ace-combat-6/native -j16` : PASS ;
- CTest `ctest --test-dir .build/ace-combat-6/native --output-on-failure` :
  **41/41 PASS**, 16,69 s ;
- `git diff --check` : PASS.
