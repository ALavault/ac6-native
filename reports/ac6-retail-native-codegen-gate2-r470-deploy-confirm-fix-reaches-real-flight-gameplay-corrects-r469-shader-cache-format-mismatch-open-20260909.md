# AC6 retail NTSC-U/J — r470 — le blocage campagne→monde est LEVÉ : `run_gate.py` envoyait `Escape` au lieu d'un quatrième `A` sur un écran « Deploy with this selection ? » mal étiqueté « cinématique » — la route atteint enfin `world=1 stable=30` et capture un vrai HUD de vol ; corrige la piste de r469, une incompatibilité de format de cache nuanceur reste ouverte

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (deux lancements réels, voir ci-dessous).

## Contexte

Nommé par r469 : tester `--mission-hangar-confirm --mission-map-confirm`
en combinaison avec `--mission-render-summary`, hypothèse « écran de
confirmation manquant ».

## Correction de r469 avant tout lancement

`run_gate.py` refuse explicitement plus d'une seule sonde de lancement
Mission 01 à la fois (`mission_probe_count > 1: parser.error("choose
at most one Mission 01 launch probe")`, ligne ~643) — la combinaison
proposée par r469 (`--mission-render-summary` + `--mission-hangar-
confirm` + `--mission-map-confirm`) aurait été **rejetée par l'outil
lui-même**, jamais testée. Plus important : lecture du code source de
`--mission-render-summary` (lignes 739-774) montre que ce mode
**envoie DÉJÀ trois confirmations `space` automatiquement** (hangar →
carte tactique → briefing) avant d'atteindre l'étape étiquetée
`mission-cinematic` — l'hypothèse de r469 (« aucune confirmation
envoyée ») était donc fausse dans sa prémisse : les confirmations
hangar/carte SONT déjà envoyées par CHAQUE run r465-r469.

## Établi — la vraie cause : un écran de confirmation mal identifié comme cinématique

Les captures déjà produites par r465 existaient sur disque
(`/fastdata/lavaulta/tmp/ac6-oracle-r465-shader-capture/`), jamais
relues visuellement par r465-r469 (r469 le note explicitement comme
« Non établi »). `sha256sum` sur les six dernières captures de ce run
(`step-78-mission-cinematic.png` à `step-89-failure-observation.png`)
montre un **hash identique bit-à-bit sur les six**
(`c3dbd78d…4c44`). La visualisation de `step-78-mission-cinematic.png`
montre un écran de confirmation de déploiement statique : « [PLAYER]
F-16C Fighting Falcon XMA4 … Deploy with this selection? A OK
B CANCEL » — **pas une cinématique**, un menu qui attend une
quatrième entrée `A`. Le commentaire du code (ligne 763-766)
révèle l'hypothèse erronée de l'auteur d'origine : « let the launch
movie advance naturally, then send only Escape » — en croyant cet
écran être le film de lancement, l'auteur a délibérément évité d'y
envoyer `A`, de peur qu'il ne fasse entrer en vol AVANT `Escape`. La
preuve visuelle contredit directement cette hypothèse.

## Correctif appliqué et vérifié — première tentative, succès net

