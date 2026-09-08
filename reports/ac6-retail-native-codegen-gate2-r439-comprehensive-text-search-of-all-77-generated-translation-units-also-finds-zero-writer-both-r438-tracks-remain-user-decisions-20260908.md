# AC6 retail NTSC-U/J — r439 — recherche textuelle exhaustive des 77 unités de traduction générées confirme aussi zéro écrivain pour `0x82935D98` ; CORRECTION : r437 avait interrogé le mauvais projet Ghidra (XEX non qualifié) — refait contre le bon projet (`ac6-us`), même conclusion — les deux pistes ouvertes restent des décisions/travail utilisateur

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r438 : les deux pistes nommées par r437 dépendent toutes deux
d'une décision utilisateur (coût d'une auto-analyse Ghidra ciblée ;
décision de committage de l'arriéré `native_vulkan_backend.cpp`).
Avant de s'arrêter sur ces deux blocages, vérification d'une piste
supplémentaire qui ne dépend d'aucune des deux : une recherche
textuelle de l'écrivain manquant à travers TOUTES les unités de
traduction générées par XenonRecomp, pas seulement le sous-ensemble
que le projet Ghidra stocké a effectivement désassemblé.

## Établi

`ls .../codegen-20260831-mapfix-96838/generated/ppc_recomp.*.cpp` :
**77 fichiers**. XenonRecomp répartit la traduction de la totalité de
la section `.text` du XEX par taille de fichier, pas par
atteignabilité — ces 77 fichiers représentent donc, sauf preuve
contraire, la quasi-totalité du code PPC compilable du binaire retail,
un ensemble bien plus large que ce que le projet Ghidra stocké avait
individuellement désassemblé (r437 : `sub_821D6C20` elle-même n'y
était pas même définie comme fonction).

`grep -rl "23960" .../generated/ppc_recomp.*.cpp` : **5 fichiers**
seulement contiennent la chaîne littérale `23960` — le seul décalage
immédiat naturel pour cette adresse avec le registre haut correspondant
(`0x82935D98 = 0x82930000 + 0x5D98`, `0x5D98 = 23960 < 0x8000`, donc
un seul encodage immédiat possible, pas d'ambiguïté haut/bas comme il
en existerait si le décalage dépassait `0x8000`). Les 5 fichiers ont
déjà été inspectés (r436) : **toutes les occurrences sont des
`PPC_LOAD_U32`, aucune `PPC_STORE_U32`/`PPC_STORE_U64`**, et toutes
regroupées dans `sub_821D6C20` elle-même (lignes ~20317-21178 d'un
seul fichier, `ppc_recomp.23.cpp`).

**Ceci est une recherche complémentaire à celle de r437, par une
méthode indépendante** (lecture textuelle du code généré plutôt que
la base de données de références de Ghidra) et qui couvre un ensemble
de fonctions bien plus large. Elle aboutit à la même conclusion :
aucun site d'écriture par adressage immédiat direct n'existe nulle
part dans le code compilable atteignable.

## Correction méthodologique trouvée en cours de route : r437 avait interrogé le MAUVAIS projet Ghidra

