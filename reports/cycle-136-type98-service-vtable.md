# AC6 — chaîne statique de pointeur et vtable du service `0x98`

Date : 2026-07-17 (Europe/Paris)

## Cible et périmètre

La cible est le `default.xex` Xbox 360 PAL qualifié, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

Cette passe est une inspection Ghidra headless en lecture seule. Elle ne lance
ni Xenia, ni Wine, ni GUI, ne modifie pas le projet Ghidra et ne demande pas de
session humaine. Elle complète le cycle 135 en suivant la valeur statique du
service global utilisé par le chargeur d'entrée 9.

## Résultat principal

Dans le projet Ghidra original, la chaîne statique est :

```text
0x826a0728 -> 0x826a0708 -> 0x820674d8
  global        service       vtable
```

`0x826a0708` contient donc le pointeur de vtable `0x820674d8`, et non une
vtable inconnue. Le projet corrigé utilisé dans certaines passes expose des
zéros à cette zone ; cette différence est traitée comme une différence de
représentation/relocation entre projets Ghidra, pas comme une preuve que la
chaîne n'existe pas. Les données et les références du projet original sont la
source de cette qualification statique.

## Vtable observée

Le dump `0x820674d8..0x82067550` donne :

| Slot | Cible | Contrat statique retenu |
|---:|---:|---|
| `+0x00` | `0x821bf958` | constructeur/thunk ; pas de nom métier |
| `+0x04` | `0x821bf8d8` | non qualifié dans cette passe |
| `+0x08` | `0x821c0038` | non qualifié dans cette passe |
| `+0x0c` | `0x821c0da0` | non qualifié dans cette passe |
| `+0x10` | `0x821c1070` | dispatch/initialisation de sous-objet |
| `+0x14` | `0x821c14a0` | non qualifié dans cette passe |
| `+0x18` | `0x821c1748` | parcours de records, filtre type 3 |
| `+0x1c` | `0x821c1960` | parcours de records, filtre type 3 |
| `+0x20` | `0x821c1130` | parcours de records à types 2/8/9 |
| `+0x24` | `0x821c1340` | calcul de taille/avance |

Les méthodes sont conservées avec des contrats offset-qualified. Aucune classe
concrète, ni composant avion, caméra, rendu ou mission n'est déduit de ces
adresses seules.

## Contrats des entrées exploitées

### Slot `+0x24` — calcul de taille/avance

`0x821c1340` est décompilée comme suit, avec `param_1` représentant l'objet de
service et `param_2` l'indicateur observé :

```c
int size = 0x120;
if (param_2 != 0)
    size = service_vtable_plus_0x20(param_1) + 0x120;
param_1[2] = size;
```

Cela établit un helper de taille/avance avec une dépendance au slot `+0x20`.
Il ne faut pas le renommer en allocateur ou en gestionnaire de ressources sans
preuve supplémentaire.

### Slot `+0x10` — initialisation/dispatch

Le corps brut `0x821c1070` :

- conserve `this` et les arguments dans les registres non volatils Xenon ;
- recopie une valeur de `this+0x04` vers `this+0x0c` lorsque le pointeur existe ;
- appelle des slots internes `+0x0c` et `+0x08` selon les arguments ;
- appelle le corps partagé `0x821c0e98` ;
- positionne le bit 0 de `this+0x88` ;
- retourne `1`.

Le contrat retenu est donc `dispatch/registration` avec état interne, pas
« création d'un avion ».

### Slots `+0x18`, `+0x1c` et `+0x20` — parcours de records

Les corps `0x821c1748`, `0x821c1960` et `0x821c1130` construisent des
descripteurs, parcourent des tables à pas variable et dispatchent plusieurs
types de records. Les deux premiers filtrent notamment le type de record 3 ; le
dernier traite les types 2, 8 et 9. Les appels auxiliaires observés sont
`0x821bf8f0`, `0x822c3388`, `0x822c3598`, `0x821d10d0` et `0x82335f18`.

