# Cycle 918 — contrat de topologie natif

`NativeGeometryMetadata` expose maintenant `NativeIndexTopology` :
`TriangleList` par défaut ou `TriangleStripRestart` pour les NDXR big-endian
qualifiés. Le renderer consomme ce contrat typé, et ne déduit plus la topologie
à partir d’une chaîne de format. Les manifests/fixtures legacy restent
triangle-list; les NDXR retail publient explicitement le mode strip.

Validation :

- CTest normal : 3/3 ;
- CTest ASan/UBSan avec SDL dummy : 3/3 ;
- audit CPack : `package_audit=pass entries=17`.
