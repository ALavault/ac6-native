# Cycle 1677 — couverture des accès mémoire générés et atomiques `rr`

## Verdict

Le codegen PAL actuel expose 52 fichiers générés, 55 663 364 octets, dont
l'empreinte agrégée est
`bf249ebf5ab102dcc934521b92109a325eeaeb20fb290bda13625d4da73febbf`.
Tous les loads scalaires générés passent par les macros `PPC_LOAD_U8/U16/U32/U64`
et les loads VMX directs atteints passent par `simde_mm_load_si128`, déjà
interceptés par les builds diagnostiques. Le seul accès mémoire généré brut
restant est la paire `ldarx`/`stdcx.` de cinq fonctions de synchronisation.

Le replay inverse `rr` neutral headless 253 qualifié atteint une seule de ces
fonctions : `0x822E4290`, avec `r3=0x82935270`, donc des adresses atomiques
effectives `0x82935280`, `0x82935288` et `0x82935298`. Elles sont hors de
`[0x1374A000,0x13AE2000)`. Les quatre autres fonctions atomiques n'atteignent
aucun point dans cette trace. Aucun accès guest-owned au frontbuffer n'est donc
observé par cette forme résiduelle, et aucune screencap n'est promue.

## Inventaire statique exact

| forme générée | occurrences | couverture |
|---|---:|---|
| `PPC_LOAD_U8` | 7 050 | `AC6_PPC_LOAD_U8` |
| `PPC_LOAD_U16` | 5 021 | `AC6_PPC_LOAD_U16` |
| `PPC_LOAD_U32` | 117 024 | `AC6_PPC_LOAD_U32` + garde handler |
| `PPC_LOAD_U64` | 12 051 | `AC6_PPC_LOAD_U64` |
| `simde_mm_load_si128` | 42 275 | trace VMX diagnostic |
| `simde_mm_loadu_si128` | 0 | aucun chemin généré |

Les 14 expressions brutes restantes sont 7 `ldarx` et 7
`__sync_bool_compare_and_swap` correspondants, dans :

- `0x821B9648` : effective `r3` ;
- `0x821B96B8` : effective `r3` ;
- `0x822E4240` : effective `r3+16` ;
- `0x822E4268` : effective `r3+24` ;
- `0x822E4290` : effective `r3+16`, `r3+24`, `r3+40`.

Il n'y a aucune autre déréférence `base` directe dans les 52 sources générées.
Ces sources et le C++ généré restent hors du suivi du projet.

## Preuve dynamique `rr`

- outil : `.tools/rr-install/bin/rr`, commit source
  `7352eb807ed75e3b51be85fa6a27f121235dbfb0` ;
- trace : `/fastdata/lavaulta/tmp/ac6-demo-rr-gate.jLnJ38/probe-trace/` ;
- route : neutral headless, 253 ticks, RTPLY/report déjà qualifiés ;
- binaire enregistré SHA-256
  `24712c2487c94f917edd7635dfd6abb082477888b2c5a4a25e9ebc7d03920bd8` ;
- breakpoints GDB sur les cinq symboles générés ; sortie normalisée SHA-256
  `a148b463b02ad541bda54f3e80071118cb188e398690eea081ff170fd5d414da`.

Résultat normalisé :

```text
RR_ATOMIC function=822E4290 guest_r3=0x82935270 effective=[0x82935280,0x82935288,0x82935298]
```

Les fonctions `0x821B9648`, `0x821B96B8`, `0x822E4240` et `0x822E4268`
n'ont aucun hit dans cette trace. La tentative de `rwatch` sur trois mots du
frontbuffer a seulement rendu un arrêt `syscall_traced` avec valeur illisible;
il n'est pas compté comme lecture guest et n'est pas promu comme preuve.

## A/B START sous `rr`

Une seconde trace indépendante a été enregistrée avec la même configuration,
le même binaire et `buttons=0x10` au tick 252. Le run direct et le run `rr`
terminent tous deux avec `max_ticks` (253 ticks), et leurs rapports sont
strictement identiques :

| élément | direct | sous `rr` |
|---|---|---|
| RTPLY SHA-256 | `31553733582cf7345375d8f197a28645621e488700f2a31b7e883b66109360b2` | identique |
| rapport SHA-256 | `8506399dca66624afcf52a8746bc4cd08c669f30021d9fb954bb8bebb01f7327` | identique |
| IB principal | `d121c8d8…358d6` | identique |
| VdSwap / frontend / mission | `116 / non / non` | identiques |
| état handler | 12 lignes, aucune violation | identique |
| hit atomique | `0x822E4290 → 0x82935280/88/98` | identique |

Le binaire capturé dans la trace START a SHA-256
`a4a5b57f2aaf7404fe2a84f8f7ef34a974b1304c14f81de8021271d183957811` et la
sortie GDB atomique SHA-256
`ebdca681055b44a423ad54bffe462a5f46781c955e31a7cde9123ab5ddfc3f63`.
Cette A/B ferme la limite précédente du gate neutral : le résultat atomique
est identique sur START sans promotion de sa sémantique.

## Qualification

- `demo-qualified` : identité de la trace/binaire, inventaire exhaustif des
  formes générées, cinq fonctions atomiques, adresse effective du seul hit et
  absence de recouvrement avec la plage PAL ;
- `demo-observed` : un passage de `0x822E4290` dans la route neutral ;
- `xenia-generic` : aucun nouvel élément ;
- `unknown` : accès atomique d'une future route non atteinte, scanout externe,
  pixels non noirs, consumer guest-owned, frontend, mission et screencap.

Cette preuve complète les A/B scalaires/vectoriels et la frontière de lecture
hôte du cycle 1676. Elle ne transforme pas le writeback en présentation.

## Validation et prochain test

Les builds codegen et diagnostique et leurs CTest `17/17` restent valides;
`AC6_DEMO_STATUS_PASS` est inchangé. Aucun Xenia/ReXGlue, Ghidra, C++ généré,
microcode ou actif propriétaire n'a été modifié.

Le prochain test ciblé est un replay `rr` frais avec la même instrumentation
sur une route START, puis une garde atomique uniquement si l'un des cinq
symboles devient atteint. Toute adresse atomique recouvrant le frontbuffer
doit être arrêtée avant effet et jointe à un PC/LR/thread/tick; sans cette
preuve, le renderer reste fail-closed.
