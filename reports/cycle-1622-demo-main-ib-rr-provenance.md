# Cycle 1622 — provenance `rr` de l'IB principal démo

## Résultat

Le gate `rr` neutral/headless tick 253 est recoupé avec la trace qualifiée. Les
watchpoints matériels portent sur des fenêtres exactes de quatre octets et le
replay inverse n'a effectué aucune resynchronisation.

Deux événements distincts sont conservés pour `0x1274A000`. La première
écriture de la fenêtre part de zéro au tick 0 dans `sub_821BAAD0`, PC
`0x821BAE5C`, valeur `0x020D0000`. Le producteur final du dword capturé
`0x00000D02` est `sub_821B0D20`, PC PAL exact `0x821B0D70`, instruction
`stwu r10,4(r11)`, bytes `95 4B 00 04`. Le premier événement ne doit donc pas
être présenté comme le producteur final.

La dernière zone de l'IB, `0x1274CF50`, reçoit `0x00000005` dans
`sub_821B9F70`, PC `0x821BA01C`, instruction `stwu r6,4(r10)`, bytes
`94 CA 00 04`, LR littéral qualifié `0x821B9F78`.

Le packet ring qui publie l'IB est produit par `sub_821B9BC8`. Les trois stores
guest `0x821B9D24`, `0x821B9D3C`, `0x821B9D44`, LR `0x821B9C80`, écrivent à
`0x126CA058` exactement :

`C0013F00, 1274A000, 00000BD5`.

Le premier store est `stwx r9,r10,r24`, bytes PAL `7D 2A C1 2E`. Les trois
instructions sont recoupées dans `xex-basefile.bin` SHA-256
`b98a9ac1…4218`, qualifié pour le même XEX démo.

Cette séquence est identique à la seconde soumission du rapport PM4 qualifié.
L'IB couvre `0x1274A000..0x1274CF54`, 3 029 dwords, SHA-256
`d121c8d8cf55bcb755fa558c4d54a9311f4520fa2e8bb5e34b25920f107358d6`.

## Qualification et limites

- `demo-qualified` : PC/valeur/fonction du producteur final du premier dword,
  writer complet de la dernière fenêtre et stores de publication ring, avec
  les LR/thread/tick disponibles dans le reçu ;
- `unknown` : LR/thread/tick du producteur final du premier dword ne sont pas
  publiés par la trace bornée et restent explicitement nuls dans le reçu ;
- `unknown` : carte de writer de chacun des 3 029 dwords et rôle sémantique du
  producteur au-delà de la publication PM4 ;
- aucune preuve retail, aucun `rr` système, aucune mutation Xenia/ReXGlue,
  Ghidra, C++ généré ou microcode ;
- cette preuve ne couvre ni START ni Vulkan, qui exigent leur propre A/B.

Reçu : `analysis/demo/ac6-demo-main-ib-rr-provenance-v1.json`.

Validation : test Python ciblé 30/30, build incrémental, CTest 18/18 sous Xvfb
avec audio dummy, audits source et complexité : PASS.

## Prochain checkpoint

Instrumenter les stores guest qui chevauchent l'IB par fenêtres exactes afin de
construire la carte de dernière écriture, puis joindre ses plages aux packets
PM4. Ne promouvoir un nom de producteur qu'après recoupement des bytes PAL et
de ses callers dynamiques.
