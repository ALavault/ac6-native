# Cycle 146 — second constructeur et lookups de ressources du sous-objet NDXR

## Cible et portée

- Target : `ac6-xbox360-pal-default-xex`
- Module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Méthode : Ghidra `analyzeHeadless`, lecture seule, `-noanalysis`.

Cette passe suit le chemin de construction supplémentaire observé dans
`0x82183960` et les producteurs de champs de `0x820fa9c0`. Elle ne modifie ni
le XEX, ni le projet Ghidra, ni une sortie générée.

## Second chemin de construction du sous-objet

Dans `0x82183960`, le parent reçoit `r3 = param_1` et appelle :

```text
0x82183978  or   r31,r3,r3
...
0x821839c8  stw  r11,0x0(r31)       ; vtable du parent
...
0x82183a28  bl   0x820f9dc8
```

Le registre `r3` est préparé par `0x82183990` comme `r31+0x1844`, ce qui
correspond à `param_1 + 0x611` en mots. Le constructeur `0x820f9dc8` écrit
donc à nouveau `0x8205c980` à l'offset zéro de ce sous-objet, puis
`0x82183960` poursuit avec `0x820f9e78(param_1+0x1898)` et
`0x822b65e8(param_1+0x1900)`.

Cela confirme une seconde occurrence statique de la famille de sous-objet
`0x8205c980`, sans prouver que les deux parents représentent la même instance
runtime. La séparation parent/sous-objet reste obligatoire.

## Lookups indexés dans `0x820fa9c0`

La méthode construit plusieurs descripteurs temporaires à `r1+0x50` puis
appelle les deux helpers bornés. Les appels au helper pointeur
`0x82234e08` incluent notamment :

```text
index 4  -> sous-objet +0x0c
index 5  -> sous-objet +0x10
index 6  -> sous-objet +0x14
index 8  -> sous-objet +0x18
index 9  -> sous-objet +0x20
index 0xa -> sous-objet +0x24
index 0xb -> sous-objet +0x28
index 0  -> sous-objet +0x2c
```

Le producteur de `+0x28` est donc confirmé sous la forme :

```text
subobject+0x28 = bounded_pointer_table_lookup(local_descriptor, 0xb)
```

Le helper vérifie d'abord le compteur à `descriptor+0x00`, puis lit
`descriptor+0x10 + 4*index`. Le worker `0x82105bb8..0x82106354` utilise
ensuite `context+0x28` comme base de son parcours, charge un mot dans `r31` et
calcule le champ 9 bits avant le dispatch indirect.

La nouvelle preuve renforce la qualification structurelle
`resource_pointer_table`; elle ne donne pas encore l'identité métier des
entrées.

## Limite toujours ouverte

Le worker conserve exactement :

```text
r31 = word32be(table_entry)
r4  = (r31 >> 16) & 0x1ff
```

alors que la feuille candidate `0x82101be0` déréférence `r4+0x1c`. Les
recherches statiques n'ont trouvé qu'une occurrence en mémoire de la valeur
`0x82101be0`, à `0x8205c9dc`, dans la table `0x8205c980`; elles n'établissent
pas que cette table est la vtable effective de toutes les instances du worker.

Le résultat de l'indirect reste donc qualifié uniquement comme `r5 = dispatch
result`. Il est interdit d'appeler `r4` un pointeur de record ou de nommer le
contenu comme NDXR/scene/renderer sans preuve supplémentaire.

## Décision et suite

- `KEEP` : séparation parent/sous-objet et identité target-qualified.
- `KEEP` : `+0x28` comme entrée d'une table de pointeurs obtenue par lookup
  borné, index `0xb`.
- `KEEP_WITH_CLARIFICATION` : `0x8205c980` apparaît dans plusieurs chemins de
  construction, mais la vtable dynamique de chaque instance reste à qualifier.
- `needs-dynamic-evidence` : contradiction entre le champ 9 bits et la feuille
  qui lit `r4+0x1c`, après cette passe statique bornée.

Aucune action humaine n'est requise maintenant. La prochaine tentative statique
utile est de rechercher les éventuels writers indirects de vtables/slots ou les
consommateurs des descripteurs ; une session Xenia ne sera envisagée qu'après
épuisement de cette corrélation.

## Validation

- `DumpRange.java 0x821838d0 0x82183d20` : PASS.
- `ReferencesTo.java 0x82234e08` : PASS ; appels recensés dans la famille
  `0x820fa9c0` et d'autres consommateurs.
- `DumpRange.java 0x820fa8e0 0x820fbe40` : PASS.
- `DumpRange.java 0x82105ba8 0x82106370` : PASS.
- CTest AC6 : `cmake --build .build/ace-combat-6/native -j16` puis CTest,
  **41/41 PASS** en 16,31 s.
- `git diff --check` : PASS.
