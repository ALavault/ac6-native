# Cycle 1690 — stores de slots de file, A/B frais

## Verdict

Deux probes `probe` codegen-ON, process/store frais, neutral et START, ont
atteint 253 ticks avec `AC6_DEMO_WATCH_RENDER_QUEUE_SLOTS=1`. Le stderr est
byte-identique A/B (`d8e6d3df…179e21`) et contient 11 stores bornés par route.

La slot consumer exacte `0x82386D90` ne reçoit aucun store direct. La slot
producer `0x82386DD0` reçoit un seul store de 4 octets à zéro au tick 252,
thread 1, par `0x820FF710`. Les trois changements non nuls concernent
uniquement les métadonnées `0x8238CD90/94/9C`, hors payload de slots. Cette
preuve ferme l’absence de payload non nul dans cette fenêtre et ce build,
mais ne ferme ni la sémantique de la file ni la mission.

## Identité

| élément | valeur |
|---|---|
| cible | `Default.xex` PAL démo, SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| architecture | Xenon big-endian / Xenos |
| binaire codegen-ON | `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp`, SHA-256 `ed44dba583e2aa78d081ca52796ddc0e52b7b775015db41ca1198814c95b2ecc` |
| store | copie fraîche de `.build/ac6-demo-store-test-3` sous `$TMPDIR`, distincte par route |
| instrumentation | `AC6_DEMO_WATCH_RENDER_QUEUE_SLOTS=1`, désactivée par défaut |
| résultat | `max_ticks`, 253 ticks, rc 4 attendu par `probe` |

Le hook lit les adresses et valeurs des stores PPC et n’ajoute aucun effet
guest. Aucun asset retail, Xenia/ReXGlue, microcode ou shader n’est suivi.

## A/B et hashes

| route | RTPLY | rapport | stderr | rows |
|---|---|---|---|---:|
| neutral | `1d41d2e26003a631f8bec19534812a258ffc5d90466be885f6f7f3df797ebef7` | `420fb9341ea0a56be33f0143f9bf9be768b08a1697fe7cb314e5e055bbf2a843` | `d8e6d3df2c8e19373f2aa12ba7c99892c6faceb5c4171a4c11560458c7179e21` | 11 |
| START (`--input-at 252,16,0,0,0,0,0,0,1`) | `31553733582cf7345375d8f197a28645621e488700f2a31b7e883b66109360b2` | `8506399dca66624afcf52a8746bc4cd08c669f30021d9fb954bb8bebb01f7327` | `d8e6d3df2c8e19373f2aa12ba7c99892c6faceb5c4171a4c11560458c7179e21` | 11 |

## Stores observés

| plage/rôle | observations par route |
|---|---|
| `0x82386D90` consumer slot | 0 store direct |
| `0x82386DD0` producer slot | 1 store, taille 4, valeur zéro, tick 252, thread 1, fonction `0x820FF710`, LR `0x820FF734` |
| `0x8238CD90` metadata producer | 4 stores; zéro au tick 221 et reset au tick 252, plus valeur non nulle au tick 252 |
| `0x8238CD94` metadata consumer | 4 stores; zéro au tick 221 et reset au tick 252, plus valeur non nulle puis reset au tick 252 |
| `0x8238CD98/9C` metadata | 1 store chacun au tick 221; seul `0x8238CD9C` est non nul |

Les lignes A/B sont identiques octet pour octet; les valeurs non nulles des
métadonnées ne sont pas promues comme payload de slot.

## Qualification

- `demo-qualified` : absence de store direct à `0x82386D90`, store producer
  nul exact, A/B byte-identique, identité PAL et bornes de hook.
- `demo-observed` : changements de métadonnées et reset du consumer au tick
  252; aucun contenu payload non nul.
- `xenia-generic` : aucun élément.
- `unknown` : writer ultérieur hors 253 ticks, sémantique des slots,
  consumer frontbuffer, pixels, frontend, audio, mission et terminal.

## Garde et prochain checkpoint

Conserver la garde sur `[0x82386D90,0x82386DF0)` et distinguer strictement
les métadonnées `[0x8238CD90,0x8238CDA0)`. Toute future valeur non nulle dans
la slot, adresse hors plage ou divergence neutral/START doit arrêter le
corridor. Le prochain test doit prolonger une seule route neutral bornée au
premier changement de slot, puis refaire START seulement si cette source est
identifiée; aucune progression synthétique ni fallback visuel n’est admis.
