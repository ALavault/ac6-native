# AC6 retail NTSC-U/J — r463 — la campagne oracle nommée par r462 (étendre la couverture du registre épinglé) est bloquée avant même de lancer une route : le manifeste `rexglue-oracle` partagé a été écrasé par le profil `native`, et le sous-module `AC6_recomp` porte un arriéré non committé qui bloque `prepare.py`

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle tenté, non abouti ce cycle (voir ci-dessous).

## Contexte

Nommé par r462, avec décision utilisateur explicite : étendre la
couverture du registre de nuanceurs épinglé (271 variantes, r456) via
une nouvelle session oracle ReXGlue — le seul motif de rejet restant
après la chaîne de correctifs mécaniques r457-r462.

## Établi — l'outil correct, la mauvaise piste évitée

`tools/ac6-oracle-run.py` et les lanceurs Xenia/Wine
(`scripts/run_xenia_ac6_oracle_baseline.sh`,
`scripts/launch_xenia_ac6_wine.sh`) sont gardés par le XEX PÉRIMÉ
`acc302c1...` (déjà identifié comme erroné par r439) — écartés. L'outil
correctement gardé par le XEX qualifié (`6eefba42...`, vérifié ligne
431 de `run_gate.py`) est
`recompilation/ace-combat-6-retail/tools/run_gate.py`, qui pilote le
véritable binaire oracle « hybrid_backend_fixes » (ReXGlue/moteur
Vulkan réel lié statiquement — le même binaire qui a produit le dump
de 253 nuanceurs de r254/r255). La route par défaut de Mission 01
(`routes/mission01-qualified-96-longstart-v2-candidate.steps`) va bien
plus loin que le dump D5B4 original : elle atteint
`[ac6-visual-phase] cinematic=0 world=1 hud=1 stable=30` (HUD de vol
réel) puis exerce individuellement tangage/roulis/lacet/accélération/
freinage — un candidat plausible pour couvrir de nouveaux états de
tirage au-delà des 271 déjà épinglés.

## Établi — pourquoi la campagne n'a pas pu démarrer

1. **Manifeste partagé écrasé** : `build/ntsc-uj/manifest.json` et
   `static-validation.json` sont un chemin PARTAGÉ entre les DEUX
   profils (`native` et `rexglue-oracle`) — `tools/build.py`/
   `tools/prepare.py` écrivent tous deux à `build/<target>/manifest.json`,
   pas à un sous-répertoire par profil. Les horodatages montrent que le
   binaire oracle (`build/ntsc-uj/cmake/ac6recomp`) a été compilé avec
   succès le 30 août à 13h24, mais qu'une préparation/compilation
   ultérieure du profil `native` (30 août, 21h13-21h17 — celle que
   cette session entière r399-r462 utilise) a écrasé le manifeste
   partagé avec le contenu du profil `native`. `run_gate.py` refuse
   alors de démarrer (`AttributeError` sur un champ `null` du mauvais
   schéma de manifeste).
2. **Sous-module `AC6_recomp` non propre** :
   `python3 tools/prepare.py --profile rexglue-oracle --target ntsc-uj
   --xex <XEX qualifié> --iso <ISO qualifiée>` (les deux chemins ont
   été vérifiés : XEX SHA-256 `6eefba42...` confirmé par
   `sha256sum` sur `/fastdata/.../.tools/ac6-recomp-us-assets-20260813/
   default.xex`) échoue immédiatement avec « AC6_recomp submodule must
   be clean ». `git status` dans
   `recompilation/ace-combat-6-retail/upstream/AC6_recomp` montre un
   arriéré substantiel et clairement délibéré (23 fichiers modifiés,
   1 non suivi) touchant précisément les fichiers du moteur de rendu
   ReXGlue (`spirv_translator.cpp`, `command_processor.cpp`,
   `render_target_cache.cpp`, `texture_cache.cpp`…) et les correctifs
   « AC6 » déjà actifs dans le log du run D5B4 de r254
   (`scaling=true deswizzle=true dof=true water_line=true`) — un
   travail réel accumulé, pas une modification accidentelle à
   discarder.

## Décisions prises

- **Ne PAS forcer le sous-module à un état propre** (`git stash`,
  `git checkout --`, ou équivalent) : ce serait une action
  destructrice sur un arriéré substantiel et manifestement
  intentionnel, sans comprendre sa provenance ni sa nécessité pour le
  profil `rexglue-oracle` — exactement le type de décision que la
  discipline de ce dépôt réserve à une décision explicite, pas à
  prendre unilatéralement dans un cycle d'investigation.
- **Ne PAS reconstruire manuellement l'invocation Vulkan/Xvfb/xdotool**
  de `run_gate.py` en contournant ses vérifications — un contournement
  fragile qui produirait une capture non vérifiable par la même
  discipline que le reste de cette campagne (et le type d'erreur que
  cycle 1457 avait déjà payé cher : reconstruire une méthodologie de
  mémoire plutôt que de la retrouver).
- **Restaurer intégralement l'état d'avant tentative** :
  `build/ntsc-uj/manifest.json`/`static-validation.json` n'ont en fait
  jamais été modifiés (l'échec de `prepare.py` est survenu avant toute
  écriture — vérifié par `diff` contre une sauvegarde prise avant la
  tentative) ; le répertoire de sortie vide créé sous `artifacts/` a
  été supprimé. **Aucun état du profil `native` de cette session n'a
  été touché.**

## Non établi

- **Comment restaurer un manifeste `rexglue-oracle` valide** sans
  perdre l'état actuel du profil `native` — nécessite soit de committer/
  isoler l'arriéré du sous-module `AC6_recomp` (décision utilisateur),
  soit de reconstruire le manifeste par un autre moyen (aucune copie de
  sauvegarde du manifeste `rexglue-oracle` d'origine n'a été trouvée
  dans `artifacts/`).
- **Si la route par défaut couvrirait effectivement les états de
  tirage manquants** — non testé, la campagne n'a jamais démarré.

## Gate

Aucune source, aucun état de build modifié ce cycle (tentative
entièrement annulée avant écriture). Gates de dépôt exécutés pour
clôturer le cycle normalement :
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
`ctest` racine inchangé (aucune source de production touchée ce
cycle).

## Named for r464

**Blocage qualifié nommé explicitement, décision utilisateur requise
avant de reprendre la campagne oracle** : le sous-module `AC6_recomp`
porte un arriéré non committé substantiel qui empêche toute
préparation du profil `rexglue-oracle` sans une décision sur son sort
(committer l'arriéré en l'état, l'isoler, ou une autre option). Sans
cette décision, la piste « étendre le registre épinglé via une
nouvelle session oracle » nommée par r456/r462 reste ouverte mais non
actionnable. Reste ouvert, non bloquant : (1) une fois le câblage
`PinnedShaderRuntime` jugé mûr, reconsidérer le committage groupé de
l'arriéré natif (r433/r434/r438/r454-r462, un arriéré DIFFÉRENT et
sans rapport avec celui du sous-module `AC6_recomp`) ; (2) mise à jour
de `tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut
(confort, pas une nécessité).

## Files

Aucun artefact gitignoré nouveau conservé (répertoire de sortie vide
supprimé ; sauvegardes temporaires sous `/fastdata/lavaulta/tmp/`, non
conservées).
