# Cycle 1650 — PC guest exact des consumers de handles PAL

## Résultat

Le hook consumer du cycle 1649 a été étendu au niveau du macro XenonRecomp :
chaque `PPC_LOAD_U32` observé porte maintenant le nom de fonction générée et
la ligne source. Un setter opt-in (`AC6_PPC_SET_LOAD_SITE`) transmet ces deux
champs sans modifier le C++ généré ni la sémantique guest.

Deux runs codegen-ON frais, depuis le même store démo PAL, donnent :

| mesure | neutral | START |
|---|---:|---:|
| ticks terminés | 300 | 300 |
| PRESENT | 163 | 163 |
| lectures de handles | 3 927 | 3 927 |
| lignes mappées | 3 927 | 3 927 |
| sites guest distincts | 64 | 64 |
| SHA stderr | `7cd4324e…b9b8432c` | `7cd4324e…b9b8432c` |
| frontend / mission / terminal | non / non / non | non / non / non |

Le reçu durable est
[`ac6-demo-event-handle-consumer-pc-probe-v1.json`](../analysis/demo/ac6-demo-event-handle-consumer-pc-probe-v1.json).

## Méthode de qualification

Le mapper
[`map_generated_guest_load_sites.py`](../recompilation/ace-combat-6-demo/tools/map_generated_guest_load_sites.py)
refuse un log mal formé, une fonction absente, une ligne qui n’est pas
`PPC_LOAD_U32`, un commentaire d’instruction manquant ou une adresse hors du
basefile PAL. Il compte les commentaires d’instructions du corps généré,
calcule le PC guest depuis l’entrée de fonction, puis vérifie les quatre bytes
PAL à ce PC. Les deux routes mappent `3 927/3 927` lignes et produisent le même
ensemble de 64 sites.

Le `context.lr` reste un marqueur d’exécution et n’est pas promu comme PC. Le
PC ci-dessous vient exclusivement de la ligne `PPC_LOAD_U32`, du contrôle de
flux généré et des bytes du basefile PAL `b98a9ac1…4218` :

| PC guest | instruction PAL | fonction | ligne générée | valeur(s) observée(s) |
|---|---|---|---:|---|
| `0x821A6364` | `80 61 00 50` — `lwz r3,80(r1)` | `sub_821A62F0` | 10515 | table `E000…` |
| `0x821A8D14` | `80 61 00 50` — `lwz r3,80(r1)` | `sub_821A8CB8` | 16986 | table `E100…` |
| `0x822EEE30` | `80 7F 00 00` — `lwz r3,0(r31)` | `sub_822EEE10` | 4674 | `E0000048/E0000054` |
| `0x822EEE38` | `80 7F 00 00` — `lwz r3,0(r31)` | `sub_822EEE10` | 4679 | `E0000048/E0000054` |
| `0x822EEE44` | `80 7F 00 04` — `lwz r3,4(r31)` | `sub_822EEE10` | 4686 | `E000004C/E0000058` |
| `0x822EEE88` | `80 9F 00 04` — `lwz r4,4(r31)` | `sub_822EEE68` | 4729 | `E0000058` |
| `0x822EEE98` | `80 7F 00 00` — `lwz r3,0(r31)` | `sub_822EEE68` | 4739 | `E0000054` |
| `0x822EEEA4` | `80 7F 00 00` — `lwz r3,0(r31)` | `sub_822EEE68` | 4746 | `E0000054` |
| `0x822E409C` | `80 7F 00 00` — `lwz r3,0(r31)` | `sub_822E4080` | 14379 | `E0000048` |
| `0x822E40B8` | `80 7F 00 00` — `lwz r3,0(r31)` | `sub_822E4080` | 14395 | `E0000048` |
| `0x822E40C8` | `80 9F 00 04` — `lwz r4,4(r31)` | `sub_822E4080` | 14404 | `E000004C` |

Le tableau complet des 64 sites, les adresses guest lues, les ticks, threads,
LR observés et hashes des sources générées sont scellés dans le JSON. Les
couples `0x822EEE30/38/44` et `0x822E40B8/0x822E40C8` sont maintenant des
points de trace guest exacts pour la jointure writer→payload.

## Qualification et limites

- `demo-qualified` : PC guest de l’instruction de load, bytes PAL, fonction,
  ligne générée, adresse/valeur/tick/thread, et A/B identique.
- `demo-observed` : `context.lr` et les handles `E000…/E100…`.
- `xenia-generic` : uniquement l’interprétation des handles `F800…` et de la
  pile POSIX de l’archive Xenia.
- `unknown` : type exact des objets PAL, payload persistant après le load,
  relation causale avec les 25 writers, transition frontend et pixels.

Le readback reste noir et aucun frontend guest-owned n’est atteint ; aucune
screencap n’est produite ou promue.

## Prochain checkpoint

Poser un watchpoint exact sur le payload lu autour de `0x822EEE30/38/44` et
`0x822E40B8/0x822E40C8`, puis joindre sa plage au writer cycle 1648 dans un A/B
neutral/START borné. Tout champ non résolu doit interrompre le corridor avant
effet ; aucune attribution Xenia ou retail ne sera fusionnée.
