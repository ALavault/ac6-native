# Cycle 40 — propriétaire de handles de ressources AC6 PAL

## Portée qualifiée

- cible : `ace-combat-6-pal` ;
- XEX SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- routine brute : `0x821b9408..0x821b973c` ;
- méthode : lectures Ghidra bornées et oracle de mnémotechniques, sans analyse globale.

## Résultat statique

Ghidra n'avait pas créé de fonction à `0x821b9408`, mais le code commence par
la sauvegarde ABI et finit par le helper de restauration à `0x821b973c`. Il
initialise l'objet reçu dans `r3`, puis résout cinq clés de chaînes avec
`0x821d2ad8` sur le même contexte en `r28` :

| clé | stockage dans l'objet en `r30` |
| --- | --- |
| `selectcommonresource` | `+0x44` |
| `selectresource00` | `+0x48` |
| `selectresource01` | `+0x4c` |
| `missionresource` | `+0x54` |
| `eventdemoresource` | `+0x58` |

Les résultats sont ensuite recherchés via `0x821d2fc0`; les objets non nuls
reçoivent des valeurs aux offsets `+0x2c` et `+0x30`. Ces offsets décrivent une
politique de handle et non une sémantique de jeu confirmée.

L'export de `0x821d2ad8` le déclare à tort sans retour car il se réduit au
helper ABI `0x82382efc`. Les cinq séquences `bl 0x821d2ad8` suivies de stores
prouvent statiquement le retour de ce chemin. Cela confirme un défaut de
frontière/décompilation, non un comportement runtime ou un hook stable.

Cette routine est une frontière prometteuse de préparation au modding : elle
associe des clés de ressources nommées à des handles de l'objet de sélection/
mission/cinématique. Stabilité et ordre runtime restent `needs-dynamic-evidence`
dans Xenia/XenonTests ; aucun patch XEX, hook ou API de mod n'est introduit.

## Validation reproductible

```sh
HOME=/tmp/ac6-ghidra-cycle40-home \
  .tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript VerifyResourceHandleOwner.java -noanalysis
```

Les lectures qui ont motivé l'oracle sont dans
`reports/ghidra-cycle-40-resource-table-context.log` et
`reports/ghidra-cycle-40-resource-owner-tail.log`.

## Prochaine action exacte

Sous Xenia ou XenonTests, tracer `0x821b95e4`, `0x821b95fc`, `0x821b9610`,
`0x821b962c` et `0x821b9644`, en enregistrant la clé, le handle retourné et
les écritures `+0x2c/+0x30`. Cela décidera si cette frontière est active dans
le démarrage retail et si elle peut devenir un point d'observation stable.