Ces corps documentent une famille de traitement de payloads/records. Ils ne
permettent pas encore d'attribuer une sémantique de modèle 3D, d'aéronef ou de
caméra.

## Jonction exacte avec l'entrée 9 et le type `0x98`

Au site `0x820a7070`, les deux appels suivants chargent le service global,
passent l'élément MDLP en `r4` et le type `0x98` en `r5`, puis dispatchent les
slots `+0x18` et `+0x1c` :

```text
0x820a7190  service = *(0x826a0728)
0x820a7194  r4 = element_mdlp
0x820a7198  r3 = service
0x820a719c  r5 = 0x98
0x820a71a4  call service->vtable[0x18]

0x820a71e4  service = *(0x826a0728)
0x820a71e8  r4 = element_mdlp
0x820a71ec  r3 = service
0x820a71f0  r5 = 0x98
0x820a71f8  call service->vtable[0x1c]
```

Le même motif réapparaît autour de `0x820aa790` et `0x820aa7e4`. La chaîne
statique permet donc maintenant d'identifier les cibles exactes :

```text
type 0x98 + MDLP element -> service +0x18 -> 0x821c1748
type 0x98 + MDLP element -> service +0x1c -> 0x821c1960
```

Le résultat stocké à l'objet construit `+0x15c`, déjà observé dans les rapports
de l'entrée 9, reste un résultat de traitement non nommé.

## Littéral `0x9a`

Le fragment d'initialisation autour de `0x82093d38` appelle le slot `+0x10` de
la même vtable et passe `0x9a` dans une autre position d'argument (`r7`). Les
appels à cette fonction viennent de `0x82093c70`, `0x82093c9c` et
`0x82093cd0`.

La preuve disponible est donc :

- `0x98` : dispatch de records MDLP sur `+0x18/+0x1c` ;
- `0x9a` : discriminateur observé pendant l'initialisation/registration sur
  `+0x10` ;
- les deux valeurs utilisent la même famille de service, mais dans des phases
  et des positions d'argument différentes.

Il est interdit de traiter `0x9a` comme alias de `0x98` sans table de types,
trace dynamique ou preuve de comparaison inter-build.

## Confiance et frontière restante

`confirmed` :

- le pointeur global `0x826a0728` et la chaîne vers `0x826a0708` ;
- la vtable `0x820674d8` et ses entrées utilisées ;
- les cibles exactes des slots `+0x18`, `+0x1c`, `+0x20` et `+0x24` ;
- la position de l'élément MDLP et du littéral `0x98` au site d'entrée 9 ;
- les contrats offset-qualified des parcours et du helper de taille.

`unknown` :

- la classe C++ concrète et son nom métier ;
- la signification exacte des records filtrés ;
- la relation sémantique entre `0x9a` et `0x98` ;
- le consommateur final dans le modèle de vol, la caméra, l'arme ou le rendu.

La frontière `needs-dynamic-evidence` demeure uniquement sur cette dernière
attribution runtime. Elle ne bloque pas l'analyse statique ni le travail sur
les autres cibles.

## Validation

- Analyse Ghidra headless en lecture seule avec `DumpU32Range.java`,
  `ReferencesTo.java`, `DumpRange.java` et `DecompileAt.java` sur le projet
  original ;
- comparaison avec les exports persistants `820a7eb0.json`, `8214f1b8.json`,
  `821c2910.json`, `82280f18.json` et `822c47c0.json` ;
- CTest AC6 : **41/41 PASS** sur le dernier run ;
- aucune session Xenia, Wine, VNC ou GUI et aucune action humaine demandée.

## Suite proposée

Poursuivre en priorité les jonctions statiques vers les consommateurs de
payloads et les contrats de records. Ne lancer une session humaine/Xenia que si
la question devient réellement discriminante pour le consommateur final ; la
chaîne de vtable et le dispatch `0x98` sont désormais suffisamment bornés pour
continuer sans cela.
