# Cycle 1651 — payload borné des consommateurs de handles PAL

## Résultat

Un hook opt-in, strictement read-only, capture pour chaque lecture de handle
guest les huit mots big-endian autour de l’adresse alignée sur 32 octets. Le
garde-fou vérifie l’absence de débordement avant de calculer chaque adresse et
ignore un mot non mappé. Le C++ généré, Ghidra, Xenia/ReXGlue et les
microcodes restent inchangés.

Le reçu durable est
[`ac6-demo-event-handle-payload-probe-v1.json`](../analysis/demo/ac6-demo-event-handle-payload-probe-v1.json).

## A/B frais

Deux processus codegen-ON ont été lancés depuis `.build/ac6-demo-store-test-3`,
neutral puis START `0x0010` au tick 252, jusqu’au tick 300 :

| mesure | neutral | START |
|---|---:|---:|
| ticks terminés | 300 | 300 |
| lectures consumer | 3 927 | 3 927 |
| lignes payload | 3 927 | 3 927 |
| groupes `(adresse,valeur,site,snapshot)` | 1 057 | 1 057 |
| snapshots `(base,mask,8 mots)` distincts | 530 | 530 |
| PRESENT | 163 | 163 |
| SHA trace | `2e49ae67…d9c115` | `179db68a…235345b` |
| SHA rapport | `c931a747…7cd9df1` | `51a2c2ce…e95161` |
| SHA stderr | `6c6e278e…545dc098` | `6c6e278e…545dc098` |
| frontend / mission / terminal | non / non / non | non / non / non |

Les lignes sont donc byte-identiques sur stderr et aucune transition START ne
peut être promue. Le JSON scelle aussi les SHA stdout et les sources du hook.

## Sites et payload observés

Le mapper `ac6-demo-generated-guest-load-map/v1` joint chaque ligne à un PC
guest et aux quatre bytes du basefile PAL `b98a9ac1…4218`. Les points utiles
sont :

| PC guest | source générée | lignes | snapshots | plage/valeurs |
|---|---|---:|---:|---|
| `0x822EEE30/38/44` | `sub_822EEE10`, lignes 4674/4679/4686 | 456 chacun | 223 | `0x82934740` ou `0x82933F80`, `E0000048/E000004C/E0000054/E0000058` |
| `0x822E409C/0x822E40B8` | `sub_822E4080`, lignes 14379/14395 | 153 chacun | 4 | `0x82934740`, mot 7 = `0`, `1`, `0xDC` au tick 222, `0x1D` au tick 252 |
| `0x822E40C8` | `sub_822E4080`, ligne 14404 | 101 | 1 | `0x8293474C`, `E000004C` |

Un état récurrent autour de `0x82934740` est :

```text
00000001 00000000 E0000048 E000004C 00000001 00000000 00000000 00000000
```

Autour de `0x82933F80`, l’état observé est :

```text
00000000 7FFFFFFF 00000005 822E33E0 82933F00 82028FA0 E0000054 E0000058
```

Ces valeurs sont des octets guest observés, pas des noms de structure ni une
sémantique importée. Les changements aux ticks 222 et 252 sont qualifiés comme
variations de payload, sans attribution de rôle.

## Qualification et limites

- `demo-qualified` : identité PAL, bornage des huit mots, PC exact dérivé de la
  source générée et vérifié sur le basefile, égalité A/B des comptes et flux.
- `demo-observed` : valeurs des mots, handles `E000…/E100…`, threads et ticks.
- `unknown` : type des objets, signification des mots, writer causal et
  persistance après consommation, transition frontend et pixels.
- `xenia-generic` : aucune preuve Xenia n’est utilisée dans ce cycle.

Le payload est joint au reçu writer du cycle 1648 uniquement par les sites et
les plages guest ; aucune arête writer→consumer n’est encore prouvée. Le
readback reste non promu et aucune screencap n’est produite.

## Validation

- rebuild `.build/ac6-demo-codegen-build-1` réussi ; binaire
  `da685f423e373eb75a80bf7a269a4b8682bf9d3e533dc2174e94ec680256d741` ;
- source `guest_bridge.cpp` : 1 199 lignes, hook désactivé par défaut ;
- JSON validé par `python3 -m json.tool` ; capsule SHA
  `7371e170f3a913c8a1b93242e283105dc7a989e1bde6b22445b3652156df6e1b`.

## Prochain checkpoint

Tracer une seule plage payload (`0x82934740` puis `0x82933F80`) avec un
watchpoint guest borné, PC/LR/thread/tick et première/dernière écriture, puis
rejouer l’A/B. Tant que le writer et la persistance ne sont pas joints,
l’état reste fail-closed et START ne déclenche aucune promotion visuelle.
