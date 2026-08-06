# Cycle 938 — contrat de chemin controls

Le test de manifestes vérifie maintenant qu’une ligne `controls` est résolue
relativement au manifeste, conservée dans `MissionManifestPaths` et disponible
pour le smoke frontend. Le profil lui-même reste validé par
`SdlInputProfile::load_manifest`.

CTest normal : 3/3. Le profil est toujours externe et optionnel ; aucun mapping
ou asset retail n’est inclus dans le paquet.
