# Cycle 1547 — préflight du reçu de projection dans la CLI native

## Résultat

`ac6-native replay` accepte désormais l'option facultative
`--projection-receipt RECEIPT_JSON`. Lorsqu'elle est présente, le reçu, le
replay AC6RTPLY v3 et l'index du cache déjà ouvert sont vérifiés ensemble avant
toute exécution de la session. Une erreur de préflight refuse l'exécution avec
le code dédié `140` et un diagnostic `projection_receipt_<erreur>`.

Sans cette option, le chemin historique reste accepté. Son rapport expose
explicitement `projection_receipt_provided=false`,
`native_output_verified=false` et `source_lineage_verified=false`.

Après préflight positif, le rapport expose les trois états correspondants,
ainsi que les SHA-256 exacts du reçu et du replay. Le préflight ne prétend pas
qualifier la lignée brute : `source_lineage_verified` reste `false`.

## Validation

- build ciblé `ac6-native` et `ac6-retail-projection-receipt-tests` : passé ;
- CTest ciblé reçu + replay de session : 2/2 passés ;
- replay M01 de quatre frames sur le cache qualifié CP3 : accepté avec reçu,
  rapport `provided=true`, sortie native vérifiée et lignée non vérifiée ;
- même replay sans reçu : accepté, trois états de preuve à `false` ;
- reçu illisible (`/dev/null`) : refus avant session, code `140` ;
- aide publique et `git diff --check` : passés.

## Risque résiduel

La CLI vérifie l'intégrité de la projection native et son identité de cache,
pas les artefacts source/parent mentionnés par le reçu. Les gates de preview
devront donc imposer l'option et une qualification séparée de cette lignée.
