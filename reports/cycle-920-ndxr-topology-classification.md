# Cycle 920 — classification NDXR fail-closed

La topologie `TriangleStripRestart` n’est plus assignée à tout NDXR par
défaut. Le décodeur la publie uniquement lorsqu’au moins un index
`0xFFFF` est observé; sinon le buffer reste `TriangleList`. Les sentinels
restent conservés dans la géométrie décodée uniquement pour les strips.

Validation : CTest normal 3/3, CTest ASan/UBSan 3/3 et `package_audit=pass`.
