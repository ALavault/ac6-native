# Cycle 1691 — slot consumer neutral jusqu’au tick 600

## Résultat

Un probe neutral codegen-ON, process et store frais, avec
`AC6_DEMO_WATCH_RENDER_QUEUE_SLOTS=1`, atteint 600 ticks. Le hook produit
2 440 stores bornés entre les ticks 221 et 599 :

- `0x82386D90` (slot consumer) : **0 store direct** ;
- `0x82386DD0` (slot producer) : 348 stores, tous des mots zéro, thread 1,
  `0x820FF710` / LR `0x820FF734` ;
- `0x8238CD90/94` : 1 045 stores chacun (métadonnées) ;
- `0x8238CD98/9C` : 1 store chacun (métadonnées).

Les valeurs non nulles appartiennent aux métadonnées et ne sont jamais
promues en payload. La fenêtre ne fournit donc toujours aucun producteur de
contenu de slot.

## Identité et sortie

| élément | valeur |
|---|---|
| cible | `Default.xex` PAL démo, SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| architecture | Xenon big-endian / Xenos |
| binaire | `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp`, SHA-256 `ed44dba583e2aa78d081ca52796ddc0e52b7b775015db41ca1198814c95b2ecc` |
| run | neutral, `probe --max-ticks 600`, rc 4 `max_ticks` attendu |
| store | copie fraîche de `.build/ac6-demo-store-test-3` sous `$TMPDIR` |
| hook | `AC6_DEMO_WATCH_RENDER_QUEUE_SLOTS=1`, opt-in et read-only |

| artefact | SHA-256 |
|---|---|
| RTPLY | `b724112495de5af96b395f93ba5d1dacd8a522bfae2aa1fcc69890b3c9aac9fb` |
| rapport | `dbe6484af6c1446db0e2e5017b34441d909cc07e81922959d8ad9d9719b16484` |
| stderr | `4465437e2485e4ec097fe13bd999b11573210530bea34dc37a10c319081d1d9a` |

## Qualification

- `demo-qualified` : identité PAL, fenêtre 221–599, bornes exactes, zéro
  store consumer et 348 stores producer tous nuls.
- `demo-observed` : activité répétée des métadonnées et resets de file ;
  aucun payload de slot.
- `xenia-generic` : aucun élément.
- `unknown` : writer de `0x82386D90`, payload futur au-delà de 600 ticks,
  consumer frontbuffer, pixels, frontend, audio, mission et terminal.

Cette preuve complète l’A/B 253 ticks du cycle 1690; elle ne justifie pas une
progression START tant qu’aucune source non nulle n’est observée.

## Garde et suite

La garde doit distinguer `[0x82386D90,0x82386DF0)` des métadonnées
`[0x8238CD90,0x8238CDA0)` et trapper toute valeur non nulle inattendue ou
divergence. Le prochain corridor doit viser un autre producteur guest
qualifié, pas interpréter les métadonnées comme contenu visuel.
