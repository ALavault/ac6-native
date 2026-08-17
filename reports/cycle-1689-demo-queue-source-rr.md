# Cycle 1689 — source de slot avant `0x820FEFA8`, rr read-only

## Résultat

Une sonde rr/GDB positionnée à l’entrée de `0x820FFCA0` joint la formule de
sélection statique à la mémoire guest dans les deux traces noinline du même
build. Le premier appel observé est identique : tick 221, thread 25,
`queue=0x82386CC0`, indices producer/consumer nuls et slot source
`0x82386D90`. Les 24 mots big-endian de cette slot sont nuls.

La construction statique déjà jointe (`r11 = r31 + r10*96 + 208`) explique
`0x82386CC0 + 0*96 + 208 = 0x82386D90`. Cette preuve montre la source guest
bornée choisie par le consumer; elle ne nomme pas la slot et ne prouve pas
qu’une autre slot ne soit jamais écrite plus tard.

## Identité et sonde

| élément | valeur |
|---|---|
| cible | `Default.xex` PAL démo, SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| architecture | Xenon big-endian / Xenos |
| runtime | `/fastdata/lavaulta/tmp/ac6-demo-atomic-rr-noinline.12070` |
| binaire | SHA-256 `ed44dba583e2aa78d081ca52796ddc0e52b7b775015db41ca1198814c95b2ecc` |
| rr | commit `7352eb807ed75e3b51be85fa6a27f121235dbfb0`, SHA-256 `33fd6e3eade957f5b0e4c7e12ddb9f6ff54ce522103ad418f1b6d14737f454d6` |
| sonde | `recompilation/ace-combat-6-demo/tools/rr_queue_consumer_source.gdb` |
| sonde SHA-256 | `cf38fe7abea14b11bae6094b5769446053e0d868d62b9ed40b2d27282bde3490` |

La sonde lit `PPCContext`, les indices et la slot via `raw_base`, en
big-endian. Elle désactive son breakpoint après la première entrée et ne
modifie aucune mémoire ni état scheduler.

## Observations A/B

| route | trace events SHA-256 | log SHA-256 | tick/thread | queue | producer (`+0x60D0`) | consumer (`+0x60D4`) | source | non-zéro |
|---|---|---|---|---|---:|---:|---|---:|
| neutral | `77014c9464e034dd7a709b1f37512ba6abca757b83724aecea6cf9e35c153fef` | `7912aced06e3c9f7c0c300d5fc70a5dc090ad82cc34cc85e16868102d64b4ba4` | 221 / 25 | `0x82386CC0` | 0 | 0 | `0x82386D90` | 0/24 |
| START | `a11313387e8704ed1ca15728585f128736caffe5402dcb7da8eedc44ad73df9a` | `a523e822de2683f84b1bf745d0bd563526acbbec78c095c465b41d93b27c2792` | 221 / 25 | `0x82386CC0` | 0 | 0 | `0x82386D90` | 0/24 |

Les `$pc` hôte sont différents par ASLR et ne sont pas promus. Le LR guest
observé est `0x822EE194` dans les deux routes. La terminaison `SIGKILL` à
`syscall_traced` est technique et n’est pas une sortie guest propre.

## Jointure statique

Dans `.build/ac6-demo-codegen-atomic-1/generated/ppc_recomp.7.cpp` (SHA-256
`8b95cfc10189d3ebfde61210f78b837b145fd327c357c8177e12167f74f5a78b`),
`sub_820FFCA0` charge `+24784` et `+24788` depuis `r31`, utilise le second
comme index `r10`, puis calcule `r31 + r10*96 + 208`. La sonde confirme les
adresses et valeurs sur bytes guest. Il s’agit d’un cross-match littéral de
contrôle de flot et d’adressage, pas d’une sémantique métier.

## Qualification

- `demo-qualified` : identité PAL/runtime, formule d’adresse, indices nuls,
  slot source exacte et snapshot A/B nul au premier consumer.
- `demo-observed` : premier consumer à tick 221/thread 25; même slot et même
  contenu dans neutral/START.
- `xenia-generic` : aucun élément.
- `unknown` : writer de la slot, payload ultérieur, rôle de la queue,
  consumer frontbuffer, pixels, frontend, mission et terminal.

## Garde et prochain checkpoint

Conserver les plages `[0x82386CC0+24784,0x82386CC0+24788+4)` et
`[0x82386D90,0x82386DF0)`, l’ABI `PPCContext` corrigée et la règle
big-endian. Le prochain test doit tracer, sans écriture, les stores qui
pourraient modifier cette slot avant/après le tick 252 dans neutral et START;
une valeur non nulle ou une divergence doit arrêter le corridor et rester
`unknown`.

Aucune preuve retail, mutation Xenia/Ghidra/C++ généré/microcode ni actif
propriétaire n’est utilisée.