En reconsidérant si l'auto-analyse Ghidra ciblée nommée par r437 était
vraiment aussi coûteuse que présenté (`-readOnly` ne persiste aucune
modification — le risque réel n'est que le temps de calcul d'une
invocation, pas un changement durable de l'état stocké), une
vérification de l'intégrité du binaire interrogé a révélé un problème
distinct et plus important : **`game-files/default.xex`** (celui que
le projet Ghidra `ace-combat-6` référence, utilisé par r437) a pour
SHA-256 **`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`**
— **différent** du XEX retail qualifié
(`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`)
utilisé par TOUS les rapports de cette chaîne (r413-r439), et par le
véritable binaire construit
(`build/ntsc-uj/source/assets/default.xex`, vérifié identique dans
chacun des 12 répertoires `codegen-*/assets/` du build).
`game-files/default.xex` est un ancien exemplaire non synchronisé,
gardé pour référence, pas le binaire réellement qualifié.

**Le bon projet Ghidra pour ce XEX qualifié est `ac6-us`**
(`PROGRAM_EXECUTABLE` pointe vers
`build/ntsc-uj/source/assets/default.xex`, le chemin correct) — pas
`ace-combat-6` (celui interrogé par r437). Les deux mêmes recherches
ont été **refaites contre `ac6-us`** :
`FindReferencesToRange.java 0x82935d98 0x82935d9c` → **0 référence**
(identique) ; `DumpFunction.java 0x821D6C20` → **aucune fonction
définie**, même comportement de repli vers `0x821d72xx` (identique).
**La conclusion de r437 tient**, vérifiée cette fois contre le bon
binaire — mais la méthodologie était fausse jusqu'ici et est corrigée
pour tout cycle futur : utiliser `ac6-us`, jamais `ace-combat-6`, pour
cette chaîne d'investigation retail qualifiée.

## Non établi

- **Un écrivain par adressage indexé/calculé** (par exemple une table
  de pointeurs où cette adresse serait un slot dont l'offset est
  calculé à l'exécution plutôt que codé en dur comme immédiat) —
  une recherche textuelle ne peut pas détecter ce cas, puisque le
  code généré ne contiendrait alors aucune occurrence littérale de
  `23960` ni de l'adresse elle-même. Ni cette recherche ni celle de
  r437 (Ghidra) ne peuvent trancher cette possibilité.
- **Si les 77 fichiers couvrent réellement 100 % du binaire** — très
  probable vu le modèle de répartition de XenonRecomp (par taille de
  fichier, pas par atteignabilité), mais non revérifié indépendamment
  ce cycle (par exemple en comparant la taille cumulée du texte généré
  à la taille de la section `.text` du XEX).

## Ce que ceci établit

**Aucune nouvelle piste exploitable n'existe sans l'une des deux
décisions déjà nommées.** Cette recherche textuelle élargit la
couverture (77 fichiers contre le sous-ensemble non défini de r437)
mais confirme le même résultat par une méthode différente — elle ne
lève pas le blocage, elle le renforce. L'hypothèse de l'adressage
indexé/calculé reste la seule piste théoriquement encore vivante, et
aucun outil actuellement disponible sans coût supplémentaire (Ghidra
non analysé pour cette région, texte généré qui ne l'aurait pas
révélé) ne peut la trancher.

## Décisions prises

- Ne pas inventer de nouvelle méthode d'investigation sans les
  ressources qu'elle nécessiterait (Ghidra, ou une refonte du chemin
  `VdSwap` déjà déférée) — rapporter honnêtement que le cycle n'a rien
  trouvé de nouveau à corriger plutôt que de forcer une action.

## Gate

Aucune source de production éditée ce cycle (recherche textuelle en
lecture seule sur du code déjà généré).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées depuis r438 — aucune source
affectée)
`ctest` (racine et natif) inchangé depuis r438 (aucune source
modifiée).

## Named for r440

Toujours les deux mêmes pistes, aucune troisième n'a été trouvée :
1. Désassembler réellement `sub_821D6C20` dans le projet Ghidra
   correct (`ac6-us`, PAS `ace-combat-6` — voir correction
   méthodologique ci-dessus) pour permettre une vraie recherche
   statique de l'écrivain. Ceci nécessite un script d'amorçage de
   désassemblage NON `-readOnly` (persistant, modifie le projet
   stocké) — le script existant de ce type dans le dépôt
   (`SeedPdataExtentDisassembly.java`) est verrouillé au SHA-256 de
   l'ancien XEX (`acc302c1...`, projet `ace-combat-6`), pas au XEX
   qualifié (`6eefba42...`, projet `ac6-us`) : il ne s'applique pas
   tel quel. Une piste réelle mais qui nécessite d'écrire ou d'adapter
   un script pour le bon projet — plus de travail qu'une simple
   invocation en lecture seule, mais borné.
2. Décision de committage de l'arriéré `native_vulkan_backend.cpp`
   (r433/r434/r438).

## Files

Aucun artefact gitignoré nouveau.
