# AC6 cycle 241 — qualification du bundle MaterialBind et ancre MATE XEX

Date : 2026-07-19

## Question

Le bundle `ac6_material_bind_xex_boundary_v1.zip` qualifie-t-il une frontière
`MaterialBind` utilisable, et quelle nouvelle frontière statique peut être
fermée contre le projet Ghidra canonique ?

## Intégrité du bundle

- archive : `ac6_material_bind_xex_boundary_v1.zip` ;
- SHA-256 compressé :
  `7acf3070750c9e7ac0aebf9fc37d1c3112adf859c946f3ac23d1346aaee5d0f6` ;
- 16 entrées ZIP, dont 13 fichiers sous une racine unique ;
- digest de contenu déterministe :
  `a7af274b1c9e219765ee16106a3fbd18d05dd2a1309216dbfc879dcdb1156ade` ;
- `unzip -t`, chemins sûrs, absence de doublons et tous les membres déclarés
  par `SHA256SUMS` : **PASS** ;
- test synthétique exécuté directement : **9/9 PASS** ;
- aucun XEX, PAC, cache shader ou autre asset retail n'est inclus.

`unittest discover` ne découvre aucun test dans la disposition livrée. Le
fichier `tests/test_material_boundary.py` est donc exécuté directement ; ses
neuf scénarios couvrent l'acceptation exacte et les rejets missing, zero,
alias, collision, état/contexte divergents, draw backend absent et réentrance.

## Résultat conservé

Le résultat global `NO_QUALIFIED_MATERIAL_BIND` est conservé. Le corpus joint
ne démontre aucun chemin statique complet :

```text
identité MATE -> technique/passe/permutation -> draw causal
```

Le contrat de breadcrumb est utile comme garde fail-closed, mais il ne doit pas
être branché au runtime tant que son producteur XEX n'est pas qualifié.

## Correction d'un candidat du bundle

La ligne C07 du tableau joint qualifie à tort `0x82118A50` comme helper d'état
ou de draw et attribue un draw direct à `0x82118A68`. Le projet Ghidra
canonique, qualifié par le XEX PAL
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`,
montre au contraire :

- à `0x82118A68` : `lbz r11,0xa(r6)`, pas un appel draw ;
- `0x82118A50` reloge un bloc de records de 0x20 octets ;
- ses descendants `0x82119740`, `0x82119DD8` et `0x82119F68` traitent des
  records de courbes/interpolation et sélectionnent des fonctions par codes
  scalaires.

C07 est donc rejeté comme parser ou bind MATE. Le verdict négatif global reste
compatible avec cette correction, mais la table des candidats ne doit pas être
utilisée sans requalification dans le projet canonique.

## Nouvelle ancre statique MATE

Un balayage headless des scalaires du layout natif MATE a trouvé
`0x823330F0`. La fonction recoupe exactement le format déjà qualifié :

- compteur principal u16 à `+0x04` ;
- tables à `+0x0C`, `+0x10` et `+0x14` ;
- records principaux de 0x10 octets ;
- compteur u16 à `record+0x0A` ;
- sous-records de 0x18 octets à partir de `record+0x20` ;
- drapeau de fixation à `+0x20`.

Lorsque le bit 0 du drapeau est présent, la routine remet à zéro les handles
runtime, retire la base du fichier aux pointeurs internes et efface le drapeau.
Elle est donc classée **`probable` défixeur/dérelocateur MATE**, et non bind
GPU. Cette sémantique est soutenue par la concordance complète du layout et
doit encore être verrouillée par l'identification de la routine inverse.

Deux branches brutes l'atteignent, à `0x821C153C` et `0x821C1724`. Elles
appartiennent à des handlers dont les limites ne sont pas correctement créées
par Ghidra. Le second handler parcourt un conteneur typé et appelle
`0x823330F0` uniquement pour le type `0x0C`. Cela qualifie **probablement**
`0x0C` comme type de ressource MATE dans ce dispatcher. Les autres cases du
switch ne sont pas renommées.

Cette ancre est plus utile que le scan précédent : la prochaine recherche doit
partir de la routine inverse de fixation et des consommateurs du type `0x0C`,
puis suivre le propriétaire runtime jusqu'à technique/passe/permutation. Elle
ne doit pas repartir d'un scan global des draws ou du cache Xenia.

## Checkout runtime actuel

Le checkout local AC6Recomp existe à
`c5b089fb6988ac504ba394db611543bda2fb2c96`; le bundle ne l'avait pas à
disposition et n'émet donc aucun patch applicable. Une configuration avec le
preset d'origine échoue car il impose `clang++-20`, absent. L'override vers le
Clang installé 21.1.8 franchit cette étape puis s'arrête sur la dépendance de
développement `gtk+-3.0`, absente de `pkg-config`.

Aucun paquet n'a été installé, aucun patch runtime n'a été appliqué et aucun
second sink de trace n'a été créé. Cette dépendance de build n'est pas un
blocage demandant une action humaine : la recherche statique autonome reste
ouverte.

## Décision et prochaine action

- verdict négatif du bundle : **KEEP_WITH_CLARIFICATION** ;
- contrat fail-closed et tests synthétiques : **KEEP** ;
- C07 / draw à `0x82118A68` : **OBSOLETE**, contredit par le XEX canonique ;
- `0x823330F0` : nouvelle ancre **probable** de défixation MATE ;
- type de ressource `0x0C` : **probable**, borné au dispatcher observé ;
- patch runtime : interdit tant que le producteur causal n'est pas qualifié.

Prochaine action autonome : identifier la routine inverse de `0x823330F0`,
les propriétaires du type `0x0C` et le premier consommateur qui conserve
l'identité MATE jusqu'à la sélection technique/passe/permutation. Aucune
session Xenia, VNC ou intervention humaine n'est requise.

Le corpus natif AC6 existant est reconstruit avec `-j16` puis passe
**44/44** en 34,00 s. Cette validation protège l'état courant ; elle ne
transforme pas l'ancre statique probable en preuve dynamique.

## Commandes exécutées

```text
sha256sum ac6_material_bind_xex_boundary_v1.zip
unzip -t ac6_material_bind_xex_boundary_v1.zip
python3 tests/test_material_boundary.py
analyzeHeadless ... -postScript FindFunctionsWithScalarSet.java 0xa 0x18 0x20
analyzeHeadless ... -postScript FindFunctionsWithScalarSet.java \
  0x4 0xa 0xc 0x10 0x14 0x18 0x20
analyzeHeadless ... -postScript DecompileAt.java 0x823330f0
analyzeHeadless ... -postScript FindPpcRawBranchesTo.java 0x823330f0
analyzeHeadless ... -postScript DumpRange.java 0x821c14c0 0x821c1780
git -C .tools/ac6-recomp-reference rev-parse HEAD
pkg-config --exists gtk+-3.0
cmake --build .build/ace-combat-6 -j16
ctest --test-dir .build/ace-combat-6 --output-on-failure -j16
```

Toutes les opérations Ghidra ont été exécutées en lecture seule, sans analyse
et sans GUI.
