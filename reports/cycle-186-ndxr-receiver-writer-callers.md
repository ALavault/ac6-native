# AC6 cycle 186 — receiver NDXR : writer et chemins de cycle de vie

## Périmètre et identité

- Cible : `ac6-xbox360-pal`.
- Binaire : `default.xex`.
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Les adresses ci-dessous sont qualifiées dans l'image Xenon big-endian dont la
  base est `0x82000000`.
- Analyse read-only headless ; aucune modification de projet Ghidra, de code
  généré ou d'asset propriétaire.

## Question

Les cycles précédents distinguaient le global owner statique
`0x823f9b28 -> 0x823f9b2c` du receiver NDXR partagé lu par
`table+0x36084` (`0x82a1c7a4`). Cette passe cherche les écritures de ce champ,
la construction de son voisinage et les appels des helpers qui initialisent la
table globale.

## Références au champ et à son slot voisin

`ReferencesTo.java` donne les résultats suivants :

- `0x82a1c79c` (`table+0x3607c`, slot voisin) n'a qu'une référence définie,
  le site data `0x8226a980`.
- `0x82a1c7a4` (`table+0x36084`, receiver partagé) est lu par
  `0x8226847c`, `0x8209f16c`, `0x820a04d0`, `0x820a04ec`, `0x820a0508`,
  `0x820ac214`, `0x820ac240`, `0x820ac274`, `0x820ac2a8`, `0x820ac2d4`,
  `0x82228df4`, `0x822334d4`, `0x82233830`, `0x822a40cc`, `0x822a40f0`,
  `0x8231c690` et `0x822ec774`; `0x8226a980` est signalé comme writer.
- `FindU32Any.java` ne trouve pas de mot immédiat supplémentaire pour ces
  adresses : les accès sont calculés à partir du global `0x826e4eb4` et des
  offsets.
- `FindGlobalPointerFieldStores.java` ne retourne pas de candidat, car le
  writer emploie `stwx` après construction de l'offset plutôt qu'un store
  immédiat détectable par ce script.

Ces résultats ne prouvent donc pas l'absence d'une écriture indirecte du
premier mot du receiver.

## Writer de la table globale

Dans le corps borné autour de `0x8226a870`, le chemin de mise en place calcule
le pointeur global puis exécute :

```text
0x8226a978  addis r10,r11,0x3
0x8226a97c  addi  r10,r10,0x607c   # r10 = table + 0x3607c
0x8226a980  stwx  r10,r11,r6       # table[0x36084] = table + 0x3607c
```

Le même bloc écrit plusieurs autres offsets voisins dans la table globale,
mais cette séquence ne renseigne pas le premier mot de `table+0x3607c` lui-même.
Elle confirme l'alias statique entre le champ receiver (`+0x36084`) et le slot
voisin (`+0x3607c`), sans confirmer la vtable runtime de l'objet pointé.

## Appelants du chemin d'initialisation

Le balayage PPC brut de branches directes vers l'entrée helper `0x8226a870`
trouve :

```text
0x82256558 -> 0x8226a870
0x822e35dc -> 0x8226a870
0x822e67f4 -> 0x8226a870
```

Le site `0x82256558` est précédé d'une initialisation de plusieurs champs de
`r3`; après le retour, le chemin écrit notamment `r3+0x360 = 1`.

Le chemin `0x822e35dc` arrive après des écritures de champs de `r31`, un appel
à `0x8226b618`, une configuration de valeur et des appels à `0x8226a018` et
`0x8226a310`, puis appelle l'helper avec `r3=r31`.

Le chemin `0x822e67f4` appelle également l'helper avec le receiver de `r3`, puis
examine `*(r31+0x260)` et construit des données locales.

Ces chemins sont compatibles avec une phase d'initialisation ou de cycle de
vie, mais aucun nom de constructeur, de classe ou de système gameplay ne doit
être attribué sans preuve supplémentaire. Les rôles restent
`unknown`/`needs-dynamic-evidence`.

Les références brutes vers l'entrée fragmentée voisine `0x8226a9d0` sont
`0x8226bfcc` et `0x8226c02c`; elles sont conservées comme helpers distincts et
ne sont pas fusionnées avec le writer intérieur `0x8226a980`.

## Conclusion et prochaine preuve utile

- `confirmed` : le helper initialise le pointeur partagé
  `table+0x36084` avec l'adresse `table+0x3607c`.
- `confirmed` : plusieurs chemins de cycle de vie appellent l'entrée du helper.
- `unknown` : la première valeur/vptr de `table+0x3607c` et son éventuelle
  réécriture runtime.
- `needs-dynamic-evidence` : identité de l'objet receiver effectivement
  dispatché par les slots `+0x04`, `+0xc8` et `+0x140`.

La prochaine expérience utile est une trace d'exécution ciblée sur l'un des
chemins d'initialisation, avec capture du contenu de `table+0x3607c` avant et
après l'appel à `0x8226a870`. Aucun run humain, VNC ou intervention GUI n'est
nécessaire pour cette étape statique ; elle ne doit être demandée que si la
capture dynamique ne peut pas être obtenue par les instruments existants.

