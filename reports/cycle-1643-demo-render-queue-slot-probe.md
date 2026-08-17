# Cycle 1643 — probe direct des payloads de slots render

## Résultat

Deux exécutions fraîches codegen-ON, neutral et START, ont atteint le tick
600 avec le probe opt-in des stores de slots. Les deux routes ont produit
2 440 records de slots et le même stderr SHA-256
`fbaa371d…64625ed3f`.

| Zone | observation | preuve |
|---|---|---|
| `0x82386DD0` (slot producer) | 348 stores/tick 252–599, toujours `0`, taille 4 | `0x820FF710`, thread 1, LR `0x820FF734` |
| `0x82386D90` (slot consumer) | aucun store direct; 696 snapshots tous nuls | SHA `2ea9ab91…452d` |
| `0x8238CD90/94/9C` | 697 valeurs non nulles, indices/métadonnées de queue | exclues du payload |

Le hook est désactivé par défaut et ne change pas le comportement. Le
consumer `0x820FFCA0` continue donc à consommer/réinitialiser ses indices,
mais aucun payload de slot non nul n’est prouvé dans cette fenêtre.

## A/B et rendu

Neutral et START restent byte-identiques sur les stores de queue et sur les
sous-arbres de rapport `outcome`, `milestones`, `graphics` et `scheduler` :
600 ticks, 463 PRESENT, aucun jalon frontend/mission/terminal. Le readback
qualifié reste uniformément noir (`0c660f2b…a4913a5f` dans le test Vulkan
générique); aucune screencap guest-owned n’est donc produite.

## Qualification

- `demo-qualified` : absence de store direct non nul dans les adresses de
  payload observées, égalité neutral/START, identité PAL démo.
- `demo-observed` : les valeurs non nulles de `0x8238CD90/94/9C` sont des
  métadonnées de queue et non des payloads.
- `unknown` : champs sémantiques de `0x820FFCA0/0x820FEFA8` et toute écriture
  de slot ultérieure au tick 600.

Le reçu durable est
[`ac6-demo-render-queue-slot-write-probe-v1.json`](../analysis/demo/ac6-demo-render-queue-slot-write-probe-v1.json).

## Prochain checkpoint

Capturer les lectures et l’appel `0x820FEFA8` du consumer, puis prolonger une
seule fenêtre neutre bornée pour localiser la première source de données; ne
promouvoir ni START, ni readback, ni screencap tant que le payload et la
transition visuelle ne sont pas endogènes et qualifiés.
