# AC6 retail NTSC-U/J — r464 — arriéré du sous-module `AC6_recomp` committé (décision utilisateur explicite) ; la campagne oracle reste bloquée par un DEUXIÈME obstacle, plus profond : le code généré NTSC-U/J du profil `rexglue-oracle` est un artefact déjà consommé et non reproductible dans ce bac à sable

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle tenté, non abouti ce cycle (voir ci-dessous).

## Contexte

Nommé par r463, avec décision utilisateur explicite : committer
l'arriéré du sous-module `AC6_recomp` en l'état pour débloquer
`prepare.py --profile rexglue-oracle`.

## Établi — l'arriéré committé, aucun secret, décision appliquée

`recompilation/ace-combat-6-retail/upstream/AC6_recomp` : 23 fichiers
modifiés + 1 non suivi (`src/ac6_world_submission_owner_probe.cpp`,
une sonde de diagnostic bornée lisible, sans rapport douteux) inspectés
(`git diff --stat`, recherche de motifs de secrets — aucun résultat),
committés tels quels : commit `6cf743269f5d4ea635b32e5b24369124fd25067b`
(« AC6 ReXGlue oracle backend fixes: deswizzle, hoisted gradients,
texture cache, and world-submission owner probe »). Pointeur de
sous-module du dépôt parent avancé au commit `abf6513a`.

**Conséquence directe repérée et corrigée** : `tools/prepare.py` et
`tools/build.py` portent chacun leur PROPRE constante
`UPSTREAM_COMMIT` (le pin du commit sous-module attendu), toutes deux
encore sur l'ancien `09144bb0...` après le commit du sous-module —
`prepare.py --profile rexglue-oracle` aurait échoué avec « AC6_recomp
submodule commit mismatch » sans correction. Mises à jour vers
`6cf743269f5d4ea635b32e5b24369124fd25067b` dans les deux fichiers, plus
`README.md` (même chaîne documentée en prose). `prepare.py
--profile rexglue-oracle` réussit alors (manifeste régénéré,
`build/ntsc-uj/manifest.json`).

## Établi — un DEUXIÈME blocage, distinct et plus profond

`tools/build.py --target ntsc-uj --profile rexglue-oracle` échoue à
l'étape de compilation elle-même, PAS au niveau de la vérification de
manifeste : la configuration CMake réussit intégralement (ReXGlue,
SDL3, glslang, SPIRV-Tools tous configurés), puis :
```
build.py: error: NTSC-U/J code generation is already consumed;
generated sources required
```
Ce garde-fou (`tools/build.py`, juste après le premier `run(configure)`
du profil `rexglue-oracle`) exige `source/generated/sources.cmake`
(la sortie CMake-intégrée de la cible `ac6recomp_codegen`, un run
XenonRecomp piloté par Ghidra) et **refuse explicitement de la
régénérer pour `ntsc-uj`** (`if arguments.target == "ntsc-uj":
parser.error(...)`) — seul `pal` peut relancer `ac6recomp_codegen`.
`tools/prepare.py` exclut délibérément `generated/` de sa
matérialisation (`ignore=shutil.ignore_patterns(".git", "generated",
"assets", "out")`), donc CHAQUE préparation fraîche du profil
`rexglue-oracle` perd ce fichier — il ne peut provenir que d'un
artefact archivé ailleurs.

**Aucune copie de cet artefact n'existe dans ce bac à sable** :
recherche exhaustive de `sources.cmake` (introuvable), de tout
répertoire `codegen-*` sous `build/ntsc-uj/` pour le profil
`rexglue-oracle` (seuls les répertoires `native/codegen-*` du profil
`native` existent — format et outillage complètement différents,
non réutilisables ici), et de tout artefact d'archivage sous
`artifacts/` portant ce nom pour ce profil (les `artifacts/*codegen*`
existants appartiennent tous au pipeline de codegen du profil
`native`, sans rapport).

Le binaire oracle déjà compilé existe toujours
(`build/ntsc-uj/cmake/ac6recomp`, 30 août 13h24) et est probablement
TOUJOURS VALIDE (le sous-module portait déjà ces mêmes modifications
sur disque, non committées, au moment de cette compilation — les
committer maintenant ne change aucun octet de fichier). Mais
`run_gate.py` exige `static-validation.json` cohérent avec le
manifeste courant (`validate_static_receipt`), et ce fichier de
validation statique daterait d'avant le commit du sous-module — sa
régénération nécessite l'étape de validation qui suit normalement un
`build.py` réussi, qui reste bloquée par ce même artefact manquant.

