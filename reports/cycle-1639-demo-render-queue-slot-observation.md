# Cycle 1639 — observation ciblée des slots de file démo

## Résultat

Un hook natif optionnel (`AC6_DEMO_WATCH_RENDER_QUEUE_WRITERS`) a été exécuté
sur un run direct neuf de la démo PAL avec START au tick 252 et 300 ticks. Il
ne modifie aucune mémoire guest et n’est pas actif par défaut. Il confirme les
stores générés suivants, en recoupement avec les bytes du basefile PAL :

| champ | observation `demo-observed` |
|---|---|
| producteur | `0x8238CD90`, `sub_820FF710`, ligne générée 42781, PC statique `0x820FF75C`, thread 1, ticks 252–299 (48) |
| consommateur normal | `0x8238CD94`, `sub_820FFCA0`, ligne générée 43739, PC statique `0x820FFD78`, thread 25, ticks 252–299 (48) |
| reset producteur | `0x8238CD90`, `sub_820FFCA0`, ligne générée 43756, PC statique `0x820FFD94`, thread 25, ticks 252–299 (48) |
| reset consommateur | `0x8238CD94`, `sub_820FFCA0`, ligne générée 43758, PC statique `0x820FFD98`, thread 25, ticks 252–299 (48) |

Les deux écritures d’index à 1 donnent 96 snapshots de 96 octets : le slot
producteur candidat `0x82386DD0` et le slot consommateur candidat `0x82386D90`
sont tous deux entièrement nuls, hash SHA-256
`2ea9ab9198d1638007400cd2c3bef1cc745b864b76011a0e1bc52180ac6452d4`.
Les écritures de reset à zéro n’ont pas de slot précédent valide.

Le champ `PPCContext.lr` observé au moment du hook vaut `0x820FF734` pour le
producteur et `0x820FFCE4` pour le worker. Ce sont des marqueurs de contexte
recompilé, pas des LR guest de l’instruction store; aucun LR guest n’est donc
promu.

## Identité et artefacts

- cible : `Default.xex`, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` ;
- basefile PAL : SHA-256
  `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` ;
- binaire codegen utilisé par le hook : SHA-256
  `61304c3000fda1b7170b111d439a2d002f360b58e9406adfce3d8d73c2140613` ;
- trace RTPLY directe : SHA-256
  `179db68a5d78cccc2b53634bc9f5f54636674ecbf9e529f82a4c9d217235345b` ;
- rapport direct : SHA-256
  `c74ca0ea7733fe49f0ee5440c73e414737f7fa3cf8e83dc39b95012e30112593` ;
- stderr du hook :
  `/fastdata/lavaulta/tmp/ac6-demo-queue-slot-probe.b4JWwf/stderr`, SHA-256
  `9b6507d1d4d9d2ad59638b1e89fd593496010c1a1cb363398491be6b7981eca8` ;
- capsule jointe :
  [`analysis/demo/ac6-demo-start-queue-rr-provenance-v1.json`](../analysis/demo/ac6-demo-start-queue-rr-provenance-v1.json).

## Portée et garde

Cette observation native complète le reçu `rr` du cycle 1638; elle ne le
remplace pas et ne change pas son A/B direct/`rr` byte-identique. Les stores
restent `demo-qualified` par le join `rr` callsite/basefile, tandis que les
contenus de slots sont seulement `demo-observed` et leur sémantique reste
inconnue. Le hook est désactivé sans la variable d’environnement et aucun
fallback visuel ou mission n’est activé.

La garde ciblée
`test_start_queue_rr_provenance_keeps_instruction_join_fail_closed` vérifie
désormais les quatre PC statiques, les 290 observations, les 96 snapshots
nuls, l’absence de promotion du LR et les limites fail-closed.

## Prochain checkpoint

Rejouer un A/B direct/`rr` avec le même binaire et, si nécessaire, instrumenter
le constructeur de l’objet de file pour obtenir le pointeur `r31` au moment de
la copie. Tant que le slot non nul, son producteur de commande et son LR guest
réel ne sont pas qualifiés, aucune transition frontend ou renderer n’est
promue.

Politique : aucune preuve retail, aucun rr système, aucune modification de
Xenia/ReXGlue/Ghidra/C++ généré/microcode et aucun actif propriétaire suivi.
