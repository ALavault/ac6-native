# AC6 retail NTSC-U/J — r471 — l'incompatibilité `.fsi.vk.xpso`/`.fbo.vk.xpso` laissée ouverte par r470 est RÉSOLUE (la contrainte `.fbo` n'était qu'une convention de nommage, pas une exigence de code) ; 239 traductions SPIR-V produites et vérifiées, mais le rebasculement du registre est reporté — une vraie question de correction (résolution multi-modification) surgit avant qu'un merge soit sûr

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun nouveau lancement oracle ce cycle — travail entièrement statique
sur les données déjà capturées par r470 (`/fastdata/lavaulta/tmp/ac6-r470-deploy-confirm-test/`).

## Contexte

Nommé par r470 : résoudre l'incompatibilité `.fsi.vk.xpso` (produit
par le lancement réussi de r470) vs `.fbo.vk.xpso` (exigé, selon la
documentation, par `rexglue_shader_translate/`).

## Établi — la contrainte `.fbo` n'était qu'une convention de nommage documentaire, jamais vérifiée par le code

Lecture de `pipeline_cache.cpp` (soumodule `AC6_recomp`) : le suffixe
`.fsi`/`.fbo` du fichier de cache vient de
`edram_fragment_shader_interlock ? "fsi" : "fbo"` (ligne ~428), lui-
même dérivé de `RenderTargetCache::GetPath() ==
Path::kPixelShaderInterlock`. **Ce chemin est forcé à `fsi` par
défaut pour AC6 spécifiquement** : `main.cpp:114`,
`rex::cvar::SetSessionDefault("render_target_path_vulkan", "fsi",
"ac6_native_graphics_fsi_renderer")`, avec un commentaire explicite :
« The host-render-target path (fbo) loses the AC6 EDRAM resolve when
presenting the 1AB6 frontbuffer on Linux. Keep FSI as the qualified
Vulkan path ». **Le chemin `fbo` est donc documenté comme CASSÉ pour
ce titre sur Linux** — la tentative 2 de r470
(`--mission-host-render-targets`, censée forcer `fbo`) était donc
vouée à échouer différemment, pas un bug d'outillage à corriger.

Lecture de `parse_rexglue_cache.py` (`recompilation/ace-combat-6-retail/
tools/rexglue_shader_translate/`) : **aucune vérification du nom de
fichier n'existe dans le code** — `--xpso` accepte n'importe quel
chemin, seuls le magic/la taille/les hachages XXH3 du contenu binaire
sont vérifiés. La mention `.fbo.vk.xpso` dans le README et les
messages d'aide est purement documentaire (reflète l'usage r254, pas
une contrainte imposée).

## Établi — le pipeline de traduction fonctionne sur les données `.fsi`, vérifié de bout en bout

1. `parse_rexglue_cache.py --xpso 4E4D07D1.fsi.vk.xpso` a d'abord
   échoué : « xsh shader set differs from the dump (xsh-only=0
   dump-only=6) ». Diagnostic : le run de r470 s'est terminé
   abruptement (`game_status=-9`), donc 6 nuanceurs dumpés (tous DÉJÀ
   présents dans le dump de référence r254, aucun des 2 nouveaux
   hachages) n'ont jamais été persistés dans le `.xsh` avant l'arrêt.
   Un répertoire de dump BORNÉ à l'ensemble exact des hachages `.xsh`
   (6 fichiers exclus, les deux nouveaux hachages conservés) fait
   passer la vérification proprement — cela RESSERRE la validation
   plutôt que de la contourner : seuls les nuanceurs corroborés à la
   fois par le `.xsh` et le dump sont retenus.
2. Résultat : **235 nuanceurs, 177 pipelines, 239 paires de
   traduction** (`shaders=235 pipelines=177 translation_pairs=239`).
3. `rexglue_shader_translate/build.sh` + le binaire produit, avec les
   MÊMES cvars de correctifs AC6 que r254
   (`ac6_neutralize_deswizzle_hashes`, `ac6_fix_hoisted_fetch_gradients_hashes`) :
   **`records=239 ok=239`** — chaque traduction réussit, aucun échec.
4. Un « bundle » au format attendu par `materialize_pinned_shader_capsule.py`
   a été construit à partir de `manifest.txt` + `spirv-out/` (calcul
   direct des digests FNV-1a64/SHA-256, réutilisant `fnv1a64()` du
   script existant) ; `build_capsule()` (importé directement, pas
   réimplémenté) l'a validé sans erreur.
5. Un registre fusionné (271 entrées existantes + les nouvelles,
   dédupliquées par `(shader_type, digest, modification, spirv)`) a
   été construit : **320 entrées, 49 réellement nouvelles** (au-delà
   des 2 nouveaux hachages : la plupart sont des modifications
   SUPPLÉMENTAIRES pour des nuanceurs déjà connus — cohérent avec le
   motif déjà noté par r255, « 18 nuanceurs traduits sous 2-3
   modifications »). Vérifié par relecture (`parse_capsule` round-trip
   sur les octets écrits) : 320 entrées, cohérent.

## Non établi — une vraie question de correction avant de fusionner, PAS un problème d'outillage

