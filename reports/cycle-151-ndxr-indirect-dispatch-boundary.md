# AC6 — frontière du dispatch indirect et absence de callsite direct (cycle 151)

## Cible et méthode

- target : `ac6-xbox360-pal-default-xex` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- projet : `ghidra-projects/ace-combat-6` ;
- mode : `analyzeHeadless -readOnly -noanalysis` ;
- aucune écriture dans le XEX, le projet Ghidra, les exports ou le runtime.

Cette passe cherche à distinguer un appel direct de la seule entrée de table
du dispatch `context->vtable[+0x5c]`. Elle ne traite pas une adresse trouvée
dans une table de données comme une preuve d’instance runtime.

## Résultats

### Feuille `0x82101be0`

Le balayage `FindDirectCallsTo.java 0x82101be0` ne trouve aucun `bl` direct vers
la feuille. `ReferencesTo.java 0x82101be0` ne trouve aucune référence code ou
donnée supplémentaire dans le projet analysé.

Le seul emplacement statique de la valeur est :

```text
0x8205c9dc  -> 0x82101be0
```

Il s’agit de l’entrée `+0x5c` de la table candidate `0x8205c980`. Le résultat
renforce la qualification suivante : la feuille est atteinte par un dispatch
indirect lorsque cette table est effectivement chargée, et non par un callsite
direct manquant dans le catalogue Ghidra.

### Méthodes associées et tables de métadonnées

Les valeurs de `0x820fbc28` et `0x82105ba8` ont aussi été recherchées dans les
blocs de données :

- `0x820fbc28` apparaît à `0x8205ca8c` (`+0x10c`) et dans la table alternée
  `0x8207c0b0` ;
- `0x82105ba8` apparaît dans la table alternée `0x8207c218` ;
- `0x8207c0b0` alterne des adresses de fonctions et des mots de taille ou de
  métadonnées (`0x4000....`). Il ne doit pas être classé comme une seconde
  vtable sans preuve de lecture par un objet.

La table `0x8205c980` reste la seule table candidate dont les entrées voisines
à `+0x10c`, `+0x110`, `+0x13c` et `+0x5c` forment une famille de fonctions
cohérente. Cela demeure un cross-match statique, pas une observation de la
vtable chargée par chaque owner au moment du worker.

## Conséquence ABI

Le worker garde la séquence :

```text
lwz    r10,0(r18)              ; premier mot de l’owner/context
rlwinm r4,r31,0x10,0x17,0x1f   ; champ numérique de 9 bits
lwz    r10,0x5c(r10)            ; slot virtuel
mtctr  r10
bctrl
```

La feuille candidate conserve son contrat local :

```text
lhz r11,0x1c(r4)
add r11,r11,r4
lwz r3,0x8(r11)
blr
```

L’absence d’un callsite direct ne résout donc pas la contradiction. Elle ferme
seulement une hypothèse : il n’existe pas, dans le projet analysé, une autre
invocation directe de cette feuille qui expliquerait un ABI différent. Restent
possibles une vtable dynamique différente, une dérivation/écriture indirecte de
la table, un alias runtime ou une compréhension incomplète du chemin de
registre.

## Statuts de confiance

- entrée `0x8205c9dc -> 0x82101be0` : `confirmed` comme donnée statique ;
- absence de callsite direct : `confirmed` pour le balayage Ghidra exécuté ;
- table `0x8207c0b0` comme vtable d’owner : `unknown` ;
- table effectivement chargée par le worker : `cross-match`/`unknown` ;
- rôle de `r4` et callee effectif : `needs-dynamic-evidence`.

Aucune action humaine, capture Xenia ou run manuel n’est requis par cette
passe. La prochaine recherche statique utile est la provenance des écritures du
premier mot des owners et la présence d’un remplacement de vtable après les
constructeurs observés. Si elle n’ajoute aucun chemin, une capture runtime
ciblée deviendra la première action externe justifiée.

## Validation documentaire

- `FindDirectCallsTo.java 0x82101be0` : PASS, aucun callsite direct ;
- `ReferencesTo.java 0x82101be0` : PASS, aucune référence supplémentaire ;
- `FindU32Any` et `DumpDataWords` sur les tables candidates : PASS ;
- aucun fichier généré, binaire ou projet Ghidra modifié : PASS.
