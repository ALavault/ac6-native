# Cycle 1687 — branches du consumer de file, rr read-only

## Verdict

Une sonde rr/GDB avec la table ABI `PPCContext` corrigée a armé les trois
callees possibles du chemin `0x820FEFA8` (`0x820FEA88`, `0x8226D6A0` et
`0x8226E398`) sur les deux traces noinline du même build. Aucun breakpoint
n’a produit de ligne `AC6_QUEUE_BRANCH` dans neutral ni dans START. La
conclusion est limitée à cette exécution : le buffer reçu à `0x820FEFA8`, dont
le champ objet `r31+0x40` était nul dans la sonde précédente, n’a pas conduit
à ces trois callees avant la fin de la trace.

Ce n’est pas une qualification de rôle métier, de consumer frontbuffer ou de
payload de file. Le chemin reste fail-closed et aucun état n’a été écrit.

## Identité et reproductibilité

| élément | valeur |
|---|---|
| cible | `Default.xex` PAL démo, SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| architecture | Xenon big-endian / Xenos |
| runtime | `/fastdata/lavaulta/tmp/ac6-demo-atomic-rr-noinline.12070` |
| binaire | SHA-256 `ed44dba583e2aa78d081ca52796ddc0e52b7b775015db41ca1198814c95b2ecc` |
| rr | `.tools/rr-install/bin/rr`, commit `7352eb807ed75e3b51be85fa6a27f121235dbfb0`, SHA-256 `33fd6e3eade957f5b0e4c7e12ddb9f6ff54ce522103ad418f1b6d14737f454d6` |
| sonde | `recompilation/ace-combat-6-demo/tools/rr_queue_consumer_branch.gdb` |
| sonde SHA-256 | `1d0e2183a8c6e1eb2ef6619ee6da2f79ba0b200ea736a447b54974ef04912b65` |

La sonde est read-only : elle lit uniquement le contexte GDB, le tick et le
thread. Elle ne pose aucun watchpoint écrivant, ne modifie pas la mémoire
guest et ne resynchronise aucune trace.

## Résultats A/B

| route | trace events SHA-256 | log GDB | SHA-256 du log | hits `0x820FEA88/0x8226D6A0/0x8226E398` |
|---|---|---|---|---:|
| neutral | `77014c9464e034dd7a709b1f37512ba6abca757b83724aecea6cf9e35c153fef` | `/fastdata/lavaulta/tmp/ac6-neutral-queue-consumer-branch-gdb.log` | `0212891977402e55c2381874d8d82b82294ead5674e403979431b8e21b3848aa` | `0/0/0` |
| START | `a11313387e8704ed1ca15728585f128736caffe5402dcb7da8eedc44ad73df9a` | `/fastdata/lavaulta/tmp/ac6-start-queue-consumer-branch-gdb.log` | `dab0f697ad6b701756d5daad19f32bf6405cc54de8c280e560227475edbd22a2` | `0/0/0` |

Les trois breakpoints sont bien installés dans chaque log. Les rapports
associés aux traces bornent l’exécution à `max_ticks=253`. La terminaison
GDB affiche `SIGKILL` à `syscall_traced`, comme les replays rr précédents ;
cette terminaison technique n’est pas promue comme une sortie guest propre.

## Jointure statique/dynamique

La sonde précédente, corrigée, joint neutral et START à l’entrée guest
`0x820FEFA8` au tick 252, thread 25, avec `r31=0x82386CC0`, `r3=0x2EEEBE90`
et le champ borné `r31+0x40 == 0`. Dans le C++ généré utilisé uniquement
comme cross-match littéral, `sub_820FEFA8` charge `lwz r11,64(r31)`, compare
les valeurs 1..4 puis branche vers `loc_820FF688` pour toute autre valeur;
le chemin observé par la sonde précédente rejoint donc l’épilogue sans les
trois appels armés. Source :
`.build/ac6-demo-codegen-atomic-1/generated/ppc_recomp.7.cpp`, SHA-256
`8b95cfc10189d3ebfde61210f78b837b145fd327c357c8177e12167f74f5a78b`.

Cette lecture du généré ne remplace pas une preuve binaire de sémantique :
elle sert uniquement à expliquer le résultat dynamique déjà joint aux bytes
PAL et à l’ABI du runtime.

## Qualification

- `demo-qualified` : identité XEX/runtime, ABI de contexte, breakpoints armés
  et résultat A/B sans divergence observée.
- `demo-observed` : entrée `0x820FEFA8` et objet de pile non nul au tick 252,
  champs bornés nuls, absence des trois appels dans les replays sondés.
- `xenia-generic` : aucun élément.
- `unknown` : rôle de la file, payload, retour/branches non instrumentés au
  niveau des blocs guest, consumer frontbuffer, pixels, frontend, mission.

## Garde et prochain checkpoint

La garde conserve la table `PPCContext` (`r3=0x00`, `r1=0x10`,
`r4..r31=0x20+(reg-4)*8`, `lr=0x100`) et refuse toute sonde sans cette
correspondance. Le prochain test minimal est un traçage borné des loads/stores
dans `[r3,r3+0x60)` et des slots `[0x82386D90,0x82386E30)`, en neutral/START
sur le même build. Toute adresse hors plage, valeur non nulle inattendue ou
divergence A/B doit arrêter le corridor et conserver `unknown`.

Aucune preuve retail n’est fusionnée; Xenia/ReXGlue, Ghidra, C++ généré,
microcodes et actifs propriétaires restent inchangés et non suivis.