Intégrer ce registre à 320 entrées (fixture + reconstruction native)
fait échouer `pinned_deswizzle_slot4_payload_renders_tile_restore`
(`native/tests/native_xenos_tests.cpp`) : `last_pixel_modification()`
ne vaut plus `0x410000000000` comme attendu. Diagnostic partiel : le
nuanceur pixel de ce test (`kPixelDigest`) porte maintenant **deux**
modifications (`0x410000000000` et une nouvelle, `0x10000000000` —
qui diffèrent exactement au bit 46, `depth_stencil_mode`) au lieu
d'une seule. Le test lui-même localise correctement l'entrée
`pixel_entry` attendue par correspondance exacte de modification
(inchangé), mais la RÉSOLUTION RÉELLE à l'exécution
(`PinnedShaderRuntime::draw_pinned()` → `derive_pixel_modification()`
→ `translate_ucode_variant()`) choisit maintenant différemment avec
deux variantes disponibles pour ce digest. **Ceci n'a pas été
investigué jusqu'à la cause précise** — la logique exacte de
sélection de `translate_ucode_variant()` face à plusieurs variantes
candidates pour un même digest n'est pas encore comprise avec
certitude, et un merge à l'aveugle risquerait de changer
silencieusement quel SPIR-V s'exécute pour des tirages déjà vérifiés
qui fonctionnaient auparavant — exactement le type de régression
correctness que ce dépôt refuse de deviner.

## Incident et récupération — `git checkout --` a failli détruire l'arriéré de tests non committé

En tentant d'annuler mes propres modifications de `native/tests/
native_xenos_tests.cpp` (comptages 271→320, logique `first_spirv`),
`git checkout -- native/tests/native_xenos_tests.cpp` a été exécuté —
**ce fichier n'est PAS à l'état HEAD committé** (le dernier commit le
touchant date de r100, une ancienne étape de ce dépôt) : il porte un
arriéré massif non committé (r255-r470, ~3843 lignes) exactement
comme `native_vulkan_backend.{h,cpp}` et les autres fichiers du
« native lane ». **Le checkout l'a instantanément réduit à 432
lignes**, détruisant tout cet arriéré. **Récupéré immédiatement** :
la copie de mise en scène du build
(`build/ntsc-uj/native/native-source/tests/native_xenos_tests.cpp`,
synchronisée juste avant l'incident et donc encore à 4259 lignes, MES
modifications de ce cycle incluses) a été recopiée par-dessus,
restaurant l'intégralité du fichier. Mes propres modifications de ce
cycle ont ensuite été annulées chirurgicalement avec `Edit` (pas
`checkout`) pour revenir exactement à l'état d'avant ce cycle.
Vérifié : `ctest` natif **11/11** après restauration complète,
`git status` cohérent avec l'état attendu (le fichier reste non suivi
par HEAD, comme avant ce cycle). **Aucune perte de travail au final**,
mais l'incident est documenté explicitement : `git checkout --`
**ne doit plus jamais être utilisé** sur un fichier du « native lane »
sans avoir d'abord vérifié `git log -1 -- <fichier>` pour confirmer
qu'il est à jour avec HEAD — la discipline « ne jamais committer
l'arriéré » de r433 a un corollaire tout aussi important, jamais
énoncé jusqu'ici : **ne jamais non plus le décharger**.

## Décisions prises

- Ne PAS installer le registre fusionné à 320 entrées tant que la
  question de résolution multi-modification n'est pas comprise —
  correctness avant couverture.
- Restaurer intégralement `native/fixtures/pinned-shader-registry.v1.bin`
  et `native/tests/native_xenos_tests.cpp` à leur état d'avant ce
  cycle plutôt que de committer un état intermédiaire risqué.
- Documenter l'incident `git checkout --` sans l'euphémiser — c'est
  exactement le type d'erreur que la discipline de correction de ce
  dépôt demande de nommer, pas de dissimuler.
- Ne pas relancer de nouveau run oracle ce cycle — les données de
  r470 suffisaient à ce travail purement statique de traduction.

## Gate

**Aucune source de production modifiée au final ce cycle** (registre
et fichier de test restaurés à l'identique). `ctest` natif : **11/11**.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
`ctest` racine : `ac6-cpp-complexity` seul échec pré-existant, comme à
chaque cycle de cette chaîne.

## Named for r472

Comprendre précisément la sémantique de résolution de
`ShaderTranslator::translate_ucode_variant()`/`derive_pixel_modification()`
face à PLUSIEURS variantes candidates pour un même digest (nouveau
depuis que r471 a démontré qu'un merge à 320 entrées en introduit
plusieurs), avant tout merge du registre. Une fois cette sémantique
comprise et un merge jugé sûr : réinstaller le registre fusionné
(reproductible depuis `/fastdata/lavaulta/tmp/ac6-r470-deploy-confirm-test/`
selon les étapes exactes ci-dessus, non conservé par convention de ce
dépôt), reconstruire `native`, sonder en direct pour voir si du
contenu non noir apparaît enfin. Reste ouvert, non bloquant : pourquoi
`--mission-host-render-targets` casse la reproduction de `world=1`
(r470, maintenant expliqué en partie — le chemin `fbo` est
documenté cassé sur Linux pour AC6, donc peut-être un échec attendu
plutôt qu'un bug) ; le committage groupé de l'arriéré
`PinnedShaderRuntime` (r433/r434/r438/r454-r470).

## Files

Bundle/manifeste/registre fusionné construits sous
`/fastdata/lavaulta/tmp/ac6-r471-fsi-parse/`, non conservés
(reproductibles par les commandes ci-dessus depuis les données de
r470, qui existent toujours sur disque à ce jour).
