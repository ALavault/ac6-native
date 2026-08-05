# Cycle 1032 — frontière statique scénario Mission 01

Date : 2026-08-06. Cette note conserve le résultat durable d'un scan borné et
parallélisé des entrées PAC. Les payloads et les buffers extraits restent sous
`/tmp` et ne sont pas committés.

## Couverture

- `DATA00.PAC` : entrées 0–465 et 878–925, avec les résultats des scans
  `/tmp/ac6-data-boundary-scan.json` (SHA-256
  `6be32b7565955f9c7c2f3551759d674fe40bcb73a3abf9b103216137cfcf5d84`),
  `/tmp/ac6-data00-39-181-scenario-scan.json` (SHA-256
  `d58883263519727313a7d4983cf13dfdd6507668a565f400ea86194f1b3f87d7`) et
  `/tmp/ac6-data00-182-465-scenario-scan.json` (SHA-256
  `4f53482f618670928da5245d00d32be169ce85fff0c717ee8dfbe32fa025ca9e`).
- `DATA01.PAC` : entrées 0–925; le scan compressé a le SHA-256
  `1b2308697008604d9ae445b343aac38b3d3ce8e33d24f8c0cdb19b0b0fe0bc81` et
  les quatre entrées raw 500, 618, 683 et 779 ont été inspectées séparément
  dans `/tmp/ac6-data01-raw-holes.json` (SHA-256
  `ab9ef553395eb423ab6cd4e6c31ba5b56f0afd3bb8d76de0fe909bf5b232813d`).

## Résultat qualifié

L'entrée `DATA.TBL[34]` est une fermeture FHM valide sans parser note. Son
payload a le SHA-256
`ce5316ffe7f2e52a17bcd7c218a74303fb911a7240fef16b33b5ea416301b0f0`; le leaf
`root/0015` a la taille 5944 et le SHA-256
`2c5d9fe0ca271e2869157cfc14fdaffa1988d5152275dbdf1647a0b3578b0fd0`.
Le leaf contient les clés big-endian `15`
`JIKKYOU_PLAYER_AWACS_MISSION_START`, `86`
`JIKKYOU_PLAYER_AWACS_SHTDWN_SHIP_DESTROYER` et `98`
`JIKKYOU_PLAYER_AWACS_MISSION_END`. Un autre nœud de la même racine contient
`mapobj_m01_l_brg1` et `mapobj_m01_l_brg2`. Seule l'identité de l'événement 15
est raccordée au runtime natif dans la slice P4.

## Résultat négatif et limite

Les chaînes lisibles `SubMisTbl`, `SubMis`, `ComTbl` et `Maneuver` n'ont pas
fourni de propriétaire FHM qualifié dans ces scans. Les hits bruts `<Obj` et
`<Act` des entrées 553 et 564 sont dans des octets de flottants NDXR; ils ne
constituent pas un script scénario. Des libellés localisés génériques existent
dans l'entrée 1, mais sa fermeture contient un FHM imbriqué invalide et elle
n'est donc pas promue comme source d'objectif Mission 01.

Cette note ne prétend pas démontrer l'absence de toute structure binaire
scénario. Elle ferme uniquement la piste textuelle/PAC immédiate. La prochaine
expérience utile est l'identification bornée de la propriété retail des unités,
vagues et transitions d'objectif; il ne faut pas rouvrir les fronts raster,
caméra ou textures sur cette base.
