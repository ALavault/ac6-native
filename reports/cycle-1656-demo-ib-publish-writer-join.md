# Cycle 1656 — producteur guest et publication de l’IB principal

## Résultat

Un A/B frais neutral/START, stores neufs, codegen ON et borné à 300 ticks, a
réactivé le hook opt-in `AC6_DEMO_WATCH_IB_WRITERS`. Les deux flux contiennent
4 958 lignes d’écriture, dont 2 858 lignes guest au tick 0 et les trois stores
de publication du ring. Les lignes normalisées sont byte-identiques
(`b96745e4…a39e3`) et les stderr combinés ont le même SHA
`3a2c7088…ed765`.

## Preuves de frontière

L’IB principal est `[0x1274A000,0x1274CF54)`, 3 029 dwords, SHA
`d121c8d8cf55bcb755fa558c4d54a9311f4520fa2e8bb5e34b25920f107358d6`. Le premier
store observé est dans `0x821BAAD0` (PC `0x821BAE5C`, LR `0x821BAE2C`, thread 1,
tick 0). La borne finale est écrite par `0x821BA01C` (`0x821B9F70`, LR
`0x821B9F78`, thread 1, tick 0). Le reçu rr précédent conserve la qualification
du producteur final du premier dword (`0x821B0D70`, bytes `95 4B 00 04`) et du
dernier dword (`0x821BA01C`, bytes `94 CA 00 04`).

La publication ring est `0x126CA058 = {C0013F00, 0x1274A000, 0xBD5}`;
les trois stores sont `0x821B9D24/3C/44`, fonction `0x821B9BC8`, thread 1,
tick 0. Les indices log du run combiné sont : premier IB 5, publication 5 786.

## Ordre causal et limite

La première ligne `AC6_DEMO_WATCH_EVENT_POST_SET` de l’entrée `0x821A7160`
arrive à l’index 5 790 (`set_tick=resume_tick=1`). L’IB et sa publication
précèdent donc cette activation scheduler dans ce run. Cela ne prouve pas une
causalité activation → renderer ; la relation START → image reste inconnue.

Le hook guest couvre 2 858 starts de quatre octets. 62 starts manquent dans la
fenêtre contiguë `[0x1274A660,0x1274A760)`, couverte seulement par une ligne
`AC6_IB_HOST_WRITE` d’offset natif `0x26CE517`; cette observation n’est pas un
writer guest. Aucun mapping complet par dword n’est promu.

## Classification

- `demo-qualified` : égalité A/B, bornes/hash IB, producteur de début/fin,
  publication ring et ordre IB → publication → première activation observée.
- `demo-observed` : couverture guest partielle, familles de stores et plage
  host-write.
- `xenia-generic` : aucune preuve utilisée.
- `unknown` : valeurs/sémantique des stores, jointure PM4/Xenos, pixels,
  frontend, mission et légitimité de START.

## Validation et prochain checkpoint

Le hook reste désactivé par défaut, sans mutation d’état. Le prochain test doit
capturer une seule plage manquante via fenêtres exactes puis joindre une valeur
IB à son lecteur/consommateur PM4, sans déduire de sémantique depuis le nom
recompilé. C++ généré, Ghidra, Xenia/ReXGlue, microcodes et actifs propriétaires
restent inchangés et non suivis.

Capsule : `analysis/demo/ac6-demo-ib-publish-writer-join-v1.json`.
