# AC6 — réconciliation de provenance des projets Ghidra (cycle 164)

Date : 2026-07-17 (Europe/Paris)

## Objet

Cette passe est une correction documentaire et headless. Elle ne modifie ni le
XEX, ni les sorties XenonRecomp, ni les projets Ghidra. Elle vérifie quel projet
représente réellement le binaire AC6 qualifié :

- cible : `ace-combat-6-pal` ;
- fichier : `game-files/default.xex` ;
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- entrée XEX : `0x821f5e90`.

Deux projets existent dans `ghidra-projects` : `ace-combat-6` et
`ace-combat-6-corrected`. Ils ont les mêmes noms de blocs, tailles et
identifiants de programme, mais leurs octets de code ne sont pas identiques.
Une adresse ne suffit donc pas à identifier une preuve entre ces deux états.

## Résultats reproduits

### Projet canonique et réimport frais

Le projet `ace-combat-6` et un réimport frais headless avec le XEX Loader
actuellement installé donnent les mêmes résultats :

```text
0x82090000: 7d 88 02 a6 91 81 ff f8 fb e1 ff f0 94 21 ff a0
0x82102148: 7d 88 02 a6 48 28 0d 75 db a1 ff 50 db c1 ff 58
0x821f5e90: 7d 88 02 a6 48 18 d0 65 3b e1 fe 10 94 21 fe 10
```

Le début de `0x82102148` est donc un prologue PPC avec sauvegarde de LR et
appel vers le helper ABI `0x82382ec0`, cohérent avec le code généré par
XenonRecomp dans `.tools/recomp-eval/ac6/output/ppc_recomp.10.cpp`.
Le mapping généré contient `sub_82102148`, puis `sub_82102564` et
`sub_82102568`; la séquence complète couvre donc bien le corps observé dans
les cycles 162–163 jusqu'avant `0x82102568`.

Le bloc `.pdata` fournit la même borne :

```text
0x8207c1c8 -> 0x82102148
0x8207c1cc -> 0x40010706
0x8207c1d0 -> 0x82102568
```

La fonction manager Ghidra peut néanmoins ne marquer que deux instructions
comme corps de fonction. Comme déjà documenté pour les helpers ABI, cette
frontière Ghidra est incomplète ; les octets bruts, `.pdata` et XenonRecomp sont
les sources de preuve retenues pour cette fonction.

### Projet `ace-combat-6-corrected`

Le même point d'entrée contient une autre séquence :

```text
0x82090000: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0x82102148: 38 80 00 00 fc 20 f8 90 38 60 00 60 48 23 3b d5
```

Dans ce projet, Ghidra décrit `0x82102148..0x82102157` comme une fonction de
quatre instructions. Son journal d'import contient des offsets `raw = ...`
et rapporte 8 058 fonctions définies, contre 8 246 dans le réimport frais et
dans le projet canonique. Cela suffit à établir une divergence de loader ou
d'état d'import ; ce projet ne doit pas servir seul de source d'octets pour le
XEX qualifié.

Le projet corrigé n'est pas supprimé : il reste un artefact historique à
réconcilier. Les rapports qui le citent doivent être relus avec leur
`project_name` et leurs octets de référence, sans fusion automatique avec le
projet canonique.

## État de preuve corrigé

### KEEP, avec clarification de provenance

- Le lecteur de la zone publiée par le worker autour de `0x82102148` reste
  supporté par les octets du projet canonique/réimport frais, la borne `.pdata`
  et la sortie XenonRecomp. Les rapports des cycles 162–163 restent utiles.
- La lecture de `receiver+0x28`, `+0x30`, `+0x5c`, puis l'usage de `+0x74` et
  `+0x78` restent une qualification statique ; ils ne prouvent ni le nom C++,
  ni l'appelant indirect, ni le propriétaire/libérateur final.

### KEEP_WITH_CLARIFICATION

- Toute preuve AC6 doit désormais préciser le projet Ghidra et comparer les
  octets au SHA-256 du XEX. `ace-combat-6` est la référence utilisée par
  `ghidra-bridge.yaml` et par le réimport frais de cette passe.
- Les résultats issus de `ace-combat-6-corrected` ne sont pas invalides par
  principe, mais ils sont `needs-revalidation` tant qu'ils ne sont pas
  confirmés par le projet canonique ou une source indépendante.

### OBSOLETE / HARMFUL_OR_ACCIDENTAL

- La pratique consistant à appeler indifféremment les deux projets « corrigés »
  sans qualifier leur provenance est obsolète et peut contaminer les exports.
  Elle est abandonnée ; aucune suppression de données n'est effectuée.

## Actions effectuées

- vérification read-only des métadonnées, blocs mémoire et entrées XEX dans les
  deux projets ;
- vérification `ListFunctionsRange`, `FindSymbolReferences` et
  `FindDirectCallsTo` autour de `0x82102148` ;
- réimport headless frais avec le même `default.xex` et comparaison des octets ;
- comparaison avec le mapping et le prologue XenonRecomp déjà générés ;
- ajout de `scripts/DumpProgramIdentity.java`, outil read-only ignoré par Git,
  pour éviter une nouvelle confusion de projet.

Aucune session Xenia, VNC, Wine ou intervention humaine n'est nécessaire pour
cette correction.

## Prochaine action exacte

1. Régénérer les exports AC6 uniquement depuis le projet canonique ou un
   réimport frais équivalent ;
2. conserver `target_id`, SHA-256, `project_name`, version du loader et hash de
   chaque artefact dans le catalogue ;
3. requalifier les rapports « corrected project » avant de les ingérer dans le
   graphe de connaissances ;
4. reprendre la recherche des références indirectes au lecteur après cette
   normalisation de provenance.

## Validation

Commandes principales exécutées en lecture seule :

```sh
.tools/build-xex1tool/xex1tool -l game-files/default.xex

analyzeHeadless ... ace-combat-6 ... \
  -postScript DumpProgramIdentity.java \
  -postScript VerifyXexEntry.java \
  -postScript DumpBytes.java 0x82090000 32 \
  -postScript DumpBytes.java 0x82102148 32 \
  -readOnly -noanalysis

analyzeHeadless ... /tmp/ac6-cycle164-fresh-project fresh \
  -import game-files/default.xex ... -readOnly/-noanalysis
```

Résultat : réimport frais et projet `ace-combat-6` concordants ; projet
`ace-combat-6-corrected` divergent et classé `needs-revalidation`.
