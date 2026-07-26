# AC6 cycle 94 — revalidation de provenance du parcours `0x8226ECB0`

## Identité et méthode

- cible déclarée : AC6 PAL `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- adresse : Xbox 360 PowerPC big-endian `0x8226ECB0` ;
- méthode : `analyzeHeadless -readOnly -noanalysis`, sans GUI ni écriture dans
  les projets Ghidra ;
- script : `scripts/DecompileAt.java 0x8226ecb0` ;
- projets relus : `ace-combat-6` et `ace-combat-6-corrected`.

## Résultat

Le journal historique `reports/logs/entry9-frame-consumer.log` attribue à
`0x8226ECB0` une itération de collection avec les appels directs
`0x8222CCD0`, `0x8222B740` et `0x82227378`. La relecture des octets distingue
désormais clairement les deux projets :

| Projet | Résultat `-readOnly` à `0x8226ECB0` |
| --- | --- |
| `ace-combat-6` | le décompilateur réduit la fonction à un thunk, mais les octets bruts montrent le caller `0x8226A508 -> 0x8226ECB0`, la boucle `count +0x4` / `entries +0x8`, les masques `+0x118 & 0x22` ou `0x402`, les slots `+0x54/+0x5C/+0x60/+0xCC`, et le triplet historique. |
| `ace-combat-6-corrected` | corps incompatible commençant par `cmpwi r3,0`; le décompilateur signale aussi des lectures impossibles à `0x817e4eb4` et un flux P-code vers une mémoire inexistante `0x3fc0826e`. |

Le répertoire `exports/8226ecb0.json` correspond au premier état et reste trop
faible à lui seul : le thunk prologue masque son corps au décompilateur. Le
dump brut du projet `ace-combat-6` est la preuve déterminante du corps retail.

## Décision

`Function8226ecb0TraversalEvidence` est promu comme frontière statique retail
qualifiée. La traduction hôte conserve les offsets et le delta de frame sans
inventer de type gameplay. En particulier, le troisième appel reçoit
`r4 = objet + 0x24f8`, `r5 = objet + 0x254c` et conserve `f1`; il ne reçoit pas
un sélecteur abstrait.

Le conflit du projet `ace-combat-6-corrected` reste documenté, mais ne bloque
plus cette frontière. Les limites restantes sont les mêmes qu'avant : aucune
identité d'avion, écriture de spawn ou position ne découle de cette boucle.

## Validation native

- build ciblé : `cmake --build .build/ace-combat-6/native -j16 --target
  ac6-unit-factory-tests` ;
- test du contrat : `ctest --test-dir .build/ace-combat-6/native
  --output-on-failure -R '^ac6-unit-factory-tests$'` : **1/1** ;
- corpus AC6 : `ctest --test-dir .build/ace-combat-6/native
  --output-on-failure` : **41/41** ;
- installation : `cmake --install .build/ace-combat-6/native --prefix
  "$PWD"`, avec absence de `bin/bin` vérifiée.

Ces portes valident le contrat de callback natif et sa non-régression dans le
port ; elles ne constituent pas une trace Xenia de mission ni une preuve de
parité gameplay.
