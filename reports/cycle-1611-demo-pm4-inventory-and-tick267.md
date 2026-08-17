# Cycle 1611 — inventaire PM4 démo et frontier tick 267

## Identité et sources

Preuves exclusivement issues de `Default.xex` démo PAL, SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`,
Xenon big-endian / Xenos. Le rapport runtime source est
`/fastdata/lavaulta/tmp/ac6-demo-task-dispatch-t253.WG1oKJ/report.json`.
L'inventaire durable est
`analysis/demo/ac6-demo-pm4-inventory-v1.json`, SHA-256
`20a2f8a4e036e4668905db8423eabb1f2d8c5c65daf97d22736456c70441a00e`.
Aucune preuve retail n'est fusionnée.

## Inventaire déterministe

La capture qualifie la ring à `0x126CA000`, et non `0x126C0000`, avec une
capacité de 131072 dwords, RPTR 7, WPTR 25 et deux soumissions. Les deux
packets ring `INDIRECT_BUFFER` référencent :

- `0x127CA0C0`, 11 dwords, SHA-256
  `ef7ab6e4832aed218b50126464de899ccf0f4bf2eaf26ecfac6371c51671d2b0` ;
- `0x1274A000`, 3029 dwords, SHA-256
  `d121c8d8cf55bcb755fa558c4d54a9311f4520fa2e8bb5e34b25920f107358d6`.

L'IB principal est consommé exactement, sans resynchronisation : 871 packets,
soit 338 type 0, 252 type 2 et 281 type 3. Les opcodes type 3 atteints sont
`REG_RMW` (2), `IM_LOAD_IMMEDIATE` (3), `DRAW_INDX_2` (2),
`INVALIDATE_STATE` (3), `WAIT_REG_MEM` (8), `COND_WRITE` (256),
`EVENT_WRITE` (1), `INTERRUPT` (1), `EVENT_WRITE_SHD` (2),
`SET_BIN_MASK_LO` (2) et `XE_SWAP` (1). Le petit IB ajoute un
`WAIT_REG_MEM`; la ring ajoute `ME_INIT` et deux `INDIRECT_BUFFER`.

Tous les headers et opcodes sont structurellement bornés. Le premier état
sémantique inconnu est le registre type 0 `0x0A02`, dans le packet principal
d'offset dword 2, header `0x00030A02`, count 4. L'exécution sémantique doit donc
s'arrêter avant effet à cet endroit. `0xC0100000` n'est pas observé comme
header de packet dans cet inventaire ; il ne peut pas être qualifié ainsi.

La table Xenia épinglée permet seulement de repérer structurellement les
groupes de registres candidats : RT/depth/scissor à l'offset 42, viewport à
59, constantes/copy à 103, fetch aux offsets 219/376/408, draws à partir de
239, copy/resolve candidats aux offsets 326/404/406, et `XE_SWAP` à 415.
Ces noms génériques ne prouvent pas encore leurs effets dans AC6. Les trois
`IM_LOAD_IMMEDIATE` ne sont publiés que par adresse, taille et hash ; aucun
microcode propriétaire n'est copié.

## Limite et prochain test PM4

Le rapport ne contient aucun PC de store permettant d'attribuer le producteur
guest de l'IB principal. La seule preuve est la référence ring et l'écriture
intermédiaire de la valeur `0x1274A000`. Le prochain test minimal journalisera
PC, LR, thread et tick de chaque store chevauchant
`[0x1274A000, 0x1274CF54)`, ainsi que le writer de la référence ring, sans
modifier les effets guest.

## Corridor scheduler

Après qualification de l'entrée `0x822F2EC0`, un replay neuf avec START au
tick 252 atteint au tick 267 l'appel indirect
`LR=0x82323C88 -> 0x82323618`. Les 24 bytes démo sont contenus dans le chunk
Ghidra canonique `0x823235D0..0x82323647`, précédés et terminés par des `blr`,
et portent le SHA-256
`2ded41d13e8af14e2e1c80e65a7d5de6890863f2b763cff0a30301967d6cd6da`.
Cette entrée callable a été ajoutée aux frontières confirmées ; aucun
comportement guest n'a été remplacé.

Le codegen strict régénéré contient 12 867 fonctions, 145 records configurés
et 52 unités C++, avec zéro diagnostic de frontière et zéro instruction non
supportée. Un replay START depuis un store neuf ne piège plus sur cette entrée :
il atteint 269 ticks et 132 notifications PRESENT. Son frontier est alors un
état scheduler : 23 threads bloqués, 33 changements du producer de la file
render et aucun changement du consumer.

## Garde fail-closed

`XenosCommandProcessor` refuse désormais explicitement les quatre registres
atteints mais absents de l'autorité épinglée, `0x0A02..0x0A05`, avant toute
mutation de l'état transactionnel. Le test unitaire utilise le header et les
quatre valeurs exactes de la capture, dont `0xC0100000` comme payload et non
comme header. Il vérifie aussi qu'une écriture antérieure du même batch n'est
pas validée. Le test ciblé passe et le replay démo complet piège bien par
`unqualified Xenos register write` sur ce flux réel.
