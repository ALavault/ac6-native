# Cycle 1641 — thunk PAL dynamiquement appelable et frontier tick 600

## Résultat

Le replay neutral headless précédent s’arrêtait au tick 385 sur un appel
indirect non qualifié : LR `0x820DC4FC`, target `0x820D3310`. Le target n’est
pas un nom de fonction inventé : Ghidra le classe comme `branch-delay-slot`
dans le chunk PAL borné `0x820D3230..0x820D3363`.

La capture dynamique donne `r3=0x2E3E0454`, vtable `0x82006A9C`, slot
`0x82006A9C+0x64 = 0x82006B00`, puis target `0x8219DC18`. Les bytes PAL
exactes de la thunk sont :

```text
0x820D3310: 81 83 00 00  81 6C 00 64  7D 69 03 A6  4E 80 04 20
```

La routine native ne l’admet que si les 16 octets sont inchangés, si l’objet,
la vtable et le slot sont mappés, et si le target du slot existe dans la table
de fonctions qualifiée. Elle modélise uniquement `lwz r12,0(r3); lwz
r11,0x64(r12); mtctr r11; bctr`; toute autre cible conserve le trap
`unqualified guest indirect call`.

Le reçu durable est
[`ac6-demo-frontier-branch-delay-thunk-v1.json`](../analysis/demo/ac6-demo-frontier-branch-delay-thunk-v1.json).

La provenance PM4 `rr` réutilisée dans ce reçu reste celle du même XEX PAL :
IB principal `0x1274A000`, 3 029 dwords, SHA-256
`d121c8d8…358d6`; dernier writer du premier dword `0x821B0D70` (PC qualifié,
LR/thread/tick explicitement inconnus), dernier dword `0x821BA01C`
(`LR=0x821B9F78`, thread 1, tick 0), et publication ring
`0x821B9D24` (`LR=0x821B9C80`, thread 1, tick 0) pour
`C0013F00, 0x1274A000, 0xBD5`. L’outil reste exclusivement
`.tools/rr-install/bin/rr`, commit `7352eb807ed75e3b51be85fa6a27f121235dbfb0`;
aucune nouvelle attribution n’est déduite des noms générés.

## A/B neutral/START

Deux processus codegen-ON frais, backend headless, stores identiques et
`SDL_AUDIODRIVER=dummy` atteignent le tick 600 :

| Champ | Neutral | START tick 252 |
|---|---:|---:|
| RTPLY | `b7241124…c9aac9fb` | `4d2f7751…ba100e1` |
| rapport | `20da97c4…75cbe6` | `b48fb162…e43161` |
| retour | 4 (`max_ticks`) | 4 (`max_ticks`) |
| PRESENT | 463 | 463 |
| producer changes | 695 | 695 |
| consumer changes | 0 | 0 |
| frontend / mission / terminal | faux / faux / faux | faux / faux / faux |

Les sous-arbres `outcome`, `milestones`, `graphics` et `scheduler` sont
identiques. Le frontier final est un état runtime où les threads guest sont
bloqués avant le jalon frontend, pas un succès de mission. Aucun pixel non noir
ou screencap n’est promu.

## Qualification

- `demo-qualified` : bytes thunk, ABI de slot, mappings mémoire et invocation
  fail-closed ; neutral/START franchissent le trap tick 385 et terminent 600
  ticks sans fault CPU ;
- `demo-observed` : thread 1, tick 385, objet/vtable/slot/target exacts ;
- `unknown` : rôle sémantique de l’objet, transition frontend, pixels évolutifs,
  audio et mission.

## Validation

- CTest codegen OFF : 18/18 ; ON : 17/17 ;
- audit source et complexité : PASS ;
- cible et basefile PAL recoupés (`de917873…5da8`, `b98a9ac1…4218`) ;
- aucun checkout Xenia/ReXGlue, Ghidra, C++ généré, microcode ou actif
  propriétaire modifié/suivi.

Prochain checkpoint : exploiter le frontier runtime tick 600 pour identifier la
première écriture/consommation guest non nulle du chemin frontend, sans élargir
la table des thunks au-delà des bytes observés.
