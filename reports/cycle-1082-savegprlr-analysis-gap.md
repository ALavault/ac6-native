# Cycle 1082 — la famille `__savegprlr` masquait les trois quarts du code

Date : 2026-08-07.

## Qualification

- Projet Ghidra canonique : `ghidra-projects/ace-combat-6`.
- Target : Xbox 360 PAL, `default.xex`.
- XEX SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Module : `default.xex` ; adresses dans l'image chargée Xenon, base `0x82000000`.
- Image de contrôle : `analysis-input/ACE6_X360.exe`, `.text` VA `0x82090000`,
  base fichier 589 824. Correspondance octet pour octet vérifiée contre trois
  plages déjà qualifiées par `RepairQualifiedFunctionBoundaries.java`
  (`0x8218C238`, `0x821C37E0`, `0x821C4FA0`) : **3/3 empreintes SHA-256 identiques**.
- XenonRecomp n'intervient pas dans ce cycle.

## Défaut

L'import du XEX marque une partie de l'îlot `__savegprlr_N` comme `noreturn`.
Un prologue `mflr r12 ; bl __savegprlr_N` termine alors son propre corps : l'appel
porte un `FlowOverride` terminal, rien ne retombe en séquence, et **le reste de la
fonction n'est jamais désassemblé**.

État mesuré avant correction, dans le projet lui-même
(`scripts/AuditPpcHelperNoReturnImpact.java`, `-readOnly -noanalysis`) :

| observable | avant |
| --- | ---: |
| entrées `noreturn` dans l'îlot | **11 / 36** |
| sites d'appel vers `__savegprlr_*` | 4 128 |
| dont sans retombée | **3 871** (93,8 %) |
| octets désassemblés sur `0x82090000`–`0x823E7FF7` | 1 569 680 / 3 506 168 = **44,8 %** |
| octets dans un corps de fonction | 823 297 = **23,5 %** |
| fonctions | 9 072 |

L'îlot de restauration n'est pas en cause : `b __restgprlr_N` est un branchement
terminal et n'a légitimement pas de retombée.

## Correction

`scripts/RepairPpcSaveHelperFamily.java`, qualifié sur les octets avant toute
écriture :

- chaîne de sauvegarde `0x82382EC0`–`0x82382F0C`, 80 octets,
  SHA-256 `c91d6acdcca047ceaa15c988eab916f741a0dcb81941ab90e45888ef845539e0` ;
- chaîne de restauration `0x82382F10`–`0x82382F60`, 84 octets,
  SHA-256 `f5f9f5c1203f5d2d1a3e554b1a00333f1a7e0fe6245eeea147424b7914efe443`.

La disposition est celle qu'asserte déjà `VerifyPpcAbiSaveRestoreHelpers.java` :
18 `std` en `0x82382EC0 + 4n` puis `stw r12,-8(r1)` et `blr` ; 18 `ld` en
`0x82382F10 + 4n` puis `lwz`, `mtlr`, `blr`. Elle confirme au passage
`0x82382F18 = __restgprlr_16` (cycle 313). Aucune aide FPR ni VMX n'existe dans ce
binaire : le balayage de `stfd f14,-0x90(r1)`, `lfd f14,-0x90(r1)` et des paires
`stvx`/`lvx r12,r1` ne retourne aucun site.

Quatre phases :

1. `noreturn = false` et nommage sur les 18 entrées de sauvegarde ; les 18 entrées
   de restauration reçoivent une étiquette, **pas** une fonction — les découper
   détruirait la sémantique de branchement terminal dont dépend chaque appelant ;
2. effacement du `FlowOverride` résiduel sur chaque site d'appel — effacer le
   drapeau ne réécrit pas les instructions déjà annotées ;
3. amorçage du désassemblage à chaque retombée rétablie ;
4. recalcul du corps de chaque fonction tronquée
   (`CreateFunctionCmd.fixupFunctionBody`).

Résultat de l'exécution :

```
AC6_CALL_SITES inspected=4128 cleared=3871
AC6_DISASSEMBLY_SEEDS 3298 of 3871
AC6_BODY_FIXUP affected=3558 grown=3556 failed=2
```

