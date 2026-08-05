# Cycle 986 — décodage des payloads campagne 11–13

## Contrat et provenance

Le décodage est borné aux plages déclarées par `DATA.TBL`; aucun PAC complet
n’est copié ni empaqueté. Corpus PAL:

```text
default.xex  acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde
DATA.TBL     82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5
DATA00.PAC   c3ed20ec6ef0260671d9cd5f3e088fab2a8d983cb6739efab350c87c6fb74816
DATA01.PAC   eddb687418d4b49e36dd8b4e06f387e79be9c0792e97ea3405ab00dab76c03b4
```

Preuve runtime associée : projet Ghidra `ace-combat-6`, cible
`PAL-default-xex`, module `default.xex`; les routes physiques sont celles
qualifiées par `0x821D1128`/`0x821CD130`/`0x821CC250`.

## Résultats

```text
mission  DATA.TBL  archive     offset       stored     expanded    stored SHA-256                                                        decoded SHA-256
3        11         DATA00.PAC  0x01F88000   5320525    18162160     8d709dbf46035870b770da114d254cae43f7f1257b429cc4d1d073f1f1ddb8f6  9ad51f392aa291aa9975cfc97db0c1133e862f611752af3020c1e0cda34b321e
4        12         DATA00.PAC  0x024A0000   6353381    21238256     af2fc342f1b141ad237a54e78cac70eddc957950706351f5336b480a6b2b8823  51de2c162e2b3fbfe0af0cd3fa39dbe78be3f571e37b45c5b4d02cb57d75c291
5        13         DATA00.PAC  0x02AB0000  12239540    37163504     fdb4a11e805f31e0e35eb0ed2c08755f8a65515b59fe8094530501bedd51ddc3  43804307b77e68e085a83f8d2838b6b112b15e54a10c01e6debf98604ae87921
```

Chaque sortie commence par `FHM `, contient 26 enfants top-level, 9 enfants
FHM imbriqués et 112 lignes FHM récursives; le parseur ne signale aucun échec.
Les types top-level reconnus sont `MDLP`, `PLAD`, `FHM ` et `ACE6`.

## Implémentation et validation

Les outils durables sont `tools/ac6_mode1_codec.py`, `tools/ac6_fhm.py` et
`tools/extract_ac6_pac.py`. La commande de qualification utilisée est:

```sh
python3 tools/extract_ac6_pac.py game-files --indices 11 12 13 \
  --output /tmp/ac6-cycle-986-payloads --decompress
```

Résultats: `decoded=3`, `fhm=3`, `py_compile=pass`,
`campaign_catalog=pass missions=15 qualified=1 partial=14 unqualified=0`.
Les payloads restent locaux sous `/tmp`; le catalogue ne conserve que les
hashes, tailles, structures et plages nécessaires à la revue.