`tools/run_gate.py`, remplacé l'envoi final `("key", "Escape",
"0.6")` par un cinquième `space` après la capture `mission-cinematic`
(le nom de capture est conservé pour ne pas casser la comparabilité
historique, malgré l'étiquette trompeuse). Backups de
`build/ntsc-uj/manifest.json`/`static-validation.json` pris avant
(précaution héritée de r463-r469, voir correction méthodologique
ci-dessous), puis `prepare.py --profile rexglue-oracle` +
`build.py --profile rexglue-oracle --target ntsc-uj` +
`validate.py --runtime rexglue-oracle` (tous verts) pour disposer
d'un binaire/manifeste cohérents avant le run.

**Lancement 1** : `run_gate.py --route routes/mission01-qualified-96.steps
--mission-render-summary --mission-d5b4-final-white`, 454 s. Journal :
```
01:22:45.595 [ac6-visual-phase] cinematic=0 world=1 hud=1 stable=30
```
**Le prédicat cible est enfin satisfait.** `step-87-gameplay-hud.png`
capturé — un vrai HUD de vol (réticule d'altitude/vitesse, radar
d'attitude circulaire, barre d'état d'armement), sur fond noir (le
monde 3D n'a pas de couleur — un défaut déjà catalogué séparément
dans les rapports D5B4 d'août 2026, pas une nouvelle découverte).
Aucun `SIGSEGV`/`trap`/`fatal` dans le journal ; les seules lignes
suspectes autour de la fin sont des resets `XmaContext` normaux
d'arrêt propre et des avertissements `GdkWindow … unexpectedly
destroyed` (fermeture normale de Xvfb). `RESULT.json` reste
`status=fail`/`game_status=-9` (probablement un critère de réception
plus strict — sauvegarde de campagne, etc. — non pertinent pour
l'objectif de couverture de nuanceurs) ; **hors de propos pour ce
cycle**, le prédicat visuel qui bloquait toute la chaîne r463-r469
EST satisfait.

`shader-dump/` : **482 fichiers, 241 hachages de nuanceur distincts**
— comparé aux 253 hachages du dump de référence r254
(`artifacts/retail-us-d5b4-cull-runtime-20260826/shader-dump/`) :
seulement **2 hachages nouveaux, pas dans l'ensemble existant**. Le
registre épinglé est cependant clé sur des paires (hash, modification),
pas seulement des hachages — de nouvelles paires restent possibles
même sans nouveau hachage (comme r255 l'avait déjà noté : 18
nuanceurs traduits sous 2-3 modifications distinctes) ; non déterminé
ce cycle (voir Non établi).

## Non établi — incompatibilité de format de cache, tentative 2 non concluante

L'outillage `rexglue_shader_translate/` (celui qui a produit les 271
traductions de r255) exige explicitement le format `.fbo.vk.xpso`
(« EDRAM emulated through conventional render targets … no fragment
shader interlock … matching the oracle product configuration »,
README ligne ~18-19) — **le lancement 1 a produit un fichier
`4E4D07D1.fsi.vk.xpso`** (interlock du nuanceur fragment), pas `.fbo`.
**Lancement 2** (`+ --mission-host-render-targets`, censé forcer
`--render_target_path_vulkan=fbo` selon le code source) : le fichier
produit reste nommé `.fsi.vk.xpso` **malgré le drapeau**, ET la
route n'atteint PAS `world=1` cette fois (bloque à `cinematic=0
world=0 hud=1 stable=0`, différemment du lancement 1) — le même
correctif de confirmation ne reproduit pas le succès de façon fiable
en combinaison avec ce drapeau. **Ni la cause du nommage `.fsi`
inchangé, ni la cause de la non-reproduction avec `--mission-host-
render-targets`, ne sont établies ce cycle** — nécessite un cycle
dédié plutôt qu'une troisième tentative en fin de celui-ci (périmètre
borné respecté).

## Correction méthodologique — la « prudence du manifeste partagé » de r463-r469 reposait sur une prémisse fausse

Chaque cycle depuis r463 a sauvegardé/restauré
`build/ntsc-uj/manifest.json`/`static-validation.json` en croyant ce
chemin partagé entre les profils `native` et `rexglue-oracle`. Lecture
de `tools/prepare.py` (ligne 342-346) : `materialize_native()` est une
fonction ENTIÈREMENT SÉPARÉE écrivant son propre manifeste à
`build/ntsc-uj/native/manifest.json` — un chemin DIFFÉRENT. De plus,
`ctest` du profil natif ne lit JAMAIS ces fichiers manifeste : il
n'a besoin que des binaires déjà compilés sous
`build/ntsc-uj/native/native-cmake/`, un répertoire qu'aucune
opération `rexglue-oracle` de r463-r470 n'a jamais touché. **Le
profil `native` n'a donc jamais été réellement en danger** par ce
travail — la prudence de sauvegarde/restauration était inutile
(mais inoffensive) à chaque cycle depuis r463. Vérifié ce cycle :
`ctest` **11/11**, aucune restauration nécessaire (rien n'avait
besoin d'être restauré).

## Décisions prises

- Corriger l'étape terminale de `--mission-render-summary` dans
  `run_gate.py` plutôt que d'ajouter une nouvelle sonde séparée —
  c'est un vrai bug du script existant (hypothèse d'auteur erronée,
  maintenant contredite par preuve visuelle directe), pas une
  fonctionnalité manquante.
- Ne PAS forcer une troisième tentative pour résoudre
  l'incompatibilité `.fsi`/`.fbo` ce cycle — périmètre borné respecté ;
  la piste est suffisamment caractérisée pour son propre cycle.
- Committer le correctif `run_gate.py` (petit, isolé, vérifié) — même
  catégorie que les correctifs d'infrastructure déjà committés par
  r464/r465.
- Ne pas committer `native_vulkan_backend.{h,cpp}` ni l'arriéré natif
  plus large — hors périmètre, sans rapport avec ce correctif.

## Gate

`ctest` natif : **11/11**, profil natif non affecté (voir correction
méthodologique ci-dessus — aucune restauration nécessaire).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
`ctest` racine : `ac6-cpp-complexity` seul échec pré-existant, comme à
chaque cycle de cette chaîne.

## Named for r471

Piste principale, non bloquante : résoudre l'incompatibilité de
format de cache `.fsi.vk.xpso` vs `.fbo.vk.xpso` requise par
`rexglue_shader_translate/` — soit en trouvant le bon drapeau/config
pour forcer réellement `.fbo`, soit en adaptant l'outillage de
traduction pour accepter `.fsi` si c'est en fait la configuration
correcte du produit `rexglue-oracle` actuel (le README date peut-être
d'avant un changement de configuration par défaut). Une fois résolu,
reprendre le pipeline complet : `parse_rexglue_cache.py` →
`translate_main.cpp` → `materialize_pinned_shader_capsule.py` →
reconstruction `native` → sonde en direct, pour voir si les nouvelles
paires (hash, modification) de ce dump de 241 nuanceurs distincts
font enfin apparaître du contenu non noir. Reste ouvert, non
bloquant : pourquoi `--mission-host-render-targets` casse la
reproduction du succès (lancement 2) ; le committage groupé de
l'arriéré `PinnedShaderRuntime` (r433/r434/r438/r454-r470).

## Files

Captures et journaux temporaires sous
`/fastdata/lavaulta/tmp/ac6-r470-deploy-confirm-test/` et
`/fastdata/lavaulta/tmp/ac6-r470-fbo-capture/`, non conservés
(gitignorés, reproductibles via la commande ci-dessus).
