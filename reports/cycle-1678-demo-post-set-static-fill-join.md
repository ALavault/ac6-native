# Cycle 1678 — jointure statique du premier store post-activation

## Résultat

Une ligne du flux `AC6_DEMO_WATCH_EVENT_POST_SET_MEMORY` est maintenant
raccordée à un callsite et à une fonction PAL exacts, sans interprétation
retail. Dans les deux routes fraîches (neutral et START, tick 300), l’entrée
`0x821A7160`, thread 1, produit au tick 3 le store suivant :

```text
store32 guest=0x17C74F84  lr=0x821A3E74
generated_function=__imp__sub_823273E0
```

Le retour `0x821A3E74` implique le `bl` PAL à `0x821A3E70`. Les bytes du
basefile PAL sont `48 18 35 71`, soit un appel à `0x823273E0`.

## Preuves statiques

| élément | preuve |
|---|---|
| cible | `Default.xex` démo PAL, SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| basefile | `.build/ac6-demo-codegen-xenon-38/xex-basefile.bin`, SHA-256 `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| callsite | `0x821A3C30..0x821A45FF`, entrée `0x821A3C30`, byte SHA-256 `bb41ae128e7548b79805f080e4953c9a65c81d0e0831509ccae29560ec9cfb3a` |
| instruction | `0x821A3E70`: `48 18 35 71`; LR observé `0x821A3E74` |
| callee | `0x823273E0..0x8232747F`, byte SHA-256 `b89e717560e2a52f7975ff89a362a8301cab15e05c3d5f90dac72af83eadb91a` |
| atlas | `analysis/demo/ac6-demo-static-decomp-atlas-v1.json`, record `0x823273E0`, pseudocode SHA-256 `bd6f73382fa16ab22bab4895d3e3595564fbbec7c5d3753990e00d094db9ea08` |

Le contrôle de flux du callee est borné par `r5` : il écrit le byte `r4` à
partir de `r3`, puis des mots répétés et une queue de bytes jusqu'à la longueur
demandée. Cela qualifie une primitive *fill/memset-like* au niveau des bytes,
mais ne donne pas le rôle jeu de son buffer ni la valeur dynamique complète.

## Qualification

- `demo-qualified` : identité XEX/basefile, callsite `0x821A3E70`, LR
  dynamique, cible `0x823273E0`, hash de bytes et égalité neutral/START.
- `demo-observed` : store `0x17C74F84` au tick 3 et association à l’entrée
  `0x821A7160`.
- `xenia-generic` : aucun.
- `unknown` : valeur/count des registres au callsite, consommateur logique du
  buffer, relation frontend/mission.

L’adresse `0x17C74F84` est hors de l’intervalle frontbuffer qualifié
`[0x1374A000,0x13AE2000)`. Cette jointure ne constitue donc ni un consumer
guest-owned de pixels ni une screencap.

## Prochain checkpoint

Armer une fenêtre read-only de 16 accès autour du même callsite et relever
`r3/r4/r5` avant `0x821A3E70`, puis joindre le résultat à l’appel suivant ou à
une écriture d’état. Aucun readback, fallback visuel ou mutation du projet
Ghidra/C++ généré n’est autorisé sur ce checkpoint.

Sources dynamiques : `analysis/demo/ac6-demo-event-post-set-memory-join-v1.json`
(SHA-256 `8516533d5dc08cb778100ae0cef74a52493c42564f9a003f794a31ad86649b4f`)
et son rapport cycle 1655. Le C++ généré consulté reste un artefact de build,
non suivi.
