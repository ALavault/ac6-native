# AC6 cycle 188 — constructeur de l'owner et candidat setter du receiver

## Cible et méthode

- Target : `ac6-xbox360-pal`.
- Module : `default.xex`.
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Analyse PPC Xenon big-endian, Ghidra headless `-readOnly -noanalysis`.
- Aucun XEX, asset retail ou résultat généré n'a été modifié.

## Initialisation de l'owner partagé

Le branchement brut `0x821d6654 -> 0x821b9408` initialise l'objet ensuite
publié par le global `DAT_8293BA10` :

```text
0x821b9418  or   r30,r3,r3       # objet construit
0x821b9430  stw  r10,0x0(r30)    # vtable = 0x82065ac4
0x821b9444  stw  r31,0x8(r30)    # owner+0x08 = NULL
...
0x821b95c0  stw  r30,-0x45f0(r11) # DAT_8293BA10 = owner
0x821b95cc  bl   0x821bab70
```

Le constructeur est donc bien relié au global owner par un site d'appel brut,
mais l'appel `0x821bab70` ne renseigne pas `owner+0x08` : il remplace son
argument par des résultats de helpers et n'effectue pas ce store.

## Candidat setter de `owner+0x08`

La vtable `0x82065ac4` contient l'entrée `+0x0c -> 0x821b9828`. Le corps de
`0x821b9828` reçoit l'objet en `r3` (`r31 = r3`) puis effectue :

```text
0x821b9884  lis   r11,-0x7de4
0x821b9888  subi  r11,r11,0x32a8 # 0x821bcd58
0x821b9898  bctrl                 # résultat dans r3
0x821b98a0  or    r4,r3,r3
0x821b98a8  stw   r4,0x8(r31)     # objet+0x08 = résultat du helper
```

Il s'agit d'un setter statique crédible pour le champ `+0x08` des objets ayant
la vtable `0x82065ac4`. Le helper appelé à `0x821b9898` est matérialisé comme
`0x821bcd58`; sa sémantique applicative n'est pas nommée.

Le même address-point `0x82065ac4` est aussi écrit par `0x821b9740`, appelé
par les branches brutes `0x821d7a34` et `0x821d7a68` dans une boucle d'objets.
Cela renforce l'appartenance à une famille d'objets, mais ne prouve pas que
ces éléments sont l'instance singleton stockée à `DAT_8293BA10`.

## Limite de la preuve

Le scan headless des chemins qui chargent directement `DAT_8293BA10`, puis
`owner+0x08`, n'a retrouvé que des appels virtuels au slot `+0x20` du receiver;
aucun chemin statiquement démontré n'appelle le slot owner `+0x0c` (`0x821b9828`)
sur le singleton. Le candidat setter est donc confirmé au niveau de l'ABI et
du champ, mais son application au singleton reste `needs-dynamic-evidence`.

## Décision de preuve

- `confirmed` : `0x821d6654` branche vers `0x821b9408` et ce chemin publie l'objet
  dans `DAT_8293BA10`.
- `confirmed` : l'objet construit possède la vtable `0x82065ac4` et son champ
  `+0x08` est initialisé à zéro.
- `confirmed` : l'entrée de vtable `0x82065ac4+0x0c` est `0x821b9828`.
- `confirmed` : `0x821b9828` écrit `objet+0x08` avec le résultat du helper
  matérialisé `0x821bcd58`.
- `cross-match` : `0x821b9740` construit d'autres objets avec le même
  address-point et ses callers bruts.
- `needs-dynamic-evidence` : le moment où le singleton `DAT_8293BA10` reçoit
  son `+0x08`, la valeur effective et le vptr runtime du receiver NDXR.

Aucune action humaine n'est requise pour cette passe. Une trace runtime ciblée
reste la prochaine preuve décisive si l'association singleton → `0x821b9828`
devient nécessaire pour la native reconstruction.

## Commandes principales

```text
analyzeHeadless ... -readOnly -noanalysis -postScript DumpRange.java ...
analyzeHeadless ... -postScript FindPpcRawBranchesTo.java 0x821b9408
analyzeHeadless ... -postScript DumpU32Range.java 0x82065ac4 0x82065b30
analyzeHeadless ... -postScript TraceGlobalVirtualSlot.java 0x8293ba10 0x8 <slot>
```