Les deux échecs sont `0x82276FE0` et `0x822C1EF0` ; ils restent ouverts.

## Effet mesuré

| observable | avant | après |
| --- | ---: | ---: |
| entrées `noreturn` dans l'îlot | 11 / 36 | **0 / 36** |
| octets désassemblés | 44,8 % | **77,6 %** |
| octets dans un corps de fonction | 23,5 % | **57,9 %** |
| corps de fonction recalculés | — | 3 556 |

## `.pdata` comme arbitre, et deux îlots de plus

Le `.pdata` du PE est la table de fonctions du linker : 8 246 entrées
`(BeginAddress, PackedData)`, monotones, **sans aucun recouvrement**, couvrant
3 137 892 des 3 506 168 octets de la plage de code (**89,5 %**).
SHA-256 `740f31d530dcfca2fcddab6ac6f93e1ab55d36106a9a015e41f074d5e6d73034`.
C'est une borne produite par le binaire lui-même, pas une liste inférée.

Trois passes s'appuient dessus.

**1. Débuts de fonction manquants** (`scripts/CreatePdataFunctionStarts.java`,
strictement additive : une fonction existante n'est jamais redimensionnée,
renommée ni supprimée, et les corps restent dérivés du flux) :

```
AC6_PDATA_SUMMARY entries=8246 out_of_range=0 already_present=7429
                  disassembly_seeded=107 created=817 failed=0
```

**2. Flux contredit par le linker** (`scripts/RepairFlowUsingPdataExtents.java`).
Règle : *si un appel se trouve dans un extent enregistré et n'en est pas la
dernière instruction, le linker affirme que la fonction continue — donc l'appel
retourne.* C'est une preuve sur l'appelé, sans heuristique sur sa forme.

Cette règle généralise la correction `__savegprlr` et trouve **trois familles
qu'une liste codée en dur aurait manquées** :

- `__savefpr_14..31` / `__restfpr_14..31` à `0x82384410` et `0x8238445C` —
  basées sur **`r12`**, et non `r1` ; un balayage de `stfd f14,-0x90(r1)` ne les
  voit pas, ce qui explique la conclusion initiale « aucune aide FPR/VMX » ;
- un îlot VMX128 autour de `0x82385894`/`0x823858A4` ;
- des souches nues comme `0x822DDBE8` (un simple `blr`), qui tronquaient
  83 fonctions à elles seules.

```
AC6_ROUND 1 cleared=907 unflagged_callees=47 bodies_grown=636
AC6_ROUND 2 cleared=0   (point fixe)
```

**3. Amorçage du désassemblage sur tous les extents**
(`scripts/SeedPdataExtentDisassembly.java`) : 42 507 mots désassemblés.
31 380 mots refusent de se désassembler — vraisemblablement des tables de saut
en ligne ; ils sont laissés en l'état plutôt qu'interprétés de force.

## État final de la tranche

| observable | avant | après |
| --- | ---: | ---: |
| octets désassemblés | 44,8 % | **94,4 %** |
| octets dans un corps de fonction | 23,5 % | **71,8 %** |
| fonctions | 9 072 | **9 891** |
| entrées `noreturn` mal marquées corrigées | — | 11 + 47 |
| sites d'appel réparés | — | 3 871 + 907 |

Mesuré contre les extents `.pdata` eux-mêmes : **7 201 fonctions complètes au
octet près**, 1 045 encore courtes. La perte résiduelle (668 372 octets sur
995 fonctions) est presque entièrement en `UNDEFINED_BYTES` derrière un `bctr` :
le flux n'atteint pas les blocs de `case` faute de récupération des tables de
saut. C'est une frontière réelle et **distincte** ; elle ne bloque pas le travail
scénario.

## Les 17 parseurs de scénario sont complets

Chaque fonction de la famille `*Bin` est désormais présente et son corps est
**exactement** la longueur enregistrée par le linker :

