# Cycle 1652 — jointure writer→consumer du payload PAL

## Résultat

Le hook read-only des stores surveille exclusivement les deux fenêtres déjà
observées par le consumer : `[0x82934740,0x82934760)` et
`[0x82933F80,0x82933FA0)`. Les intersections sont calculées en 64 bits avant
toute adresse dérivée ; le hook est désactivé par défaut et borné à 16 384
records par type de store.

Le reçu durable est
[`ac6-demo-event-handle-payload-writer-probe-v1.json`](../analysis/demo/ac6-demo-event-handle-payload-writer-probe-v1.json).

## A/B frais

Neutral et START `0x0010` au tick 252 ont été rejoués depuis le même store,
jusqu’au tick 300, avec les hooks consumer, payload et payload-writer actifs :

| mesure | neutral | START |
|---|---:|---:|
| ticks terminés | 300 | 300 |
| lectures consumer / payload | 3 927 / 3 927 | 3 927 / 3 927 |
| stores dans les fenêtres | 374 | 374 |
| stores mappés | 374 | 374 |
| sites guest distincts | 19 | 19 |
| paires writer→lecture postérieure même tick/thread | 351 | 351 |
| discordances valeur→mot 7 | 0 | 0 |
| PRESENT | 163 | 163 |
| SHA stderr | `dd9d73a4…194986` | `dd9d73a4…194986` |
| frontend / mission / terminal | non / non / non | non / non / non |

## Arête causale qualifiée

Dans `sub_822EEE10`, le contrôle de flux et les bytes PAL donnent la séquence
exacte suivante :

| PC guest | bytes PAL | instruction | ligne générée | effet observé |
|---|---|---|---:|---|
| `0x822EEE30` | `80 7F 00 00` | `lwz r3,0(r31)` | 4674 | lecture `E0000048` à `0x82934748` |
| `0x822EEE38` | `80 7F 00 00` | `lwz r3,0(r31)` | 4679 | seconde lecture `E0000048` |
| `0x822EEE3C` | `FB DF 00 10` | `std r30,16(r31)` | 4681 | store 8 octets à `0x82934758` |
| `0x822EEE44` | `80 7F 00 04` | `lwz r3,4(r31)` | 4686 | lecture `E000004C` à `0x8293474C` |

Le writer `0x822EEE3C` est observé 351 fois (thread 12 aux ticks 1–299,
avec les occurrences du thread 1). Pour chacune des 351 paires, la lecture
`0x8293474C` postérieure au même tick et au même thread expose un mot 7 égal
aux 32 bits bas de la valeur écrite. Les valeurs distinctives du corridor sont
`0xDC` au tick 222 et `0x1D` au tick 252 ; elles sont retrouvées dans le
snapshot postérieur. C’est une jointure guest writer→consumer du payload, pas
une interprétation de structure.

Les 23 autres stores concernent l’initialisation ou d’autres champs des deux
fenêtres. Aucun store U8, U16 ou U128 n’est atteint dans ce run ; les chemins
restent instrumentés et bornés.

## Qualification et limites

- `demo-qualified` : 374/374 stores mappés aux bytes du basefile PAL, séquence
  exacte, 351 paires sans discordance, A/B identique.
- `demo-observed` : valeurs de payload, threads, ticks et plages guest.
- `unknown` : type sémantique de l’objet, consumer ultérieur, relation avec le
  renderer, transition frontend et pixels.
- `xenia-generic` : aucune preuve Xenia utilisée dans ce cycle.

Le résultat ne justifie toujours pas une screencap ni la promotion de START.
Le readback reste fail-closed et aucun actif retail/propriétaire n’a été suivi.

## Validation

- rebuild codegen-ON réussi ; binaire
  `a587917a47c2b62692531ca9cb505d3a2eaf1a2ed46302354d2a131f785b01a6` ;
- mapper `map_generated_guest_payload_writers.py` : 374/374 rows, 19 sites,
  basefile SHA `b98a9ac1…4218` ;
- capsule SHA : `48e9133065efe0bfbb4a90301d4aa09eded09a4480944cfd6fb0e6e769405918`.

## Prochain checkpoint

Suivre le consumer postérieur à `0x822EEE44` dans la même chaîne, puis joindre
sa première consommation à un état Xenos/PM4 ou arrêter au premier champ
inconnu. Aucun fallback visuel ne sera activé.
