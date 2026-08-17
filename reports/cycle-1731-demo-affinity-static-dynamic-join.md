# Cycle 1731 — jointure PAL de `KeSetAffinityThread`

## Verdict

La sonde codegen-ON, opt-in et en lecture seule, rejoint la fonction PAL
`0x821A5390` avec 19 appels à `xboxkrnl.exe!KeSetAffinityThread` par route.
Les valeurs `r4` observées sont exactement les six masques one-hot
`1,2,4,8,16,32`; les bytes PAL du corps comparent l'entrée à `6`, calculent
`1 << r4` pour `0..5`, puis passent ce masque à l'import à l'instruction
`0x821A53DC`. Cette jointure est **demo-qualified** pour le chemin statique et
les valeurs dynamiques observées.

Elle ne prouve pas encore un ordonnanceur six-vCPU : le scheduler de la
recompilation reste constitué de fibers guest `ucontext` reprises
séquentiellement par un seul thread hôte. Neutral et START restent
byte-identiques et terminent à `max_ticks=1100` avec 23 threads guest bloqués,
0 runnable et 0 finished. Aucun effet renderer, pixel, audio ou mission n'est
promu par ce cycle.

## Identité et provenance

| élément | valeur |
|---|---|
| cible | `Default.xex`, démo PAL, Xenon big-endian/Xenos |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| basefile SHA-256 | `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| binaire codegen-ON | `83bda520cabcba2d939be4df2224f4f82240a2752475c420fbbb898bf3c34e23` |
| capsule | `analysis/demo/ac6-demo-affinity-static-dynamic-join-v1.json` |
| capsule SHA-256 | `868a080fa2c7f57d96982d12341727c563db08dfa082994474c311f322f695ff` |
| traces | neutral `4123e4b5…a0e9d`, START `a9e5e7c9…ae207` |
| rapports | neutral `18c331a6…bc78`, START `2c9790c0…65cf` |
| variables | `AC6_DEMO_WATCH_AFFINITY=1`, XMA watch opt-in inchangé |

La sonde n'a utilisé ni Xenia, ni le patch Xenia archivé, ni `ptrace`, ni
preuve retail. Les sorties temporaires restent sous
`/fastdata/lavaulta/tmp/ac6-cycle1731-affinity.RKX0DV/`.

## Preuve statique PAL

L'atlas canonique borne `0x821A5390..0x821A543F` par `.pdata`, avec le hash de
bytes `69cdc3705fab26d4c0cb9a2012a5a1750a6d822d736c7e671a1982af13b246bc`.
La fonction importe `KeSetAffinityThread` ordinal 151 et les références
objet attendues. Le contrôle de flux littéral autour de `0x821A53DC` compare
`r4` à six, forme le bit demandé et transmet le résultat; le retour est
converti par `cntlzw/subfic`. Les noms générés ne sont pas utilisés comme
preuve sémantique.

## Preuve dynamique A/B

Chaque route a 19 lignes `AC6_AFFINITY`; les stderr neutral/START ont le même
SHA `8f7e29ed19e266f35528c98bea9ec9a65674d75e5edd76718440cd100a69141d`.
Les threads, ticks, objets, pointeurs précédents, valeurs précédentes, `r4`,
résultats et LR `0x821A53DC` sont identiques entre les routes. L'ensemble des
masques bruts est `{0x1,0x2,0x4,0x8,0x10,0x20}`. Une valeur précédente
`0x17760000` est observée au tick 106, mais sa sémantique n'est pas nommée.

Le handler de production continue à retourner le succès existant et à écrire
`1` dans le pointeur mappé; la sonde lit l'ancien contenu avant cet effet et ne
change pas la route de production. Une tentative séparée de retenir l'état
d'affinité a provoqué un `SIGSEGV` hôte dans `record_import_edge/std::map`
avant tout rapport guest-visible; elle a été retirée et ne constitue pas une
preuve pour ou contre la sémantique Xenon.

## Classification et garde

- **demo-qualified** : frontière PAL `0x821A5390`, appel `0x821A53DC`, chemin
  one-hot `0..5`, et stabilité neutral/START des 19 observations.
- **demo-observed** : pointeurs/valeurs précédents, résultat `0`, état terminal
  23/0/23/0.
- **xenia-generic** : topologie Xenon à trois cœurs/six contextes et toute
  stratégie de mapping vCPU hôte.
- **unknown** : propriétaire des masques, effet d'une affinité conservée sur
  la progression, writer EDRAM non nul, pixels, frontend, XMA/audio et
  résultat de mission.

La garde reste fail-closed : aucune activation de `AC6_DEMO_EXPERIMENTAL_AFFINITY`
en production, aucun parallélisme hôte ajouté, et aucune promotion renderer.

## Checkpoint suivant

Qualifier dans les bytes PAL le writer et le contrat du pointeur précédent,
puis reprendre le watchpoint ciblé du premier store EDRAM non nul. Toute
expérience d'état retenu devra être bornée, explicitement opt-in et protégée
contre un crash hôte avant effet guest.
