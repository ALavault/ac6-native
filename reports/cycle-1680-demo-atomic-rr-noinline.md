# Cycle 1680 — A/B `rr` des sites atomiques après frontière `noinline`

## Verdict

Une nouvelle génération du runtime conserve exactement 6 `ldarx` et 8
`stdcx.`; l'attribut `noinline` rend les deux adaptateurs GDB-observables sans
changer le code guest. Deux traces `rr` process-fresh, neutral et START,
atteignent l'entrée guest `0x822E4290` à tick 0, thread 13, avec
`r3=0x82935270` et LR guest `0x822EE194`. Aucune route n'entre dans
`AC6_PPC_LDARX` ou `AC6_PPC_STDCX`, donc aucune adresse atomique effective et
aucun accès frontbuffer ne sont observés. Le résultat est une absence bornée,
pas une preuve que les sites seront inatteignables après transition frontend.

## Identité et build

| élément | valeur |
|---|---|
| cible | `Default.xex` PAL démo, SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| rr | `.tools/rr-install/bin/rr`, source commit `7352eb807ed75e3b51be85fa6a27f121235dbfb0` |
| binaire runtime | `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp`, SHA-256 `ed44dba583e2aa78d081ca52796ddc0e52b7b775015db41ca1198814c95b2ecc` |
| adaptateur source | `tools/ppc_context_adapter.h` SHA `138be495fa44ceff0e3f06db1ba611f0a33a0d3fdbf91642d83c6c01dc298643` |
| implémentation | `src/guest_bridge/graphics_mmio_cpu.hpp` SHA `bcbe03debf63ee4fd0e6c45936f1001b1600b6a2650a8bac29c4981bc6f317d0` |
| script GDB | `tools/rr_atomic_probe.gdb` SHA `b91b48fa183693b73f8e2e36970b0039e0920841aefb8fcaa9def6aa8b58cbc7` |
| génération | 52/52 fichiers, 0 diagnostic de frontière, 0 instruction non supportée |
| appels statiques | 6 `PPC_LDARX`, 8 `PPC_STDCX`; 0 `__sync_bool_compare_and_swap`; 0 déréférencement brut `base +` |

`noinline` est limité aux deux fonctions d'adaptation et ne modifie ni le
XEX, ni XenonRecomp, ni le C++ généré. CTest avec
`SDL_AUDIODRIVER=dummy xvfb-run -a` passe **17/17**.

## Traces `rr` et GDB

Répertoire temporaire :
`/fastdata/lavaulta/tmp/ac6-demo-atomic-rr-noinline.12070/`.

| route | trace/data | events SHA-256 | rapport SHA-256 | RTPLY SHA-256 | GDB log SHA-256 |
|---|---:|---|---|---|---|
| neutral, 253 ticks | 311445834 octets | `77014c9464e034dd7a709b1f37512ba6abca757b83724aecea6cf9e35c153fef` | `420fb9341ea0a56be33f0143f9bf9be768b08a1697fe7cb314e5e055bbf2a843` | `1d41d2e26003a631f8bec19534812a258ffc5d90466be885f6f7f3df797ebef7` | `16efc94a46444f7f6788bf411ace8995a642bbfdf6b24ec1b9d7c72b245395fe` |
| START `0x10` tick 252, 253 ticks | 311406657 octets | `a11313387e8704ed1ca15728585f128736caffe5402dcb7da8eedc44ad73df9a` | `8506399dca66624afcf52a8746bc4cd08c669f30021d9fb954bb8bebb01f7327` | `31553733582cf7345375d8f1978a28645621e488700f2a31b7e883b66109360b2` | `8fd421ddeacf2ec29ad00a2dcc1bbb18cfebca4651c85d5260cf7374e757309d` |

Les deux logs GDB contiennent chacun exactement une ligne :

```text
AC6_RR_ATOMIC_ENTRY function=0x822E4290 lr=0x822ee194 r3=0x82935270 tick=0 thread=13
```

Ils ne contiennent aucune ligne `AC6_RR_ATOMIC op=ldarx/stdcx`. Les stderr
runtime sont vides et aucun `AC6_FRONTBUFFER_*` ou `RuntimeTrap` n'apparaît.
Les deux rapports conservent les IB qualifiés
`ef7ab6e4832aed218b50126464de899ccf0f4bf2eaf26ecfac6371c51671d2b0` et
`d121c8d8cf55bcb755fa558c4d54a9311f4520fa2e8bb5e34b25920f107358d6`,
116 PRESENT, `frontend=false`, `mission=false`, `terminal=false`.

## Qualification

- `demo-qualified` : identité XEX/rr, présence de l'entrée atomique exacte,
  PC/LR guest, `r3`, tick, thread, zéro appel d'adaptateur, A/B RTPLY/rapport
  et absence de frontbuffer.
- `demo-observed` : exécution de `0x822E4290` à tick 0 dans les deux routes.
- `xenia-generic` : aucun.
- `unknown` : sites atomiques après transition frontend, consumer guest-owned,
  pixels, frontend, mission et screencap.

## Prochain checkpoint

Conserver cette garde comme preuve négative et déplacer la sonde sur le
premier tick où une transition guest est effectivement observée. Si un
wrapper est atteint, joindre son adresse effective, PC/LR, thread et tick;
trap avant toute adresse dans `[0x1374A000,0x13AE2000)`. Ne pas synthétiser
une transition START ni promouvoir le writeback hôte Vulkan.

Politique : aucune preuve retail fusionnée, aucun checkout Xenia/ReXGlue/Ghidra
modifié, aucun C++ généré ou microcode suivi, aucun actif propriétaire ajouté.
