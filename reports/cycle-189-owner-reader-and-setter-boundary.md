# AC6 cycle 189 — lecteurs de l'owner et borne du setter de famille

## Cible et méthode

- Target : `ac6-xbox360-pal`.
- Module : `default.xex`.
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Projet Ghidra : `ace-combat-6`.
- Analyse Xenon PPC big-endian headless, `-readOnly -noanalysis`.
- Aucun XEX, asset retail, projet Ghidra ou résultat généré n'a été modifié.

## Réduction des lecteurs de `DAT_8293BA10`

Le global `DAT_8293BA10` possède de nombreux lecteurs statiques, mais ils ne
forment pas tous des chemins d'objets. Les fonctions `0x82186f00`, `0x821875e0`,
`0x82187bf0` et `0x82188210` calculent notamment :

```text
FUN_82234dd0(DAT_8293BA10 + 0x15aa78, index dérivé de DAT_8293BA08)
```

Puis elles copient des mots et flottants du résultat vers des globals de
paramètres. Le motif `lwz ...,0xc(r3)` qui apparaît dans ces corps est un accès
à un record retourné par `FUN_82234dd0`, pas un chargement de slot de vtable :
aucun `mtspr CTR`/`bctrl` ne suit ces lectures. Ces fonctions sont donc des
lecteurs de données et sont exclues du chemin setter/dispatch.

Le script read-only
`scripts/SummarizeGlobalReferenceFunctions.java` produit cette réduction sans
attribuer de sémantique gameplay. Il conserve les quelques fonctions qui ont
à la fois une référence au global, un candidat `lwz ...,0xc(...)` et le contexte
local nécessaire à une vérification ultérieure.

## Borne brute du setter de la famille `0x82065ac4`

La table `0x82065ac4` contient un address-point dont l'entrée `+0x0c` vise
`0x821b9828`. La table des fonctions Ghidra découpe ce point en un petit thunk
(`0x821b9828..0x821b982c`), mais le flux brut contigu poursuit le corps utile à
`0x821b9830..0x821b99b0` :

```text
0x821b9858  lwz  r3,-0x70d0(r30)
0x821b985c  lwz  r11,0x0(r3)
0x821b9860  lwz  r11,0x4(r11)
0x821b9864  mtspr CTR,r11
0x821b9868  bctrl
...
0x821b9884  lis  r11,-0x7de4
0x821b9888  subi r11,r11,0x32a8       # helper matérialisé 0x821bcd58
0x821b9890  rlwinm r11,r11,0,0,31
0x821b9894  mtspr CTR,r11
0x821b9898  bctrl                       # résultat dans r3
0x821b98a0  or   r4,r3,r3
0x821b98a8  stw  r4,0x8(r31)            # this+0x08 = résultat
0x821b98ac  lwz  r11,0x0(r3)
0x821b98b0  lwz  r11,0xc(r11)
0x821b98b4  mtspr CTR,r11
0x821b98b8  bctrl                       # dispatch sur la ressource reçue
```

Cette continuation confirme un contrat ABI de setter/initialisation pour des
objets de la famille `0x82065ac4`: elle reçoit `this` en `r3`, obtient une
ressource, la stocke à `this+0x08`, puis appelle son propre slot `+0x0c`.
La représentation de fonction découpée par Ghidra doit être conservée dans
les rapports : le point d'entrée est `0x821b9828`, mais la preuve de corps
provient du flux brut jusqu'à `0x821b99b0`.

## Association au singleton : toujours non démontrée

Le constructeur publié par `0x821d6654 -> 0x821b9408` écrit bien la même
vtable `0x82065ac4`, initialise `owner+0x08` à zéro et publie l'objet dans
`DAT_8293BA10` à `0x821b95c0`. Toutefois :

- `FindPpcRawBranchesTo.java 0x821b9828` ne trouve pas d'appel direct au
  setter; l'entrée est consommée par dispatch de vtable.
- Le scan des fonctions qui chargent directement `DAT_8293BA10`, son premier
  mot puis le slot `+0x0c` ne trouve aucun chemin complet vers `0x821b9828`.
- Le corps `0x821b9828` lit un autre global de ressource avant le dispatch; ce
  motif ne suffit pas à identifier `DAT_8293BA10` comme `this` effectif.
- Les appels bruts à `0x821b9740` dans la boucle `0x821d7a34/0x821d7a68`
  construisent d'autres objets de la même famille et ne prouvent pas leur
  identité avec le singleton.

La qualification reste donc :

- `confirmed` : vtable, initialisation et publication de l'owner singleton ;
- `confirmed` : contrat statique du setter/initialiseur de la famille
  `0x82065ac4` ;
- `cross-match` : réutilisation de cette famille pour d'autres objets ;
- `needs-dynamic-evidence` : appel effectif du setter sur le singleton,
  valeur finale de `owner+0x08` et vptr runtime du receiver NDXR.

Une session humaine n'est pas requise pour ce cycle. Si l'identité runtime du
receiver devient bloquante, la prochaine preuve utile est une observation
Xenia ciblée sur l'initialisation, avec enregistrement du premier mot de
`DAT_8293BA10` et de `owner+0x08`; il ne faut pas remplacer cette lacune par une
nouvelle hypothèse statique.

## Artefacts

- `reports/ghidra-cycle-189-owner-function-summary.log`
- `reports/ghidra-cycle-189-vtable-method-boundary.log`
- `reports/ghidra-cycle-189-owner-constructor-context.log`
- `scripts/SummarizeGlobalReferenceFunctions.java`

