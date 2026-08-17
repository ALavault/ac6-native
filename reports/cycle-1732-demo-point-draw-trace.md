# Cycle 1732 — trace PAL des point-draws bootstrap

## Verdict

Deux exécutions codegen-ON fraîches de la démo PAL, neutral et START au tick
252, ont produit les mêmes 24 `PointList` bootstrap au tick 0. Chaque ligne
est sur le thread guest 1, non prédicat, avec les mêmes hashes VS/PS et le
même état brut : `0x2000=0`, `0x2104=0`, `0x2180=0x1000000E`,
`0x2200=0`, `0x2201=0`, `0x2208=4`. Les deux fetch words sont nuls, donc la
sonde n'a lu aucun payload de mémoire.

Cette preuve ferme uniquement la branche observée des point-draws : leur
`color_mask` brut est nul et leurs registres depth bruts sont nuls. Elle ne
fournit ni pixel, ni source EDRAM non nulle, ni sémantique nommée pour les
registres opaques. Aucune screencap n'est qualifiable.

## Identité et exécutions

| élément | valeur |
|---|---|
| cible | `Default.xex`, démo PAL, Xenon big-endian/Xenos |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| basefile SHA-256 | `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| binaire codegen-ON | `fab052a88a479504634ebaf5f6a5e5b221f332e008f29b377e1db8811a5f5c91` |
| scope | `AC6_DEMO_WATCH_POINT_DRAWS=1`, headless, `max_ticks=253` |
| capsule | `analysis/demo/ac6-demo-point-draw-trace-v1.json` |

Neutral a le rapport `ecca3ef5…b94af` et le trace `1d41d2e2…7ebef7`.
START a le rapport `8fa22c09…6f7c4` et le trace `31553733…360b2`. Le stderr
de la sonde est byte-identique, SHA `68cdea5d…788a6`, et les deux routes
retournent 4 après `max_ticks=253`. Le rapport complet diffère seulement par
les événements de contrôle de l'entrée START; les sous-arbres graphics,
scheduler et outcome sont identiques.

## État observé

Les 24 lignes par route portent le VS `099625f3…e4e3` et le PS
`4913603d…8e25`. La sonde est strictement en lecture et bornée à 64 lignes.
Elle n'interprète aucun registre : les champs sont rapportés comme words
Xenos bruts. `fetch0=0`, `fetch1=0`, adresse calculée `0`, longueur `0` et
aucun octet n'est lu. Le renderer agrégé atteint 5 loads, 26 draws, 1
present et 4 modules validés, sans readback.

Le capsule `ac6-demo-neutral-edram-knownness-v1` reste l'autorité distincte
pour l'analyse sémantique déjà qualifiée du premier rectangle uniforme noir;
ce cycle n'en étend pas la portée. Les readbacks Vulkan noirs du cycle 1724
restent les seuls readbacks guest-owned reproduits, et ne sont pas une
screencap.

## Classification et garde

- **demo-qualified** : cardinalité 24, tick/thread, égalité A/B et état brut
  nul de `color_mask`/depth pour ces point-draws.
- **demo-observed** : hashes de microcodes, mode `0x2208=4`, fetch words nuls.
- **xenia-generic** : aucune donnée nouvelle.
- **unknown** : sens des registres bruts, writer EDRAM non nul après le
  bootstrap, pixels frontend, XMA/audio et mission.

Le hook est désactivé par défaut et n'est pas un chemin de rendu. Aucun Xenia,
patch Xenia, `ptrace`, retail, microcode suivi ou actif propriétaire n'a été
utilisé. Le prochain checkpoint reste le premier writer EDRAM non nul après
la frontière XMA ordinal 548; toute lecture d'image reste fail-closed.