| fonction | `.pdata` | corps | instructions |
| --- | ---: | ---: | ---: |
| `SubMisTblBin::getReadBuffSize` `0x82309620` | 312 | 312 | 78 |
| `SubMisTblBin::read` `0x82309758` | 356 | 356 | 89 |
| `SubMisBin::getReadBuffSize` `0x8232C7E0` | 196 | 196 | 49 |
| `SubMisBin::read` `0x8232C8A8` | 344 | 344 | 86 |
| `SetBin::getReadBuffSize` `0x8232F4D0` | 292 | 292 | 73 |
| `SetBin::read` `0x8232F5F8` | 356 | 356 | 89 |
| `ObjBin::getReadBuffSize` `0x8232FF78` | 476 | 476 | 119 |
| `ObjBin::read` `0x82330158` | 996 | 996 | 249 |
| `ActBin::getReadBuffSize` `0x82330540` | 328 | 328 | 82 |
| `ActBin::read` `0x82330688` | 392 | 392 | 98 |
| `OrderBin::getReadBuffSize` `0x823310E8` | 288 | 288 | 72 |
| `OrderBin::read` `0x82331208` | 1 120 | 1 120 | 280 |
| `ManeuverBin::getReadBuffSize` `0x823316A0` | 356 | 356 | 89 |
| `ManeuverBin::read` `0x82331808` | 560 | 560 | 140 |
| `ComTblBin::getReadBuffSize` `0x82331BB0` | 96 | 96 | 24 |
| `ComTblBin::read` `0x82331C10` | 328 | 328 | 82 |
| `ComBin::read` `0x82331D98` | 224 | 224 | 56 |

La décompilation de `SubMisTblBin::getReadBuffSize` livre directement le format
de conteneur générique, offsets relatifs à leur propre base :

```
noeud:
  u32 offset_enfant0   (relatif au noeud)  -> u8 count
  u32 offset_table     (relatif au noeud)
table:
  u32 entry_count
  u32 entry_offset[entry_count]            (relatifs à la table)
```

et sa taille de tampon vaut `count * 0x10` plus la somme des
`SubMisBin::getReadBuffSize` de chaque entrée — c'est-à-dire l'arithmétique de
mise en page recherchée. L'extraction complète du schéma est la tranche suivante.

## Ce que la correction rend visible : la famille de parseurs de scénario

Les chaînes d'erreur du binaire sont auto-descriptives et nomment leurs propres
classes. `scripts/InspectScenarioBinParsers.java` mesure la famille avant/après.
Chaque parseur **s'identifie désormais par sa propre chaîne**, sans supposition
sur son adresse :

| fonction | avant | après | identité prouvée par |
| --- | ---: | ---: | --- |
| `SubMisTblBin::getReadBuffSize` `0x82309620` | 2 instr. | **78** | `.../ submis empty! : %d` |
| `SubMisTblBin::read` `0x82309758` | 2 instr. | **89** | `.../ submis empty!` |
| `SetBin::getReadBuffSize` `0x8232F4D0` | 2 instr. | **73** | `.../ act empty! : %d` |
| `SetBin::read` `0x8232F5F8` | 2 instr. | **89** | `.../ act empty! : %d` |
| `ObjBin::getReadBuffSize` `0x8232FF78` | 2 instr. | **119** | `.../ child empty!` |
| `ObjBin::read` `0x82330158` | **2 instr.** | **249** | `.../ data empty!` |
| `ActBin::read` `0x82330688` | 2 instr. | **98** | `.../ order empty! : %d` |
| `OrderBin::getReadBuffSize` `0x823310E8` | 2 instr. | **72** | `.../ data empty!` |
| `OrderBin::read` `0x82331208` | **2 instr.** | **280** | `.../ child empty!` |
| `ManeuverBin::getReadBuffSize` `0x823316A0` | 2 instr. | **89** | `.../ comtblm empty!` |
| `ManeuverBin::read` `0x82331808` | 2 instr. | **140** | `.../ comtblm empty! : %d` |
| `ComTblBin::read` `0x82331C10` | 2 instr. | **82** | `.../ com empty! : %d` |
| `ComBin::read` `0x82331D98` | 2 instr. | **56** | — |

