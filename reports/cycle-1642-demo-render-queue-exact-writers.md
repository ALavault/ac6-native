# Cycle 1642 — writers exacts de la file render guest

## Résultat

Le probe codegen-ON neutral/START frais jusqu’au tick 600 a été rejoué avec
`AC6_DEMO_WATCH_RENDER_QUEUE_WRITERS=1`, hook désactivé par défaut. Il corrige
une ambiguïté du compteur agrégé du scheduler : ce compteur observe le ring
après les resets du worker et annonçait `consumer_changes=0`, alors que les
stores guest exacts montrent une consommation à chaque tick.

Le reçu durable est
[`ac6-demo-render-queue-write-provenance-v1.json`](../analysis/demo/ac6-demo-render-queue-write-provenance-v1.json).

| Adresse | valeur | occurrences/route | thread | PC/LR | ticks |
|---|---:|---:|---:|---|---|
| `0x8238CD90` producer | `1` | 348 | 1 | `0x820FF710` / `0x820FF734` | 252–599 |
| `0x8238CD94` consumer | `1` | 348 | 25 | `0x820FFCA0` / `0x820FFCE4` | 252–599 |
| `0x8238CD94` reset | `0` | 348 | 25 | `0x820FFCA0` / `0x820FFCE4` | 252–599 |
| `0x8238CD90` reset | `0` | 348 | 25 | `0x820FFCA0` / `0x820FFCE4` | 252–599 |
| `0x8238CD90/94` reset | `0` | 348/adresse | 1 | `0x820FF8D8` / `0x820FF91C` | 252–599 |

Le worker `0x820FFCA0` est donc un writer consommateur observé, sans que son
slot soit qualifié : les 696 snapshots de slots (producer/consumer) sont tous
nuls, SHA-256 `2ea9ab91…452d`. Aucun payload non nul ni rôle frontend ne doit
être inventé.

## A/B neutral/START

Les deux routes ont exactement 2 090 records de stores, 696 valeurs non nulles,
696 snapshots de slots et le même stderr SHA-256
`b8d5a2f7…b1dc7810`. Les rapports et RTPLY restent ceux du cycle 1641 : 600
ticks, 463 PRESENT, aucun jalon frontend/mission/terminal et aucun readback
promu.

## Qualification et limite

- `demo-qualified` : adresses, valeurs, PC, LR, thread et ticks des stores
  producer/consumer ; égalité neutral/START ; slots zéro ;
- `demo-observed` : le compteur scheduler échantillonné masque les resets ;
  le hook exact expose la consommation ;
- `unknown` : payload de slot, owner frontend et progression mission.

Aucune mutation de comportement runtime n’a été faite : le hook est opt-in,
aucune preuve retail n’est fusionnée, et aucun actif propriétaire, microcode ou
shader n’est suivi.

## Prochain checkpoint

Qualifier le premier contenu non nul écrit dans les slots autour de
`0x82386D90/0x82386DD0`, puis joindre ce contenu au consumer `0x820FFCA0` et au
jalon frontend. Le compteur agrégé du scheduler ne doit plus être utilisé seul
pour conclure à l’absence de consommation.
