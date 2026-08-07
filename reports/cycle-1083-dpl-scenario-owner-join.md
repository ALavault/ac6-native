# Cycle 1083 — le join DPL → `DATA.TBL` et le propriétaire du scénario Mission 01

Date : 2026-08-07. Suite directe du cycle 1082, sur le corpus réparé.

## Qualification

- Projet Ghidra canonique : `ghidra-projects/ace-combat-6`.
- Target : Xbox 360 PAL, `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- `DATA.TBL` SHA-256 `82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5`.
- Charge utile analysée : entrée 9 décodée,
  `reports/logs/cycle-739-pac-mission-gate/pac/entry_9_mode1_c13234234_u42446032_off1028000.bin`,
  42 446 032 octets,
  SHA-256 `cd81e02189516cb5ba0c08d41659a90ae927fe2eccdad53cf5216db44b6d7a05`.
- Nœud racine du scénario : enfant 0 de l'entrée 9,
  `.../fhm/idx_0009/000_00_00_00_10.bin`, 3 477 248 octets,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- Aucune exécution d'émulateur, de bridge ou du produit natif. Statique et
  octets seulement.

## 1. Le join, instruction par instruction

Le site `DPL::[%#x,%#x]` de `0x8219BDD8` se lit désormais en entier :

```
8219be94  lwz   r11,0x4eb4(r21)      ; PTR_DAT_826E4EB4
8219be98  addi  r3,r11,0x70          ; + 0x70  -> current_level_runtime
8219be9c  bl    0x820943B0           ; getter de niveau
8219bea0  bl    0x821B6E58           ; niveau -> identifiant de ressource
8219bea4  lis   r11,-0x7dfa
8219bea8  or    r5,r3,r3             ; premier %#x = identifiant
8219beac  addi  r14,r11,0x7b00       ; 0x82067B00 = "DPL::[%#x,%#x]"
8219beb0  addi  r3,r1,0xa0           ; tampon char[512]
8219beb4  or    r4,r14,r14
8219beb8  li    r6,0x0               ; second %#x = 0
8219bebc  bl    0x823800D0           ; sprintf
```

`0x821B6E58` est l'une des six fonctions déjà relues à la main
(`reports/code/0x821b6e58__Function_821B6E58.cpp`). Hors modes 3 et 4, elle
indexe `DAT_82065840` avec le niveau borné à `0..0x0F`. Les trois tables, lues
dans `.rdata` :

| table | mode | entrées | valeurs |
| --- | --- | ---: | --- |
| `DAT_82065840` | campagne / défaut | 16 | **51**, puis **9, 10, 11 … 23** |
| `DAT_82065880` | 4 | 14 | 31, 31, 34, 35, 36, 37, 38, 41, 42, 43, 44, 45, 48, 49 |
| `DAT_820658B8` | 3 | 8 | 24, 24, 25, 26, 27, 28, 29, 30 |

Toutes ces valeurs sont `< 0x39d`, donc elles empruntent la branche directe de
`0x821D1128` décrite dans `DPL_ARCHIVE_HANDLE_CHAIN.md` : **l'identifiant DPL
est l'index physique `DATA.TBL`**.

La campagne donne donc quinze missions consécutives, entrées **9 à 23**, à
partir du sélecteur 1 — et `DAT_82065840[1] = 9` reproduit exactement la borne
« sélecteur 1 → identifiant DPL 9 → index physique 9 » établie par la chaîne
d'archive, par une route indépendante.

**Mission 01 = `DATA.TBL` entrée 9.**

## 2. Le nœud de scénario et sa table de dispatch

`0x82249718` est le lecteur de scénario racine, `0x822493F0` sa fonction de
dimensionnement jumelle. Le format de conteneur, lu dans la décompilation :

```
noeud+0 : u32 offset d'en-tête    (relatif au noeud, 0 = absent)
noeud+4 : u32 offset de table     (relatif au noeud)
table+0 : s32 count
table+4 : u32 offset[count]       (relatifs à la table)
```

Un enfant est vide si ses deux premiers `u32` sont nuls — le test exact que
fait le code avant chaque slot.

Appliqué aux octets réels de l'enfant 0 de l'entrée 9, le parcours est
cohérent de bout en bout :

| slot | `read` / `getReadBuffSize` | pas | enfants | contenu |
| ---: | --- | ---: | ---: | --- |
| 0 | `0x82309D20` / `0x82309C00` | 0x08 | **230** | **Obj & Unit** |
| 1 | `0x82309A88` / `0x82309978` | 0x0C | 339 | non nommé |
| 2 | `SubMisTblBin` `0x82309758` / `0x82309620` | 0x10 | **4** | sous-missions |
| 3 | **`RadioTblBin`** `0x823094D8` / `0x823093C8` | 0x10 | **54** | radio |
| 4 | `0x823092E0` / `0x823092A0` | 0x10 | 0 | **vide** |
| 5 | — / `0x82309120` | 0x08 | 4 | non nommé |
| 6 | — / `0x82308F18` | 0x14 | 0 | **vide** |
| 7 | — / `0x82308F50` | 0x14 | 3 | non nommé |
| 8 | — | — | 2 | hors table de dispatch |
| 9 | — | — | 5 | hors table de dispatch |

Les slots 4 et 6 sont vides dans les octets, et le code teste précisément cette
vacuité : la structure décompilée et la charge utile concordent.

## 3. La famille de parseurs se nomme toute seule

Les références rétablies au cycle 1082 permettent d'associer chaque fonction
aux chaînes d'erreur qu'elle matérialise. **19 fonctions s'identifient ainsi
sans aucune supposition d'adresse** :

```
0x823093C8  RadioTblBin::getReadBuffSize      0x823094D8  RadioTblBin::read
0x82309620  SubMisTblBin::getReadBuffSize     0x82309758  SubMisTblBin::read
0x8232C7E0  SubMisBin::getReadBuffSize        0x8232C8A8  SubMisBin::read, MapmaskBin::read
0x8232F4D0  SetBin::getReadBuffSize           0x8232F5F8  SetBin::read
0x8232FF78  ObjBin::getReadBuffSize           0x82330158  ObjBin::read, DurableBin::read, WeaponBin::read
0x82330540  ActBin::getReadBuffSize           0x82330688  ActBin::read
0x823310E8  OrderBin::getReadBuffSize         0x82331208  OrderBin::read + les 6 Order*Bin::read
0x823316A0  ManeuverBin::getReadBuffSize      0x82331808  ManeuverBin::read, ComTblMBin::read
0x82331BB0  ComTblBin::getReadBuffSize        0x82331C10  ComTblBin::read
0x82331E78  ComBin::read
```

Deux corrections à la liste de candidats non suivie `tools/ac6_parser_inspect.py` :

- `0x823094D8`/`0x823093C8` sont **`RadioTblBin`**, pas des candidats
  incertains — ils étaient déjà listés comme « pistes non qualifiées » dans
  `analysis/address_catalog.tsv` et sont maintenant nommés par le binaire ;
- `ComBin::read` est à **`0x82331E78`**, pas à `0x82331D98`.

## 4. Le propriétaire cherché depuis les cycles 1070 et 1073

Parcours d'atteignabilité sur le graphe d'appel, profondeur 6 :

| départ | parseurs nommés atteints |
| --- | --- |
| slot 0 `0x82309D20` | **ObjBin, SetBin, ActBin, OrderBin, ManeuverBin** (67 fonctions visitées) |
| slot 1 `0x82309A88` | aucun |
| slot 3 `RadioTblBin::read` | aucun (sous-arbre propre) |
| `SubMisBin::read` | aucun |

Le slot 0 est donc bien le bloc que le chargeur nomme lui-même **`Obj & Unit`**,
et il porte l'arbre de comportement `Set → Act → Order{Disappear, Stop, Lead,
Jump, Flag, Property}` ainsi que `Obj → {Durable, Weapon}`.

> **Précision apportée par le cycle 1084.** Les 230 sont les entrées de
> *niveau 0* du slot 0. Chacune contient de zéro à plusieurs enregistrements
> `ObjBin`, pour un total de **434**. La coïncidence numérique ci-dessous porte
> donc sur les entrées de niveau 0, pas sur les enregistrements `Obj`.

**Ses 230 enfants correspondent exactement aux 230 objets du census runtime du
cycle 1080** (`0x822707C8`, histogramme `0x820568D4:1`, `0x82009AB0:1`,
`0x82009440:228`). Le cycle 1080 concluait que ces 230 objets ne recevaient
« aucun nom sémantique par ordre, adresse ou plausibilité ». Ils en reçoivent
un ici : ce sont les 230 enregistrements du slot 0, lus par une famille de
parseurs décompilée et nommée par le binaire.

## 5. Portée exacte de ce cycle

Ce cycle **établit** :

- la chaîne niveau → `DAT_82065840` → identifiant DPL → index physique
  `DATA.TBL`, et donc Mission 01 = entrée 9, par une route indépendante de la
  chaîne d'archive ;
- le format de conteneur et la table de dispatch, vérifiés sur les octets ;
- l'identité de 19 parseurs, prouvée par leurs propres chaînes d'erreur ;
- la localisation du bloc `Obj & Unit` et sa cardinalité 230, égale au census
  runtime.

Il **n'établit pas** :

- le schéma de champ d'un enregistrement `Obj` ou `Order` — seule la
  structure de conteneur est lue, pas la disposition interne des
  enregistrements de 0x08 / 0x0C / 0x10 / 0x14 octets ;
- la correspondance individuelle entre un enfant du slot 0 et un objet du
  census runtime : la cardinalité coïncide, l'appariement un à un n'est pas
  prouvé ;
- l'identité de vague ni de condition d'objectif. `retail_units_and_waves` et
  `retail_objectives` restent **ouverts** dans le contrat v2.

## Prochaine tranche

Extraire la disposition de champ de `ObjBin::read`, `ActBin::read` et
`OrderBin::read` vers `analysis/scenario-schema/`, puis micro-exécuter chacun
sur ces octets et comparer au parseur natif avec
`tools/compare_ac6_function_snapshots.py`. C'est la preuve `microexec` que la
porte v2 réclame, toujours sans oracle.

Ne pas rejouer : la question « où sont les données de scénario Mission 01 ? »
est close par ce cycle. Ne pas relancer de run bridge pour nommer les 230
objets ; leur source statique est identifiée.