## Décisions prises

- **Ne PAS contourner le garde « already consumed »** pour `ntsc-uj`
  en modifiant `build.py` pour forcer `ac6recomp_codegen` sur ce
  cible — ce garde-fou existe manifestement pour traiter cette
  génération de code comme une ressource rare/coûteuse déjà dépensée
  (cohérent avec la discipline `CLAUDE.md` sur les blocages
  qualifiés) ; le contourner reviendrait à dépenser une ressource sans
  décision explicite, exactement le type d'action que ce cycle doit
  éviter.
- **Restaurer intégralement l'état du profil `native`** : seul
  `build/ntsc-uj/manifest.json` (chemin partagé) avait été écrasé par
  les deux tentatives `prepare.py --profile rexglue-oracle` de ce
  cycle — restauré depuis la sauvegarde prise avant toute tentative
  (SHA-256 identique après restauration : `8b169dc4...`).
  `static-validation.json` n'avait jamais été touché (SHA-256
  inchangé : `c1dbbc31...`). **Vérifié en direct** : `ctest` natif
  11/11, lancement réel contre l'ISO qualifiée —
  `presented_frames=5 state=2`, capture `pixels=921600 non_black=0
  distinct_colors=1` — identiques à la ligne de base de tous les
  cycles r454-r462. Le profil `native` n'a subi AUCUNE régression.
- Les répertoires de travail `build/ntsc-uj/cmake/`,
  `build/ntsc-uj/source/` (régénérés par les tentatives
  `rexglue-oracle` de ce cycle, gitignorés, jetables) ont été laissés
  en l'état — aucun risque, une future tentative peut les réutiliser
  ou les régénérer.

## Non établi

- **D'où provenait `source/generated/sources.cmake`** lors de la
  compilation réussie du 30 août — aucune trace de ce run ni de son
  artefact archivé n'a été trouvée dans ce bac à sable.
- **Si le binaire oracle existant (30 août) reste utilisable tel
  quel** pour `run_gate.py` malgré un `static-validation.json`
  désormais périmé par rapport au manifeste — nécessiterait soit de
  reconstruire ce fichier de validation par un autre moyen que
  `build.py`/`validate.py` (non identifié ce cycle), soit une décision
  explicite d'accepter un léger défaut de fraîcheur documentaire du
  binaire (le contenu du binaire lui-même est probablement inchangé).

## Gate

**Native profile vérifié sans régression** (voir ci-dessus). Gates de
dépôt exécutés pour clôturer le cycle :
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
`ctest` racine confirmé inchangé (seul l'échec préexistant connu
`ac6-cpp-complexity`).

## Named for r465

**Deuxième blocage qualifié nommé, décision utilisateur requise** :
comment procéder face à l'absence de `source/generated/sources.cmake`
pour le profil `rexglue-oracle` sur `ntsc-uj`, sachant que
`build.py` refuse délibérément de le régénérer pour cette cible.
Options à trancher par l'utilisateur, aucune prise seule ce cycle :
(a) autoriser explicitement une régénération `ac6recomp_codegen` pour
`ntsc-uj` (lever le garde-fou, dépense potentielle de ressource Ghidra/
temps non quantifiée ce cycle) ; (b) rechercher un archivage externe à
ce bac à sable (autre session, sauvegarde) de cet artefact ; (c)
tenter d'utiliser le binaire oracle déjà compilé du 30 août tel quel,
en construisant `static-validation.json` par un autre moyen que le
pipeline standard ; (d) abandonner la piste oracle pour cette
campagne. Reste ouvert, non bloquant : (1) une fois le câblage
`PinnedShaderRuntime` jugé mûr, reconsidérer le committage groupé de
l'arriéré natif (r433/r434/r438/r454-r462, toujours distinct de
celui du sous-module `AC6_recomp` déjà committé) ; (2) mise à jour de
`tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut du
profil natif (confort, pas une nécessité).

## Files

Sauvegardes temporaires sous `/fastdata/lavaulta/tmp/ac6-native-manifest-backup-r464/`
(manifest.json, static-validation.json, logs de build), non
conservées comme preuve committée. Aucun artefact gitignoré nouveau
retenu comme preuve.
