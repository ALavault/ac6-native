# AC6 retail NTSC-U/J — r465 — le blocage `ac6recomp_codegen` nommé par r464 est LEVÉ (décision utilisateur explicite) : le profil `rexglue-oracle` régénère, compile et valide de bout en bout ; la campagne oracle tourne mais bute sur un blocage déjà connu et documenté ailleurs (l'arrêt du « movie worker » en cinématique) — zéro nouveau nuanceur capturé

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle exécuté avec succès techniquement (build + validate + run_gate),
mais route bloquée avant d'atteindre du contenu de vol non déjà couvert.

## Contexte

Nommé par r464, avec décision utilisateur explicite : autoriser la
régénération de `ac6recomp_codegen` pour `ntsc-uj`/`rexglue-oracle`
(option (a) des quatre proposées).

## Établi — le garde-fou levé, le profil `rexglue-oracle` reconstruit et validé de bout en bout

`tools/build.py` refusait explicitement de relancer
`ac6recomp_codegen` pour `ntsc-uj` (« already consumed »). Le garde a
été retiré (diff minimal et isolé, ~6 lignes) après vérification que
la cible ne dépend d'aucune ressource Ghidra en direct :
`upstream/AC6_recomp/cmake/rexglue_bootstrap.cmake` montre qu'elle
exécute simplement `rex::rexglue codegen ac6recomp_config.toml` — un
passage de recompilation hors-ligne, déterministe, auto-suffisant sur
`assets/default.xex` + la table statique d'adresses de fonctions du
fichier de configuration tracké.

Sauvegarde préalable de `build/ntsc-uj/manifest.json`/
`static-validation.json` (chemin partagé entre profils, risque déjà
identifié par r463/r464). `prepare.py --profile rexglue-oracle` puis
`build.py --profile rexglue-oracle --target ntsc-uj` : la codegen
s'exécute réellement cette fois, compilation complète réussie
(231/232 cibles, `ac6recomp` lié). `validate.py --target ntsc-uj
--runtime rexglue-oracle` produit un `static-validation.json` neuf et
cohérent — le profil `rexglue-oracle` est de nouveau pleinement
buildable et validable dans ce bac à sable, ce qu'il n'était plus
depuis (au moins) le 30 août.

## Établi — la campagne oracle tourne, mais bute sur un blocage DÉJÀ CONNU, sans rapport avec ce cycle

`tools/run_gate.py --route routes/mission01-qualified-96.steps
--mission-render-summary --mission-d5b4-final-white` (route choisie
pour porter le marqueur `campaign-new-game` qu'exige
`--mission-render-summary`, absent de la route « longstart » par
défaut) tourne **573 s**, capture 24 images jusqu'à
`step-89-failure-observation.png` (cinématiques de mission, carte
tactique, briefing), puis échoue le prédicat de journal attendu
(`[ac6-visual-phase] cinematic=0 world=1 hud=1 stable=30`,
`game_status=-9`, `clean_shutdown=false`). La fin de
`ac6recomp.log` montre le run bloqué dans une boucle d'attente du
« movie worker » (`KeWaitForMultipleObjects`/`KeSetEvent` répétés sans
progression) — **exactement le même symptôme déjà documenté** dans
`reports/retail-us-mission01-flight-long-candidate-20260828.md`
(« l'amorce/movie worker reste figée sans transition de campagne »,
image Xvfb identique bit-à-bit entre deux captures espacées de 5
minutes). Ce n'est donc pas une nouvelle découverte : c'est la
reproduction, sur le binaire fraîchement reconstruit, d'un blocage de
cinématique déjà caractérisé et distinct du sujet de ce cycle.

**228 fichiers de dump de nuanceurs capturés (114 nuanceurs
distincts : 57 fragment + 57 vertex)** — comparaison exhaustive des
identifiants avec les 253 nuanceurs déjà présents dans le dump
original de r254 (`artifacts/retail-us-d5b4-cull-runtime-20260826/
shader-dump/`) : **0 nuanceur nouveau**. Tous les nuanceurs capturés
sur cette route sont un sous-ensemble déjà couvert par le registre
épinglé de 271 variantes (la route s'arrête avant tout contenu de vol
réel, précisément la zone où r456/r462 avaient caractérisé les
tirages non couverts).

## Non établi

- **Pourquoi le movie worker reste figé** pour cette route/cinématique
  précise — hors périmètre de ce cycle, déjà nommé comme sa propre
  frontière par le rapport du 28 août cité ci-dessus.
- **Si une route différente** (contournant la cinématique bloquante,
  ou avec un `--storage-seed` reprenant après elle) capturerait les
  nuanceurs manquants — non tenté ce cycle (aurait dépensé davantage
  de budget oracle sans certitude, sur un problème déjà nommé ailleurs
  comme sa propre frontière substantielle).

## Décisions prises

- Ne pas retenter avec une durée plus longue ou une route différente
  ce cycle : le symptôme (image figée, movie worker en boucle) est
  déjà caractérisé comme un blocage réel et substantiel par un rapport
  antérieur, pas un simple retard — insister sans le résoudre d'abord
  aurait juste re-consommé du temps d'oracle pour le même résultat nul.
- Garder le correctif du garde-fou `build.py` (petit, isolé, propre) —
  contrairement à l'arriéré natif entremêlé, c'est une vraie correction
  d'infrastructure qui débloque durablement le profil `rexglue-oracle`
  pour de futures tentatives, une fois le blocage cinématique résolu
  séparément.
- Restaurer intégralement l'état du profil `native` : `manifest.json`/
  `static-validation.json` restaurés depuis la sauvegarde (SHA-256
  identiques : `8b169dc4...`/`c1dbbc31...`). **Vérifié en direct** :
  `ctest` natif 11/11, lancement réel — `presented_frames=5 state=2`,
  capture `pixels=921600 non_black=0 distinct_colors=1` — identiques à
  la ligne de base r454-r464. Aucune régression du profil `native`.

## Gate

**Profil `native` vérifié sans régression** (voir ci-dessus). Gates de
dépôt exécutés :
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
`ctest` racine confirmé inchangé (seul l'échec préexistant connu
`ac6-cpp-complexity`).

## Named for r466

**La piste « étendre le registre épinglé via l'oracle » reste ouverte
mais nécessite de résoudre D'ABORD le blocage du movie worker en
cinématique** (déjà nommé comme sa propre frontière substantielle par
`reports/retail-us-mission01-flight-long-candidate-20260828.md`) —
sans cela, aucune route oracle n'atteint le contenu de vol réel où les
tirages non couverts (caractérisés par r456/r462) se trouvent
effectivement. C'est un investissement d'investigation distinct,
probablement son propre cycle dédié, pas une simple relance de route.
Reste ouvert, non bloquant : (1) une fois le câblage
`PinnedShaderRuntime` jugé mûr, reconsidérer le committage groupé de
l'arriéré natif (r433/r434/r438/r454-r462) ; (2) mise à jour de
`tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut du
profil natif (confort, pas une nécessité).

## Files

Sauvegardes temporaires sous
`/fastdata/lavaulta/tmp/ac6-native-manifest-backup-r465/` et le
répertoire de capture oracle
`/fastdata/lavaulta/tmp/ac6-oracle-r465-shader-capture/` (24 captures,
dump de 114 nuanceurs, logs), non conservés comme preuve committée.
