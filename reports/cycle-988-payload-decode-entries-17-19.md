# Cycle 988 — décodage des payloads campagne 17–19

## Contrat et provenance

Les entrées physiques `DATA.TBL` 17–19 sont reliées aux sélecteurs campagne
9–11 par la chaîne qualifiée `0x821B6E58` → `0x821D1128` → `0x821CD130` →
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
9        17         DATA00.PAC  0x04F70000  12644179   36000976     31bc46ec084218aad163694f0de696462a3061ded0229f840245fef0d901e2a8  f7448daf41808679e647b1477c7ae7100229dccb6b1aabd2274d8fa6d7be6980
10       18         DATA00.PAC  0x05B80000   8735481   28144112     840c4833b5713de0f46dd2ec83396d74fdf870d6cd4decf2cea0a96bc6aa2b8  27251d6f29155cb9d1004e96fbbb825870eadb3fd90f2da63147e1dc2bc27fc7
11       19         DATA00.PAC  0x063D8000   8228662   29290992     ad71051ef48a5a60166d38ac1b1d6ea53b91e70895ee66fd0963d38b032b1ca3  36e8c0c27cc4a99a84186953f2a210529d8d60a59ea867d0c6bb9063627845e2
```

Les trois sorties commencent par `FHM `, portent 26 enfants top-level et ne
présentent aucun échec de parse. L’entrée 17 contient 1 052 lignes
récursives et 13 FHM imbriqués; les entrées 18 et 19 en contiennent 112 et 9.
Les types top-level reconnus sont `MDLP`, `PLAD`, `FHM ` et `ACE6`.

## Comparaison hash 11–19

La marche récursive bornée couvre 2 564 nœuds et 1 961 hashes uniques. Elle
trouve 27 groupes partagés, dont 26 non vides et un hash de payload vide. Le
FHM de 480 octets (`0f83f27d…`) et le bloc `ACE6` de 4 octets
(`b18a4e45…`) sont partagés par les neuf entrées. Les nouveaux liens
remarquables comprennent un `NTXR` de 69 632 octets et un `NFH` de 3 408
octets partagés entre les entrées 14 et 17. Les bytes restent adressés par
hash; aucune sémantique n’est déduite et aucun manifeste natif n’est promu.

## Validation

```sh
python3 tools/extract_ac6_pac.py game-files --indices 17 18 19 \
  --output /tmp/ac6-cycle-988-payloads --decompress
```

Résultat attendu et observé: `decoded=3`, `fhm=3`. Le catalogue conserve les
extents, hashes, structures et lacune `payload_dependency_inventory` pour les
missions 9–11; les missions 12–15 restent `not_attempted`.