Les messages « X empty! » d'un parseur nomment son enfant. La hiérarchie de
conteneurs se lit donc directement, statiquement :

```
SubMisTbl ─> SubMis ─> Set ─> Act ─> Order ─> {Disappear, Stop, Lead, Jump, Flag, Property}
Obj ─> {Durable, Weapon}
Maneuver ─> ComTblM
ComTbl ─> Com
```

`OrderBin::read` porte à lui seul les huit variantes `Order*` ; `ObjBin::read`
porte `DurableBin` et `WeaponBin`.

Restent sans fonction, et donc ouverts pour la passe de débuts de fonction :
`SubMisBin::getReadBuffSize` `0x8232C7E0`, `SubMisBin::read` `0x8232C8A8`,
`ActBin::getReadBuffSize` `0x82330540`, `ComTblBin::getReadBuffSize` `0x82331BB0`.

## Requalification d'une borne négative

`research/hypotheses.yaml` enregistrait `H-GHIDRA-SUBMISTBL-STRING-REFERENCE`
comme **rejetée** au cycle 1036 : « zero references and zero split-materialization
hits » pour `0x8200F5A8`
(`Error / SubMisTblBin::getReadBuffSize ( ) / submis empty! : %d`).

Le **même script**, `FindPpcSplitAddressMaterialization.java`, sans modification,
sur le corpus réparé :

```
82309668 lis r11,-0x7dff ; +24 823096c8 subi r24,r11,0xa58 => 0x8200f5a8
823096a0 lis r11,-0x7dff ; +10 823096c8 subi r24,r11,0xa58 => 0x8200f5a8
823096c0 lis r11,-0x7dff ; +2  823096c8 subi r24,r11,0xa58 => 0x8200f5a8
SUMMARY target=0x8200f5a8 window=24 hits=3
```

Le site unique est `0x823096C8`, à l'intérieur de `SubMisTblBin::getReadBuffSize`
— c'est-à-dire dans le trou d'analyse ouvert par le `noreturn` de `0x82382EE8`.
La seule variable qui a changé est la couverture du désassemblage.

**`H-GHIDRA-SUBMISTBL-STRING-REFERENCE` est un faux négatif de couverture, pas une
propriété du binaire.** Les conclusions des cycles 1036, 1070 et 1073 tirées de
l'absence de référence ou de propriétaire doivent être rejouées sur ce corpus avant
d'engager la moindre session runtime supplémentaire.

Une nuance de méthode explique aussi pourquoi la recherche a paru robuste : le
compilateur partage un `lis` entre plusieurs `addi`. En `0x823096C0` il émet
`lis r11,0x8201`, puis un `addi r29,r27,0x4` **sans rapport**, puis
`addi r24,r11,-0xa58`. Un appariement qui ignore le registre rapporte la mauvaise
constante et masque la bonne ; `FindPpcSplitAddressMaterialization.java` gère
correctement le registre, mais ne pouvait rien trouver dans des octets non
désassemblés.

## Récupération des références d'adresses scindées

`scripts/RecoverPpcSplitAddressReferences.java` propage les demi-hautes de `lis`
par registre, invalide un registre dès qu'une autre instruction l'écrit
(`Instruction.getResultObjects()`), et crée une référence pour chaque paire
`lis`/`addi` ou `lis`/`ori` qui résout dans une plage mémoire mappée.

```
AC6_SPLIT_REFS functions=9891 pairs=15112 created=7944 already_present=2394 unmapped=4774
AC6_SUBMISTBL_XREF_COUNT 1 expected_site=0x823096c8
```

`0x8200F5A8` porte désormais **exactement une référence, depuis `0x823096C8`**,
dans `SubMisTblBin::getReadBuffSize` — le site prédit.

| observable | avant | après |
| --- | ---: | ---: |
| chaînes sans référence | 4 186 / 4 963 (84 %) | **3 237 / 4 968 (65 %)** |
| globales sans référence | — | 2 257 / 5 294 |

