# Cycle 33 — résolution de la frontière post-slot AC6 PAL

Pour le XEX PAL qualifié par SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`,
le conflit du cycle 32 est résolu statiquement. Le dump Ghidra borné de
`0x823461b8..0x823462a7` montre :

```text
0x823461b8  mfspr r12,LR
0x823461bc  bl 0x82382efc
0x823461c0  stwu r1,-0x70(r1)
...
0x823461ec  bctrl
...
0x82346200  b 0x82382f4c
```

La preuve indépendante déjà enregistrée au cycle 19 montre que
`0x82382efc` sauvegarde `r29..r31` et LR puis finit par `blr`. Il s'agit d'une
sous-entrée de helper ABI, pas d'une branche sans retour. Le marquage Ghidra
`noreturn` masque donc le corps après le prologue et ne décrit pas le contrôle
runtime.

Le corps récupéré verrouille la frontière statique de `Function_823461B8` : il
prend l'objet en `r3`, conserve l'argument `r4`, appelle une opération sur
`objet+0xc`, effectue un dispatch indirect via le slot `+0x10` de la vtable du
pointeur `objet+0x4`, écrit l'argument dans `objet+0x4`, puis exécute une autre
opération sur `objet+0xc`. La branche finale va au helper de restauration ABI.
Ces offsets restent des rôles structurels ; aucun nom de service ou de gameplay
n'est affirmé.

Le statut de `0x823462a8` peut revenir de `manual-review` à
`statically-understood`. Une trace Xenia/XenonTests reste nécessaire pour
confirmer les identités d'objets et toute future stabilité de hook, mais elle
n'est plus nécessaire pour décider si `0x823461b8` retourne.

Reproduction :

```sh
HOME=/tmp/ac6-ghidra-cycle35-home \
  .tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript DumpRange.java 0x823461b8 0x823462a8 \
  -noanalysis
```

Sortie : `workspaces/ace-combat-6/reports/ghidra-cycle-35-post-slot-body.log`.
