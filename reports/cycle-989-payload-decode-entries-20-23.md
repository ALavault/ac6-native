# Cycle 989 — décodage des payloads campagne 20–23

## Contrat et provenance

Les entrées physiques `DATA.TBL` 20–23 sont reliées aux sélecteurs campagne
12–15 par la chaîne qualifiée `0x821B6E58` → `0x821D1128` → `0x821CD130` →
`0x821CC250`. La provenance reste le projet Ghidra canonique
`ace-combat-6`, cible `PAL-default-xex`, module `default.xex`:

```text
default.xex  acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde
DATA.TBL     82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5
DATA00.PAC   c3ed20ec6ef0260671d9cd5f3e088fab2a8d983cb6739efab350c87c6fb74816
DATA01.PAC   eddb6874184d49e36dd8b4e06f387e79be9c0792e97ea3405ab00dab76c03b4
```

Les archives retail sont lues uniquement sur les plages déclarées; aucun PAC
complet n’est copié, empaqueté ou téléversé.

## Résultats bornés

```text
mission  DATA.TBL  archive     offset       stored     expanded    stored SHA-256                                                        decoded SHA-256
12       20         DATA00.PAC  0x06BB8000   3479562   12653040     242eadb997ab84d51b96840108a0991c179970eeaf5f842195cf649ff552fc86  eaecae03f33ba2f6215fbbdff9c48c8967015644f81d8a234507d43ab698e199
13       21         DATA00.PAC  0x06F10000  13628378   48476816     3c1d94fa4ba0eb20d4a95f19f71fa8d77f44cf1d194333de6550f586a12e402f  16c35b83f5f4018c888c3ae80a51eae6dfcd69a49ab7e363c8ea4d5f054b74fa
14       22         DATA00.PAC  0x07C10000   3508627   13681136     7f6fd95f8f358c589169b88608a7d6583e777760237937c681634337e1aea14c  dc320871b064c65a620c08c855b8ce3cc08b3d63765e44172473ee2dc8e9fc64
15       23         DATA00.PAC  0x07F70000  17669780   55944256     f249557a4e0ebae28c756573be140775a075bca5b96abb87d7920ed500b7e855  1a733dd9db6a4ab82666b8452814583a12e53ec47b120f945f5b188e576667f9
```

Les quatre sorties commencent par `FHM `, portent 26 enfants top-level et ne
présentent aucun échec de parse. Les entrées 20 et 22 exposent 112 lignes
récursives et 9 FHM top-level; l’entrée 21 en expose 837 et 13, et l’entrée
23 en expose 1 930 et 17. Les types top-level reconnus sont `MDLP`, `PLAD`,
`FHM ` et `ACE6`.

## Comparaison hash 11–23

La marche récursive bornée couvre 5 555 nœuds et 4 472 hashes uniques. Elle
trouve 44 groupes partagés, dont 43 non vides et un hash de payload vide. Le
FHM de 480 octets (`0f83f27d…`), le bloc `ACE6` de 4 octets
(`b18a4e45…`), ainsi que plusieurs blocs de contrôle sont partagés par les
13 entrées. Les nouveaux liens de la famille 21/23 incluent des payloads
`CAPT` et des blocs binaires sans sémantique qualifiée. Les bytes restent
adressés par hash; aucune interprétation ni manifeste natif n’est promu.

## Validation

```sh
python3 tools/extract_ac6_pac.py game-files --indices 20 21 22 23 \
  --output /tmp/ac6-cycle-989-payloads --decompress
```

Résultat observé: `decoded=4`, `fhm=4`. Le catalogue conserve les extents,
hashes, structures et la lacune `payload_dependency_inventory` pour les
missions 12–15; les 15 routes physiques sont désormais reliées à un payload
décodé, mais aucune mission supplémentaire n’est promue `qualified`.
