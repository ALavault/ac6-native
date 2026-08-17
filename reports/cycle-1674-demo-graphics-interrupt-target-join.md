# Cycle 1674 — cible indirecte de l’interruption graphique

## Verdict

La cible indirecte interne du callback graphique est maintenant jointe sur
les bytes PAL démo et sur deux rapports runtime process-fresh. Le callback
`0x821B9710`, appelé sur le thread guest 2 au tick 1 avec `source=1`, effectue
un `bctrl` dont le retour guest est `0x821B9768`; la cible effectivement
résolue est `0x821C5190`. Neutral et START donnent exactement la même arête,
une seule fois.

Cette preuve qualifie la frontière d’interruption et son handler exécuté; elle
ne qualifie toujours pas un consumer de pixels. Aucun load observé ne touche
`0x1374A000..0x13AE2000`, et aucune screencap n’est promue.

## Identité et sources

- Cible : `ac6-demo-xbox360-pal`, `Default.xex`, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`;
  Xenon big-endian / Xenos.
- Trace neutral :
  `/fastdata/lavaulta/tmp/ac6-int-neutral-800-1786828401.json`, SHA RTPLY
  `bbc7ecb8fc20bfdbbdd1d70d24a4fdc78f6619dd5622c761e88541d776fd7068`.
- Trace START :
  `/fastdata/lavaulta/tmp/ac6-int-start-800-1786828402.json`, SHA RTPLY
  `54a2860fccb21ab3be0595ae5532a7f9e0dbc3b7b971aaf334a7a087f5a427e1`.
- Atlas statique démo :
  `analysis/demo/ac6-demo-static-decomp-atlas-v1.json`, SHA
  `7ee1e677dfac287fdcd8d80b1c5f34575cbabf1c41ab79e70bd1581f87114e2d`.
- Atlas des sites indirects : `analysis/demo/ac6-demo-indirect-sites-v1.json`.

## Join dynamique

| champ | neutral | START |
|---|---:|---:|
| caller | `0x821B9710` | `0x821B9710` |
| site / LR de retour | `0x821B9768` | `0x821B9768` |
| cible | `0x821C5190` | `0x821C5190` |
| thread guest | `2` | `2` |
| tick premier/dernier | `1 / 1` | `1 / 1` |
| compteur | `1` | `1` |
| `r3` à l’appel | `0x00000100` | `0x00000100` |
| `r4` contexte | `0x10041A00` | `0x10041A00` |
| `r31` cible | `0x821C5190` | `0x821C5190` |

L’arête d’entrée vers le callback lui-même est aussi identique : thread 2,
`lr=0`, cible `0x821B9710`, 800 appels aux ticks 1–799. Les champs ci-dessus
sont issus de `control_flow.edges`, pas d’un nom déduit.

## Preuve statique de la cible

L’atlas `.pdata` donne pour `0x821C5190` :

- plage `0x821C5190..0x821C5323`;
- bytes SHA-256 `2ec755d70c3feb6adc7ae4eacf85403e1a2609cd4d5fe74c17c33f65a61ac1d3`;
- pseudocode SHA-256 `9ae78c3dcdb91951358d0c82e4a50c977c40c47b6a8a05cb6a52068fc37a9c1f`;
- appels directs `0x823270F8`, `0x82375FD4`, `0x82375FE4`, `0x82376064`;
- imports `KeQueryPerformanceFrequency`, `KfAcquireSpinLock`,
  `KfReleaseSpinLock`;
- global référencé `0x82000608`.

Le site computed-call interne de cette fonction est `0x821C528C` (instruction
`bctrl`, hash `79cb7a343751219f212b2198c45b0992afa52c689ec6d9a9b055591a73a09aea`).
L’atlas le conserve `resolution=unknown`, sans cible inventée. Dans les deux
runs, ce site ne produit aucune arête dynamique supplémentaire; la valeur
chargée qui gouvernerait cet appel reste donc non qualifiée.

Le rapport renderer précédent documente seulement le comportement de pont
observé : le handler est atteint après `PM4_INTERRUPT source=1`, met à jour la
frontière d’attente et permet la reprise déterministe. Cela ne doit pas être
renommé en consumer de framebuffer.

## Classification

- `demo-qualified` : arête exacte `0x821B9710 + 0x821B9768 → 0x821C5190`,
  thread/tick/registres, identité A/B et bytes de la cible.
- `demo-observed` : exécution du handler et reprise de la frontière PM4.
- `xenia-generic` : aucune nouvelle preuve.
- `unknown` : champ d’état lu par `0x821C5190`, cible de `0x821C528C`, lecture
  hôte de scanout, pixels, frontend, mission et screencap.

## Garde et prochain test

La garde existante reste opt-in (`AC6_DEMO_WATCH_GRAPHICS_INTERRUPT=1`), limitée
à 8192 appels et sans mutation de GuestMemory. Le prochain test ciblé doit
journaliser uniquement, dans `0x821C5190`, les loads de l’état global
`0x82000608` et la valeur de `r9` au site `0x821C528C`; il doit trapper si une
adresse sort de la plage d’état autorisée. Il ne faut pas écrire de pixels,
contourner le callback ou promouvoir START.

## Contraintes

Aucun Xenia/ReXGlue, projet Ghidra, C++ généré, microcode, preuve retail ou
actif propriétaire n’a été modifié ou suivi.
