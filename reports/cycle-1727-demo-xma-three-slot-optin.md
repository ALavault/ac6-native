# Cycle 1727 — trois slots XMA PAL traversés en opt-in

## Verdict

Deux probes headless fraîches, neutral et START, ont traversé les trois
entrées PAL de la table XMA et les trois écritures exactes de
`0x7FEA1A80` (`0x01000000`, `0x02000000`, `0x04000000`). L'expérience reste
strictement opt-in et test-only. Elle ne qualifie ni le nom ni l'effet du
registre, ni l'ABI de l'import ordinal 548, ni un paquet XMA ou un échantillon
audio. Après le troisième store, les deux routes atteignent `max_ticks=1100`:
23 threads sont bloqués, une seule présentation est observée, et aucun
milestone frontend/mission/terminal n'est atteint. Il n'y a donc toujours pas
de screencap qualifiable.

## Identité et périmètre

| élément | valeur |
|---|---|
| cible | `Default.xex`, démo PAL, Xenon big-endian/Xenos |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| basefile PAL SHA-256 | `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| binaire codegen-ON | `.build/ac6-demo-codegen-build-1/ac6-demo-recomp` |
| binaire SHA-256 | `8c2138cab60eb123b31e89852c4fc0af030b47fc5c32f1eee5b933881424c841` |
| hook lifecycle SHA-256 | `1ddca123d962c6b95b523427cd002e61cbd10e568f3f4893c5ea3ff4d5c8a606` |
| hook XMA dispatch SHA-256 | `815de350f09e84a512accb9b51ced1891bfed224c7baa3ae187c2e131fdfe07b` |
| header bridge SHA-256 | `21e2603729b65b7c4bc205fb19cc5ab5e0e6844e90eff1b5285697088e4bbe9c` |
| borne | `probe --until frontend --max-ticks 1100 --backend headless` |
| stores | `/fastdata/lavaulta/tmp/ac6-cycle1727-xma-three.hp2Gs4/{neutral,start}-store` |
| rr | gate existant conservé; aucune réexécution rr dans ce cycle |

Variables opt-in utilisées :
`AC6_DEMO_EXPERIMENTAL_XMA_CREATE=1`,
`AC6_DEMO_EXPERIMENTAL_XMA_KICK=1`,
`AC6_DEMO_WATCH_XMA_ADDRESS=1`,
`AC6_DEMO_WATCH_XMA_KICK=1`,
`AC6_DEMO_WATCH_XMA_LATE=1`,
`AC6_DEMO_WATCH_XMA_CREATE=1`.

## Évidence PAL observée

La lecture précédente est reproduite à `tick=106`,
`0x7FEA1800 -> 0x2E800000`, puis le store global
`0x829DA52C <- 0x2E800000` (`lr=0x8234F078`). À `tick=1048`, thread 21,
la boucle PAL appelle l'import à `lr=0x82357298` avec les trois slots
`0x17360050`, `0x173600B0`, `0x17360110` et
`r4=0,r5=0x6180,r6=0,r7=1`. Les sorties de contexte test-only sont
respectivement `0x2E800000`, `0x2E800040`, `0x2E800080`, avec statut nul.

| slot PAL | contexte rendu | PC du store | LR guest | valeur wire | valeur logique |
|---|---:|---:|---:|---:|---:|
| `0x17360050` | `0x2E800000` | `0x82357240` | `0x823572AC` | `0x01000000` | `1` |
| `0x173600B0` | `0x2E800040` | `0x82357240` | `0x823572AC` | `0x02000000` | `2` |
| `0x17360110` | `0x2E800080` | `0x82357240` | `0x823572AC` | `0x04000000` | `4` |

Les trois stores sont acceptés uniquement avec l'ordre de bits attendu; toute
adresse, taille, valeur ou slot divergent piège avant effet. Le hook ne
mappe aucune lecture et n'est pas consulté par la route de production.

## Neutral / START et frontière après XMA

Les stderr neutral et START sont byte-identiques :
`f425991a3c3d0ed165009595152121cfcdab10669e27521f978920a63236e2ff`.
Les sous-arbres `graphics` des deux rapports sont identiques (digest normalisé
`0a15df8790bcdd050a57cef6a3843b4ae3b4108fd0f7c4e1c4523b0fdb331e98`). Les
traces restent distinctes car l'identité d'entrée est distincte.

| route | trace SHA-256 | rapport SHA-256 | résultat | ticks | PRESENT | frontend/mission/terminal |
|---|---|---|---|---:|---:|---|
| neutral | `4123e4b5115d9b2518bbaf0baa74c23af2a85fcba3afd0e8f994627bfc5a0e9d` | `18c331a6171b89429f1c277d9aaa208cc78c8d0be3bd935f5f53d1e60158bc78` | `max_ticks` | 1100 | 1 | non / non / non |
| START | `a9e5e7c9fe0279b0da38fd480f08ccd66ab546101989b5e467f38ecd3ecae207` | `2c9790c056395dcada28185618e4d163f10b1262002a513bb88990eadb2265cf` | `max_ticks` | 1100 | 1 | non / non / non |

À la frontière finale, le rapport joint seulement un appel indirect répété :
thread 1, tick 1100, `LR=0x822E559C`, cible `0x822F8848`, clé d'attente
`0xE000004C`, dernier tick d'appel indirect 1099. Le compteur observé est
`1 030 473`; le nom et la sémantique de la cible restent inconnus.
Le scheduler rapporte `23` threads, `0` runnable, `23` bloqués, `0` terminés.
La file de rendu a une base observée `0x82386CC0`, `1695` changements du
producteur et `0` changement du consommateur dans cette fenêtre; ce constat
ne donne pas la sémantique de la file.

La capture graphique structurée reste celle déjà qualifiée : ring
`0x126CA000`, capacité `131072` dwords, deux IB
(`ef7ab6e4…d2b0`, `d121c8d8…358d6`), `5` loads shader, `26` draws et un
`XE_SWAP` vers le frontbuffer `format=6`, tiled, `1280×720`. Aucun pixel non
nul n'est produit par ce cycle.

## Classification

- **demo-qualified** : identité PAL, lecture `0x7FEA1800`, global
  `0x829DA52C`, trois tuples slot/contexte/PC/LR/tick, ordre des trois valeurs
  wire, égalité stderr neutral/START, et état scheduler final observé.
- **demo-observed** : acceptation test-only des trois stores et progression
  jusqu'à la frontière `0x822E559C -> 0x822F8848`.
- **xenia-generic** : aucun élément ajouté dans ce cycle.
- **unknown** : nom/effet de `0x7FEA1A80`, ABI et effet de l'ordinal 548,
  consumer matériel/audio, paquets XMA, PCM anglais/japonais, source EDRAM
  non nulle, pixels frontend, screencap, entrée START causale et mission.

## Garde et prochain checkpoint

Conserver l'ordinal 548 en trap par défaut et l'expérience trois-slots hors
production. Le prochain test est une instrumentation read-only de la cible
`0x822F8848` et de la clé `0xE000004C` (PC/LR/thread/tick et writer du signal),
puis une source EDRAM non nulle avant resolve. Ne pas appeler
`vgmstream-cli`/FFmpeg et ne pas produire de screencap tant qu'un consumer
guest-owned et un readback non nul ne sont pas prouvés.

Validations : CTest codegen-OFF `18/18`, codegen-ON `17/17`, avec
`SDL_AUDIODRIVER=dummy xvfb-run -a` et `TMPDIR=/fastdata/lavaulta/tmp`.

