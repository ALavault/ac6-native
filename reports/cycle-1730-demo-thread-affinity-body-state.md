# Cycle 1730 — scheduler Xenon/host et état du corps `0x822F8848`

## Verdict

La sonde codegen-ON neutral/START confirme que les « 23 threads bloqués »
sont des threads guest gérés par des fibers `ucontext` déterministes dans un
seul thread hôte, pas 23 `pthread` Linux bloqués dans `pthread_cond_wait`.
Les deux routes terminent à tick 1100 avec 23/23 threads guest bloqués,
0 runnable et 0 finished. Le code courant ne virtualise pas encore six
processeurs Xenon et le handler `KeSetAffinityThread` ne conserve pas le
`r4` brut; cette omission est désormais une frontière observée, pas une
preuve que l'affinité cause le gel.

Le hook d'état, opt-in et désactivé par défaut, a capturé 8 530 stores par
route dans `[0x82934000,0x82935000)`. Les dix adresses ne contiennent que des
zéros ou les pointeurs guest `0x829342A0` et `0x82934500`; ces stores sont de
l'état objet et ne qualifient ni un writer EDRAM ni des pixels.

## Identité et artefacts

| élément | valeur |
|---|---|
| cible | `Default.xex`, démo PAL, Xenon big-endian/Xenos |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| basefile PAL SHA-256 | `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| binaire codegen-ON | `ca3180188d24c0bc3f2598c15e2a34390007ad3fc869956d457ef1c55bcf8d4e` |
| probe | `AC6_DEMO_WATCH_BODY_STATE=1`, `AC6_DEMO_WATCH_INDIRECT_OBJECT=1`, `max_ticks=1100` |
| artefacts temporaires | `/fastdata/lavaulta/tmp/ac6-cycle1730-body-state.sfFBWG/` |
| Xenia/patch/ptrace | non utilisés |

Le rapport neutral a le SHA `18c331a6171b89429f1c277d9aaa208cc78c8d0be3bd935f5f53d1e60158bc78`
et son trace JSONL le SHA `4123e4b5115d9b2518bbaf0baa74c23af2a85fcba3afd0e8f994627bfc5a0e9d`.
Les équivalents START sont `2c9790c056395dcada28185618e4d163f10b1262002a513bb88990eadb2265cf`
et `a9e5e7c9fe0279b0da38fd480f08ccd66ab546101989b5e467f38ecd3ecae207`.

## Affinité et modèle d'exécution

Le scheduler source (`src/guest_bridge/scheduler.hpp`) crée des fibers
`ucontext`, les reprend séquentiellement depuis `run_runnable_threads()` et
ne possède aucun champ de processeur virtuel. L'observation dynamique rejoint
`xboxkrnl.exe!KeSetAffinityThread` ordinal 151 : appels avec `r4` brut dans les
groupes `{1,2,4,8,16,32}`, notamment thread 25 à tick 221 avec `r4=2`.
Le handler actuel valide l'objet, écrit `1` dans le pointeur d'affinité
précédente et retourne le succès; il ne mémorise pas `r4` dans `GuestThread`.

Cela est compatible avec une frontière d'implémentation à qualifier, mais pas
avec une conclusion causale sur les 23 blocages. Le modèle Xenon six HT,
les paires `0/1`, `2/3`, `4/5` et toute politique vCPU restent
`xenia-generic` jusqu'à une preuve issue du XEX et d'un A/B guest-visible.

## État du corps et limites renderer

La jointure dynamique reste `LR=0x822E559C` → objet `0x82934280` → vtable
`0x8202A488` → slot 4 → `0x822F8848`. Chaque route compte 853 activations et
8 530 stores; les stderr sont byte-identiques
(`1f82cc94cc348f7754a8725fb2d6d5763d6d4a513118c1cfa4fddf6160b80e56`).
Les stores portent `LR=0x822F042C` et `__imp__sub_822F0410`, avec dix adresses
et trois valeurs. La présence de pointeurs guest non nuls ne constitue pas
une source EDRAM et ne change pas le readback noir qualifié du cycle 1724.

Le dépôt retail audité séparément et son backend ReXGlue/Xenia restent un
cross-match générique : aucune preuve pixel/shader PAL n'est importée.

## Classification

- **demo-qualified** : stabilité A/B du callsite, de l'objet/vtable/slot et
  de l'état objet observé.
- **demo-observed** : appels `KeSetAffinityThread`, 23 blocked, stores bornés
  et hash identique neutral/START.
- **xenia-generic** : topologie Xenon et éventuelle stratégie vCPU.
- **unknown** : sémantique de `r4`, effet d'une affinité retenue sur la
  progression, writer EDRAM non nul, pixels, frontend, XMA/audio, mission.

## Checkpoint suivant

Ajouter une trace read-only des valeurs raw `r4`, du résultat précédent et du
processeur guest courant à chaque `KeSetAffinityThread`, puis exécuter un A/B
borné avec état d'affinité retenu uniquement pour mesurer une différence
guest-visible. Ne pas activer le parallélisme hôte ni un mapping six-vCPU
avant cette preuve. En parallèle, poursuivre le watchpoint EDRAM ciblé; les
stores d'état objet de ce cycle ne doivent pas être promus en preuve renderer.

Capsule durable : `analysis/demo/ac6-demo-thread-affinity-body-state-v1.json`.
