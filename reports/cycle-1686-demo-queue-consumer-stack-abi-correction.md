# Cycle 1686 — correction ABI et objet de travail du consumer

## Verdict

Le décodage exact de `PPCContext` montre que l’entrée
`0x820FEFA8` reçoit, dans le même runtime noinline, le même objet de pile
non nul sur neutral et START. Les deux routes ont un seul appel à tick 252,
thread 25, avec :

```text
r1  = 0x2EEEBE40
r3  = 0x2EEEBE90   (= r1 + 0x50)
r4  = 0x00000001
r5  = 0x00000000
r6  = 0x00000000
r7  = 0x00000030
r30 = 0x82386D08
r31 = 0x82386CC0
LR  = 0x820FFD8C
```

L’objet `r3` est donc un buffer de pile construit par la callsite, pas un
pointeur nul. Les champs bornés observés par la sonde corrigée sont tous
`0x00000000` dans les deux routes. Ce résultat ferme l’erreur de décodage des
cycles 1684/1685 ; il ne donne encore aucune sémantique métier au buffer ni
au consumer.

## ABI et identité

La disposition utilisée est celle du `PPCContext` généré et épinglé :

| registre | offset dans `PPCContext` |
|---|---:|
| r3 | `0x00` |
| r0 | `0x08` |
| r1 | `0x10` |
| r2 | `0x18` |
| r4..r31 | `0x20 + (reg-4)*8` |
| lr | `0x100` |

Cette table est une règle d’ABI de l’outil, pas une preuve de sémantique AC6.
Les sondes sont strictement read-only.

| élément | valeur |
|---|---|
| cible | `Default.xex` PAL démo, SHA `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| runtime | `/fastdata/lavaulta/tmp/ac6-demo-atomic-rr-noinline.12070`, binaire SHA `ed44dba583e2aa78d081ca52796ddc0e52b7b775015db41ca1198814c95b2ecc` |
| rr | commit `7352eb807ed75e3b51be85fa6a27f121235dbfb0`, binaire SHA `33fd6e3eade957f5b0e4c7e12ddb9f6ff54ce522103ad418f1b6d14737f454d6` |
| sonde | `tools/rr_queue_consumer_stack.gdb`, SHA `a3924894a8ff97f0e5afe7e97b124d5890ce2b6523098d0a28ac201b570ed4cf` |
| neutral trace | events SHA `77014c9464e034dd7a709b1f37512ba6abca757b83724aecea6cf9e35c153fef` |
| START trace | events SHA `a11313387e8704ed1ca15728585f128736caffe5402dcb7da8eedc44ad73df9a` |
| neutral log | `/fastdata/lavaulta/tmp/ac6-neutral-queue-consumer-stack2-gdb.log`, SHA `281b6e791c1f8a16d44554a9c18e396be84c5a98ee024e18b44026568d06b451` |
| START log | `/fastdata/lavaulta/tmp/ac6-start-queue-consumer-stack2-gdb.log`, SHA `b6df96cfc06d841b4d4e664fb4148d890538e0d91e978652a96608b6140f3809` |

## Preuve dynamique et statique

La callsite guest déjà jointe dans `sub_820FFCA0` est :

```text
addi r3,r1,80
bl   0x820FEFA8
LR   0x820FFD8C
```

Elle explique exactement `0x2EEEBE40 + 0x50 = 0x2EEEBE90`. Les deux traces
produisent les mêmes valeurs ; les PC hôte diffèrent seulement par ASLR.

Sur la trace START t300 d’un binaire historique, la sonde `entry` corrigée
observe également 16 appels successifs ticks 252–267 avec r3
`0x2EEEBE90`; les offsets d’objet `0,4,8,0x14,0x18,0x40,0x44,0x48,0x4c,0x50`
valent tous zéro. Le log correspondant est
`/fastdata/lavaulta/tmp/ac6-queue-consumer-entry-gdb-corrected.log`, SHA
`579611b358fb808c09a5245323ef93ad6d88ca2757f58ec46cb52ce3e446c424`.

## Qualification

- `demo-qualified` : XEX/runtime identifiés, table ABI appliquée exactement,
  A/B même-build, objet/r1/r3/LR/thread/tick et champs bornés joints.
- `demo-observed` : le buffer de pile non nul et entièrement nul à cette
  frontière, avec base de file `0x82386CC0`.
- `xenia-generic` : aucun élément.
- `unknown` : rôle du buffer, payload de queue, branches effectives,
  consumer guest-owned du frontbuffer, pixels, frontend, mission et résultat.

Les cycles 1684 et 1685 sont conservés pour la traçabilité, mais cycle 1685
est explicitement supersédé : son `r3=0` venait d’un mauvais offset GPR.

## Garde et prochain test

Toute sonde future doit utiliser la table ABI ci-dessus et refuser un registre
non mappé. Le prochain test minimal est un watchpoint/trace exact sur les
stores et loads de `[r3, r3+0x60)` et sur les slots guest bornés
`[0x82386D90,0x82386DD0+96)`, uniquement dans les deux routes identiques.
Un champ inattendu, une adresse hors plage ou une divergence neutral/START
doit arrêter le corridor avant effet. Aucun état synthétique, fallback visuel,
retail ou actif propriétaire n’est autorisé.

Politique : Xenia/ReXGlue/Ghidra, C++ généré et microcodes restent inchangés
et non suivis.
