# AC6 cycle 187 — séparation du writer global et des objets de cycle de vie

## Cible et méthode

- Target : `ac6-xbox360-pal`.
- Module : `default.xex`.
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Analyse PPC Xenon big-endian, headless et statique.
- Les dumps sont bornés aux helpers et à leurs appelants; aucune sortie
  générée, aucun XEX et aucun asset n'est modifié.

## Le helper `0x8226a870` n'est pas le constructeur du receiver NDXR

Le début du helper effectue deux dispatchs sur son argument `r3` :

```text
0x8226a87c  r29 = r3
0x8226a880  lwz r11,0(r29)
0x8226a884  lwz r11,0x48(r11)
0x8226a88c  bctrl
...
0x8226a8a0  r3 = r29
0x8226a8a4  lwz r11,0x44(r11)
0x8226a8ac  bctrl
```

Il charge ensuite le global `0x826e4eb4`, appelle le slot `+0x36240` via
`0x8224f710`, appelle `0x82269ee8` sur le même argument, puis publie plusieurs
adresses dans la table globale. Le writer déjà identifié est :

```text
0x8226a978  addis r10,r11,0x3
0x8226a97c  addi  r10,r10,0x607c
0x8226a980  stwx  r10,r11,r6       # table[0x36084] = table+0x3607c
```

Le helper ne fait aucun `stw`/`stwx` au offset zéro de `table+0x3607c`. Il
publie le pointeur partagé, mais n'installe pas sa vtable.

## Appelants : objets distincts et arguments observables

### Chemin `0x82256558`

Le site est un branchement de queue (`b`, pas `bl`) vers `0x8226a870`. Le corps
précédent initialise de nombreux flottants à des offsets de `r3` (`+0x0c` à
`+0x8c`), puis le helper reprend le même objet. La vtable n'est pas écrite dans
ce chemin; après le retour logique, un autre fragment écrit `byte r3+0x360`.

Ce chemin prouve un objet de cycle de vie consommé par le helper, pas une
écriture du premier mot du receiver NDXR.

### Chemin `0x822e35dc`

Le caller fait d'abord :

```text
0x822e3518  r31 = r3
0x822e351c  bl 0x8226a610
...
0x822e3570  r31 = r3
0x822e3574  bl 0x8226b618
...
0x822e35d4  bl 0x8226a310
0x822e35d8  r3 = r31
0x822e35dc  bl 0x8226a870
```

`0x8226a610` initialise un objet local avec une table à l'offset zéro
(`0x82054f7c`) et remet ses champs à zéro. Cette table est distincte des
address-points NDXR `0x8205c980`/`0x8205c9a4`; ce chemin ne peut donc pas être
utilisé comme preuve d'une vtable NDXR.

`0x8226a310` lit ensuite `table+0x36054` et utilise ses slots `+0xd4`, `+0xd8`
et `+0x4c`. Il ne touche pas le premier mot de `table+0x3607c`.

### Chemin `0x822e67f4`

Le caller passe `r3=r31` au helper, puis lit `r31+0x260` et prépare des
structures locales à partir de `r31+0x348`. Là encore, aucun calcul ne vise
`table+0x3607c` et aucun store à son offset zéro n'est visible.

## Helper `0x82269ee8`

Le helper appelé par `0x8226a870` reçoit `r3` comme owner et parcourt deux
éléments via le slot `owner->vtable+0xfc`, puis effectue d'autres dispatchs
`+0x13c`, `+0x94` et `+0x1a0`. Il utilise également `table+0x36078` comme
ressource et appelle `0x8226eed8`.

Cette chaîne explique pourquoi `0x8226a870` peut être une étape de cycle de vie
ou d'état, mais elle ne fournit toujours pas de writer du premier mot du
receiver NDXR. Les slots restent qualifiés par adresse et contrat, sans nom
gameplay.

## Décision de preuve

- `confirmed` : `0x8226a870` publie l'alias
  `table+0x36084 -> table+0x3607c`.
- `confirmed` : son argument `r3` est un objet distinct, dispatché aux slots
  `+0x48/+0x44`; plusieurs callers l'initialisent avant l'appel.
- `confirmed` : le chemin `0x822e35dc` initialise un objet avec la table
  `0x82054f7c`, distincte des tables NDXR.
- `unknown` : premier mot/vptr runtime de `table+0x3607c` et éventuel writer
  indirect.
- `needs-dynamic-evidence` : identité runtime du receiver effectivement
  dispatché par les slots NDXR `+0x04`, `+0xc8` et `+0x140`.

La recherche statique a donc éliminé `0x8226a870`, `0x8226a610` et
`0x8226a310` comme preuves suffisantes d'une construction NDXR. La prochaine
preuve décisive reste une observation runtime du premier mot de
`table+0x3607c`, mais elle n'est pas encore une intervention humaine requise :
les traces instrumentées ou l'oracle Xenia peuvent être tentés ultérieurement.

