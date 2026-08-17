# Cycle 1738 — audit du profil persistant Xenia Edge

## Constat

Le wrapper [`run_xenia_edge_native.sh`](../scripts/run_xenia_edge_native.sh),
SHA `5bfba913…c595`, est conçu pour conserver un profil : il passe toujours
`--storage_root`, `--content_root` et `--cache_root` sous une racine stable,
et `mkdir -p` est idempotent. L’override `XENIA_EDGE_PROFILE_XUID` est borné à
16 chiffres hexadécimaux.

L’audit read-only ne trouve toutefois pas la racine par défaut
`.tools/xenia-edge-profile` et n’a pas exécuté deux lancements réels avec le
même profil. La réutilisation est donc **implémentée statiquement, non validée
runtime**. L’archive Edge du cycle 1735 n’utilisait pas ce wrapper et ne peut
pas servir de preuve de réutilisation.

## Garde et prochain test

Le résultat durable est
[`ac6-xenia-edge-profile-reuse-audit-v1.json`](../analysis/oracle/ac6-xenia-edge-profile-reuse-audit-v1.json).
Il reste oracle `xenia-generic`, sans lien de preuve avec les pixels ou l’audio
PAL. Le prochain test autorisé est borné à une racine absolue temporaire : deux
lancements successifs, hashes de configuration et lignes de chargement de
profil comparés, sans modification du checkout Edge ni des actifs du jeu.
