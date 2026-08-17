# Cycle 1726 — sonde XMA opt-in bornée après `0x7FEA1A80`

## Verdict

Deux probes headless frais, neutral et START, ont été exécutés avec le même
binaire codegen-ON et deux stores importés séparément. Le hook expérimental
accepte exactement l'écriture PAL observée à `0x7FEA1A80` (`0x01000000` wire),
puis l'exécution atteint immédiatement l'import `xboxkrnl.exe` ordinal 548 au
tick 1048 (`lr=0x82357298`). Aucun nouveau consumer, packet XMA, flux PCM,
pixel ou screencap n'est qualifié. La route sans variable d'environnement
conserve le même trap ordinal 548.

Cette expérience confirme seulement une transition contrôlée de la frontière
observée; elle ne donne pas la sémantique du registre et ne devient pas une
implémentation de `play` ou `replay`.

## Identité et portée

| élément | valeur |
|---|---|
| cible | `Default.xex`, démo PAL, Xenon big-endian/Xenos |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| binaire basefile PAL | `b98a9ac1…4218` |
| binaire codegen-ON testé | `.build/ac6-demo-codegen-build-1/ac6-demo-recomp` |
| binaire SHA-256 | `15d480ad215a61f1d26e418ee03acb407b3e07d210500101188c91bd7dde8d61` |
| hook hand-written | `src/guest_bridge/lifecycle.hpp` |
| hook SHA-256 | `9df8383ae3c0b8e273e8ea008bc49199fb25fd2638cd63fec5c8814b3ec19ca4` |
| borne | `probe --until frontend --max-ticks 1100`, backend `headless` |
| stores | `/fastdata/lavaulta/tmp/ac6-cycle1726-xma-kick.nKruLn/{neutral,start}-store` |
| environnement opt-in | `AC6_DEMO_EXPERIMENTAL_XMA_CREATE=1`, `AC6_DEMO_EXPERIMENTAL_XMA_KICK=1`, sondes XMA activées |

Les microcodes, shaders, audio décodé et actifs propriétaires restent hors du
projet. Aucun checkout Xenia/ReXGlue, Ghidra ou C++ généré n'a été modifié.

## Observations directes

| champ | neutral | START (`0x10` au tick 252) |
|---|---:|---:|
| retour | `3` | `3` |
| ticks complétés | `1048` | `1048` |
| PRESENT | `911` | `911` |
| tentative PAL | `tick=1048 thread=21 pc=0x82357240 lr=0x823572AC` | identique |
| adresse/valeur wire | `0x7FEA1A80 / 0x01000000` | identique |
| hook exact | `logical=0x00000001` | identique |
| résultat | import ordinal 548, avant tout consumer | identique |

Le stderr neutral/START est byte-identique, SHA-256
`795f50d6f3737e1435ecf69ae404713585de363138c4a476d6fb747f5cd32958`.
Les rapports conservent les mêmes IB démo : intermédiaire
`ef7ab6e4832aed218b50126464de899ccf0f4bf2eaf26ecfac6371c51671d2b0` et
principal `d121c8d8cf55bcb755fa558c4d54a9311f4520fa2e8bb5e34b25920f107358d6`,
26 draws et un `XE_SWAP`; aucun pixel n'est déduit de ces compteurs.

| sortie | neutral | START |
|---|---|---|
| trace | `ab54c75ebccfdf3a41a840ab691a728e1d120a7616cca98a5f6ff42fa804fc43` | `2ce0a717f8d3df56aa87644d7fc9624f5e02b1020a0b5eeaf0992f6877db417f` |
| rapport | `77d1c1a6514b99a4d9bccef21a0bfc8a23aa7a0b979220a7081623201b1b20bb` | `42ebac6800212ab1ce69c9a0329246b55b28b60848c3b5fee63701b8f5f232b8` |

Le contrôle production, sans `AC6_DEMO_EXPERIMENTAL_XMA_KICK`, a également
retourné `3`, tick 1048/thread 21, ordinal 548; sa baseline reste séparée :
trace `6a759832c5471405591dac956bbc96dd99ab087d274067d01eea18849b428f20`,
rapport `f117a3e07b7da8d362c07be2a4d5b3f88c89a45eb1453ae53851eddd2d6fc0b1`.

## Classification

- **demo-qualified** : identité PAL, PC/LR/thread/tick, adresse et valeur du
  store, égalité neutral/START et conservation du trap par défaut; les hashes
  des IB restent ceux de la capture démo déjà qualifiée.
- **demo-observed** : le writer test-only a reçu une valeur exactement bornée
  et l'a laissée passer avant le retour vers l'import 548.
- **xenia-generic** : le terme « context kick » et l'arithmétique générique
  ayant motivé le hook; aucune sémantique n'est importée dans la démo.
- **unknown** : nom/effet de `0x7FEA1A80`, ABI de l'import 548, état XMA,
  timestamps/volume, consumer audio, PCM anglais/japonais, EDRAM non nul,
  pixels frontend, screencap et mission.

## Garde et prochain checkpoint

Le mapping est conditionné par `AC6_DEMO_EXPERIMENTAL_XMA_KICK`; les lectures,
adresses, tailles et valeurs divergentes piègent avant effet. La route par
défaut n'enregistre ni ne consulte ce MMIO. Le prochain test utile est une
preuve indépendante de l'effet de l'import ordinal 548 (ou une capture
matérielle autorisée), puis seulement le premier consumer audio. Tant que
cette preuve manque, conserver le trap et ne pas appeler FFmpeg/vgmstream ni
produire de screencap.

Validations : CTest OFF `18/18`, ON `17/17`, avec
`SDL_AUDIODRIVER=dummy xvfb-run -a`.