**La cible « moins de 1 000 chaînes sans référence » n'est pas atteinte** et ne
l'était pas atteignable par cette passe seule : les 3 237 restantes ne sont pas
matérialisées par une paire `lis`/`addi`. Elles se répartissent entre tables de
pointeurs en `.rdata`, chargements relatifs à une base de données courtes, et
chaînes réellement mortes d'une bibliothèque liée mais jamais appelée. Distinguer
ces trois cas demande une passe distincte ; elle n'est pas requise pour la suite.

## Corpus réexporté

Réexport avec `.tools/ghidra-ai-bridge-venv` v0.2.0 — le seul pont qui émet
`pcode`/`cfg`, socle de la micro-exécution prévue. L'ancien corpus est conservé
en `exports.pre-s0/`.

| observable | avant | après |
| --- | ---: | ---: |
| fonctions exportées | 8 827 | **9 891** |
| taille du corpus | 96 Mo | **320 Mo** |
| fonctions tronquées à un appel `noreturn` | 3 659 | **382** |
| corps décompilé < 200 caractères | 5 147 | 1 625 |
| longueur médiane du C décompilé | 173 | **589** |
| p90 de la longueur décompilée | 853 | **2 673** |

## Ce que la remontée des appelants livre immédiatement

Les appelants des parseurs étaient dans le trou d'analyse. Ils en sortent :

```
SubMisTblBin::getReadBuffSize 0x82309620  <- 0x822493F0, 0x82249718
SubMisTblBin::read            0x82309758  <- 0x82249718
```

`0x82249718` est le **lecteur de scénario racine** : il parcourt la table
enfants d'un nœud et attribue chaque slot à un sous-parseur, en avançant le
curseur d'écriture de la taille de l'enregistrement :

| slot | `read` | `getReadBuffSize` | pas |
| ---: | --- | --- | ---: |
| 0 | `0x82309D20` | `0x82309C00` | 0x08 |
| 1 | `0x82309A88` | `0x82309978` | 0x0C |
| 2 | **`0x82309758` `SubMisTblBin`** | **`0x82309620`** | 0x10 |
| 3 | `0x823094D8` | `0x823093C8` | 0x10 |
| 4 | `0x823092E0` | `0x823092A0` | 0x10 |
| 5 | — | `0x82309120` | 0x08 |
| 6 | — | `0x82308F18` | 0x14 |

`0x823094D8`/`0x823093C8` sont les candidats `RadioTblBin` déjà listés comme
pistes non qualifiées dans `analysis/address_catalog.tsv` ; ce cycle les place
dans la table de dispatch du scénario, sans encore les nommer.

`0x822493F0` est la fonction de dimensionnement jumelle du même nœud.

Un cran au-dessus, trois fonctions appellent les deux : `0x82097560`,
`0x8219BDD8`, `0x8219F8C0` — trois instanciations de la même routine, chacune
avec un tampon `char[512]`. Leurs chaînes nomment les blocs qu'elles allouent :

```
'Mission Data'   'Mission(System)'   'Obj & Unit'   'Radio'
'Effect'  'EFFECT'  'PostEffect'  'Sound'  'Game Menu'
'Debriefing Record'  'Suspend'  'Replay'
'DPL::[%#x,%#x]'   'TextData::[%d,%d]'
```

`'Obj & Unit'` et `'Radio'` sont exactement les domaines que
`retail_units_and_waves` et `retail_objectives` laissent ouverts, et
`DPL::[%#x,%#x]` est le couple d'identifiants de l'archive documenté dans
`DPL_ARCHIVE_HANDLE_CHAIN.md`. La chaîne propriétaire cherchée par les cycles
1070 et 1073 est donc à un ou deux sauts, en statique.

Rien de tout cela n'est encore qualifié comme propriétaire : ce cycle établit la
chaîne d'appel et les étiquettes, pas la liaison à une entrée `DATA.TBL`
nommée. C'est l'objet de la tranche suivante.

## État et prochaine frontière

Deux frontières restent ouvertes, dans cet ordre :

