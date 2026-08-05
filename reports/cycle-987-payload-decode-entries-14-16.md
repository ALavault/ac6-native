# Cycle 987 — décodage des payloads campagne 14–16

## Résultat borné

Les plages `DATA.TBL` 14–16 sont lues depuis `DATA00.PAC`, déscramblées par
la clé pi/XOR indexée, puis décompressées en raw DEFLATE. Les bytes décodés
restent locaux; seuls les extents, hashes et résumés structuraux entrent dans
le catalogue.

```text
mission  DATA.TBL  offset       stored   expanded   stored SHA-256                                                        decoded SHA-256
6        14         0x03660000   7918063  27320816   68dde5fdc8f7664a7375bd4fce638d938f3161d58e689e3a7dfa92bb8e6232f0  1c386251c8b8505da5b8a667395edad233349f4092a6a4443403b07691b37a09
7        15         0x03DF0000   8783785  30120272   2e514c771c3ae47c376b9dfd4a44809364628bc2c63e8dba5664f82ba5f26892  d503092155a2d8c4f4bdcc86971e607e720d94abfa51461629a0b37b750c3947
8        16         0x04658000   9510233  32293360   7b64eeedfcfb63453ec298cc72681cac3df9e21a84b6bfec6a5c253dbe8c683c  e667df8084e710d2a9db957062bc0f6f78ac455fc749bf42831b7ed5e5ea20a1
```

Structure: 26 enfants top-level, 9 FHM imbriqués et 112 lignes récursives
pour les entrées 14 et 16; l’entrée 15 possède 13 FHM imbriqués et 728
lignes. Tous les parses sont sans échec; les types top-level reconnus sont
`MDLP`, `PLAD`, `FHM ` et `ACE6`.

## Comparaison hash 11–16

Une marche récursive bornée produit 1 288 nœuds, 877 hashes uniques et 18
groupes de hash présents dans plusieurs entrées. Dix-sept groupes ont des
bytes non vides; le dix-huitième est le hash vide des slots sans taille. Le
FHM de 480 octets (`0f83f27d…`) et le bloc `ACE6` de 4 octets
(`b18a4e45…`) sont identiques dans les six entrées. Les groupes binaires
restants sont catalogués par hash et `magic_hex`, sans leur attribuer de
sémantique.

## Validation

```text
python3 tools/extract_ac6_pac.py game-files --indices 14 15 16 --output /tmp/ac6-cycle-987-payloads --decompress
decoded=3 fhm=3
campaign_catalog=pass missions=15 qualified=1 partial=14 unqualified=0
```

Les routes physiques restent `DPL 14–16 → DATA.TBL 14–16`; aucune route native
partielle n’est promue par ce décodage.
