# Cycle 1720 — sonde directe de l’aperture XMA tardive

## Verdict

Deux exécutions fraîches, neutral et START, ont été rejouées avec le même
binaire codegen-ON et l’expérience XMA strictement opt-in. La trace
read-only couvre les deux familles d’adresses statiquement jointes au cycle
1718 ainsi que les deux accès fixes `0x7FEA1804` et `0x7FEA1818`.

Les deux routes produisent exactement une tentative observée : le store
`0x823572D8` de `0x82357240` vers `0x7FEA1A80`, valeur wire `0x01000000`, au
tick 1048/thread 21, puis le même trap avant effet. Aucun accès à
`0x7FEA1804`, `0x7FEA1818`, au chemin `0x82357310` ou au chemin
`0x823575A8` n’est atteint avant ce trap. Aucun mapping MMIO ni effet XMA
n’est ajouté.

Cette sonde ferme seulement la provenance dynamique de la frontière tardive;
elle ne nomme pas le registre, ne qualifie aucun packet ou flux audio, et ne
modifie pas la route de production.

## Identité, portée et sources

| élément | valeur |
|---|---|
| cible | `Default.xex`, démo PAL, Xenon big-endian/Xenos |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| basefile PAL SHA-256 | `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| binaire instrumenté | `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp` |
| binaire SHA-256 | `d80c12c25729d4f008d7471ea73c7ca07850f652a79560d5c5b28bfbb25c9edb` |
| hook `xma_import_trace.hpp` SHA-256 | `7aa156f8d9eae07ad525987d3667297db9eb3c9d4a1203bc910cf6831c5aea00` |
| intégration `guest_bridge.cpp` SHA-256 | `ca9187fa622151366ef83b9115167487a62c6d533d7a99450874da6464d5a7c0` |
| mode | `probe`, headless, `max_ticks=1050`, stores copiés frais |
| variables | `AC6_DEMO_EXPERIMENTAL_XMA_CREATE=1`, `AC6_DEMO_WATCH_XMA_ADDRESS=1`, `AC6_DEMO_WATCH_XMA_LATE=1` |
| temporaires | `/fastdata/lavaulta/tmp/ac6-cycle1720-final.8PoMxE` |

Le gate `rr` des cycles 1715–1717 reste la preuve A/B antérieure et n’est pas
réattribué à ce nouveau binaire. La présente sonde est directe uniquement.

## Plages surveillées

Les bornes viennent des bytes PAL du cycle 1718, pour tout index `u16` :

| origine PAL | plage surveillée | résultat dynamique |
|---|---|---|
| `0x82357310` | `[0x7FEA1A40,0x7FEA3A40)` | aucune tentative propre à cette fonction |
| `0x823575A8` | `[0x7FEA1940,0x7FEA3940)` | aucune tentative propre à cette fonction |
| stores fixes | `0x7FEA1804..0x7FEA1807` | aucune tentative |
| lecture fixe | `0x7FEA1818..0x7FEA181B` | aucune tentative |

`0x7FEA1A80` appartient numériquement à la première fenêtre, mais la seule
ligne observée provient de `0x82357240` et constitue la frontière déjà connue,
pas une preuve d’exécution de `0x82357310`.

## Résultats neutral/START

| champ | neutral | START (`--input-at 252,16,0,0,0,0,0,0,1`) |
|---|---:|---:|
| retour du probe | `3` | `3` |
| ticks complétés | `1048` | `1048` |
| PRESENT | `911` | `911` |
| RTPLY SHA-256 | `ab54c75ebccfdf3a41a840ab691a728e1d120a7616cca98a5f6ff42fa804fc43` | `2ce0a717f8d3df56aa87644d7fc9624f5e02b1020a0b5eeaf0992f6877db417f` |
| rapport SHA-256 | `ec64edad8c9d8657e25390c12ae4c2d7d1f2f1a3d351e059ddc16ed714011219` | `18ba4ec3fcf27338b465d02c40972f653aba415eab9d173abc4002c186f518ed` |
| stderr SHA-256 | `c1423f545587d0e696d3bfa35605e390763cca044ee741991da06761f9ad85ad` | identique |
| accès tardifs | `1` | `1` |
| hits `0x7FEA1804/1818` | `0` | `0` |

Observation commune exacte :

```text
op=store32 tick=1048 thread=21 address=0x7FEA1A80 size=4
value=0x0000000001000000 pc=0x82357240 lr=0x823572AC
function=__imp__sub_82357240 line=14268
```

Le trap est `unmapped guest 32-bit write`, avant effet mémoire. Les rapports
conservent `frontier.address=0x7FEA1A80`, `frontier.tick=1048`,
`frontier.thread=21`, `frontier.lr=0x823572AC` et `presents=911`.

## Classification et limites

- **demo-qualified** : identité XEX/basefile, bornes des fenêtres, deux stores
  frais, PC/LR/thread/tick/valeur wire, égalité de la tentative et du trap
  neutral/START, absence des trois autres groupes d’accès.
- **demo-observed** : tentative de store vers `0x7FEA1A80` et trap avant effet.
- **xenia-generic** : aucun élément utilisé.
- **unknown** : sémantique du registre, effet matériel, packets XMA, données
  XMA, XMA/XWB, PCM, audio anglais/japonais, pixels et transition frontend.

Le hook est désactivé par défaut et ne mappe aucune adresse. Aucun fichier
Ghidra, Xenia, ReXGlue, C++ généré, microcode ou actif propriétaire n’a été
modifié ou suivi.

## Prochain checkpoint

Ne pas contourner `0x7FEA1A80` et ne pas promouvoir les plages statiques comme
des registres. Le prochain progrès utile exige une preuve indépendante de
l’effet de ce store (ou une capture PAL/XMA autorisée), puis seulement une
nouvelle sonde des packets et du premier consumer audio. La route par défaut
reste fail-closed sur l’ordinal 548.