1. **Tables de saut `bctr`** — 668 372 octets sur 995 fonctions restent hors
   corps parce que le flux n'atteint pas les blocs de `case`. `.pdata` affirme
   que ces octets sont du code ; 31 380 mots refusent de se désassembler et sont
   vraisemblablement les tables elles-mêmes, en ligne. Frontière réelle, mais
   **elle ne bloque pas le travail scénario** : les 17 parseurs sont complets.
2. **Propriétaire des données de scénario.** Les conteneurs `*Bin` sont
   décompilés, mais aucun conteneur au format dérivé n'a été trouvé parmi les
   enfants extraits de l'entrée 9 : `root[14]`/`root[15]` sont des dictionnaires
   de noms, et le seul candidat de forme, `013_FHM/000_00_00_00_10.bin`, contient
   des flottants de bornes de monde (`24000.0`, `30.0`, `2500.0`), pas une table.
   La remontée des appelants, faite ci-dessus, donne la chaîne
   `0x8219BDD8`/`0x8219F8C0`/`0x82097560` → `0x82249718` → `SubMisTblBin`. Reste
   à joindre `DPL::[%#x,%#x]` à une entrée `DATA.TBL` nommée pour clore le
   propriétaire.

Ne pas rejouer : la question « `0x8200F5A8` est-elle référencée ? » est close par
ce cycle. Ne pas relancer de run bridge ou Xenia sur la question du propriétaire
objectifs/vagues tant que les cycles 1070 et 1073 n'ont pas été rejoués sur le
corpus réparé.

## Artefacts

- `scripts/AuditPpcHelperNoReturnImpact.java` — audit lecture seule, avant/après ;
- `scripts/RepairPpcSaveHelperFamily.java` — correction, qualifiée sur les octets ;
- `scripts/InspectScenarioBinParsers.java` — mesure de la famille `*Bin` ;
- sauvegarde du projet avant écriture :
  `ghidra-projects/ace-combat-6.rep.bak-pre-s0`.

## Addendum — la frontière `0x8237CC58` / `0x8237CEF0`, relue

Le plan demandait de rejouer cette frontière **en statique** après S0, avant
toute nouvelle session bridge. Fait ici, sans oracle.

`0x8237CC58` commençait par `mflr r12 ; bl 0x82382EDC` — la signature de
troncature de ce cycle. Elle est levée : la fonction porte maintenant un corps
complet, `[0x8237CC58, 0x8237D1B3]`, soit **1372 octets**.

Deux adresses citées par les cycles 460 et 514 se requalifient :

| adresse | ce qu'on en disait | ce qu'elle est |
| --- | --- | --- |
| `0x8237CEF0` | « le caller PAL », « le `bctrl` à `0x8237CEF0` » | **intérieure** à `0x8237CC58`, à `+0x298` : l'instruction `subic. r30,r30,0x1`, adresse de **retour** du `bctrl` de `0x8237CEEC` |
| `0x8237D0FC` | « l'appel invité à `0x8237D0FC` » | adresse de **retour** du `bl 0x8237CC58` de `0x8237D0F8` — un appel **récursif** |

`0x8237CEEC` est un appel indirect via une table indexée par
`*(u32 *)*(état + 0xF8)`, mise à l'échelle par 4 sur la base `r23`. Et
`0x8237D0F8` boucle sur `r30 = *(r30 + 0x18)` : `0x8237CC58` se rappelle
elle-même le long d'une liste chaînée, ce qui confirme par le listing la lecture
du cycle 460 — « il parcourt récursivement les timelines enfants » — que ce
cycle avait dû faire sur un listing vide.

Ce que le corps montre par ailleurs, sans que ce soit revendiqué au-delà : un
curseur en `+0xD8` borné par `(*(état+0x2C) − *(état+0x28)) >> 3`, une
transformation de huit flottants composée en `+0x140..+0x15C` depuis
`+0x40..+0x5C`, et deux tables de pointeurs (`PTR_LAB_8267A1D0`,
`PTR_LAB_8267A208`) indexées par une étiquette de type.

**Aucun run N3 n'est engagé**, et aucune conclusion des cycles 460/514 n'est
promue : ce qu'on gagne est que la prochaine session bridge, si elle a lieu,
partira d'un listing réel plutôt que d'un vide.
