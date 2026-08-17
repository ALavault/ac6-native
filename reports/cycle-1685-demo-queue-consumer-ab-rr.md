# Cycle 1685 — A/B `rr` de l’entrée consumer de file (supersédé)

> Erratum ABI : la sonde brute de ce cycle indexait `r3` à `ctx+24` (qui est
> `r2`). Les assertions `r3=0` et « objet nul » de cette version sont
> invalides. Le résultat corrigé, avec les offsets `PPCContext` exacts, est
> publié dans cycle 1686 ; ce rapport n’est pas une preuve d’acceptation.

## Verdict

La sonde brute, sans dépendance au TLS actif, a été rejouée sur les deux
traces `rr` process-fresh du même runtime/codegen noinline. Neutral et START
produisent chacun exactement **un** appel de `0x820FEFA8`, au tick 252, sur le
thread guest 25, avec LR `0x820FFD8C`, r31 `0x82386CC0` et le même tuple de
registres. L’A/B est donc byte/context-identique à cette frontière ; START ne
provoque aucune entrée supplémentaire du consumer dans cette fenêtre.

`r3=0` dans les deux routes. Les champs d’un objet guest ne peuvent pas être
lus et le rôle de `0x820FEFA8`/`0x820FFCA0` reste inconnu. Cette preuve ne
qualifie ni le frontbuffer ni un pixel, et ne légitime pas une screencap.

## Identité et méthode

| élément | valeur |
|---|---|
| cible | `Default.xex` PAL démo, SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| architecture | Xenon big-endian / Xenos |
| runtime | `ac6-demo-atomic-rr-noinline.12070`, binaire SHA `ed44dba583e2aa78d081ca52796ddc0e52b7b775015db41ca1198814c95b2ecc` |
| rr | `.tools/rr-install/bin/rr`, source `7352eb807ed75e3b51be85fa6a27f121235dbfb0`, binaire SHA `33fd6e3eade957f5b0e4c7e12ddb9f6ff54ce522103ad418f1b6d14737f454d6` |
| sonde | `tools/rr_queue_consumer_raw.gdb`, SHA `1ab05fa76edbca6c2306dc31b159031d220583e023c8296e33e0256a020549b4` |
| neutral | `/fastdata/lavaulta/tmp/ac6-demo-atomic-rr-noinline.12070/neutral.trace`, events SHA `77014c9464e034dd7a709b1f37512ba6abca757b83724aecea6cf9e35c153fef` |
| START | `/fastdata/lavaulta/tmp/ac6-demo-atomic-rr-noinline.12070/start.trace`, events SHA `a11313387e8704ed1ca15728585f128736caffe5402dcb7da8eedc44ad73df9a` |

La sonde s’arrête sur `__imp__sub_820FEFA8`, lit uniquement le contexte GDB
(GPR/LR/PC) et tente de lire le tick/thread via les symboles TLS ; elle ne
modifie aucune mémoire ni aucun état. Les deux replays atteignent leur
frontier borné tick 253.

## Observation A/B

| route | appels | tick | thread | LR guest | PC hôte (variable) | r3 | r4 | r5 | r6 | r7 | r31 |
|---|---:|---:|---:|---|---|---|---|---|---|---|---|
| neutral | 1 | 252 | 25 | `0x820FFD8C` | `0x619f54963bfb` | `0x00000000` | `0x00000001` | `0x00000000` | `0x00000000` | `0x00000030` | `0x82386CC0` |
| START `0x10` au tick 252 | 1 | 252 | 25 | `0x820FFD8C` | `0x633d31a92bfb` | `0x00000000` | `0x00000001` | `0x00000000` | `0x00000000` | `0x00000030` | `0x82386CC0` |

Les PC hôte diffèrent naturellement selon le processus ASLR et ne sont pas
une adresse guest. La callsite guest reste la séquence déjà jointe dans
`sub_820FFCA0` :

```text
addi r3,r1,80
bl   0x820FEFA8
LR   0x820FFD8C
```

## Qualification

- `demo-qualified` : identité XEX, runtime identique A/B, un appel exact par
  route, tick/thread/LR/GPR communs, sonde read-only.
- `demo-observed` : entrée `0x820FEFA8` à tick 252 avec r3 nul et queue base
  `0x82386CC0`.
- `xenia-generic` : aucun élément.
- `unknown` : objet/payload guest, rôle sémantique, prochain consumer de
  pixels, transition frontend, mission et résultat.

L’observation cycle 1684 des 16 appels START ticks 252–267 provient d’un
runtime/tracé t300 différent ; elle reste conservée comme observation
historique mais ne remplace pas cet A/B même-build.

## Garde et prochain test

Garde : toute promotion doit exiger exactement un couple neutral/START issu du
même runtime, la même identité de trace et un r3 non nul avant de déréférencer
un objet. Tant que r3 vaut zéro, le corridor s’arrête sans effet visuel.

Prochain test minimal : instrumenter le retour de `0x820FEFA8` et les branches
directes de `0x820FFCA0` sur une paire identique, puis capturer uniquement les
stores/loads guest bornés autour du slot `[0x82386D90,0x82386DD0+96)`. Aucun
état synthétique, resynchronisation, fallback visuel, retail ou actif
propriétaire ne doit être introduit.

Politique : Xenia/ReXGlue/Ghidra et le C++ généré restent inchangés ; les
microcodes et actifs propriétaires ne sont ni copiés ni suivis.
