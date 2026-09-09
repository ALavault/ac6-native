# AC6 retail NTSC-U/J — Gate 2 runtime natif

0. **r476 — piste A du plan approuvé : campagne oracle ciblée tôt-démarrage exécutée intégralement (route scellée `mission01-qualified-96.steps` tronquée à `--duration 60`, chaîne complète rejouée : dump → traduction → fusion). **10/10 traductions réussies, mais ZÉRO nuanceur réellement nouveau** — les 10 correspondent exactement à des entrées déjà présentes dans le registre à 320 entrées (cohérent : la route de vol de r470 traverse elle-même cet écran-titre avant le vol). **Ceci corrige la prémisse de r475/r476** : le trou n'est pas une couverture de nuanceur manquante. Diagnostic de trace ajouté (`AC6_NATIVE_VD_TRACE`, non committé) : les tirages rejetés du sondage natif échouent TOUJOURS côté vertex (`pixel_ok=1` systématique), et 2 des 3 digests vertex rejetés sont EXACTEMENT ceux capturés ce cycle — mais sous la MAUVAISE modification (`0x1`/`0x900000000` capturés vs `0x900000000`/`0x0` demandés par les tirages réels). Le nuanceur EST dans le registre, juste pas sous la bonne paire (nuanceur, modification) pour ce tirage précis. `ctest` 11/11, aucune régression. Toujours NON committé (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r476-early-boot-capture-shows-shaders-already-registered-real-gap-is-a-modification-mismatch-not-missing-coverage-20260909.md`.
   **Nommé pour r477** : relancer une capture oracle ciblée avec un
   minutage différent (viser au-delà de l'écran de diagnostic/erreur
   transitoire observé à l'étape 1 de ce cycle) pour capturer les
   MÊMES nuanceurs vertex déjà identifiés sous la BONNE modification —
   une cible précise (2-3 nuanceurs), pas une exploration à l'aveugle.
   Reste ouvert sinon : Piste B (committer l'arriéré natif) et
   Piste C (garde-fou HUD) du plan approuvé, indépendantes.

1. **r475 — pas de plateau natif : les 86 tirages du sondage tombent TOUS sur le trou de couverture du registre déjà connu (9/9 rejets « no pinned variant matches this draw state »), pas un blocage de chargement. Preuve directe : 36 tirages AVANT la capture de diagnostic, 50 DE PLUS après (déclenchant même le chemin MSAA de r459) — le moteur GPU/VD continue de traiter du contenu réel bien après ce que r473/r474 avaient lu comme un « plateau silencieux ». Les 86 tirages sont TOUS `vertex_count=1 index_count=1` (auto-indexés), cohérent avec des éléments d'interface précoces (logo/titre), PAS de la géométrie 3D de vol. **Ceci explique pourquoi le registre à 320 entrées (r472, nuanceurs de vol) n'a aucun effet** : le contenu précoce que le sondage natif atteint utilise un ensemble de nuanceurs complètement différent, jamais capturé par la route oracle de r470 (qui ciblait le gameplay en vol, pas l'écran-titre). **Corrige r473/r474** : ni une confirmation manquante ni un chargement figé — le même trou de couverture déjà connu, pour une phase de jeu différente. Aucune source modifiée ce cycle (analyse par trace uniquement).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r475-no-plateau-all-86-draws-hit-the-known-registry-coverage-gap-not-a-native-specific-stall-20260909.md`.
   **Nommé pour r476** : une campagne oracle ciblée sur le contenu de
   tout début (logo/écran-titre, pas la route de vol de r470)
   capturerait probablement les nuanceurs que le sondage natif atteint
   réellement — le chemin le plus direct vers du contenu visible
   natif, mais un travail substantiel (refaire r463-r472 pour une
   route différente), pas une décision à prendre seul vu le budget
   oracle déjà dépensé. Reste ouvert sinon : décodage de
   `fetch_const[1]` ; décision de committage groupé de l'arriéré
   natif (r433/r434/r438/r454-r474).

1. **r474 — mécanisme d'injection d'entrée minimal CONSTRUIT dans `native_guest_input.cpp` (`AC6_NATIVE_INPUT_AUTO_CONFIRM`, gardé par variable d'environnement, réutilise le point d'injection déjà existant `get_state()`/`is_connected()`, comportement par défaut inchangé) — fichier propre à HEAD avant ce cycle, committé directement (pas l'arriéré entremêlé). **Vérifié sans régression** : `ctest` 11/11. Relancé avec l'entrée simulée ET une fenêtre portée à 90 s (3,6× la limite précédente) : **86 tirages, contenu STRICTEMENT IDENTIQUE** à avant — aucun effet, malgré l'entrée ET la fenêtre plus longue. Le journal montre un plateau silencieux après les 86 tirages initiaux (pas un écran qui continue de se rafraîchir comme le chemin oracle pendant ses 573 s). **Corrige la piste de r473** : l'hypothèse « il manque juste une confirmation, comme côté oracle » est affaiblie — ressemble davantage à un blocage de contenu/chargement propre au natif qu'à une entrée manquante. Toujours NON committé côté arriéré (mais CE correctif isolé EST committé, fichier propre).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r474-minimal-input-injection-built-and-verified-does-not-unblock-content-real-stall-is-not-a-missing-confirm-20260909.md`.
   **Nommé pour r475** : investiguer précisément ce que représentent
   les 86 tirages et pourquoi rien ne se produit après (trace du
   chargement natif), pas une nouvelle tentative d'entrée. Le
   mécanisme d'injection reste disponible une fois la vraie cause
   comprise.

1. **r473 — pourquoi le registre étendu (r472) n'a toujours aucun effet visible identifié : (1) AUCUN mécanisme d'injection d'entrée n'existe dans le produit natif (`ac6recomp_main.cpp` boot en mode ouvert, sans jamais rejouer d'entrée — confirmé par recherche exhaustive, le seul outil apparenté `tools/ac6_controller_input_replay.py` porte dans son propre docstring qu'il ne cible PAS le runtime natif) ; (2) la fenêtre de sonde la plus longue jamais essayée (25 s, r454-r472) est environ 23× plus courte que le minutage réel observé côté oracle pour atteindre `world=1` avec la CORRECTE séquence de confirmation (573 s, r470/r471) — indépendamment de l'entrée, la fenêtre actuelle ne pourrait de toute façon jamais atteindre le vol réel. Relancé avec le registre à 320 entrées fraîchement installé : 86 tirages (inchangé depuis r453), 8 replis « no pinned variant » (compte IDENTIQUE à avant l'extension) — confirme que le contenu actuellement sondé (probablement écran-titre/logos) n'exerce AUCUNE des 49 nouvelles entrées. Ce n'est PAS un échec du registre (toujours correctement installé, `ctest` 11/11) — juste la preuve que rien ne l'atteint encore dans ce harnais. Aucune source modifiée ce cycle (décision délibérée : construire un mécanisme d'entrée natif est un vrai morceau d'infrastructure, hors périmètre d'un cycle borné).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r473-native-probe-has-no-input-injection-and-its-25s-window-is-far-shorter-than-the-573s-oracle-timeline-20260909.md`.
   **Nommé pour r474** : (1) identifier précisément ce que les 86
   tirages actuels représentent (instrumenter/tracer le contenu de
   la fenêtre de 25 s) ; (2) si un mécanisme d'entrée s'avère
   nécessaire, dimensionner concrètement sa construction avant de
   l'entreprendre — décision d'investissement, pas à prendre à la
   légère. Le registre à 320 entrées (r472) reste correctement
   installé et vérifié ; cette découverte explique seulement pourquoi
   son effet n'est pas encore visible.

1. **r472 — la sémantique de résolution multi-modification laissée ouverte par r471 est COMPRISE : `draw_pinned()` essayait les candidats « indice de profondeur abandonné » AVANT le candidat « indice exact + ParamGen », l'inverse de l'intention déjà documentée dans son propre commentaire (« falls back ... when no variant carries the hint »). Réordonné (candidats exacts d'abord, repli ensuite). **Le registre fusionné à 320 entrées (r471) est installé et vérifié sans régression** : `native/fixtures/pinned-shader-registry.v1.bin` mis à jour (320 entrées, 255 signatures, 65 avec plusieurs modifications), deux tests cassés par le merge corrigés (comptages exacts + une seconde divergence trouvée dans le chemin LEGACY `translate_ucode()`, sa propre règle à deux niveaux modification==0-d'abord non reflétée par le test). `ctest` natif 11/11. **Le fichier de registre est COMMITTÉ** (jamais suivi par git avant ce cycle malgré la négation `.gitignore` de r255 — un ajout propre, pas un committage d'arriéré) ; le correctif de code source et les assertions de test restent NON committés (arriéré déjà catalogué). **Le but final n'est toujours pas atteint** : la sonde `--probe-entry` native reste inchangée (`non_black=0`) parce qu'elle ne traverse jamais la longue séquence de confirmation de déploiement que r470/r471 ont dû utiliser côté oracle — pas une preuve que le registre étendu est inutile, juste que rien ne l'exerce encore dans ce harnais.**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r472-modification-resolution-order-fixed-320-entry-registry-installed-still-black-native-probe-never-drives-deploy-confirm-20260909.md`.
   **Nommé pour r473** : étendre le harnais `--probe-entry` du produit
   `native` (ou un rejeu d'entrée équivalent) pour driver la même
   séquence de confirmation que r470/r471, puis relancer la sonde pour
   vérifier si le registre à 320 entrées produit enfin du contenu
   visible non noir. Reste ouvert, non bloquant : le committage groupé
   de l'arriéré `PinnedShaderRuntime` (r433/r434/r438/r454-r472).

1. **r471 — l'incompatibilité `.fsi.vk.xpso`/`.fbo.vk.xpso` (r470) est RÉSOLUE : le suffixe `.fbo` requis par la doc de `rexglue_shader_translate/` n'était qu'une convention de nommage, jamais vérifiée par le code (`parse_rexglue_cache.py` ne teste aucun nom de fichier). `main.cpp` force `render_target_path_vulkan=fsi` par défaut pour AC6 spécifiquement, avec un commentaire explicite : le chemin `fbo` est CASSÉ sur Linux pour ce titre (perd le resolve EDRAM AC6). Pipeline de traduction vérifié de bout en bout sur les données `.fsi` de r470 : **239/239 traductions réussies**, registre fusionné à 320 entrées (49 nouvelles) construit et vérifié round-trip. **Merge reporté** : l'intégrer fait échouer un test déjà vérifié (`pinned_deswizzle_slot4_payload_renders_tile_restore`) — un digest partagé porte maintenant deux modifications candidates, et la sémantique de résolution de `translate_ucode_variant()` face à ce cas n'est pas comprise avec certitude ; correctness avant couverture, tout restauré à l'identique. **Incident documenté** : un `git checkout --` a failli détruire l'arriéré de tests non committé (~3843 lignes, native_xenos_tests.cpp n'est pas à HEAD) — récupéré via la copie de mise en scène du build, aucune perte finale, mais `git checkout --` est maintenant explicitement proscrit sur le native lane sans vérifier `git log -1` d'abord. `ctest` natif 11/11. Toujours NON committé côté source (aucune source de production modifiée ce cycle).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r471-fsi-fbo-mismatch-resolved-translation-verified-merge-deferred-pending-modification-review-20260909.md`.
   **Nommé pour r472** : comprendre la sémantique de résolution de
   `translate_ucode_variant()`/`derive_pixel_modification()` face à
   plusieurs variantes candidates pour un même digest, avant tout merge
   du registre. Une fois jugé sûr : réinstaller le registre fusionné
   (reproductible depuis les données de r470, étapes documentées),
   reconstruire `native`, sonder en direct. Reste ouvert, non
   bloquant : le committage groupé de l'arriéré
   (r433/r434/r438/r454-r470).

1. **r470 — LE BLOCAGE CAMPAGNE→MONDE EST LEVÉ : `run_gate.py` envoyait `Escape` au lieu d'un quatrième `A` sur un écran de confirmation « Deploy with this selection? A OK / B CANCEL » mal identifié comme une cinématique (les 6 dernières captures de chaque run r465-r469 étaient bit-à-bit identiques — jamais relues visuellement avant ce cycle). **Corrige la prémisse de r469** : les confirmations hangar/carte sont DÉJÀ envoyées automatiquement par `--mission-render-summary` (lu dans le code source) — la vraie cause était ce cinquième `A` manquant. Correctif appliqué à `tools/run_gate.py` (committé) : `("key", "Escape", "0.6")` → `("key", "space", "0.6")` à ce point précis. **Vérifié en direct, lancement 1 : `[ac6-visual-phase] cinematic=0 world=1 hud=1 stable=30` ENFIN atteint**, HUD de vol réel capturé (`step-87-gameplay-hud.png`), 482 fichiers de nuanceurs dumpés (241 hachages distincts, 2 nouveaux vs les 253 existants — de nouvelles paires (hash,modification) restent possibles, non déterminé). **Non résolu** : le cache produit est `.fsi.vk.xpso`, pas `.fbo.vk.xpso` requis par `rexglue_shader_translate/` — incompatibilité de format bloquant la suite du pipeline ; un second lancement avec `--mission-host-render-targets` (censé forcer `.fbo`) n'a ni changé le nom de fichier ni reproduit le succès (bloqué différemment). **Correction méthodologique** : la prudence « manifeste partagé » de r463-r469 reposait sur une prémisse fausse — `native` a son propre manifeste séparé (`build/ntsc-uj/native/manifest.json`), `ctest` n'en dépend même pas ; le profil natif n'a jamais été en danger. `ctest` natif 11/11.**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r470-deploy-confirm-fix-reaches-real-flight-gameplay-corrects-r469-shader-cache-format-mismatch-open-20260909.md`.
   **Nommé pour r471** : résoudre l'incompatibilité de format de
   cache `.fsi` vs `.fbo` requise par `rexglue_shader_translate/`
   (trouver le bon réglage, ou adapter l'outillage si `.fsi` est en
   fait la configuration actuelle correcte), puis reprendre le
   pipeline complet (`parse_rexglue_cache.py` → `translate_main.cpp` →
   `materialize_pinned_shader_capsule.py` → reconstruction native →
   sonde) sur les 241 nuanceurs distincts de ce dump. Reste ouvert,
   non bloquant : pourquoi `--mission-host-render-targets` casse la
   reproduction du succès ; le committage groupé de l'arriéré
   `PinnedShaderRuntime` (r433/r434/r438/r454-r470).

1. **r469 — analyse statique (aucun processus lancé, relecture du journal déjà capturé par r465) : le tick du gestionnaire monde (`rex_sub_8226CEA0`, sonde diagnostique déjà écrite `ac6_world_submission_owner_probe.cpp`, activée par `--mission-render-summary` dans CHAQUE run r465-r468) n'est **JAMAIS appelé, zéro occurrence** sur 573 s. Le flux `NtReadFile` async vers `DATA00.PAC` s'arrête à **95,2 %** (2 158 782 464 / 2 267 086 848 octets) après ≈3 min de lecture active, sans reprendre. **Le jeu continue pourtant à rendre activement** après cet arrêt (compteur de frame déjà à 15 000+, tirages réels 128-149 par frame, présentations GPU continues) — **ce n'est pas un gel**, ce qui affaiblit encore la piste « movie worker figé » (r466/r467) et renforce une piste nouvelle : un écran de confirmation (hangar/carte/sortie) probablement affiché en boucle, jamais avancé faute d'entrée. `run_gate.py` porte déjà des drapeaux nommés pour cela (`--mission-hangar-confirm`, `--mission-map-confirm`, `--mission-sortie-long-press`…), jamais essayés en combinaison avec `--mission-render-summary` par cette chaîne. Profil `native` non touché ce cycle (aucune reconstruction).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r469-world-owner-tick-never-runs-pac-stream-stalls-at-95-percent-while-game-keeps-rendering-20260909.md`.
   **Nommé pour r470** : relancer `run_gate.py` avec
   `--mission-hangar-confirm --mission-map-confirm` (et/ou
   `--mission-sortie-long-press`/`--mission-launch-hold`) en plus de
   `--mission-render-summary --mission-d5b4-final-white`, pour voir si
   fournir les confirmations d'écran manquantes débloque enfin la
   transition campagne→monde. Si cela échoue, relire une capture
   d'écran déjà produite (r465-r468) pour identifier directement
   l'écran affiché à l'arrêt. Reste ouvert, non bloquant : le
   committage groupé de l'arriéré `PinnedShaderRuntime`
   (r433/r434/r438/r454-r468) reste une piste indépendante si le
   plafond campagne/monde s'avère trop coûteux à lever.

1. **r468 — analyse statique (aucun processus lancé) : `world=1` ([ac6-visual-phase]) est détecté par UN SEUL hash de nuanceur pixel du compositeur monde (`0x17e5e4ac3e713245`, `command_processor.cpp:4204-4206`, `NotifyWorldCompositorDraw()`) — le MÊME hash déjà connu de r254 dans la liste `neutralize` (pas une découverte accidentelle). L'hypothèse la moins chère (trou de montage d'assets façon r452) est ÉCARTÉE : l'ISO est passée directement en argument, pas via `assets/`. Relecture du journal DÉJÀ CAPTURÉ par r465 (573 s, pas de nouveau run) : 39 nuanceurs pixel distincts liés durant tout le run, AUCUN n'est ce hash — cohérent avec (pas contradictoire avec) le plafond campagne/monde déjà établi par r463-r467, et affaiblit l'hypothèse d'un détecteur trop étroit (le nuanceur n'est simplement jamais soumis). **Cette frontière préexiste à toute la campagne r454-r468** (`reports/retail-us-mission01-flight-long-candidate-20260828.md` documente déjà le même plafond). Cause racine du blocage lui-même toujours NON établie. Profil `native` non touché ce cycle (aucune reconstruction).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r468-world-compositor-detector-traced-to-one-shader-hash-never-bound-rules-out-asset-mount-gap-and-narrow-detector-bug-20260909.md`.
   **Nommé pour r469** : (1) identifier précisément où/pourquoi la
   transition campagne→monde ne se déclenche jamais (Ghidra statique
   sur le code de transition lui-même, ou gdb ciblé sur ce code plutôt
   que le mauvais fil déjà écarté) ; (2) reconnaître cette frontière
   comme substantielle et potentiellement hors périmètre d'un cycle —
   dans ce cas, le committage du câblage `PinnedShaderRuntime`
   (r433/r434/r438/r454-r467, arriéré natif différent et sans rapport)
   reste une piste non bloquante indépendante.

1. **r467 — session gdb en direct sur le « movie worker » TENTÉE deux fois, non aboutie sur la cause racine, mais une CORRECTION importante trouvée par un troisième lancement (sans gdb, en contournant les trois obstacles d'environnement plutôt qu'en les résolvant). Tentative 1 (point d'arrêt conditionnel sur `is_ac6_movie_worker_wait`) : 12+ min sans jamais toucher le point d'arrêt (évaluation de condition gdb trop coûteuse sur un site appelé en boucle serrée) — tuée. Tentative 2 (rapportée par le fork précédent, jamais committée) : trois obstacles distincts — `ptrace_scope=1` sans root ferme l'attachement à un `run_gate.py` normal ; un lancement direct sous gdb révèle un VRAI `SIGSEGV` spécifique au débogueur dans `rex_sub_821E4378` (jamais vu hors gdb) ; `handle SIGSEGV pass` contourne le crash mais ralentit trop pour tenir dans le budget d'une tâche d'arrière-plan. **Troisième lancement (celui qui compte) : `ac6recomp` relancé EN DIRECT, sans gdb du tout**, sur le même Xvfb — le journal montre le motif `KeSetEvent(ptr=82916E3C)` → `KeWaitForMultipleObjects(objects=[82916E2C 82916E08]) result=0` **DÈS LA PREMIÈRE SECONDE après le boot** (`00:44:50`, avant tout chargement de monde ou cinématique), identique en forme à ce que r466 avait lu comme le symptôme du blocage. **Ceci corrige r466** : cette boucle n'est PAS spécifique au blocage — elle tourne en continu depuis le tout début du processus, ce qui ressemble beaucoup plus à un sondage normal par tick (attendu, pas cassé) qu'à un thread réellement bloqué. **Cause racine du blocage de transition monde/campagne toujours NON établie** — la piste « movie worker figé » est affaiblie par cette preuve ; le vrai blocage est probablement ailleurs. Nettoyage complet effectué (tous les processus `ac6recomp`/`gdb`/`Xvfb` orphelins tués, répertoires temporaires supprimés) ; profil `native` non touché ce cycle (aucune reconstruction). Toujours NON committé côté source (aucune source de production modifiée ce cycle).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r467-movie-worker-busy-spin-present-from-boot-not-stall-specific-corrects-r466-20260909.md`.
   **Nommé pour r468** : la vraie cause du blocage de transition
   monde/campagne reste à trouver ailleurs que dans la boucle du
   movie worker — regarder ce qui gouverne l'activation
   `[ac6-visual-phase] world=1` elle-même plutôt que ce site de
   wait précis ; envisager une session Ghidra headless (projet
   `ac6-us`) pour les xrefs statiques de cette activation plutôt
   qu'un nouveau lancement gdb long. Reste ouvert, non bloquant :
   (1) le trou de couverture du registre épinglé (r456) reste la
   piste principale une fois ce blocage résolu ; (2) une fois le
   câblage `PinnedShaderRuntime` jugé mûr, reconsidérer le
   committage groupé de l'arriéré natif (r433/r434/r438/r454-r462) ;
   (3) mise à jour de `tools/prepare.py`/`tools/build.py` pour le
   chemin ISO par défaut du profil natif (confort, pas une
   nécessité).

1. **r466 — le « movie worker » nommé par r465 tracé jusqu'à sa source : `thirdparty/rexglue-sdk/src/kernel/xboxkrnl/xboxkrnl_threading.cpp:102-104` (adresses `0x82916E3C`/`2C`/`08`, un VRAI thread invité, pas un artefact hôte) — instrumentation présente depuis le tout PREMIER commit du sous-module vendu `AC6_recomp` (`ddf7c285`, avril 2026, auteurs d'origine ReXGlue), donc antérieure à toute cette campagne. Journal de r465 réanalysé (pas de nouvelle route lancée) : le thread ne bloque JAMAIS — `KeWaitForMultipleObjects` retourne `result=0` (immédiat) à chaque appel sur 159 644 lignes, boucle serrée sans attente réelle. Corrélé avec `render_hooks.cpp` : une VRAIE cutscene en moteur (pas de FMV/XMV) démarre à 23:57:35, se termine proprement ~46 s plus tard à 23:58:22, puis le monde 3D ne démarre JAMAIS pour les 6 min 45 s restantes du run, pendant que le movie worker continue sa boucle. **Cause racine non établie** (lequel des deux objets d'attente reste signalé, et pourquoi) — nécessite une session gdb en direct. Aucun état de build touché ce cycle.**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r466-movie-worker-stall-characterized-real-guest-thread-busy-spin-predates-this-campaign-20260909.md`.
   **Nommé pour r467** : session gdb en direct sur le binaire oracle
   (profil `rexglue-oracle`), point d'arrêt sur le retour de
   `xeKeSetEvent`/`KeWaitForMultipleObjects` pour les trois adresses
   nommées, lecture de l'état interne des objets noyau au moment
   précis du blocage (~23:58:22, juste après la fin de cutscene) —
   tranchera entre stub hôte incomplet et limitation déjà connue des
   auteurs d'origine ReXGlue. Une fois résolu, la piste « étendre le
   registre épinglé via l'oracle » (r454-r465) pourra reprendre. Reste
   ouvert, non bloquant : une fois le câblage `PinnedShaderRuntime`
   jugé mûr, reconsidérer le committage groupé de l'arriéré natif
   (r433/r434/r438/r454-r462) ; mise à jour de
   `tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut
   du profil natif (confort, pas une nécessité).

1. **r465 — le blocage `ac6recomp_codegen` nommé par r464 est LEVÉ (décision utilisateur explicite) : garde retiré de `build.py` (diff isolé, ~6 lignes — la cible n'est qu'un passage `rex::rexglue codegen` hors-ligne, sans dépendance Ghidra en direct). Le profil `rexglue-oracle` régénère, compile (231/232 cibles) et valide (`static-validation.json` neuf) de bout en bout pour la première fois depuis le 30 août. **La campagne oracle tourne mais bute sur un blocage DÉJÀ CONNU, sans rapport avec ce cycle** : le « movie worker » reste figé en cinématique (même symptôme exact que `reports/retail-us-mission01-flight-long-candidate-20260828.md`, image Xvfb identique entre deux captures espacées de 5 min), route arrêtée à 573 s avant tout contenu de vol réel. **228 fichiers/114 nuanceurs capturés, comparaison exhaustive avec les 253 nuanceurs déjà connus : ZÉRO nouveau nuanceur** — la route n'atteint jamais la zone de tirages non couverts caractérisée par r456/r462. Profil `native` restauré et vérifié sans régression (`ctest` 11/11, `presented_frames=5 state=2`, capture inchangée).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r465-codegen-regenerated-oracle-runs-but-hits-known-cinematic-stall-zero-new-shader-coverage-20260909.md`.
   **Nommé pour r466** : la piste « étendre le registre épinglé via
   l'oracle » reste ouverte mais nécessite de résoudre D'ABORD le
   blocage du movie worker en cinématique (déjà nommé comme sa propre
   frontière substantielle par le rapport du 28 août cité) — un
   investissement d'investigation distinct, probablement son propre
   cycle dédié. Reste ouvert, non bloquant : (1) une fois le câblage
   `PinnedShaderRuntime` jugé mûr, reconsidérer le committage groupé
   de l'arriéré natif (r433/r434/r438/r454-r462) ; (2) mise à jour de
   `tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut
   du profil natif (confort, pas une nécessité).

1. **r464 — l'arriéré du sous-module `AC6_recomp` nommé par r463 a été COMMITTÉ tel quel (décision utilisateur explicite) : commit `6cf743269f5d4ea635b32e5b24369124fd25067b`, pointeur du dépôt parent avancé (`abf6513a`). `UPSTREAM_COMMIT` corrigé dans `tools/prepare.py`/`tools/build.py`/`README.md` (portait encore l'ancien pin). `prepare.py --profile rexglue-oracle` réussit alors. **Mais un DEUXIÈME blocage, plus profond, est apparu** : `build.py` refuse explicitement de régénérer le code généré NTSC-U/J du profil `rexglue-oracle` (`source/generated/sources.cmake`, sortie `ac6recomp_codegen`, traité comme une ressource « déjà consommée ») et aucune copie de cet artefact n'existe dans ce bac à sable — recherche exhaustive infructueuse. **Aucun contournement tenté** (le garde-fou n'a pas été levé). État du profil `native` intégralement restauré et vérifié sans régression (`ctest` 11/11, `presented_frames=5 state=2`, capture inchangée — seul `manifest.json`, chemin partagé entre profils, avait été temporairement écrasé par les tentatives `rexglue-oracle`). **Blocage qualifié nommé, décision utilisateur requise.**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r464-submodule-backlog-committed-oracle-still-blocked-by-consumed-ntsc-uj-codegen-20260908.md`.
   **Nommé pour r465** : décision utilisateur sur comment procéder
   face à l'absence de `source/generated/sources.cmake` pour
   `rexglue-oracle`/`ntsc-uj` — (a) autoriser explicitement une
   régénération `ac6recomp_codegen` pour cette cible ; (b) rechercher
   un archivage externe de cet artefact ; (c) tenter d'utiliser le
   binaire oracle déjà compilé du 30 août tel quel, en reconstruisant
   `static-validation.json` autrement ; (d) abandonner la piste oracle
   pour cette campagne. Reste ouvert, non bloquant : une fois le
   câblage `PinnedShaderRuntime` jugé mûr, reconsidérer le committage
   groupé de l'arriéré natif (r433/r434/r438/r454-r462, distinct de
   celui du sous-module déjà committé) ; mise à jour de
   `tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut
   du profil natif (confort, pas une nécessité).

1. **r463 — la campagne oracle nommée par r462 (décision utilisateur explicite : dépenser une session oracle pour étendre le registre épinglé) N'A PAS PU DÉMARRER : `build/ntsc-uj/manifest.json`/`static-validation.json` sont un chemin PARTAGÉ entre les profils `native` et `rexglue-oracle` (pas de sous-répertoire par profil) ; le manifeste `rexglue-oracle` a été écrasé par la préparation du profil `native` (30 août, 21h13-21h17), rendant `tools/run_gate.py` inutilisable. La tentative de régénération (`prepare.py --profile rexglue-oracle`) échoue séparément : le sous-module `AC6_recomp` porte un arriéré substantiel et manifestement délibéré (23 fichiers modifiés touchant le moteur ReXGlue lui-même) qui bloque `prepare.py` (« submodule must be clean »). **Aucune action destructrice tentée** (pas de `git stash`/`checkout` forcé sur le sous-module, pas de contournement manuel de `run_gate.py`) — décision explicite de ne pas prendre seul la décision du sort de cet arriéré. État restauré intégralement : rien n'a été écrit dans `build/ntsc-uj/` (vérifié par diff contre une sauvegarde), le répertoire de sortie vide créé sous `artifacts/` a été supprimé. **Blocage qualifié nommé, décision utilisateur requise.**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r463-oracle-shader-coverage-campaign-blocked-by-shared-manifest-and-dirty-submodule-20260908.md`.
   **Nommé pour r464** : décision utilisateur sur le sort de l'arriéré
   non committé du sous-module `AC6_recomp` (committer en l'état,
   isoler, ou autre option) — sans cette décision, la campagne oracle
   pour étendre le registre épinglé (271 variantes, r456) reste
   ouverte mais non actionnable. Reste ouvert, non bloquant : (1) une
   fois le câblage natif jugé mûr, reconsidérer le committage groupé
   de l'arriéré natif (r433/r434/r438/r454-r462, sans rapport avec
   celui du sous-module) ; (2) mise à jour de
   `tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut
   (confort, pas une nécessité).

1. **r462 — le chemin de viewport `PA_CL_CLIP_CNTL::clip_disable` IMPLÉMENTÉ à partir d'une citation exacte du code source public de Xenia (`draw_util.cc:362-386` + `vulkan_command_processor.cc:2440-2444` + `xenos.h:1139-1141` — lecture GitHub, PAS un run d'oracle N3, même méthodologie que le cycle 399). Point clé découvert par la lecture : l'étendue fixe utilisée n'est PAS la taille de la cible de rendu mais `min(8192, VkPhysicalDeviceLimits::maxViewportDimensions)` du périphérique HÔTE — un piège de lecture naïve évité par la citation directe. **Vérifié sans régression** : `ctest` 11/11 ; lancement réel confirme la disparition du motif ciblé (0 occurrence). **Fait notable : les 9 replis de ce run portent désormais TOUS le même motif unique** — le trou de couverture du registre épinglé (271 variantes, r456) — chaque AUTRE motif de rejet rencontré depuis r457 est maintenant éliminé par un correctif mécanique vérifié. `presented_frames`/capture inchangés. Toujours NON committé (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r462-clip-disable-viewport-path-implemented-from-xenia-source-citation-eliminates-last-non-oracle-rejection-20260908.md`.
   **Nommé pour r463** : un seul motif de rejet restant, et il pointe
   vers une décision utilisateur — étendre la couverture du registre
   de nuanceurs épinglé (271 variantes, r456) nécessite probablement
   une nouvelle session oracle Xenia, un blocage qualifié (dépense de
   budget N3) au sens de `CLAUDE.md`, pas à prendre unilatéralement.
   Reste ouvert, non bloquant : une fois le câblage jugé mûr,
   reconsidérer le committage groupé de l'arriéré
   (r433/r434/r438/r454-r462) ; mise à jour de
   `tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut
   (confort, pas une nécessité).

1. **r461 — un scissor démesuré (idiome matériel « pas de découpage supplémentaire », `scissor_tl=0x0 scissor_br=0x20002000` = région (0,0)-(8192,8192), observé en direct) est maintenant BORNÉ à la surface EDRAM réelle (réutilisant la géométrie de tuile déjà calculée : `image_width - origin_x`/`image_height - origin_y`) au lieu d'être rejeté par la constante arbitraire `kMaxEdramRegionWidth`/`Height`. Contrairement à la question de disposition d'échantillons EDRAM sous MSAA (r459, explicitement refusée faute de preuve), ceci est le comportement standard, bien compris, d'un test de scissor GPU. **Vérifié sans régression** : `ctest` 11/11 ; lancement réel confirme la disparition du motif ciblé (0 occurrence). Le tirage précédemment rejeté progresse maintenant plus loin dans le pipeline et révèle une garde `clip_disable viewport path not qualified this cycle` **jamais atteinte auparavant** (le rejet de scissor court-circuitait systématiquement avant). `presented_frames`/capture inchangés. Toujours NON committé (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r461-oversized-scissor-clamped-to-surface-instead-of-rejected-eliminates-rejection-new-clip-disable-gate-surfaced-20260908.md`.
   **Nommé pour r462** : (1) le motif de rejet du registre épinglé
   (couverture des 271 variantes, r456) nécessite probablement une
   session oracle — décision utilisateur ; (2) la garde `clip_disable
   viewport path not qualified this cycle`, nouvellement atteignable,
   jamais investiguée. Reste ouvert sinon : une fois le câblage jugé
   mûr, reconsidérer le committage groupé de l'arriéré
   (r433/r434/r438/r454-r461) ; mise à jour de
   `tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut
   (confort, pas une nécessité).

1. **r460 — support des masques d'écriture RB_COLOR_MASK partiels IMPLÉMENTÉ (nommé par r459, piste 2) : traduction mécanique et sans ambiguïté des 4 bits (R/G/B/A) vers `colorWriteMask` Vulkan (`PipelineKey` étendu, comme `sample_count` en r459). Une trace en direct a montré la vraie valeur rencontrée : `RB_COLOR_MASK=0x0` (aucune composante écrite). **Vérifié sans régression** : `ctest` 11/11 ; lancement réel confirme la disparition du motif de rejet (0 occurrence) ET une amélioration réelle : le compte de replis passe de 9 à 8, un tirage échouant auparavant est maintenant réellement épinglé (exécuté via `execute_frame()`, pas la validation structurelle de repli) — cohérent avec un masque nul qui n'écrit par construction aucun pixel visible (`non_black`/`distinct_colors` inchangés, comme attendu). Un nouveau motif sans rapport, non investigué, est apparu une fois : « render target region exceeds the bounded size ». Toujours NON committé (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r460-partial-write-mask-support-implemented-verified-eliminates-rejection-one-draw-now-succeeds-20260908.md`.
   **Nommé pour r461** : (1) le motif de rejet du registre épinglé
   (couverture des 271 variantes, r456, maintenant 7 des 8 replis
   restants) nécessite probablement une session oracle — décision
   utilisateur ; (2) le nouveau motif « render target region exceeds
   the bounded size », apparu une fois, non investigué. Reste ouvert
   sinon : une fois le câblage jugé mûr, reconsidérer le committage
   groupé de l'arriéré (r433/r434/r438/r454-r460) ; mise à jour de
   `tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut
   (confort, pas une nécessité).

1. **r459 — support d'une cible EDRAM multi-échantillonnée IMPLÉMENTÉ dans `PinnedShaderRuntime` : `edram_image_` devient l'image Vulkan multi-échantillonnée elle-même (persistante entre tirages, mêmes sémantiques clear/load que le chemin 1×), résolue explicitement (`vkCmdResolveImage`) vers une nouvelle image compagnon 1× (`edram_resolve_image_`) une seule fois, à la présentation — PAS via une résolution automatique de sous-passe par tirage, qui aurait perdu l'accumulation entre tirages successifs sur la même cible. `PipelineKey`/le cache de passes incluent maintenant `sample_count`. Le cas `sample_count==1` reste bit-pour-bit inchangé. **Vérifié sans régression** : `ctest` 11/11, et un lancement réel confirme la disparition totale du motif de rejet MSAA (0 occurrence, contre 1 auparavant) ; `presented_frames=5 state=2` inchangé. Les 9 replis restants portent maintenant deux motifs SANS RAPPORT avec MSAA (couverture du registre épinglé, déjà connu ; et un nouveau, non investigué, sur les masques d'écriture partiels). **Non établi, signalé honnêtement** : la géométrie de tuile EDRAM (pitch/origine) n'a PAS été ajustée pour `sample_count > 1` — aucune preuve citable (retail ou oracle) pour la disposition exacte des échantillons par tuile dans ce dépôt ; deviner aurait violé la discipline « pas de règle plausible sans contrôle ». Toujours NON committé (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r459-msaa-plumbing-implemented-and-verified-eliminates-its-own-rejection-tile-geometry-under-msaa-flagged-unverified-20260908.md`.
   **Nommé pour r460** : (1) le motif de rejet du registre épinglé
   (couverture des 271 variantes, r456) nécessite probablement une
   session oracle — décision utilisateur ; (2) le nouveau motif
   « partial render-target write masks are not qualified this cycle »
   observé une fois, sans rapport avec MSAA, non investigué. Reste
   ouvert sinon : une fois le câblage jugé mûr, reconsidérer le
   committage groupé de l'arriéré (r433/r434/r438/r454-r459) ; mise à
   jour de `tools/prepare.py`/`tools/build.py` pour le chemin ISO par
   défaut (confort, pas une nécessité).

1. **r458 — le rejet MSAA nommé par r457 est confirmé un VRAI trou de couverture, pas un bug de décodage : une trace de diagnostic locale (`AC6_NATIVE_VD_TRACE`, non committée) sur `derive_edram_render_target()` montre `RB_SURFACE_INFO=0x0a020280` → `msaa_bits=2` = un vrai encodage Xenos 4× MSAA (pas une valeur corrompue). **Corrige la prévision de r457** (qui supposait « probablement un autre correctif ciblé sans oracle, même modèle ») : `PinnedShaderRuntime` ne sait simplement pas encore créer/résoudre une cible EDRAM multi-échantillonnée — un vrai morceau de travail de moteur de rendu (image Vulkan multisample + résolution), pas une correction d'une ligne. Non implémenté ce cycle (dimensionnement hors périmètre d'un cycle d'investigation). `ctest` 11/11, aucune régression. Toujours NON committé (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r458-msaa-rejection-confirmed-genuine-4x-msaa-not-a-decode-bug-real-coverage-gap-named-for-r459-20260908.md`.
   **Nommé pour r459** : (1) dimensionner et implémenter le support
   d'une cible EDRAM 4× MSAA dans `PinnedShaderRuntime` — travail de
   moteur réel, probablement son propre cycle dédié voire plusieurs ;
   (2) le premier motif de rejet caractérisé par r456 (couverture du
   registre épinglé) nécessite probablement une session oracle —
   décision utilisateur. Reste ouvert sinon : une fois le câblage
   jugé mûr, reconsidérer le committage groupé de l'arriéré
   (r433/r434/r438/r454-r458) ; mise à jour de
   `tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut
   (confort, pas une nécessité).

1. **r457 — corrigé un vrai bug de garde dans `draw_pinned()` (listes de rectangles) : la vérification `draw.index_format != 1u` s'appliquait SANS CONDITION à tout tirage de liste de rectangles, y compris les tirages auto-indexés (`index_address==0`, `indexed_draw=false`) pour lesquels `index_format` vaut toujours 0 par construction (`native_xenos.cpp`, décodage `DRAW_INDX_2`). Or r435 avait établi que TOUS les tirages réels observés sont auto-indexés — donc AUCUN tirage de liste de rectangles réel ne pouvait jamais être épinglé, un rejet garanti par la structure du code, pas par la couverture du registre de nuanceurs. Corrigé en gardant la vérification par `indexed_draw &&`. Vérifié : `ctest` 11/11 (dont `ac6_native_xenos_tests`, aucune régression), et un lancement tracé contre l'ISO réelle confirme la disparition du motif de rejet « 16-bit guest indices ». **Toujours pas de contenu visible ce run** : `presented_frames=5 state=2`, capture `pixels=921600 non_black=0 distinct_colors=1` — un nouveau motif de rejet distinct est apparu pour les tirages débloqués : « MSAA render targets are not qualified this cycle », non investigué ce cycle. Toujours NON committé (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r457-real-bug-fixed-rect-list-index-format-check-wrongly-rejected-every-auto-indexed-draw-new-msaa-gap-surfaced-20260908.md`.
   **Nommé pour r458** : investiguer le motif « MSAA render targets
   are not qualified this cycle » — probablement un autre correctif
   ciblé sans besoin d'oracle, même modèle que ce cycle. Reste ouvert
   sinon : (1) le premier motif de rejet caractérisé par r456
   (couverture du registre épinglé) nécessite probablement une
   session oracle — décision utilisateur ; (2) une fois le câblage
   jugé mûr, reconsidérer le committage groupé de l'arriéré
   (r433/r434/r438/r454-r457) ; (3) mise à jour de
   `tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut
   (confort, pas une nécessité).

1. **r456 — régression d'image indéfinie (r455) CORRIGÉE et vérifiée : `publish_write_address()` détecte maintenant via `pinned_->edram_resolves()` (delta) si une résolution a réellement eu lieu ; sinon, appelle `present_target_->clear(...)` DIRECTEMENT (pas via `backend_->present_to_offscreen()`, qui doublerait le compte dans la somme additive de r455). Vérifié : `ctest` 11/11, capture réussit maintenant (`pixels=921600`=1280×720, plus d'erreur « not in a blittable layout »). `non_black=0 distinct_colors=1` — un noir uniforme, EXACTEMENT le comportement de l'ancien chemin, préservé sans régression. **Caractérisé pourquoi le vrai contenu n'apparaît toujours pas ce cycle** : 9 replis, deux motifs déjà documentés dans le code — registre épinglé de 271 variantes non exhaustif, et expansion de listes de rectangles qualifiée seulement pour index 16 bits (r265). PAS de nouveaux bugs — des limitations de couverture connues. Toujours NON committé (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r456-undefined-image-regression-fixed-verified-capture-now-succeeds-still-a-black-clear-shader-coverage-gaps-characterized-20260908.md`.
   **Nommé pour r457** : (1) étendre la couverture du registre
   épinglé pour les deux motifs de rejet caractérisés (nouvelles
   traductions oracle, qualifier l'expansion 16 bits pour d'autres
   formats) ; (2) une fois le câblage jugé mûr (r454-r456 sans
   régression connue), reconsidérer le committage groupé de
   l'arriéré. Reste ouvert sinon : mise à jour de
   `tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut
   (confort, pas une nécessité).

1. **r455 — défaut de synchronisation `presented_frames` (r454) CORRIGÉ et vérifié : `diagnostics()` additionne maintenant `backend_.present_count()` ET `pinned_runtime_->present_count()` (additifs, jamais superposés). Relancé contre l'ISO réelle : `presented_frames=5 state=2`, conforme à la ligne de base. Diagnostic de capture ajouté (`AC6_NATIVE_CAPTURE=1`, `readback_offscreen_pixels()`/`offscreen_error()`, pas de fichier/PNG, juste un comptage pixels non-noirs/couleurs distinctes). **La capture révèle un SECOND vrai défaut** : `error=offscreen image is not in a blittable layout` — sur ce run, les 8 tentatives de tirage sont TOUTES tombées en repli (aucun nuanceur épinglé ne correspondait), `edram_rt_valid_` jamais vrai, donc le présent direct de `publish_write_address` ne touche l'image NI par résolution NI par effacement de secours. **Régression réelle par rapport à l'ancien chemin** (qui effaçait toujours) — pas un crash, `presented_frames` correct, mais l'image reste indéfinie en permanence si aucun tirage épinglé ne réussit jamais. Toujours NON committé (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r455-presented-frames-sync-fixed-and-verified-capture-diagnostic-added-reveals-a-second-real-gap-20260908.md`.
   **Nommé pour r456** : garantir une image définie sur le chemin de
   présent direct même quand aucun tirage épinglé n'a réussi (effacement
   de secours conditionnel, sans réintroduire le double-effacement déjà
   corrigé) ; relancer la capture pour vérifier visuellement du vrai
   contenu. Reste ouvert sinon : reconsidérer le committage groupé de
   l'arriéré (r433/r434/r438/r454/r455) une fois mûr ; mise à jour de
   `tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut
   (confort, pas une nécessité).

1. **r454 — `PinnedShaderRuntime` câblée dans les chemins réels `VdSwap` et de vidage d'anneau (4 fichiers, NON committés, déjà tous porteurs d'arriéré). `drain_locked()` route les tirages via `execute_frame()` (avec synchro mémoire invité→SSBO, 512 Mio, r438) au lieu de `submit()` seul ; `publish_write_address()` (chemin `VdSwap` réel) route le présent via `execute_frame()` au lieu de l'effacement placeholder. **Régression trouvée et corrigée dans le même cycle** : un rejet de nuanceur non épinglé faisait échouer TOUT le lot (`presented_frames=0 state=1`, pire qu'avant) — corrigé par repli sur validation structurelle pure pour ce lot, préservant exactement le comportement d'avant r454. Relancé : `present_count` progresse 1→5 via le VRAI moteur (pas l'effacement), 8 replis propres enregistrés, `ctest` 11/11. **Un vrai défaut de synchronisation trouvé et NON corrigé** : `presented_frames` (diagnostic affiché) lit toujours le compteur de `VulkanBackend`, jamais celui de `PinnedShaderRuntime` — reste à 0 même quand le vrai moteur présente. Contenu visuel pas encore vérifié à l'œil. **NON committé** — premier jet fonctionnel, pas encore prêt (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r454-pinned-shader-runtime-wired-into-the-live-vdswap-and-ring-drain-paths-real-presents-confirmed-diagnostic-counter-gap-found-not-committed-20260908.md`.
   **Nommé pour r455** : (1) corriger le défaut de synchronisation de
   `presented_frames` et capturer une image réelle pour vérification
   visuelle ; (2) une fois mûr, reconsidérer le committage groupé de
   l'arriéré (r433/r434/r438/r454, maintenant plus entremêlé). Reste
   ouvert sinon : mise à jour de `tools/prepare.py`/`tools/build.py`
   pour le chemin ISO par défaut (confort, pas une nécessité).

1. **r453 — quantification de l'impact du correctif r452 : lancement complet tracé contre l'ISO réelle sur 25 s → **86 vrais tirages (`vd draw`) capturés**, contre 0-1 par présent dans TOUTES les captures de r425 à r450. 5 présents complets (`1280×720`), arrêt propre confirmé une troisième fois. Le correctif ne fait pas que supprimer le crash — il débloque un traitement de contenu substantiellement plus riche, cohérent avec la chaîne causale r399-r451 (le jeu peut maintenant lire/utiliser le vrai contenu PAC). Non établi : si ce contenu est RÉELLEMENT rendu à l'écran (`PinnedShaderRuntime` toujours non branché, r434/r435/r438, inchangé). Ceci rend la piste « contenu visuel réel » nettement plus intéressante à reprendre maintenant qu'il y a de vraies données à rendre (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r453-iso-launch-fix-confirmed-substantially-richer-rendering-86-draws-vs-near-zero-before-20260908.md`.
   **Nommé pour r454** : reprendre le fil du contenu visuel réel
   (`PinnedShaderRuntime` jamais branché au chemin `VdSwap`) —
   maintenant justifié par la richesse de contenu confirmée. Reste
   ouvert sinon : mise à jour de `tools/prepare.py`/`tools/build.py`
   pour le chemin ISO par défaut (confort, pas une nécessité) ;
   décision de committage de l'arriéré `native_vulkan_backend.cpp`
   (r433/r434/r438).

1. **r452 — LE CRASH EST RÉSOLU, vérifié deux fois : lancer `ac6recomp` directement contre l'ISO retail réelle (déjà présente dans ce workspace, `disc-image/Ace Combat 6 - Fires of Liberation (USA, Japan)....iso`) au lieu du répertoire `build/.../source/assets/` (qui ne contient que le XEX, cause racine de r451) **fait disparaître le `SIGSEGV`**. `NativeGuestMediaService` supporte déjà nativement un mode ISO complet (streaming direct, déjà qualifié pour `DATA00.PAC` 2,2 Gio par r240). Le manifeste de build référence l'ISO à un chemin périmé (déplacée depuis vers `disc-image/`) — artefact gitignoré, pas un fichier à corriger. **Deux lancements identiques et reproductibles** : `exit=0`, `presented_frames=5 state=2`, **`generated entry terminated its own thread`** — un arrêt PROPRE jamais vu dans toute la chaîne r427-r451. **AUCUN changement de source nécessaire** — ferme la chaîne d'investigation r399-r452 par un test positif.**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r452-crash-fixed-verified-launch-against-the-real-iso-instead-of-the-incomplete-assets-directory-no-source-change-needed-20260908.md`.
   **Nommé pour r453** : (1) mettre à jour `tools/prepare.py`/
   `tools/build.py` pour référencer par défaut le chemin actuel de
   l'ISO (confort/fiabilité, pas une nécessité) ; (2) reprendre le
   fil du contenu visuel réel (`PinnedShaderRuntime`, r434/r435/r438)
   maintenant que le boot complet fonctionne sans crash. Reste
   ouvert sinon : décision de committage de l'arriéré
   `native_vulkan_backend.cpp` (r433/r434/r438).

1. **r451 — CAUSE RACINE ULTIME ET COMPLÈTE, ferme la chaîne r399-r451 : `tools/prepare.py` (ligne ~400-402) ne copie QUE `default.xex` dans `build/ntsc-uj/source/assets/` — `DATA00.PAC`/`DATA01.PAC`/`DATA.TBL`/les paquets de contenu ne sont JAMAIS copiés, alors qu'ils existent bel et bien dans `game-files/` (et les deux ISO complètes dans `disc-image/`). Point d'arrêt matériel réarmé et laissé actif jusqu'au crash : confirme définitivement AUCUN second écrivain sur le pool (r450). La trace `NtReadFile` de r448 montre `queue+332=0` — EXACTEMENT le symptôme déjà documenté par r276 (bien avant cette session), dont le correctif existant (`NtQueryInformationFile` remplit de vraies tailles via `native_guest_media_service()`) ne peut rien faire pour un fichier que le service ne trouve jamais, puisqu'il n'a jamais été monté. **Chaîne causale complète en 10 étapes, chaque maillon soutenu par une preuve mesurée, de `prepare.py` jusqu'au `SIGSEGV`.** Reste ouvert : le chemin/format exact attendu par le service de fichiers natif, et l'extension réelle de `prepare.py` (PAS un blocage qualifié — changement à faible risque mais qui mérite son propre cycle de vérification).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r451-final-root-cause-prepare-py-never-stages-the-pac-content-files-only-the-xex-20260908.md`.
   **Nommé pour r452** : vérifier le chemin/format exact attendu par
   `native_guest_media_service()` pour `DATA00.PAC`, étendre
   `tools/prepare.py` pour copier les fichiers de contenu réels
   depuis `game-files/`, reconstruire, relancer le probe, vérifier si
   le crash disparaît enfin. Reste ouvert sinon : décision de
   committage de l'arriéré `native_vulkan_backend.cpp`
   (r433/r434/r438).

1. **r450 — correction majeure de cadrage, EN DIRECT : `0xFE` (r449) N'EST PAS de la mémoire jamais initialisée — point d'arrêt matériel sur l'octet exact (`0x173b0038`) depuis le tout début du process : `Old value=0, New value=254`, écrit par `sub_823830F0` appelé depuis `sub_821D5F48`. `sub_823830F0` décompilée = un vrai `memset` générique (motif classique alignement+mots+queue). **`sub_821D5F48` appelle `memset(pool, 0xFE, taille)` DÉLIBÉRÉMENT juste après l'allocation** — un motif de « poison »/sentinelle normal, PAS un oubli. La vraie question se déplace encore : pourquoi rien ne réécrit ce pool avec de vraies données avant que `sub_821CC508` ne le lise. Cohérent avec un pipeline de contenu asynchrone (PAC) dont le déclencheur/achèvement n'a jamais lieu dans cet environnement offline (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r450-0xfe-is-a-deliberate-memset-poison-pattern-live-watched-the-write-real-question-is-why-nothing-overwrites-it-with-real-data-20260908.md`.
   **Nommé pour r451** : tracer ce qui devrait réécrire le pool après
   le `memset(0xFE)` initial — laisser un point d'arrêt matériel armé
   plus longtemps pour confirmer définitivement l'absence totale de
   deuxième écrivain. Reste ouvert sinon : décision de committage de
   l'arriéré `native_vulkan_backend.cpp` (r433/r434/r438).

1. **r449 — la garbage de r448 (handle/décalage `NtReadFile`) remonte jusqu'à un VRAI POOL MÉMOIRE (`0x173b0000`-région, correspond à `pool=0x173b0028` déjà vu dans la trace r448). Le « handle » vient d'une lecture indexée signée (`TABLE[sign_extend(octet)]`), fidèlement traduite (vérifié source généré ET désassemblage x86). **Vérifié EN DIRECT** : l'octet d'index vaut `0xFE` (-2 signé) → index négatif → lecture AVANT la table. Vidage mémoire de 64 octets autour : motif clair, blocs de 16 octets avec un VRAI pointeur de chaînage valide vers le bloc suivant (`0x173b0030`→`0x40`→`0x50`→…, une vraie liste libre bien formée) entourés de remplissage `0xFE` UNIFORME. **Ce n'est PAS un bug de codegen** — le pool est correctement alloué et chaîné, mais son CONTENU (y compris l'octet d'index) n'a jamais été peuplé par de vraies données. Pointe vers une étape de population de données manquante (lecture PAC/fichier réelle), pas un défaut de traduction. Reste ouvert : quelle étape devrait peupler ce pool (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r449-garbage-traced-to-a-real-memory-pool-whose-payload-was-never-populated-only-its-freelist-chain-is-valid-20260908.md`.
   **Nommé pour r450** : identifier quelle étape (native ou invitée)
   devrait peupler ce pool (`0x173b0000`-région) avec de vraies
   données avant que `sub_821CC508` ne le lise — chercher les
   écrivains de cette plage mémoire. Reste ouvert sinon : décision de
   committage de l'arriéré `native_vulkan_backend.cpp`
   (r433/r434/r438).

1. **r448 — CAUSE RACINE TROUVÉE, rejoint une investigation antérieure déjà documentée (r240/r241/r275, bien avant cette session). `0x823D035C` (r447) n'est PAS du code PPC — Ghidra l'a nommé spontanément `NtReadFile`, un slot de table d'import du noyau. Ce stub natif existe déjà, extensivement instrumenté par des cycles antérieurs. Lancement avec `AC6_NATIVE_IMPORT_TRACE=1` (variable déjà supportée, jamais activée cette session) : `handle=0x82918a78` (un pointeur `FILE_OBJECT`, PAS un `HANDLE`) et `offset=0xFEFEFEFE` (motif de remplissage mémoire non initialisée) — correspondance EXACTE avec les commentaires r240/r241 déjà présents dans le code. `queue@0x829ddd80` confirme sans ambiguïté le même descripteur suivi par r443-r446. **Chaîne causale complète établie de bout en bout** : arguments incorrects → `NtReadFile` refuse correctement de fabriquer un succès (`STATUS_INVALID_HANDLE`) → 5 tentatives épuisées (r446) → `sub_821D5F48` échoue → objet de rendu jamais construit (r436-r441) → `sub_821D7DE0` continue quand même (r442) → `sub_821D6C20` déréférence le pointeur nul → `SIGSEGV`. Reste ouvert : POURQUOI le handle/décalage sont incorrects — bug de codegen ou étape d'initialisation manquante (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r448-root-cause-found-and-connects-to-earlier-campaign-r240-r241-r275-nt-readfile-garbage-handle-and-offset-20260908.md`.
   **Nommé pour r449** : identifier pourquoi le handle passé à
   `NtReadFile` est un pointeur `FILE_OBJECT` plutôt qu'un `HANDLE`
   réel, et pourquoi le décalage est non initialisé — lecture
   statique du désassemblage de `sub_821CC508`/`sub_821D5F48` autour
   du site d'appel réel (la technique `lr`/backchain s'est révélée
   inutilisable, `PPC_CONFIG_SKIP_LR`). Reste ouvert sinon : décision
   de committage de l'arriéré `native_vulkan_backend.cpp`
   (r433/r434/r438).

1. **r447 — `0x13D` (r446) tracé jusqu'à du VRAI CODE INVITÉ, PAS un stub hôte manquant — recadrage important. `sub_821F50A0` est un simple thunk vers `sub_821F75F0`. `sub_821F4E70` initialise un bloc de statut NT (`STATUS_PENDING`=0x103) puis fait un dispatch indirect via un objet noyau réel (`*0x823F07CC` → objet → `+16` → cible). **Résolu EN DIRECT** (pas statiquement) : `*0x823F07CC=0x823F07A0` (adresse `.data` valide), `*(0x823F07A0+16)=0x823D035C` — **dans `.text`, du vrai code PPC recompilé, aucun stub hôte nulle part dans cette chaîne d'appels.** L'hypothèse change : probablement une donnée/condition réelle divergente dans cet environnement offline, ou un bug de codegen localisé — pas un stub manquant. `0x823D035C` tombe dans `sub_823CFE08` (~4 Ko), non décompilée ce cycle (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r447-error-0x13d-traced-to-real-guest-dispatch-code-not-a-missing-native-host-stub-20260908.md`.
   **Nommé pour r448** : décompiler `sub_823CFE08`
   (`0x823CFE08`-`0x823D0E00`) pour comprendre précisément ce qui
   produit `0x13D`, et déterminer si c'est une condition de données
   légitime ou un bug de codegen localisé. Reste ouvert sinon :
   décision de committage de l'arriéré `native_vulkan_backend.cpp`
   (r433/r434/r438).

1. **r446 — `sub_821CC508` a sa PROPRE lecture de fichier asynchrone réelle (`sub_821F4E70`/`func_0x821f50a0` équivalent `ReadFile`/`GetLastError`), indépendante de la branche fichiers de `sub_821CC008` déjà écartée par r445. Le site d'échec confirmé (`+6171`, r444) est en réalité la fin d'une boucle de **5 nouvelles tentatives** — décrémente un compteur, réessaie tant que non épuisé, journalise puis abandonne (`-1`) sinon. **Capturé EN DIRECT** : `sub_821F50A0` (équivalent `GetLastError`) retourne un code d'erreur **constant `0x13D` (317)** sur plusieurs appels consécutifs associés à ce site — pas un code Win32 standard reconnu, pas encore documenté ailleurs dans ce dépôt. Reste ouvert : signification exacte de `0x13D`, stub hôte natif responsable (`sub_821F4E70` n'appelle qu'un helper générique en surface), et fichier/handle concerné (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r446-sub-821cc508-does-its-own-real-async-read-live-captured-consistent-error-code-0x13d-across-retries-20260908.md`.
   **Nommé pour r447** : tracer plus profondément `sub_821F4E70`
   au-delà de son helper de prologue générique pour trouver le vrai
   stub hôte natif responsable de `0x13D`, et résoudre l'index
   `pcVar19`/la table `iRam8293b94c` vers un fichier/handle concret.
   Reste ouvert sinon : décision de committage de l'arriéré
   `native_vulkan_backend.cpp` (r433/r434/r438).

1. **r445 — `sub_821D4988` décompilée (nouvel outil `scripts/DecompileD4988Logger.java`, committé) : journal d'événements générique à tampon circulaire de 64 entrées (verrou/écriture/déverrouillage/attente avec `WAIT_TIMEOUT`=0x102 comme issue normale) — PAS une chaîne de diagnostic, ce qui explique l'échec de lecture de r444 (`0x8275a414` est une valeur numérique loggée, pas un pointeur de chaîne). **Correction importante en direct** : points d'arrêt sur les instructions EXACTES de la branche « énumération de fichiers » de `sub_821CC008` (construction de chemin, ouverture) — AUCUN ne se déclenche sur ce run. `sub_821CC008` emprunte sa branche SANS accès fichier. **L'hypothèse « fichier manquant » de r443/r444 est donc écartée pour ce chemin d'exécution** — la vraie cause de l'échec de `sub_821CC508` reste à trouver ailleurs (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r445-sub-821d4988-is-a-generic-event-logger-not-a-diagnostic-string-file-hypothesis-corrected-by-live-evidence-20260908.md`.
   **Nommé pour r446** : revenir à l'intérieur de `sub_821CC508` avec
   cette information (branche fichiers de `sub_821CC008` écartée) —
   tracer en direct les valeurs réellement comparées juste avant le
   site d'échec confirmé (`+6171`) sans présupposer une cause
   fichier. Reste ouvert sinon : décision de committage de l'arriéré
   `native_vulkan_backend.cpp` (r433/r434/r438).

1. **r444 — site d'échec exact localisé EN DIRECT à l'intérieur de `sub_821CC508` : deux sites de retour `-1` distincts existent (machine à états), points d'arrêt sur les deux confirment que le SECOND (`+6171`) est réellement emprunté. Contexte immédiat : `ctx.r3 = 0x8275a414` puis `call sub_821D4988` (SANS vérification de son propre retour) puis `-1` inconditionnel — motif « journaliser puis échouer », comme `sub_821F5B18` (r442) pour un autre échec. Tentative de lecture de la chaîne à `0x8275a414` infructueuse (chaîne vide) — non résolue. Reste ouvert : sémantique de `sub_821D4988`, et tracer `sub_821CC008` pour identifier le fichier/chemin exact impliqué (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r444-live-pinpointed-the-exact-failure-exit-inside-sub-821cc508-a-call-to-sub-821d4988-immediately-before-the-minus-one-return-20260908.md`.
   **Nommé pour r445** : (1) décompiler `sub_821D4988` pour comprendre
   son rôle et pourquoi la lecture de la chaîne à `0x8275a414` a
   échoué ; (2) tracer en direct `sub_821CC008` pour identifier le
   fichier/chemin exact impliqué. Reste ouvert sinon : décision de
   committage de l'arriéré `native_vulkan_backend.cpp`
   (r433/r434/r438).

1. **r443 — pinpointé EN DIRECT (points d'arrêt sur les 4 callees candidats plutôt que sur les sites de branchement) : c'est la vérification 2 de `sub_821D5F48` qui échoue — `sub_821CC508(0x829ddd80)` retourne `0xFFFFFFFF` (-1), déclenchant le branchement `js` vers l'échec immédiatement. Vérifications 3 et 4 (`sub_821D28C8`/`sub_821D5600`) JAMAIS atteintes, confirmant que l'exécution s'arrête là. `sub_821CC508` partage le même descripteur constant `0x829ddd80` avec `sub_821CC008` (déjà analysée par r441 : énumère de vrais fichiers par chemin, taille, ouverture/fermeture) — motif classique de sondage de complétion d'une opération de contenu/fichier asynchrone qui échoue dans cet environnement offline. Piège méthodologique noté : `finish`+`$eax` a donné une valeur incohérente (0x102) ; la lecture correcte a nécessité un point d'arrêt exact sur l'instruction chargeant `ctx.r3` (cohérent avec le piège déjà connu des décalages `ctx.rN`). Reste ouvert : ce que représente `0x829ddd80` et pourquoi l'opération échoue (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r443-live-verified-sub-821cc508-returns-negative-one-pinpointing-which-of-4-checks-fails-20260908.md`.
   **Nommé pour r444** : décompiler `sub_821CC508` elle-même (déjà
   désassemblée dans le projet Ghidra `ac6-us` par r441) pour
   comprendre quelle condition la fait retourner -1, et tracer en
   direct l'appel `sub_821CC008` qui la précède pour identifier le
   fichier/chemin exact concerné et si un stub hôte natif
   (`NtCreateFile`/`NtReadFile`) est en cause. Reste ouvert sinon :
   décision de committage de l'arriéré `native_vulkan_backend.cpp`
   (r433/r434/r438).

1. **r442 — chaîne causale complète établie, sans contradiction résiduelle : `sub_821D5F48` n'a que 2 sorties (`return;`) — un échec précoce (`r3=0`, `loc_821D6138`, 4 sites convergents) et un succès tardif juste après le bloc d'écriture (déjà prouvé non atteint par r441). Par élimination, `sub_821D5F48` échoue et retourne 0 dans ce run. **Correction du cadrage « doit réussir » de r440/r441** : `sub_821F5B18` (appelé par `sub_821D7DE0` sur cet échec) est un classifieur de chaîne (motif `strcmp`), PAS un abandon fatal — et la décompilation de r440 montre déjà qu'aucun branchement de sortie ne suit cet appel. Le boot continue TOUJOURS sans condition, qu'il y ait échec ou non. Chaîne causale : `sub_821D5F48` échoue silencieusement (log non fatal) → jamais d'écriture de `*0x82935D98` → `sub_821D6C20` appelé quand même → déréférence non gardée → `SIGSEGV`. Reste ouvert : laquelle des 4 conditions échoue (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r442-must-succeed-framing-corrected-func-821f5b18-is-a-non-fatal-logger-boot-continues-regardless-20260908.md`.
   **Nommé pour r443** : identifier laquelle des 4 conditions
   (lignes 18900/19153/19629/20046 du source généré, celle de 20046
   la plus proche donc la plus économique à vérifier en premier)
   échoue réellement dans cet environnement offline — nécessite une
   corrélation adresse-hôte ↔ adresse-invité plus rigoureuse (le
   réordonnancement du code compilé a invalidé l'approche textuelle
   simple ce cycle), probablement via point d'arrêt sur les appels
   précédant chaque `goto`. Reste ouvert sinon : décision de
   committage de l'arriéré `native_vulkan_backend.cpp`
   (r433/r434/r438).

1. **r441 — `sub_821D5F48` décompilée (nouvel outil `scripts/DecompileD5F48SettingsHelpers.java`, committé) : bootstrap Xenos/renderer très clair (résolution 1280×720, tailles de tampon en Mio codées en dur — confirme et dépasse l'hypothèse de r440). Un ÉCRIVAIN RÉEL de `*0x82935D98` trouvé dans le source généré (`ppc_recomp.23.cpp:20257`, `addi r11,r11,23960` + `stw r3,0(r11)`) — **auto-correction** : cette occurrence était déjà dans les résultats `grep "23960"` de r436/r439, mal classée comme un `lwz` de plus. **Vérifié EN DIRECT** (point d'arrêt sur l'instruction hôte exacte `0x555555724df8`) : ce point d'arrêt **ne se déclenche JAMAIS** — le processus va directement de `sub_821D5F48` au crash déjà connu dans `sub_821D6C20`. **Contradiction avec r437 pleinement réconciliée** : le code écrivain est réel mais conditionnel, `sub_821D5F48` retourne dans ce run via un chemin de retour ANTÉRIEUR (parmi plusieurs imbriqués) qui ne l'atteint jamais. La vraie divergence avec le matériel réel est CETTE condition de retour antérieur, pas le bloc d'écriture lui-même (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r441-sub-821d5f48-decompiled-real-renderer-bootstrap-live-verified-the-writer-code-exists-but-executes-a-different-earlier-return-path-20260908.md`.
   **Nommé pour r442** : tracer EN DIRECT le chemin de retour
   réellement emprunté par `sub_821D5F48` (points d'arrêt sur les
   différents `return`/branchements imbriqués) pour identifier la
   condition précise qui évite le bloc d'écriture. Reste ouvert
   sinon : décision de committage de l'arriéré
   `native_vulkan_backend.cpp` (r433/r434/r438).

1. **r440 — `sub_821D6C20`/`sub_821D7DE0` désassemblées et décompilées dans le bon projet Ghidra (`ac6-us`, nouvel outil ciblé `scripts/DecompileD6C20Dispatcher.java`, verrouillé au bon SHA `6eefba42...`, committé). Établi : `sub_821D7DE0` est la boucle de jeu (motif « doit réussir » identique pour `sub_821D5F48` ET `sub_821D6C20`, appelées juste avant une boucle infinie). `sub_821D6C20` déréférence `*0x82935D98` de façon totalement non gardée comme SA TOUTE PREMIÈRE action (le site de crash), puis (branche de succès) lit ~35 valeurs de réglages indexées et les pousse dans la vtable de cet objet et d'objets liés, avec des drapeaux de capacité en fin de fonction — motif classique d'une CONFIGURATION DE PÉRIPHÉRIQUE DE RENDU/GPU à partir des réglages du titre. **Hypothèse forte** : `*0x82935D98` est le manager de rendu/GPU du jeu, jamais construit dans cet environnement offline — convergence plausible avec r425 (VdSwap sans PresentPacket) et r434/r435 (contenu visuel jamais branché). Toujours pas résolu : quel stub hôte devrait le construire (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r440-piram82935d98-decompiled-strong-evidence-of-a-renderer-device-manager-singleton-never-constructed-offline-20260908.md`.
   **Nommé pour r441** : décompiler `sub_821D5F48` (l'étape
   d'initialisation juste avant, même motif « doit réussir ») et les
   fonctions de lecture de réglages (`func_0x82234d40` et voisines)
   pour confirmer l'hypothèse et si possible relier explicitement à
   r425/r434/r435. Reste ouvert sinon : décision de committage de
   l'arriéré `native_vulkan_backend.cpp` (r433/r434/r438).

1. **r439 — recherche textuelle exhaustive des 77 unités de traduction générées (couverture bien plus large que le projet Ghidra partiel de r437) confirme aussi zéro écrivain pour `0x82935D98` (seules des `PPC_LOAD_U32`, toutes dans `sub_821D6C20`). **Correction méthodologique de r437** : le projet Ghidra interrogé (`ace-combat-6`) pointait vers `game-files/default.xex`, SHA `acc302c1...` — DIFFÉRENT du XEX retail qualifié (`6eefba42...`) que tout le reste de cette chaîne utilise. Le bon projet est `ac6-us` (pointe vers `build/ntsc-uj/source/assets/default.xex`, SHA correct) — refait les mêmes recherches contre `ac6-us` : mêmes résultats (zéro référence, `sub_821D6C20` jamais désassemblée). La conclusion de r437 tient, vérifiée cette fois contre le bon binaire. Aucune troisième piste trouvée (PAS un blocage qualifié — les deux pistes déjà nommées restent ouvertes).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r439-comprehensive-text-search-of-all-77-generated-translation-units-also-finds-zero-writer-both-r438-tracks-remain-user-decisions-20260908.md`.
   **Nommé pour r440** : (1) désassembler réellement `sub_821D6C20`
   dans le projet Ghidra `ac6-us` (nécessite un script d'amorçage non
   `-readOnly`, adapté ou nouveau, gardé au SHA qualifié
   `6eefba42...` — celui existant dans `scripts/` est verrouillé au
   mauvais SHA) ; (2) décision de committage de l'arriéré
   `native_vulkan_backend.cpp` (r433/r434/r438).

1. **r438 — SSBO de `PinnedShaderRuntime` élargi de 64 à 512 Mio (RAM physique Xbox 360 réelle, pas une estimation) dans `native_vulkan_backend.cpp` — applique et vérifie (reconstruction complète via `tools/build.py`, `ctest` 11/11, lancement réel reproduisant EXACTEMENT le SIGSEGV déjà connu de r435-r437 sur un chemin séparé, aucune interaction). **NON committé** : `git diff --stat` montre +2887 lignes sur ce fichier (l'arriéré `PinnedShaderRuntime` déjà catalogué et délibérément non committé par r433) — committer maintenant inverserait cette décision sans élément nouveau. Séparément : lacune trouvée et corrigée LOCALEMENT dans `tools/build.py` (gitignoré, non committable) — la cible `ac6_native_guest_threads_tests` (ajoutée par r424 dans `CMakeLists.txt`) n'était jamais construite par le script. Incident de cycle géré : une reconstruction manuelle a temporairement cassé `native-cmake` (variables CMake manquantes), récupérée intégralement via `tools/build.py` lui-même (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r438-pinned-shader-shared-memory-window-resized-to-512mib-not-committed-plus-build-py-guest-threads-tests-gap-fixed-locally-20260908.md`.
   **Nommé pour r439** : si l'utilisateur tranche la décision de
   committage de l'arriéré `native_vulkan_backend.cpp` (r433/r434),
   ce correctif de 512 Mio serait committé avec. Indépendamment :
   piste 1 de r437 (auto-analyse Ghidra ciblée sur
   `sub_821D6C20`/`0x82935D98`) reste ouverte si le coût est accepté.

1. **r437 — point d'arrêt matériel armé du tout début du process jusqu'au crash : le pointeur global `0x82935D98` (r436) n'est écrit NULLE PART pendant toute l'exécution — pas une question d'ordre d'exécution, aucun écrivain exercé, point final. Recherche statique de l'écrivain manquant tentée via Ghidra headless (réutilisé en lecture seule depuis le workspace voisin `ace-combat-squadron-leader/.tools/`, projet `ace-combat-6` de CE dépôt) : **bloquée** — `sub_821D6C20` n'a jamais été désassemblée/créée comme fonction dans le projet Ghidra stocké (cohérent avec le fait qu'elle n'était jamais exercée avant ce cycle de correctifs), donc les scripts de recherche par référence/adresse ne trouvent rien à chercher. Deux hypothèses non départagées : stub hôte manquant pour un service Xbox 360 non modélisé, ou contrôle conditionnel légitime qui diverge dans cet environnement offline. **Toujours pas corrigé** (PAS un blocage qualifié — auto-analyse Ghidra ciblée possible si le coût est jugé acceptable par l'utilisateur).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r437-null-pointer-confirmed-never-written-anywhere-in-run-static-search-blocked-by-unanalyzed-ghidra-region-20260908.md`.
   **Nommé pour r438** : deux pistes indépendantes — (1) si
   l'utilisateur accepte le coût, lancer une auto-analyse Ghidra
   ciblée sur la région de `0x821D6C20`/`0x82935D98` pour permettre
   la recherche statique de l'écrivain manquant ; (2) en parallèle,
   revenir à la ligne ouverte de r434/r435 sur le contenu visuel réel
   (SSBO 64 Mio de `PinnedShaderRuntime`), indépendante de ce défaut.

1. **r436 — le SIGSEGV de r435 est root-causé en direct (gdb, 3 points d'arrêt successifs sur le même lancement) : `sub_821D6C20` déréférence un pointeur d'objet global NUL à l'adresse invité `0x82935D98` (`object_ptr=0x0`, `vtable_ptr=0x0`, `ctr_candidate=0x0` — chaîne complète mesurée, pas déduite). Codegen vérifié FIDÈLE (6 instructions x86 correspondent une pour une à `ppc_recomp.23.cpp:20296`) — ce n'est PAS un bug de codegen, `PPC_CALL_INDIRECT_FUNC(0)` échoue légitimement à résoudre l'adresse invité `0x0`. Ce site n'avait JAMAIS été atteint avant ce cycle (r346, ~90 s de mesure directe, confirmait déjà zéro appel) — débloqué seulement par les correctifs r413-r424, donc pas une régression récente, un défaut préexistant révélé pour la première fois. **Pas encore corrigé** : il faut d'abord tracer qui devrait peupler ce pointeur (PAS un blocage qualifié — préalable nécessaire avant tout correctif).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r436-segv-root-caused-null-global-object-pointer-at-0x82935d98-vtable-dispatch-20260908.md`.
   **Nommé pour r437** : tracer les sites d'écriture du pointeur
   global à `0x82935D98` (statiques dans le XEX ou dynamiques via un
   stub hôte) pour déterminer si un stub hôte manquant ou un
   changement d'ordre d'exécution introduit par un correctif
   antérieur (r413/r422/r430) en est la cause, avant tout correctif
   sur `sub_821D6C20` lui-même.

1. **r435 — adresses invité réelles capturées en direct (`AC6_NATIVE_VD_TRACE`) : `index_address`/`vertex_address` toujours `0x00000000` (tirages auto-indexés), `fetch_const[0]` porte l'adresse réelle (`0x1274027b`, `0x126c01df`…) — environ **310 Mio**, très au-delà des **64 Mio** du SSBO à décalage direct de `PinnedShaderRuntime` (confirme r434 avec des valeurs mesurées, pas déduites). **Découverte séparée et prioritaire, non liée à cette instrumentation (vérifié 3 façons : trace désactivée, fichier remis à l'état du commit r432, 3 lancements consécutifs)** : un `SIGSEGV` reproductible du thread d'entrée (`sub_821D6C20` ← `sub_821D7DE0` ← `__xstart`), après plusieurs cycles `vd drain`/`vd swap` réussis — **contredit directement la clôture « aucun blocage connu » affirmée par r431/r432**. Pas encore désassemblé/root-causé ce cycle (PAS un blocage qualifié — priorité de fait pour la suite, pas un arrêt).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r435-real-draw-addresses-captured-fetch-constants-outside-64mib-window-plus-newly-reproducible-entry-thread-segv-20260908.md`.
   **Nommé pour r436 (priorité élevée)** : investiguer
   `sub_821D6C20`/`sub_821D7DE0` — désassemblage, capture gdb de
   l'état au crash, comparaison au binaire retail — pour déterminer
   si c'est un bug de codegen ou un défaut de robustesse d'un stub
   hôte, avant toute reprise du travail sur le contenu visuel réel.

1. **r434 — cause précise trouvée pour « contenu visuel réel toujours un placeholder » : `PinnedShaderRuntime` (moteur de rendu réel, `draw_pinned()`/`execute_frame()`, testé de bout en bout r256-r270) n'est appelé QUE depuis `native_xenos_tests.cpp` — jamais depuis `native_runtime.cpp`/`native_guest_vd.cpp`/`ac6recomp_main.cpp`, le chemin réel `VdSwap`. Ce n'est pas qu'un simple oubli de câblage : son SSBO mémoire partagée est borné à 64 Mio avec adressage DIRECT (`draw.vertex_address`/`index_address` utilisés tels quels comme décalage d'octet), dimensionné pour des adresses synthétiques de test (`0x1000`, `0x2000`…), pas pour de vraies adresses invité 32 bits qui peuvent se situer n'importe où dans les 512 Mio de RAM physique Xbox 360. Router `execute_frame` vers le chemin réel sans mesurer d'abord la plage d'adresses réelle échouerait probablement en `fail closed` généralisé, ou pire romprait silencieusement si un mappage était deviné. **Décision : ne pas câbler à l'aveugle ce cycle** — mesurer avant (PAS un blocage qualifié, prudence délibérée).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r434-pinned-shader-runtime-never-wired-to-live-present-path-real-address-space-mismatch-found-20260908.md`.
   **Nommé pour r435** : capturer en direct (gdb, sur le modèle de
   r426/r428/r429) les adresses invité réelles (`vertex_address`,
   `index_address`, fetch de texture) d'un vrai `DrawPacket` sur le
   chemin `drain_locked`/`submit` du jeu, pour trancher si la fenêtre
   de 64 Mio à adressage direct peut fonctionner telle quelle ou si
   `PinnedShaderRuntime` a besoin d'une refonte de son contrat mémoire
   partagée avant tout câblage vers le chemin `VdSwap` réel.

1. **r433 — catalogue précis de l'arriéré `native/` élargi (découvert par r432) : 20 chemins inventoriés, deux groupes. 14 fichiers suivis avec delta contre HEAD (dont `native_vulkan_backend.cpp` +2881/-0 sur 3015 lignes et `native_xenos_tests.cpp` +3825/-16 sur 4241 — le moteur de rendu natif et ses tests vivent presque entièrement hors de HEAD, dernière trace committée `e0afccdd` du 2026-08-31, un rattrapage précédent pour r2-r76) ; 6 chemins jamais committés (`native_pinned_shaders.*`, `native_vulkan_device.*`, la fixture binaire, le générateur `tools/`, plus le fichier scratch `.new_header_part` déjà disposé par r432). Provenance par grep des rapports : arriéré continu depuis r94 jusqu'à r432, pas récent. Ce code est celui que `ctest` exerce et fait passer depuis r413 — pas du travail spéculatif. Un précédent de committage groupé existe déjà (`e0afccdd`). **Décision de committer explicitement NON prise ce cycle** — catalogue seul, reste la décision de l'utilisateur (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r433-backlog-catalogue-no-commit-decision-taken-20260908.md`.
   **Nommé pour r434** : si l'utilisateur souhaite committer cet
   arriéré (maintenant catalogué précisément), décider du découpage
   (un seul commit de rattrapage façon `e0afccdd`, ou par
   sous-système) ; sinon, revenir aux lignes ouvertes déjà nommées par
   r431/r432 (contenu visuel réel des présents, ou fichier scratch
   Vulkan déjà disposé).

1. **r432 — correctif appliqué et vérifié : r430 double-présentait dans le chemin de secours pour tests (`readback_==0`, `guest_vd_service_present_executes_offscreen`) — trouvé par une reconstruction VRAIMENT propre (reconfiguration CMake depuis zéro + 123 cibles), pas par les lancements ciblés qui avaient validé r430. Garde `readback_ != 0u` ajoutée à l'appel direct de r430. `ctest` complet 11/11, `presented_frames=5 state=2` inchangé sur le chemin réel. **Découverte séparée et plus large** : l'arriéré non committé de `native/` dépasse largement ce que r424 avait nommé (« l'arriéré Vulkan ») — `native_guest_media`, `native_guest_memory`, `native_shader_translator`, `native_xenos`, des fichiers de test, TOUS modifiés sans committer. Décision explicite : NE PAS committer cet arriéré élargi ce cycle — reste la décision de l'utilisateur (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r432-r430-regression-fixed-full-clean-rebuild-catches-double-present-vulkan-backlog-scope-larger-than-expected-20260908.md`.
   **Nommé pour r433** : si l'utilisateur souhaite poursuivre, cataloguer
   précisément l'arriéré `native/` élargi avant toute décision de
   committer, fichier par fichier.

1. **r431 — correctif appliqué et vérifié en direct : `presented_frames` reflète enfin la réalité (`5`), `state=2` (`kRunning`). `diagnostics()` (`native_runtime.h`) synchronise désormais `presented_frames`/`state` depuis `backend_.present_count()` À LA LECTURE (pas seulement dans le code mort `submit_ring()`) — `diagnostics_` rendu `mutable`, `state()` routé à travers `diagnostics()`. Vérifié : `ctest` natif 11/11, lancement autonome propre, `presented_frames=5` identique avec et sans trace. **La chaîne r399-r431 est close, plus aucun blocage connu** (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r431-presented-frames-diagnostic-reconnected-applied-and-live-verified-20260908.md`.
   **Nommé pour r432** : aucun blocage restant. Ligne ouverte la
   moins chère si poursuivie : le contenu visuel réel des présents
   (remplacer l'effacement placeholder), ou l'audit dédié de l'arriéré
   Vulkan/registre de shaders épinglés déjà nommé par r424.

1. **r430 — correctif appliqué et vérifié en direct : `VdSwap` déclenche désormais un vrai présent Vulkan (`present_count` progresse 1→5 sur les 5 appels de la fenêtre). Le plan d'injection PM4 de r429 est ABANDONNÉ : `discover_write_index_locked` lit l'index d'écriture depuis le champ `+10952` de l'objet dispositif du jeu, PAS depuis le curseur `VdSwap` — il n'existe pas de « bon emplacement » où injecter un paquet dans l'anneau à partir de ce curseur. Cohérent avec le matériel réel : `VdSwap` programme directement l'affichage, ce n'est pas une commande PM4. Correctif = appel direct à `backend_->present_to_offscreen(...)`, aucune écriture de mémoire invitée, aucun risque de corruption. `presented_frames=0` s'affiche TOUJOURS — attendu, défaut séparé déjà nommé (r292, code mort `submit_ring()`), hors périmètre de ce correctif. `ctest` natif 11/11, lancement autonome propre (PAS un blocage qualifié — chaîne r418-r430 close pour le mécanisme de présentation).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r430-vdswap-present-fix-applied-and-live-verified-present-count-increments-20260908.md`.
   **Nommé pour r431** : reconnecter `diagnostics_.presented_frames` à
   `backend_.present_count()` sur le chemin réel, pas seulement dans
   le code mort `submit_ring()`.

1. **r429 — tampon frontal confirmé par motif de double-tamponnage (`r4+20` alterne entre exactement 2 adresses fixes sur les appels 2-5) ; dimensions réelles de la cible obtenues en direct (`1280×720`, `NativeGuestVdService::bind_offscreen`, déjà câblé dans le runtime réel — le vide de r294 est comblé). Correctif ENTIÈREMENT SPÉCIFIÉ (paquet PM4 `XE_SWAP` 5 mots, dimensions interrogées en direct, PAS codées en dur) mais PAS ENCORE appliqué : l'emplacement exact d'écriture dans l'anneau n'est pas confirmé avec assez de certitude, risque de corrompre un paquet déjà valide (PAS un blocage qualifié — prudence délibérée, pas un obstacle réel).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r429-front-buffer-and-dimensions-confirmed-fix-designed-not-yet-applied-20260908.md`.
   **Nommé pour r430** : confirmer précisément l'emplacement
   d'écriture dans l'anneau (capturer ce qui est DÉJÀ écrit dans les
   64 octets réservés à `r30`, pas seulement autour du curseur
   `r30+4`), puis appliquer le correctif conçu, reconstruire, et
   vérifier en direct que `present_count`/`presented_frames`
   progresse enfin.

1. **r428 — les 7 arguments de `VdSwap` capturés en direct (5 appels) : `r6=0x16520008` fixe (zone `readback` déjà connue) ; `r5` pointe vers un bloc de format/mode statique INCHANGÉ d'un appel à l'autre ; `r4` pointe vers un état par-appel dont le mot `+20` (candidat tampon frontal, non confirmé) prend des adresses mémoire plausibles (`0x2e33449c`, `0x8288db80`). Écarté au passage : le bloc de code juste avant l'appel `VdSwap` écrit un paquet `EventWriteShd` ordinaire (opcode `0x58`, `0xDEADBEEF` = charge utile codée en dur, pas un artefact), PAS le paquet `XE_SWAP` manquant. Décodage partiel, pas encore assez sûr pour un correctif (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r428-vdswap-argument-semantics-partially-decoded-live-20260908.md`.
   **Nommé pour r429** : confirmer si le mot `+20` de `r4` pointe vers
   une vraie surface de rendu, décoder `r7`/`r8`/`r9`/`r10`, puis
   concevoir et appliquer le correctif du stub `VdSwap`.

1. **r427 — CAUSE RACINE TROUVÉE : le jeu invité ne construit JAMAIS lui-même le paquet PM4 `XE_SWAP` — `sub_821F03B0` réserve de l'espace dans l'anneau (`sub_821E4F88`, lu en entier : n'écrit RIEN, un simple allocateur de curseur) puis transmet ses 7 paramètres de présentation (tampon frontal, palette, dimensions) EN ARGUMENTS à l'appel noyau `VdSwap`, confiant au NOYAU la construction/injection du paquet — comportement Xbox 360 réel et attendu. Notre stub hôte de `VdSwap` (`tools/materialize_native_import_stubs.py`) ne lit QUE `ctx.r3` (comptabilité de curseur via `publish_write_address`) et ignore intégralement `ctx.r4`..`ctx.r10` — il ne synthétise ni n'injecte jamais le paquet. Ce n'est PAS un défaut du jeu retail ni de la chaîne r399-r422 : une lacune d'implémentation localisée dans le stub hôte lui-même (PAS un blocage qualifié — correctif à concevoir).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r427-root-cause-found-native-vdswap-stub-never-synthesizes-the-xe-swap-packet-20260908.md`.
   **Nommé pour r428** : décortiquer les sept arguments de `VdSwap`
   (structures pointées sur la pile dans `sub_821F03B0`), puis
   concevoir, appliquer et vérifier en direct un correctif du stub qui
   synthétise réellement un paquet PM4 `XE_SWAP` et l'injecte au
   curseur réservé avant `publish_write_address`.

1. **r426 — confirmé au niveau octet : aucun paquet PM4 `XE_SWAP` (opcode `0x64`) n'apparaît jamais autour du curseur transmis à `VdSwap`, sur les 5 appels capturés — de vrais paquets PM4 existent juste avant (opcodes `0x36`/`0x46`/`0x3c`, en-têtes de type 3 authentiques). Une fenêtre de sonde 3× plus longue (75s vs 25s) ne change rien : l'anneau plafonne au même niveau, le jeu ne progresse PAS vers un swap avec plus de temps. Correction méthodologique en cours de route : `ctx.r3` de `__imp__VdSwap` vit à `ctx+0x00`, pas `ctx+0x08` (même piège que r412, corrigé avant publication) (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r426-no-xe-swap-packet-ever-appears-near-vdswap-cursor-confirmed-byte-level-20260908.md`.
   **Nommé pour r427** : lire `sub_821F03B0` ligne par ligne (et
   remonter `sub_8234F558`/`sub_8233E0A8`/`sub_8233B5A0` si
   nécessaire) pour localiser soit la condition qui gate l'émission du
   paquet `XE_SWAP`, soit ce qui bloque le jeu tôt et l'empêche de
   progresser vers son premier swap réel.

1. **r425 — `VdSwap` (le vrai appel noyau de présentation) EST appelé par le jeu invité (5x/25s, jamais vérifié avant), et son chemin de validation drain parfois du contenu réellement neuf de l'anneau (`vd swap commit`, 1/5 appels) — mais MÊME ALORS, le lot décodé ne contient jamais de paquet `Present`, malgré du rendu réel (jusqu'à 50 tirages dans un autre lot). Ferme l'hypothèse r292-r296 (bug de scrutation/découverte) : la découverte fonctionne, le drainage a lieu, il manque simplement le paquet `XE_SWAP` lui-même dans le flux (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r425-vdswap-called-but-never-carries-a-present-packet-even-on-clean-commit-20260908.md`.
   **Nommé pour r426** : lire `sub_821F03B0` (l'appelant direct de
   `VdSwap`) en entier pour localiser où/si le jeu construit un paquet
   PM4 `XE_SWAP` avant d'appeler ce noyau, et si sa structure
   correspond à ce que `native_xenos.cpp:401` attend.

1. **r424 — test ciblé ajouté et vérifié pour la classe de régression IRQL de r421/r422 : `native/tests/native_guest_threads_tests.cpp` (nouveau, `ac6_native_guest_threads_tests`, `TIMEOUT 10`) — simule un thread qui élève l'IRQL (imbriqué) puis le libère via `release_residual_dpc_level()` sans jamais appeler `lower_dpc_level()` (exactement le déroulement `GuestThreadTerminated`), vérifie qu'un second thread progresse ensuite ; vérifie aussi que l'exclusion mutuelle réelle entre threads n'est pas affaiblie. `ctest` natif 11/11 (nouvelle cible incluse), `ac6recomp` toujours propre en lancement autonome (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r424-regression-test-added-for-r421-r422-irql-fix-20260908.md`.
   **Nommé pour r425** : aucun blocage restant identifié. Revenir à
   `presented_frames=0`/`present=0` (r418, jamais résolu) comme
   prochaine question produit.

1. **r423 — CORRIGE r422 : son commit `2c07b1ec` a balayé un arriéré non committé de neuf cycles antérieurs (`r239, r240, r255, r276, r278, r280, r281, r282, r285, r286`) sur trois fichiers, en plus du correctif IRQL décrit dans son message — contenu cohérent et déjà vérifié par les suites de tests de r422 (rien n'est cassé), mais le message de commit ne le mentionnait pas. Pas de revert/amend (contenu sain, précédent `e0afccdd` pour ce type de rattrapage) — correction de traçabilité uniquement (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r423-corrects-r422-commit-scope-backlog-swept-in-20260908.md`.

1. **r422 — correctif appliqué et vérifié en direct : `raise_dpc_level()`/`lower_dpc_level()`/`release_residual_dpc_level()` (nouveau, `native_guest_threads.h/.cpp`) — garde d'exception qui libère l'IRQL résiduel d'un thread au moment où `GuestThreadTerminated` est attrapée, au lieu d'orpheliner `g_dpc_level_mutex` pour toujours. `tools/materialize_native_import_stubs.py` régénère `KeRaiseIrqlToDpcLevel`/`KfLowerIrql` et le catch `ExCreateThread` en conséquence ; même garde ajoutée dans `ac6recomp_main.cpp`. **4/4 lancements AUTONOMES** (pas sous gdb, avec ET sans `AC6_NATIVE_VD_TRACE=1`) sortent proprement (`exit=0`), dans le délai exact de leur fenêtre de sonde. `ctest` natif 10/10, `pytest` 222/1-skip, gates + `ctest` racine verts (PAS un blocage qualifié — chaîne r418-r422 close).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r422-irql-mutex-fix-applied-and-live-verified-shutdown-hang-gone-20260908.md`.
   **Nommé pour r423** : aucun blocage restant identifié. Ligne ouverte
   la moins chère : un test ciblé simulant « IRQL élevé puis
   `GuestThreadTerminated` ». Sinon, revenir à `presented_frames=0`/
   `present=0` (r418, jamais résolu) comme prochaine question produit.

1. **r421 — le thread précis identifié EN DIRECT (contournement `ptrace_scope` : gdb LANCE le processus au lieu de s'y attacher) : bloqué dans `__imp__KeRaiseIrqlToDpcLevel`, en train d'exécuter du vrai code de jeu (10 niveaux PPC imbriqués, `sub_821F8008<-...<-sub_823A8F90`), sur un `std::recursive_mutex` global `g_dpc_level_mutex` (`tools/materialize_native_import_stubs.py:186`, choix de conception r191 documenté) — implémenté en `lock()`/`unlock()` BRUT, sans garde RAII, architecturalement inévitable puisque Raise/Lower sont deux fonctions PPC générées séparées. Une exception `GuestThreadTerminated` (r277/r417) levée entre les deux orpheline le verrou pour toujours. Explique tout r418-r420 (PAS un blocage qualifié — correctif à concevoir, pas de conception unilatérale).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r421-hang-thread-pinned-live-blocked-on-non-raii-global-irql-mutex-20260908.md`.
   **Nommé pour r422** : concevoir et vérifier en direct un correctif
   (IRQL réellement `thread_local`, ou garde d'exception qui libère
   l'IRQL résiduel dans le chemin `GuestThreadTerminated`) ; confirmer
   que `shutdown()` retourne en lancement AUTONOME (pas seulement sous
   gdb, cf. r419).

1. **r420 — cause racine du blocage d'arrêt trouvée SANS capture en direct : `VulkanDevice::~VulkanDevice()` est correct et complet (`vkDeviceWaitIdle`/`vkDestroyDevice`/`vkDestroyInstance`) — RÉFUTE l'hypothèse de r419. La vraie cause : `NativeRuntime::shutdown()` appelle `native_guest_threads_stop_and_join()` EN PREMIER, qui fait un `thread.join()` INCONDITIONNEL et SANS DÉLAI sur chaque thread invité `ExCreateThread` enregistré. Le mécanisme d'arrêt propre (r277/r417) ne fonctionne que si le thread ciblé est précisément dans un stub d'attente au moment de l'arrêt — sinon `.join()` bloque pour toujours, `shutdown()` ne retourne jamais, `runtime` (donc `offscreen_device_`) n'est jamais détruit. Explique tout ce qu'ont observé r418/r419 sans capture en direct (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r420-shutdown-hang-root-caused-unconditional-timeoutless-thread-join-20260908.md`.
   **Nommé pour r421** : identifier PRÉCISÉMENT quel thread invité
   bloque et pourquoi (trace de diagnostic bornée, ou contournement
   `ptrace_scope` si l'utilisateur l'autorise) avant de concevoir un
   correctif — ne pas se contenter d'un délai arbitraire sur `.join()`.

1. **r419 — le blocage d'arrêt de r418 CONFIRMÉ réel, PAS spécifique à `AC6_NATIVE_VD_TRACE` : un lancement autonome (sans gdb, sans le drapeau de trace) ne se termine JAMAIS de lui-même après avoir imprimé `generated entry terminated its own thread` (`timeout 60` sans effet, `3:49` CPU observé avant arrêt forcé). CORRECTION MÉTHODOLOGIQUE : toutes les vérifications "sortie normale" de r414-r417 étaient sous gdb, dont le `quit` de fin de script tue l'inférieur de force — aucune ne prouve une vraie auto-terminaison. Inventaire des fils : des threads pilote Vulkan (`vkcf`/`vkrt`/`vkps`) sont désormais VIVANTS, jamais observés avant r414 — hypothèse non vérifiée : destruction Vulkan (`vkDestroyDevice`/`vkDestroyInstance`) manquante de la séquence d'arrêt (PAS un blocage qualifié — l'attache ptrace a été refusée par ce bac à sable, pas de contournement tenté).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r419-process-never-exits-standalone-not-vd-trace-specific-vulkan-driver-threads-now-alive-20260908.md`.
   **Nommé pour r420** : lire `native/src/native_vulkan_backend.cpp`
   (jamais lu) pour localiser la séquence de destruction Vulkan et si
   `shutdown()` l'appelle réellement.

1. **r418 — `presented_frames=0` est du câblage mort déjà documenté par r292 (`submit_ring()` jamais appelé par le vrai runtime, seul appelant = un test), SANS RAPPORT avec r399-r417. Le vrai chemin GPU (`NativeGuestVdService`) montre une nette amélioration sur le binaire corrigé par r414 : l'anneau ne se tait plus après une salve (11 salves contre 5 avant, `write_index` en progression continue) — mais `present=0` dans TOUTES les salves, aucun paquet de présentation jamais émis. Nouveau symptôme sans rapport apparent : le processus met ~100-150s de trop à sortir avec `AC6_NATIVE_VD_TRACE=1` activé, non corrélé de façon confirmée à ce drapeau (PAS un blocage qualifié — piste ouverte, pas caractérisée).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r418-gpu-ring-no-longer-goes-silent-still-zero-present-packets-new-shutdown-hang-with-vd-trace-20260908.md`.
   **Nommé pour r419** : (1) isoler si le nouvel arrêt de processus
   long est spécifique à `AC6_NATIVE_VD_TRACE=1` avant de le qualifier
   de blocage réel ; (2) si confirmé indépendant, tracer où le paquet
   `Present` invité serait censé être émis (`sub_821D7AE0`/
   `sub_821D7CD0`, toujours jamais lus) et pourquoi il ne l'est jamais.

1. **r417 — CORRIGE r416 : la boucle par image tourne bien de façon soutenue (`sub_821D7DE0` : `goto` inconditionnel, vraiment infinie ; 292 déclenchements de `NtWaitForSingleObjectEx` capturés sur 5s côté `sub_82331E78`, cadence quasi constante ~16-17ms — un rythme réaliste, pas un blocage). « `generated entry terminated its own thread` » est le mécanisme d'arrêt PROPRE et voulu de r277 (vérifié dans `tools/materialize_native_import_stubs.py` : les stubs d'attente lancent `GuestThreadTerminated` sur `stop_requested()`), pas un nouveau point d'arrêt côté jeu (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r417-per-frame-loop-runs-sustained-terminated-message-is-designed-shutdown-not-a-bug-20260908.md`.
   **Nommé pour r418** : lire `sub_821D7AE0`/`sub_821D7CD0` (jamais lus)
   pour localiser où `presented_frames` devrait s'incrémenter et
   pourquoi il reste `0` malgré une boucle qui tourne visiblement à un
   rythme réaliste — probablement sans rapport avec la chaîne
   allocateur r399-r415, plus probablement lié à l'absence d'une vraie
   surface d'affichage dans ce harnais de sonde sans fenêtre. Écart non
   résolu, à noter : r416 n'avait capté que 2 entrées de
   `sub_821D7AE0`/`sub_821D7CD0` sur 120s, en contradiction apparente
   avec les 292 déclenchements de `sub_82331E78` sur 5s ici — vérifier
   si c'est un artefact du script r425 avant de conclure quoi que ce
   soit sur la fréquence réelle de ces deux fonctions.

1. **r416 — la frontière de boot avance : `sub_821D5F48` (jamais retourné à travers seize constats depuis r358, voir `reports/handoff/CURRENT.json`) RETOURNE désormais, `sub_821D7AE0`/`sub_821D7CD0` (la boucle par image, jamais atteinte) s'exécutent — MAIS seulement DEUX fois, puis le thread du point d'entrée se termine de lui-même (`generated entry terminated its own thread`) et `presented_frames` reste `0`. Reproduit à l'identique sur 2 lancements indépendants (timing quasi identique, `+1.9s`) (PAS un blocage qualifié — nouveau point d'arrêt, pas résolu).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r416-boot-frontier-advances-sub821d5f48-returns-first-time-ever-per-frame-loop-runs-twice-then-thread-exits-20260908.md`.
   **Nommé pour r417** : lire le code PPC de `sub_821D7AE0`/
   `sub_821D7CD0` et de leur appelant pour déterminer si l'arrêt après
   deux itérations est une sortie normale (sous-système à deux passes,
   pas "la" boucle de présentation) ou un nouveau blocage empêchant
   une boucle soutenue.

1. **r415 — le double-octroi ORIGINAL de r399 a disparu : rejoué SANS MODIFICATION le repro exact de r398 (`sub_8236E868`, requêtes taille-1/taille-256, `+294`/`+343`) sur le binaire corrigé par r414. r398 : les deux requêtes recevaient `0x10082ab0`, déterministe sur 3 lancements — LE symptôme qui a motivé la qualification "r399". Ce cycle : quatre adresses toutes DISTINCTES (`0x10024200`/`0x100a4520`/`0x100b1fc0`/`0x100b1fe0`), déterministe sur 2 lancements. Preuve directe, pas une inférence indirecte comme le saut `883→67239` de r414 (PAS un blocage qualifié).**
   Voir
   `reports/ac6-retail-native-codegen-gate2-r415-r399-double-issue-resolved-confirmed-against-original-r398-repro-20260908.md`.
   **Nommé pour la suite** : la chaîne causale précise entre le
   débordement précoce (`__xstart`, r410-r412) et CE symptôme tardif
   précis (`sub_8236E868`) n'a jamais été tracée lien par lien — jugée
   non nécessaire pour clore ce fil (disparition déterministe du
   symptôme jugée suffisante), mais nommée si un doute resurgit. Le fil
   r399-r415 est considéré clos en l'absence d'un blocage qualifié
   exigeant plus.

1. **r414 — correctif appliqué et vérifié en direct : `sub_821FA9E0` retourne désormais le bon pointeur (nouveau script `apply_sub_821fa9e0_leave_return_fix.py`, sauvegarde/restauration de `ctx.r3` autour de l'appel `RtlLeaveCriticalSection` de r366). Rejoué SANS MODIFICATION le harnais `r424_return_chain.gdb` : les trois étages (`sub_821FA9E0`/`sub_823857E0`/`sub_8237FA50`) propagent maintenant `0x100015a0` au lieu de `0`, 3/3 croissances capturées. Signal indirect fort : le compteur alloc/free combiné passe de `883` à `67239` sur le même run — le tableau croissant statique C++ grandit désormais normalement au lieu de déborder silencieusement (PAS un blocage qualifié).**
   Processus reconstruit (`ninja ac6recomp`) et testé stable (sortie
   normale, aucun crash introduit). Voir
   `reports/ac6-retail-native-codegen-gate2-r414-return-value-fix-applied-and-verified-live-20260908.md`.
   **Nommé pour r415** : rechercher si un double-octroi équivalent à
   `seq≈874`/`877` (r401-r409) se produit encore ailleurs dans la
   séquence désormais bien plus longue (`67239` événements), ou si ce
   correctif ferme réellement le blocage r399 original.

1. **r413 — le commit manquant de r411 EXPLIQUÉ : le correctif INVENTÉ de r366 (`tools/apply_sub_821fa9e0_leave_fix.py`, fuite de section critique, 2026-09-07) réutilise `ctx.r3` comme registre de travail pour l'appel `RtlLeaveCriticalSection` SANS sauvegarder/restaurer la valeur de retour qu'il vient de recharger deux lignes plus haut — `sub_821FA9E0` retourne donc `0` au lieu du nouveau pointeur `0x100015a0` à chaque fois que le verrou a été pris (systématiquement, pour toute croissance réelle). CE N'EST PAS un bug retail : c'est une régression introduite par ce correctif appliqué à la recompilation elle-même (PAS un blocage qualifié).**
   `r424_return_chain.gdb` capture les trois étages en un seul run :
   `sub_821F9E10` retourne correctement `0x100015a0` ; `sub_821FA9E0`
   (le `realloc()` qui l'englobe) retourne `0x0` ; `sub_823857E0` et
   `sub_8237FA50` propagent fidèlement ce `0` déjà perdu. Désassemblage
   x86 statique (`sub_821FA9E0+2637`..`+2678`) localise l'instruction
   exacte : le rechargement correct (`lwz r3,356(r31)`, `+2637`) est
   immédiatement suivi, si le drapeau "verrou tenu" est mis, par
   `add $0x580,%ebp; ...; mov %rax,(%rbx)` (`+2657`..`+2669`) qui
   écrase `ctx.r3` avec le pointeur de section critique
   (`r27+1408=r27+0x580`, le même offset que `RtlEnterCriticalSection`)
   avant `call RtlLeaveCriticalSection` — jamais restauré ensuite. Voir
   `reports/ac6-retail-native-codegen-gate2-r413-missing-commit-explained-r366-invented-fix-clobbers-its-own-return-value-20260908.md`.
   **Nommé pour r414** : appliquer et vérifier en direct un correctif
   qui sauvegarde `ctx.r3` avant le bloc `if` et le restaure après
   `RtlLeaveCriticalSection`, confirmer par capture live que
   `sub_821FA9E0` retourne désormais le bon pointeur, que
   `sub_8237FA50` commet `begin`, et mesurer si le débordement
   r410/r411/r412 disparaît sur un run complet.

1. **r412 — cause racine confirmée au niveau instruction : `sub_821F90A8` (requête de capacité, sous `sub_82385AF0`) retourne délibérément `-1` (sentinelle "libéré") une fois le tampon rendu au tas ; `sub_8237FA50` compare ce retour au besoin avec `cmplw` (NON SIGNÉ) — `-1` devient `0xFFFFFFFF`, "capacité maximale", et la revérification de capacité est désactivée pour toujours après la première libération. Capturé en direct sur 188 requêtes consécutives : `0x80`(=128, correct) tant que le bloc est marqué en cours d'utilisation, `-1` systématiquement après (PAS un blocage qualifié).**
   `r422_capacity_query.gdb` — lecture CORRIGÉE (la valeur de retour
   de `sub_821F90A8` est stockée dans `ctx.r3` en mémoire, PAS laissée
   dans `%rax` au `ret` x86 ; une première tentative lisant `$rax`
   donnait systématiquement `0`, contredisant la dérivation depuis le
   code source — désassemblage x86 statique a montré la vraie
   convention). C'est ce défaut, pas le saut de commit de `begin`
   documenté par r411, qui rend le débordement PERMANENT : même si le
   commit avait fonctionné et fait pointer le tableau vers le nouveau
   tampon (`0x100015a0`), sa PROCHAINE libération aurait déclenché
   exactement le même défaut de sentinelle. Voir
   `reports/ac6-retail-native-codegen-gate2-r412-root-cause-nailed-negative-one-sentinel-read-as-unsigned-max-20260908.md`.
   **Nommé pour la suite** : relire `loc_8237FAF0` (le saut de commit
   non expliqué de r411) reste ouvert mais n'est plus bloquant pour
   comprendre le débordement lui-même ; déterminer si ce défaut existe
   dans le binaire retail réel (aucun oracle utilisé pour ce cycle) ou
   proposer/qualifier un correctif est la prochaine décision (précédent
   r1111/r1113 : ne pas deviner sans preuve).

1. **r411 — le commit du redimensionnement du tableau croissant ne s'exécute JAMAIS : `begin` (`0x82a5eef0`) n'est écrit qu'UNE SEULE FOIS dans tout le run (`seq=2`, création), jamais après — y compris après une croissance réelle et confirmée en direct (`alloc(256o)->0x100015a0`, `free(0x10000770)`, `seq=5`/`6`). Le tableau continue ensuite d'écrire indéfiniment dans son tampon déjà libéré (`end` avance de 4 octets à chaque appel, 17 déclenchements consécutifs captés, `seq` figé à `6` — aucune nouvelle allocation ne se produit) : un débordement de tas NON BORNÉ, pas un simple use-after-free ponctuel. CORRIGE/PRÉCISE r410 (la lecture complète de `sub_821FA9E0` réfute l'hypothèse d'un bug dans le `realloc()` lui-même — il est textuellement correct) (PAS un blocage qualifié).**
   `r419_grow_alloc_ret.gdb` capture l'allocation interne en direct
   (entrée ET retour) : `seq=5`, `r5(size)=0x100`, pile
   `sub_821F9E10<-sub_821FA9E0<-sub_823857E0<-sub_8237FA50<-
   sub_8237FB58<-sub_821F7B28<-__xstart`, retour `r3=0x100015a0` — une
   adresse neuve, distincte. `r421_begin_full_history.gdb` (point
   d'observation matériel sur `begin` armé dès l'entrée du
   constructeur, run complet jusqu'à `total-seq=883`) montre que
   `0x100015a0` n'est JAMAIS stocké dans `begin` — le chemin de commit
   `loc_8237FAF0` (texte cité par r410) n'exécute pas malgré un retour
   non nul confirmé. `r420_globals_watch.gdb` montre que `end` continue
   ensuite d'avancer de 4 octets par appel, sans aucun nouvel
   alloc/free, jusqu'à au moins `0x10000834` (17 déclenchements) —
   la capacité n'est jamais revérifiée avec succès. Voir
   `reports/ac6-retail-native-codegen-gate2-r411-grow-commit-never-executes-array-overflows-its-freed-buffer-forever-20260908.md`.
   **Nommé pour r412** : lire `sub_82385AF0` (requête de capacité) en
   entier, ou isoler par point d'arrêt x86 la branche exacte
   `cmplwi r3,0`/`bne` juste après le premier `bl sub_823857E0` de
   `sub_8237FA50` pour capturer en direct `cr0`/`r3` à cet instant
   précis lors de l'épisode `seq=5`/`6`.

1. **r410 — l'écrivain de `0x100007f0..+0xc` n'est PAS un sous-système sans rapport : c'est le tas général lui-même, agissant comme client de sa propre API. `sub_8237FA50` (`push_back` d'un tableau croissant global, appelé depuis la boucle des constructeurs statiques C++, `sub_821F7B28`) libère son propre tampon (`free(0x10000770)`, pile d'appel exacte capturée : `sub_821FA6F8<-sub_821FA9E0<-sub_823857E0<-sub_8237FA50<-sub_8237FB58<-sub_821F7B28<-__xstart`) puis continue d'écrire à travers un pointeur de fin resté périmé — un use-after-free interne à un agrandissement de tampon, pas une corruption externe. CORRIGE la version précédente de r410 elle-même (« sans rapport avec l'allocateur », réfutée par la lecture directe du code PPC de `sub_8237FA50`/`sub_821F7B28`) (PAS un blocage qualifié).**
   `0x10000770` = exactement le pointeur retourné par `seq=2` (r409,
   128 octets) ; le tampon du tableau croissant global (globales de
   contrôle invité `0x82a5eef0`/`0x82a5eeec`) EST ce bloc. `end` du
   tableau (`0x82a5eeec`) atteint la borne de capacité exacte
   (`0x10000770+0x80=0x100007f0`) puis la dépasse d'un mot à chaque
   appel, tandis que `begin` (`0x82a5eef0`) reste bloqué à `0x10000770`
   sur les quatre écritures capturées — alors qu'un `free()` de ce même
   pointeur vient d'avoir lieu dans le MÊME appel. Ceci ferme la boucle
   avec r409 : `loc_821FA288` ne fait que propager une valeur déjà
   écrite par ce use-after-free. Voir
   `reports/ac6-retail-native-codegen-gate2-r410-mechanism-traced-to-x86-neighbor-coalesce-merges-live-block-20260908.md`.
   **Nommé pour r411** : lire directement le code PPC de
   `sub_823857E0`/`sub_821FA9E0` pour trancher entre registre `r30`
   périmé après l'appel, ou `sub_823857E0` qui échoue à agrandir
   réellement le tampon avant de libérer l'ancien.

1. **r409 — origine réelle localisée en direct : une découpe à `seq=8` (alloc de 80 octets) écrit un reliquat de `0x823f` unités là où `3` étaient dues, créant un nœud fantôme de 533 Ko `[0x100007c0,0x10082bb0)` — l'adresse même au centre de r399-r408. Première conséquence concrète confirmée : chevauchement mémoire à `seq=284`, 590 appels avant la chaîne r399. CORRIGE r408 (`seq=348` était une écriture de comptabilité intermédiaire pendant un free ordinaire, pas un free anormal) (PAS un blocage qualifié).**
   **CORRIGÉ PAR r410 CI-DESSUS** : l'affirmation « l'erreur naît
   pendant le traitement de la découpe elle-même, pas d'un en-tête déjà
   corrompu » ci-dessous est fausse — r410 (`r414_header_trace.gdb`) a
   montré que l'en-tête était DÉJÀ garbage à `seq=8`-pre, avant même que
   `sub_821F9E10` n'exécute `loc_821FA288`. La cause est le
   use-after-free documenté par r410, pas la découpe elle-même.
   En-tête du bloc trouvé (le bloc alloué `seq=2`/128 octets, libéré
   `seq=6`) confirmé CORRECT (`9` unités) juste avant `seq=8` — l'erreur
   naît pendant le traitement de la découpe elle-même, pas d'un en-tête
   déjà corrompu. Quatre écritures capturées en direct (point
   d'observation matériel armé seulement à l'entrée de l'appel ciblé,
   technique affinée après l'incident r408c), chaîne d'appel jamais vue
   dans ce fil (`sub_821F7A88<-sub_821F59E0<-sub_821D74A8<-
   sub_821DE8D8<-sub_82346B48<-sub_8233E1D8<-sub_8234F2C8`). À
   `seq=284`, la première découpe du nœud fantôme (`r5=0x8c`, retourne
   `0x100007d0`) chevauche physiquement le bloc encore vivant alloué à
   `seq=3` (`[0x100007f0,0x10000d80)`, jamais libéré) — le premier
   chevauchement mémoire confirmé du run entier. À `seq=349`, le
   fragment `[0x10080000,0x10082bb0)` du nœud fantôme porte la MÊME
   signature "lien retour NUL" que r403 avait documentée pour
   `0x10082aa0` — la même anomalie de chaînage, pas un mécanisme
   défensif. Corroboration indépendante : le compteur d'unités libres
   du tas (`heap+0x30`) passe négatif entre `seq=200` et `seq=300`. Voir
   `reports/ac6-retail-native-codegen-gate2-r409-phantom-node-created-at-seq8-writer-pinned-20260908.md`.
   **Nommé pour r410** : convertir les trois adresses hôte des
   écritures du champ de taille en labels PPC (comptage d'instructions,
   technique r403) ; capturer en direct le registre/champ source de
   `0x823f` ; lire la nouvelle chaîne d'appel si nécessaire (précédent
   r1111/r1113).

1. **r408 — CORRIGÉ PAR r409 CI-DESSUS : `seq=348` était une écriture de comptabilité intermédiaire pendant un free ordinaire, pas un free anormal. Recontextualise toute la chaîne r358-r407 : le tas octet-par-octet écrit dans une page RÉSERVÉE-MAIS-NON-COMMISE dès `seq=348` (529 appels avant `seq=877`, le pool). Sur matériel réel ceci lèverait une violation d'accès — le titre s'exécute sur console, donc la divergence recomp/retail précède TOUT ce que r358-r407 ont examiné. Origine non établie (PAS un blocage qualifié).**
   Deux runs combinés par corrélation de `seq` (déterministe, confirmé
   d'un run à l'autre) : (r408b, tous les appels
   `NtAllocateVirtualMemory` du run) aucune commission capturée entre
   `seq=0` (réserve+commet `[0x10000000,0x10010000)`, pile
   `sub_821F9860<-sub_821F7D50<-sub_821F7E28<-__xstart`, l'init du tas
   déjà identifié par r404) et `seq=877` ne couvre `0x10080000` ;
   (r408c, points d'observation matériel sur `0x10080000` et
   `0x10082aa0`, armés dès le début du processus, plafonné à 14
   déclenchements) TOUS les déclenchements portent sur `0x10080000`, le
   premier dès `seq=348` — 529 appels avant `seq=877` — via une
   activité alloc/free/coalescence tout à fait ordinaire du tas
   octet-par-octet (`sub_821F94F8<-sub_821FA6F8`, `sub_821F9E10`,
   `sub_821F85F8`). Le double-octroi bucket-17 documenté par r404-r407
   n'est donc probablement qu'un SYMPTÔME tardif d'un état de tas déjà
   faux dès `seq≈348` ou avant, pas la cause elle-même. Anomalie notée
   sans interprétation : le premier accès capturé est un `free()`, pas
   un `alloc()` — aucune allocation observée n'a émis ce bloc. Voir
   `reports/ac6-retail-native-codegen-gate2-r408-commit-tracking-lags-byte-level-heap-real-overlap-locus-still-open-20260908.md`.
   **Nommé pour r409, dans l'ordre** : (1) lire `sub_821F9860` (init du
   tas) pour son dimensionnement initial ; (2) instantané précoce
   (`seq≈5`) de `heap+384`/du descripteur `0x10000630` ; (3) étendre la
   capture de séquence avec le RETOUR de `sub_821F9E10`
   (`r5_in`/`r3_out`) pour chercher le premier retour `0x1008xxxx`
   avant `seq=348` ; (4) placer `MmAllocatePhysicalMemoryEx` (suspect
   nommé par r389, jamais mis sur cette chronologie) sur le même
   compteur (précédent r1111/r1113).

1. **r407 — `sub_821F92B8` lue en entier : DEUX chemins possibles (table de 64 segments existants / réserve-puis-commit via `NtAllocateVirtualMemory`) ; un seul segment existe dans l'instantané déjà capturé et semble épuisé (`+0x30=12`), mais lequel des deux chemins l'appel `seq=877` a pris N'EST PAS établi (PAS un blocage qualifié).**
   Suite de r406 : `sub_821F92B8` (`ppc_recomp.27.cpp:12734`) commence
   par une recherche dans une table de 64 pointeurs de segments
   (`heap+96`..`+352`). Un seul slot non nul dans l'instantané déjà
   capturé par r401 (`heap2_call1_size1.bin`) : segment
   `[0x10000000, 0x10100000)` (1 Mo), dont le champ comparé à la taille
   demandée (`+0x30`) vaut `12` — bien inférieur aux deux requêtes de ce
   fil (8888 octets, `0x80310` octets), suggérant un segment épuisé.
   Si aucune entrée ne convient, la fonction appelle
   `NtAllocateVirtualMemory` (`0x823D037C`) : RÉSERVE seule
   (`0x60002000`, boucle de repli qui divise par deux) puis COMMIT
   séparé (`0x60001000`) — corrige une première version de ce rapport
   qui décodait `0x60002000` comme `RESERVE|COMMIT` combinés sans avoir
   vérifié les constantes dans `xtypes.h`. Le stub hôte délègue à
   `BaseHeap::Alloc` (`upstream/.../rexglue-sdk/src/system/xmemory.cpp`,
   2070 lignes, non lue), dans le MÊME fichier que
   `MmAllocatePhysicalMemoryEx` — le candidat que r389 avait déjà mis
   en cause pour un chevauchement structurellement identique ; le
   rapprochement est donc plus direct qu'il n'y paraissait, sans être
   confirmé. **Quel chemin `seq=877` a réellement pris N'EST PAS
   capturé ce cycle** (le journal de r406 ne loggue que l'ENTRÉE de
   `sub_821F92B8`, pas ses branches internes) — une première version de
   ce rapport l'affirmait à tort, corrigée avant publication. Voir
   `reports/ac6-retail-native-codegen-gate2-r407-sub821f92b8-read-calls-ntallocatevirtualmemory-not-mmallocatephysical-20260908.md`.
   **Nommé pour r408** : étendre la capture fusionnée avec des points
   d'arrêt sur `sub_821F8368` (chemin segment existant) ET
   `NtAllocateVirtualMemory` (chemin noyau) pour trancher directement
   lequel `seq=877` emprunte ; si noyau, capturer la plage retournée et
   la comparer à `0x10082aa0` (précédent r1111/r1113).

1. **r406 — corrige r404 : ordre exact établi en direct par capture fusionnée (points d'observation matériel + compteur de séquence). Le nœud du bucket 17 (`0x10082aa0`) est un reliquat ORDINAIRE créé par un split normal (`seq=874`) ; le pool de `sub_8236E868` (`seq=877`, trois appels plus tard, zéro libération entre les deux) est obtenu via `sub_821F92B8` (chemin "croissance", jamais lu) et recouvre cette adresse déjà distribuée (PAS un blocage qualifié).**
   Une relecture du journal de libérations déjà capturé par r404
   (`r405_free_check_r5.log`) a montré que 40 des 47 pointeurs libérés
   tombent DANS la plage du pool (`[0x10011c60, 0x10091f80)`) — mais
   TOUS avant sa création (`seq<=838` contre `seq=877` pour le pool,
   aucune libération entre les deux) : réutilisation légitime côté
   petits blocs, qui n'explique PAS le chevauchement. Une capture
   fusionnée (les trois points d'observation matériel de r404 PLUS un
   compteur de séquence partagé sur les entrées de
   `sub_821F9E10`/`sub_821FA6F8`/`sub_821F92B8`, même run) établit
   l'ordre exact : `seq=874` (appel `r5=0x22b8=8888` octets, chaîne
   `sub_821F7A88 <- sub_821F59E0 <- sub_821D74A8 <- sub_823B86D0 <-
   sub_823B8770`, identique à celle déjà relevée par r404) insère
   `0x10082aa0` dans le bucket 17 comme reliquat ordinaire d'un split —
   légitime à ce moment. `seq=876` et `seq=877` (le pool, `r5=0x80310`)
   tombent tous deux dans `sub_821F92B8` (arbre de grands blocs épuisé
   après le split de `874`) ; pendant `877`, `sub_82372128` (le
   formateur du pool) écrase les champs de chaînage retour du nœud
   `0x10082aa0` — même mémoire physique, confirmé indépendamment de
   r404. **Le sens de r404 était inversé** : ce n'est pas une insertion
   tardive dans un bloc déjà remis, c'est `sub_821F92B8` qui, en
   croissant le tas, rend une plage qui recouvre une adresse déjà
   distribuée au niveau octet — structurellement le même type de défaut
   que celui documenté par r389 pour `MmAllocatePhysicalMemoryEx`, mais
   sur un chemin jamais rapproché de celui-là jusqu'ici. Voir
   `reports/ac6-retail-native-codegen-gate2-r406-r404-direction-reversed-remainder-overlaps-freshly-carved-pool-20260908.md`
   (et r405,
   `reports/ac6-retail-native-codegen-gate2-r405-remainder-path-read-not-yet-the-bug-20260908.md`,
   pour la lecture de source de `loc_821FA1D4`/`loc_821FA0BC` qui a
   nommé `sub_821F92B8`).
   **Nommé pour r407** : lire `sub_821F92B8` en entier pour identifier
   le stub hôte qu'elle appelle et comment elle calcule la plage
   retournée ; si elle appelle un stub déjà audité (candidat nommé :
   `MmAllocatePhysicalMemoryEx`, déjà mis en cause par r389), rapprocher
   explicitement les deux fils. Vérifier en direct la plage exacte
   retournée par `sub_821F92B8` à `seq=877` avant tout correctif
   (précédent r1111/r1113).

1. **r404 — une invocation de `sub_821F9E10` insère en direct un nœud "libre" (bucket 17, `0x10082aa0`) depuis l'intérieur d'un bloc de 525 Ko qu'une invocation antérieure de la MÊME fonction, chaîne d'appel différente, avait déjà remis à `sub_8236E868` et jamais libéré (PAS un blocage qualifié). CORRIGÉ PAR r406 CI-DESSUS : l'ordre était inversé.**
   Suite de r403 : point d'observation matériel armé sur `0x10000208`
   (en-tête bucket 17) et `0x10082aa8`/`0x10082aac` (chaînage retour du
   nœud) depuis l'entrée du constructeur `GuestAddressSpace`. Six
   déclenchements : `sub_821F9E10` (chaîne
   `sub_821F7A88 <- ... <- sub_823A5BA0`) insère correctement et
   complètement le nœud `0x10082aa0` en tête du bucket 17 ; `sub_82372128`
   (formateur générique de pool, relu en entier, aucune adresse codée en
   dur) écrase ensuite les deux champs de chaînage retour du nœud, sans
   toucher l'en-tête — d'où l'insertion à moitié effacée que r403 avait
   documentée. Capture de registre confirmant `sub_8236E868` demande
   exactement `0x80310` octets (525 072, la taille exacte de son pool —
   corrige la lecture `+150=784` de r398, une troncature 16 bits) et que
   le pointeur retourné (`0x10011c60`) contient le nœud `0x10082aa0`
   inséré ensuite. Capture filtrée sur `sub_821FA6F8` (fonction `free`)
   confirmant qu'aucun des 47 pointeurs libérés sur l'intégralité du run
   n'est `0x10011c60` — le pool n'a jamais été libéré (une première
   capture avait filtré par erreur sur `ctx.r4` au lieu de `ctx.r5`, le
   registre réel du pointeur libéré selon le prologue de
   `sub_821FA6F8` ; corrigée avant publication). Ni codegen (réfuté
   quatre fois, r401-r404), ni taille mal demandée, ni "chevauchement
   entre deux sous-systèmes indépendants" (hypothèse (a) de r380) — une
   seule fonction, deux invocations, distribue deux fois la même
   mémoire. La branche précise qui traite ce bloc comme disponible n'est
   PAS encore identifiée. Voir
   `reports/ac6-retail-native-codegen-gate2-r404-writer-found-sub82372128-pool-collides-with-live-node-20260908.md`.
   **Nommé pour r405** : lire en entier `loc_821FA1D4` (suite, à partir
   de `bne cr6,loc_821FA2F4`) et `loc_821FA0BC` (jamais lu, chemin
   grand-bloc, `r29>=128`, pertinent pour la requête `0x80310`) dans
   `sub_821F9E10`, pour localiser la branche exacte. Aucun correctif
   tant qu'elle n'est pas confirmée en direct (précédent r1111/r1113).

1. **r403 — mécanisme exact localisé : le nœud `0x10082aa0` a été à moitié inséré dans le bucket 17, ses champs de chaînage retour jamais écrits ; `sub_821F9E10`'s triple-vérification refuse À RAISON de le retirer, donc le vrai défaut est en amont (PAS un blocage qualifié).**
   Lecture complète de `sub_821F9E10` depuis `loc_821F9EC4` jusqu'au
   retour commun `loc_821FA528`. Une première lecture (déchaînement à
   triple vérification lui-même fautif, même famille que r356/r382) a
   été **écrite puis réfutée avant publication** par un contrôle direct :
   les avertissements `Uninitialized memory read` du rejeu microexec de
   r402 (`821fa06c`/`821fa070` pour `call1`, `821f9f04`/`821f9f08` pour
   `call2`), comptés en instructions depuis les labels connus,
   correspondent exactement à `lwz r9,0(r11)`/`lwz r7,4(r10)` — les
   champs retour du nœud trouvé (`0x10082aa8`/`0x10082aac`) sont NULS,
   confirmé en relisant ces deux adresses dans l'instantané déjà
   capturé (les deux valent `0x00000000`). Avec ces liens à zéro, la
   deuxième des trois comparaisons échoue et les deux `stw` qui
   réécriraient l'en-tête du bucket 17 ne s'exécutent jamais — **le
   déchaînement refuse À RAISON de retirer un nœud dont les liens retour
   ne correspondent pas à son bucket.** Le vrai défaut est donc en amont :
   quelque chose a écrit l'en-tête du bucket 17 (`0x10000208`,
   `[0]=[4]=0x10082aa8`) SANS écrire les champs retour du nœud lui-même —
   une insertion à moitié faite, pas encore tracée jusqu'à son écrivain.
   Ceci confirme et affine le verdict r401/r402 (pas de mauvaise
   traduction de codegen dans `sub_821F9E10`) plutôt que de le
   contredire. Voir
   `reports/ac6-retail-native-codegen-gate2-r403-bucket17-header-not-updated-20260908.md`.
   **Nommé pour r404** : point d'observation matériel sur `0x10000208`
   ET sur `0x10082aa8`/`0x10082aac` depuis le début du processus
   (technique r371/r389, déjà nommée par r389 pour la sentinelle et
   jamais exécutée) pour capturer l'écrivain exact de l'en-tête sans les
   champs retour. Deux candidats déjà visibles dans le corps de
   `sub_821F9E10`, ni l'un ni l'autre lu en entier : le chemin de
   réinsertion du reliquat (`loc_821FA1D4`) et le chemin de bloc neuf via
   `NtAllocateVirtualMemory` (`loc_821FA564`). Aucun correctif tant que
   l'écrivain n'est pas confirmé en direct (précédent r1111/r1113).

1. **r402 — les traces microexec de r401 confirmées réelles (pas un chemin d'erreur), écritures concrètes capturées, sémantique exacte encore ouverte (PAS un blocage qualifié).**
   Doute soulevé avant publication de r401 (deux imports host non
   stubés dans le prologue, `KeGetCurrentProcessType`/`KeBugCheckEx`,
   auraient pu faire bifurquer la trace vers un chemin d'erreur avec
   `callee_entries=0` et ~200 pas). Fermé : `heap+20` vaut `0x2` dans
   les deux instantanés (bit testé = 0, le prologue saute PAR-DESSUS
   ces imports) ; rejoué avec les deux imports stubés en plus,
   résultat identique (`steps=212`/`127`, `stubbed_calls=2` inchangé —
   jamais atteints). Les instantanés `dump heap` post-exécution
   montrent des écritures substantielles et cohérentes avec les
   adresses déjà connues de la campagne (`call1` : 9 plages, dont le
   champ sentinelle `0x10000184` et le champ de chaînage propre du nœud
   `0x1009fa10` ; `call2` : seulement 2 octets). **Le verdict de r401
   (concordance, pas de mauvaise traduction de codegen) tient et est
   mieux fondé** — pas rétracté. Reste ouvert : la sémantique exacte des
   champs écrits à `0x10082ac8`/`0x10082acc` (chaînage de freelist vs.
   tags de bornage physiques) et quelle branche exacte `call1` (bucket 2,
   VIDE selon le contrôle direct de cette table) a réellement prise pour
   quand même retourner `0x10082ab0` avec ces écritures. Voir
   `reports/ac6-retail-native-codegen-gate2-r402-microexec-writes-confirm-real-path-20260908.md`.
   **Nommé pour r403** : lire `sub_821F9E10` en entier depuis
   `loc_821F9EC4` à travers `loc_821F9F80` pour trancher la sémantique
   des champs et la branche réellement prise, en utilisant le harnais
   microexec maintenant fonctionnel sur `ac6-us` plutôt que
   l'observation uniquement live.

1. **r400 — arbre stabilisé, gate JF restauré (PAS un blocage qualifié).**
   La situation d'arbre non committé nommée "décision de l'utilisateur,
   inchangée" depuis r239 (~160 cycles) bloquait en pratique le gate
   `mission01-final-gate-v3.json` (`evidence size mismatch`), pas
   seulement en principe. Résolu ce cycle : 57 fichiers racine vestiges
   d'un fil PAL/NDXR/ENTRY9 sans rapport avec la campagne NTSC-US
   supprimés (confirmé : aucune référence dans NEXT.md/RESUME.md/
   EVIDENCE.md/AGENTS.md, aucun équivalent ailleurs dans l'arbre) après
   confirmation explicite de l'utilisateur ; erreur immédiate corrigée
   dans le même cycle (`GLOBAL_OFFLINE_LADDER.md` et
   `XENIA_WINE_ORACLE_HANDOFF.md` restaurés, tous deux vivants malgré
   l'apparence) ; `recompilation/ace-combat-6-demo` réduit à son archive
   d'analyse statique (config/docs/tools), son build/source machinery
   retiré, conforme au texte déjà à jour d'`AGENTS.md` ; trois fichiers
   de travail en cours réel (`retail_session.cpp` : sélection caméra par
   mode de vue + ancre free-flight NTSC-U/J ; `ntxr_texture.h` : lecture
   pack par clé GIDX ; `retail_flight_orientation.h` : extraction
   pitch/yaw/roll) re-pinnés via `refresh_contract_evidence.py` et
   committés. **Les trois gates requis par `CLAUDE.md` sont maintenant
   verts** (`audit_ac6_mission01_native_gate.py --require JF` = pass,
   `audit_ac6_contract_artifacts.py` = pass, `audit_ac6_contract_addresses.py`
   = pass 321/321) — première fois depuis le début de r239. Voir
   `reports/ac6-retail-native-codegen-gate2-r400-tree-stabilization-mission01-gate-restored-20260908.md`.
   **Reste non touché** : 72 fichiers modifiés / 206 non trackés hors du
   chemin qui bloquait le gate (dont la surcouche HUD non tracée
   `native_hud_gpu_overlay.{h,cpp}`, cf. règle HUD nommée ci-dessous) ; le
   blocage runtime r399 (inchangé, voir item 1).

1. **r401 — discriminant microexec exécuté : mauvaise traduction de codegen RÉFUTÉE pour `sub_821F9E10`, la cause est en amont (état du tas), PAS un blocage qualifié.**
   État exact capturé en direct (`ctx.r3=0x10000000`, `r4=0`, `r5=1` puis
   `r5=256`, même `sp`) et instantané du tas invité
   `[0x10000000,0x10200000)` au moment précis des deux appels
   `sub_821F9E10` qui retournent tous deux `0x10082ab0` dans le run natif
   compilé (re-confirmé, identique à r398). Rejoué dans
   `MicroExecuteFunction.java` (interprète p-code Ghidra, indépendant du
   C++ compilé, sur les MÊMES octets d'instruction retail) : **les deux
   cas retournent également `0x10082ab0`**, sortie propre (`exit=return`,
   pas de fault). Deux moteurs d'exécution indépendants s'accordent sur la
   même sortie à partir du même état — une mauvaise traduction de codegen
   n'aurait pas dû survivre à une réimplémentation indépendante.
   **`sub_821F9E10` fait ce que ses instructions disent de faire ; le
   problème est en amont, dans l'état du tas au moment de ces deux
   appels**, pas dans la traduction de la fonction. A nécessité d'élargir
   le gate SHA du harnais (`scripts/MicroExecuteFunction.java`, figé sur
   le seul hash PAL de la suite de calibration) à un `Set` incluant le
   hash NTSC-U/J — prouvé sans effet sur le chemin PAL existant par
   comparaison directe avant/après (sortie identique octet pour octet).
   La calibration automatisée complète n'a pas pu tourner (charge utile
   extraite manquante, préexistante, sans rapport avec ce cycle) ; un
   second constat préexistant (le fichier de référence
   `rotation-822a1e80.ppc.json` ne correspond déjà plus à une exécution
   fraîche, indépendamment de ce cycle) a aussi été trouvé — les deux
   nommés pour r402, pas résolus ici. Voir
   `reports/ac6-retail-native-codegen-gate2-r401-microexec-discriminator-codegen-refuted-20260908.md`.
   **Nommé pour r402** : (1) tracer en arrière depuis l'instantané figé
   du tas (`heap2_call1_size1.bin`) pour trouver quelle fonction a écrit
   la freelist dans cette forme AVANT ces deux appels — continuation de
   la chasse à l'écrivain r378-r398, maintenant avec un instantané
   rejouable au lieu d'une observation uniquement live ; le suspect nommé
   par r389 (`MmAllocatePhysicalMemoryEx`, chevauchement de plage avec la
   freelist vivante) reste jamais suivi ; (2) réparer la lacune de
   calibration (payload manquant + fichier de référence obsolète) sans
   quoi tout futur changement à ce harnais reste invérifiable ; (3) un
   blocage qualifié (budget oracle Xenia, trace des quatre premières
   allocations de `sub_8236E868`) et le choix "un bug à la fois" restent
   nommés pour l'utilisateur si la chasse à l'écrivain ne tranche pas —
   voir le plan approuvé.

2. **r399 (historique, affiné par r401 ci-dessus) — tracer en direct depuis les sites d'appel `+294` (taille 1) et `+343` (taille 256) de `sub_8236E868` jusque dans `sub_821F9E10` pour trouver exactement où ces deux requêtes convergent sur la même adresse retournée (`0x10082ab0`), et quelle instruction/branche échoue à avancer le curseur/pointeur-de-tas responsable ; une fois isolé, appliquer le correctif via le script idempotent établi, reconstruire, et vérifier si `sub_821D5F48` revient enfin et si la boucle par image s'exécute.**
   r398 a désassemblé `__imp__sub_8236E868` directement et armé un
   point d'arrêt à chacun des décalages hôte où les 4 premiers appels
   d'allocation retournent leur valeur (`+150`=784, `+225`=55944,
   `+294`=1, `+343`=256), tous en UNE SEULE exécution -- contournant la
   limitation `gdb print <nom-local>` de r397. Résultat identique sur 3
   lancements indépendants : la requête de 1 octet et la requête de
   256 octets, émises l'une après l'autre SANS libération entre les
   deux, retournent le MÊME pointeur exact, `0x10082ab0`.
   `0x10082ab0 + 0x18 = 0x10082ac8`, précisément le nœud freelist
   corrompu chassé depuis r378, et précisément le décalage `0x18`
   identifié à l'origine par r380 (la découverte de r380 et celle-ci ne
   sont PAS en conflit ; r392 avait réfuté une attribution DIFFÉRENTE).
   L'hypothèse principale de r397 (Write 1 issue de la requête de 55944
   octets, `0x10091f80`) est réfutée. **Ceci n'est plus une corrélation
   plausible : c'est un fait live, déterministe, même-invocation** :
   l'allocateur générique délivre le MÊME bloc vivant à deux requêtes
   différentes sans rapport, sans qu'aucune ne le libère -- un bug de
   double-émission de l'allocateur lui-même, pas un unlink manquant
   côté appelant (r380/r392's thread), pas une collision de
   sous-système non coordonné (r386-r389's thread). Règle la question
   "quel côté viole son contrat de propriété mémoire" tournée en rond
   depuis r386-r397. Aucun correctif appliqué : la branche exacte à
   l'intérieur de `sub_821F9E10` qui échoue à marquer `0x10082ab0`
   consommé avant la seconde requête n'est pas encore isolée -- c'est
   la QUATRIÈME tentative d'attribution dans ce sous-fil, les trois
   premières s'étant effondrées sous un examen plus strict (r380 par
   r392 ; r393 par r395 puis re-confirmé par r396). `sub_821D5F48` n'est
   toujours jamais revenu ; la boucle par image ne s'exécute toujours
   jamais, après SEIZE découvertes du sous-système allocateur
   (r358-r398).

1. **r389 (historique, dépassé par ce qui précède) — r388 a effectué le test décisif nommé par r387 : capturé en direct
   l'adresse de base réellement retournée par l'appel `MmAllocatePhysicalMemoryEx`
   de `sub_821D5F48` (`*__imp__sub_821D5F48+1214`, `ctx.r3` = `0x16f80000`),
   calculé la plage `[0x16f80000, 0x2e780000)` avec la taille `0x17800000`
   capturée par r387, et confirmé que `0x280c0b10` (le nœud empoisonné de
   r386) tombe DEDANS. **Lecture (1) confirmée** : la mémoire de la
   freelist des gros blocs est LA MÊME mémoire que cette réservation de
   375 Mio, pas une collision entre deux régions indépendantes (de toute
   façon structurellement impossible vu le compteur monotone unique
   `allocate_guest`, r387). Lecture (2) réfutée pour ce nœud.

   Ceci ne résout PAS encore l'investigation. r386 avait déjà armé un
   point de surveillance matériel sur `0x280c0b10` lui-même depuis le
   tout début du processus et trouvé UNE SEULE écriture dans toute
   l'exécution -- le memset empoisonnant (`0xFE`). Aucune routine
   n'écrit jamais de pointeur « next » valide dans la mémoire propre de
   ce nœud. Son appartenance apparente à la freelist (atteinte par le
   scanner de `sub_821F8A00` en partant de la sentinelle `0x10000180`)
   doit donc venir de quelque chose qui écrit le CHAÎNAGE DE LA
   SENTINELLE elle-même vers `0x280c0b10` -- une cible de surveillance
   différente de celle déjà vérifiée, pas encore armée.

   Mécanisme plausible mais explicitement NON confirmé : une routine
   d'insertion/découpe de pool qui suppose que la mémoire fraîchement
   découpée est déjà mise à zéro (convention `next=0` = terminateur) et
   qui, de ce fait, n'écrit jamais explicitement de terminateur dans le
   nouveau nœud -- hypothèse brisée ici car cette mémoire a été
   empoisonnée à `0xFE`, pas à zéro, par le memset antérieur et non lié
   de `sub_821D5F48`. Correspondrait à la même forme "écriture manquante
   sur un chemin" que tous les correctifs précédents de cette campagne,
   mais PAS confirmé en direct -- pas de correctif appliqué, conformément
   à la leçon apprise deux fois par r382.

   Gate : `ctest` 10/10 (aucune source modifiée ce cycle, investigation
   seule). `git status` après `ctest` : 523 chemins, identique à
   r385-r387, aucune dérive. Aucun commit -- décision de l'utilisateur
   inchangée.

   **Nommé explicitement pour r389** : armer un point de surveillance
   matériel sur le champ de chaînage propre de la sentinelle
   (`0x10000180`, avec la convention de décalage de champ établie depuis
   r369/r371) depuis le tout début du processus (technique de r371), pour
   capturer l'écriture exacte qui lie `0x280c0b10` dans la chaîne pour la
   première fois, et identifier la fonction invité responsable. Lire
   cette fonction en entier, déterminer si elle suppose une mémoire mise
   à zéro pour son terminateur, et si oui si le correctif doit écrire un
   terminateur explicite dans le nouveau nœud, ou si le memset de
   `sub_821D5F48` ne devrait tout simplement pas toucher cette plage (une
   question d'ORDONNANCEMENT entre les deux mécanismes). C'est la
   NEUVIÈME découverte du sous-système allocateur (r358-r388) sans encore
   atteindre la boucle par image. Le thread principal n'est TOUJOURS
   jamais revenu de `sub_821D5F48` ; la boucle par image ne s'est encore
   jamais exécutée, ce cycle ou tout cycle précédent. Le choix permanent
   de l'utilisateur "continuer un bug à la fois" reste en vigueur -- pas
   d'escalade unilatérale vers un audit exhaustif par lot.

   Voir §4.109 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r388-overlap-decisive/`.

2. **r387/r388 — close, contexte historique.** r387 a
2. **r386/r387 — close, contexte historique.** r386 a
   localisé l'écrivain du septième candidat de r385/r384 : un point de
   surveillance matériel armé sur `0x280c0b10` (l'unique nœud réel de la
   freelist des gros blocs) depuis le tout début du processus a capturé
   UNE SEULE écriture, ~1,6s après l'armement, ancienne valeur `0` :
   `sub_823830F0` (un `memset` générique, confirmé correct par lecture
   complète) appelé depuis `sub_821D5F48` -> `sub_821D7DE0`, remplissant
   de `0xFE` toute une région obtenue via `sub_821F4078(dest=adresse
   fixe, r4=-1, r5=0, r6=0x20000004)` -- structurellement un appel de
   réservation+commit à adresse fixe façon `VirtualAlloc`/`mmap`, PAS
   acheminé par l'allocateur à freelist étudié depuis r311.

   **Ce N'EST PAS une huitième instance du même bug "Leave manquant"**
   (r358, r365/366, r376, r378-380, r383) -- c'est un chevauchement de
   plage d'adresses entre DEUX mécanismes de gestion mémoire
   indépendants : la freelist, et la réservation à adresse fixe de
   `sub_821F4078`. Lequel des deux est fautif n'est pas déterminé (`sub_821F4078`
   lue seulement structurellement, pas en entier, ce cycle). Aucun
   correctif appliqué -- ceci ne correspond plus au patron "ajouter un
   appel Leave/unlink manquant, en miroir d'un frère correct" autorisé
   jusqu'ici.

   **Nommé explicitement pour r387** : (a) lire `sub_821F4078` en
   entier pour déterminer quel côté du chevauchement est réellement en
   tort ; (b) noter explicitement qu'après SEPT découvertes dans le
   sous-système allocateur (r358 à r386) sans jamais atteindre la
   boucle par image, et cette découverte étant de nature architecturale
   différente des six précédentes (un possible conflit de propriété de
   plage d'adresses entre sous-systèmes, pas un correctif d'une ligne),
   ceci peut à nouveau justifier un point de contrôle stratégique avec
   l'utilisateur avant de continuer unilatéralement -- signalé ici
   explicitement, pas décidé seul, dans la continuité du précédent de
   r381.

   Voir §4.107 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r386-corrupted-node-writer/`.

2. **r386 — continuation normale (PAS un blocage qualifié), désormais close.** r385 a
   appliqué le contrôle strict de registre de r369 à l'invocation de
   `sub_821F8A00` atteinte via `sub_821F9150` (r384) et l'a CONFIRMÉE
   comme une vraie boucle infinie figée : trois échantillons SIGINT,
   20s d'écart, montrent **tout le fichier de registres identique
   bit-à-bit** (`rbx=0x10000180`, `r11=0`). La lecture croisée
   désassemblage/source (`ppc_recomp.27.cpp:11468-11640`) PROUVE
   mathématiquement l'auto-perpétuation : avec `r11=0`, `LOAD_U16`
   près de `0xFFFFFFF8` puis `LOAD_U32(0)` retournent tous deux 0, donc
   `r11` ne peut jamais converger vers le sentinel `0x10000180` (la
   MÊME tête de freelist des gros blocs partagée depuis
   r369/r371/r378-383, confirmée vivante et correctement écrite
   ailleurs dans le run).

   `sub_821F9150` (jamais lue avant r384) a été lue en entier ce
   cycle : `NtAllocateVirtualMemory` -> `sub_821F8248` (jamais
   examinée, initialise l'en-tête du nouveau bloc) -> épissage via
   l'un de deux appels internes à `sub_821F8A00`. La relation exacte
   entre les champs que `sub_821F8248` initialise et le pointeur
   « next » que le scanner déréférence ensuite n'a PAS été résolue ce
   cycle -- deviner ici répéterait l'erreur de r382 (corréler au lieu
   de prouver).

   **Confirmé : une SEPTIÈME instance du même défaut de chaînage
   cassé** (r358, r365/366, r376, r378-380, r383). **Pas corrigée** --
   aucun correctif appliqué, l'écrivain exact n'ayant pas été localisé
   en direct.

   **r386 doit** : (a) parcourir la freelist des gros blocs en direct,
   tête-à-queue, juste avant que cette invocation ne commence son scan,
   pour trouver le nœud précis dont le champ « next » lit déjà 0
   (technique de r371) ; (b) armer un point de surveillance matériel
   sur ce champ exact depuis le démarrage du processus (technique de
   r371, PIE-safe, avant tout code invité) pour trouver l'écrivain ;
   (c) lire `sub_821F8248` en entier (jamais examinée) et auditer par
   chemin de sortie la fonction impliquée, selon la même technique qui
   a trouvé chaque défaut précédent ; (d) continuer la discipline « un
   bug à la fois, vérifié en direct avant correctif » explicitement
   choisie par l'utilisateur (r381/r382) -- pas de correctif par lot,
   pas de déduction depuis le seul désassemblage ou la seule
   corrélation. Le thread principal n'est toujours jamais revenu de
   `sub_821D5F48` (r384) -- ceci reste le test ultime une fois (si) ce
   septième défaut corrigé. La situation d'arbre non commité (r239+,
   sur la suppression massive du 2026-09-02) reste la décision de
   l'utilisateur, inchangée.

   Voir §4.106 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r385-sub821f9150-loop/`.

2. **r385 (historique, résolu par r386 ci-dessus)** -- confirme le
   sixième candidat de r384 comme une vraie boucle infinie figée, mais
   ne localise pas l'écrivain. Voir §4.106.

3. **r384 (historique, résolu par r385/r386 ci-dessus) — continuation normale (PAS un blocage qualifié).** r383 a
   construit la technique primaire nommée nécessaire par r382 : une
   trace d'exécution pas-à-pas réelle (`nexti`, sautant par-dessus les
   appels) de l'invocation LIVE `ENTER#545` elle-même, du site d'Enter
   jusqu'à son retour (`FUNC_BASE` calculé en direct sous ASLR, boucle
   de trace sortie du callback `stop()` du point d'arrêt). La trace (122
   instructions hôte uniques, pc de sortie identique à l'adresse de
   retour déjà capturée par r382 pour `ENTER#545`) a révélé un
   TROISIÈME chemin : un `jmp` inconditionnel (`+758`) sautant
   directement vers la queue commune de la fonction, sans passer par
   AUCUN des deux sites d'appel Leave connus -- réfutant la corrélation
   `+758` de r382 comme pure coïncidence.

   Identifié précisément par lecture croisée du source généré : le
   chemin d'allocation « gros bloc » (`r29>=128`) appelle
   `sub_821F92B8` ; en cas d'échec (`r3==0`), `loc_821FA630 ->
   loc_821FA634 -> loc_821FA660 -> loc_821FA664 -> return;` ne passe
   JAMAIS par la vérification Leave gardée par `r22` de `loc_821FA528`
   -- même forme de bug que r358/r365-366/r376, cinquième instance
   indépendante dans le même allocateur.

   **Correctif appliqué et vérifié** (`tools/apply_sub_821f9e10_largeblock_failure_leave_fix.py`,
   nouveau script suivi, idempotent) : insère le Leave gardé par `r22`
   en tête de `loc_821FA664`, utilisant `r27` (pas `r30`) comme pointeur
   d'objet tas, exactement comme `loc_821FA528` elle-même. Un premier
   faux négatif (décalage hôte codé en dur périmé après reconstruction,
   hérité du script de r382) a été capturé et corrigé en réarmant le
   point d'arrêt Leave symboliquement sur `*__imp__RtlLeaveCriticalSection`
   lui-même. Re-vérifié : `ENTER#545 leaves_since_enter=1 LEAK=False`.
   Équilibre agrégé (60s) : `enter=575 leave=575 net=0` -- entièrement
   équilibré. **Cinquième fuite de section critique confirmée en direct,
   corrigée et vérifiée dans cette campagne.**

   **MAIS re-testé la boucle par image** (`sub_821D7AE0`/`sub_821D7CD0`,
   fenêtre de 120s post-correctif) : **zéro exécution, inchangé**. La
   poignée de main avec l'ouvrier se déclenche encore exactement une
   fois (`b5a0=1 a620=1 a610=1`) et le thread principal ne redemande
   jamais -- le motif déjà caractérisé par r367/r377, maintenant
   confirmé persister même avec l'allocateur entièrement et prouvément
   propre (`net=0`). La sous-investigation de l'allocateur (r356-r358,
   r365/366, r376, r378-383) est maintenant close à un point d'arrêt
   honnête et complet -- un vrai progrès, mais pas la réponse à
   « pourquoi `presented_frames` reste à 0 ».

   **r384 doit** : revenir à la direction déjà nommée par r377/r382,
   maintenant sur un allocateur prouvément propre -- instrumenter
   directement la décision de re-demande de la poignée de main
   (génération 2+) du thread principal (le code qui décide de réémettre
   ou non une requête vers l'ouvrier après que la première soit
   complétée), puisque c'est maintenant la seule candidate restante pour
   le vrai blocage de la boucle par image. La situation d'arbre non
   commité (r239+, sur la suppression massive du 2026-09-02) reste la
   décision de l'utilisateur, inchangée.

   Voir §4.104 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r383-enter545-instruction-trace/`.

1. **r383 (historique, PAS un blocage qualifié).** L'utilisateur
   a choisi explicitement « continuer un bug à la fois » sur le point de
   décision stratégique de r381. r382 a vérifié en direct la troisième
   paire Enter/Leave de `sub_821F9E10` et confirmé un déséquilibre
   reproductible (`enter=589 leave=587 net=2` sur 60s, stable). Une
   NOUVELLE technique (suivi par adresse de retour PAR INVOCATION) a
   confirmé en direct qu'`ENTER#545` lui-même retourne à son appelant
   (`sub_823801B8`) sans jamais avoir appelé Leave -- une fuite réelle, à
   cadre unique. Un `jmp` inconditionnel statique (`+758`, correspondant
   au chemin rapide « correspondance exacte de taille », `goto
   loc_821FA528;` ligne ~16519) semblait corréler exactement (un point
   d'arrêt dédié s'y est déclenché une fois, au moment précis où
   `outstanding_before=1`). Un correctif a été appliqué
   (`tools/apply_sub_821f9e10_smallbucket_leave_fix.py`, même motif
   idempotent que r358, patchant aussi le doublon dans `sub_821F9E08`
   comme pour r358) et reconstruit.

   **MAIS la re-vérification par la MÊME technique de suivi de retour,
   sur le binaire reconstruit, PROUVE qu'`ENTER#545` fuit TOUJOURS,
   inchangé** (`leaves_since_enter=0, LEAK=True`). Le nouveau Leave
   inséré s'exécute bien une fois dans la même fenêtre, mais sur une
   invocation DIFFÉRENTE et bien plus tardive (séquence globale 587, pas
   545/546) -- le déséquilibre agrégé reste inchangé (2, avant et après).
   **La corrélation `+758` est explicitement rétractée** comme
   coïncidence probable avec un appel sans rapport empruntant
   légitimement le même chemin rapide avec `r22` déjà à 0, pas un lien
   causal avec `ENTER#545`. Le correctif est CONSERVÉ (réel, correct,
   inoffensif, en miroir d'une logique sœur déjà correcte, passe les
   gates) mais ne résout PAS la fuite reproductible `ENTER#545`/`#546`
   que cette sous-investigation (r378-r382) poursuit depuis.

   **r383 doit** : re-dériver l'instruction de contournement réelle
   d'`ENTER#545` en utilisant le suivi par adresse de retour par
   invocation comme outil PRINCIPAL -- armer le suivi de retour d'abord,
   puis biséquer l'intervalle entre l'Enter et son retour connu avec des
   points d'arrêt conditionnels supplémentaires, plutôt que de deviner
   des branches candidates depuis le désassemblage optimisé et
   réordonné, qui a maintenant produit DEUX fausses pistes de suite pour
   cette investigation précise (`+1949`/`+1970`/`+2063` en première
   tentative, puis `+758` en seconde). La situation d'arbre non commité
   (r239+, sur la suppression massive du 2026-09-02) reste la décision
   de l'utilisateur, inchangée.

   Voir §4.103 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r382-9e10-enter-leave-balance/`.


1. **r380 — continuation normale (PAS un blocage qualifié).** r379 a
   tracé en direct l'écrivain exact du NULL de r378 (technique de r371,
   point d'observation armé avant tout code invité sur
   `0x10082ac8`) : trois écritures seulement avant sortie normale du
   processus -- bootstrap correct (`sub_821F9E10`, `next=head`), puis
   deux initialiseurs légitimes SANS RAPPORT (`sub_82377C00`, table de
   8 handles ; `sub_82372128`/`sub_82372198`, pool de 64 emplacements)
   qui zèrent ce qu'ils croient être leur propre mémoire privée --
   lecture complète des deux corps confirme qu'AUCUNE des deux n'est
   défectueuse en elle-même. L'adresse `0x10082ac8` sert donc à trois
   usages sans rapport en ~10ms de démarrage. Deux hypothèses non
   départagées : (a) un appel d'allocation a distribué ce bloc aux
   écritures 2/3 sans le retirer de la liste que `sub_821F8A00`
   parcourt encore ailleurs (un déchaînement manquant côté allocation,
   même famille de défaut que les quatre déjà trouvés mais dans une
   fonction différente) ; (b) l'écriture 1 elle-même était
   prématurée/erronée. **PAS corrigé** : appliquer un correctif à
   `sub_82377C00`, `sub_82372128` ou `sub_821F9E10` maintenant serait
   deviner sans contrôle en direct -- explicitement refusé (précédent
   r1111/r1113), quatrième fois dans cette chaîne qu'une hypothèse
   plausible sur l'écrivain lui-même est abandonnée au profit d'une
   hypothèse mieux fondée un niveau plus haut. **Nommé pour r380** :
   mettre un point d'arrêt sur les points d'entrée « allocate » de
   l'allocateur général (`sub_821F92B8`/`sub_821F9150`) entre l'écriture
   1 et l'écriture 2, capturer la taille demandée et l'adresse
   retournée, vérifier si le déchaînement a eu lieu ; si l'écriture 1
   s'avère être l'erreur à la place, auditer la logique d'appartenance
   de bucket de `sub_821F9E10` comme r356 l'a fait pour `sub_821FA6F8`.
   Voir
   `reports/ac6-retail-native-codegen-gate2-r280-doc-deadlock-refuted-codec-starves-on-us-tbl-over-pal-pac-20260906.md`
   §4.100 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r379-null-writer/`.

2. **(contexte, r379 — voir ci-dessus)**

3. **r379 — continuation normale (PAS un blocage qualifié).** r378 a
   appliqué le contrôle plus strict de r369 au candidat que r377 avait
   trouvé (6 échantillons au même décalage dans `sub_821F8A00`) : point
   d'arrêt compté sur `*__imp__sub_821F8A00+480`, capturant `r11`
   (curseur) sur 30 arrêts consécutifs. `r11` atteint `0x00000000` à
   l'arrêt #5 et y reste identique bit à bit sur les 26 arrêts suivants
   -- **confirmé : une VRAIE boucle infinie gelée**, pas un artefact
   d'échantillonnage. Lecture mémoire ciblée : le champ `next` brut de
   l'entrée `0x10082ac8` est un NULL nu (`0x00000000`) que la
   vérification de terminaison de cette boucle (uniquement contre la
   sentinelle `0x10000180`) ne reconnaît jamais -- un QUATRIÈME bug de
   chaînage de freelist indépendant (après r358, r365/366, r376), même
   sous-système allocateur, forme de défaut similaire mais entrée/bucket
   et valeur de corruption différentes (NULL, pas auto-référence). La
   nouvelle chaîne d'appel n'est PAS un nouveau sous-système -- un
   appelant inexaminé du même allocateur étudié depuis r311. **PAS
   corrigé** : le symptôme est confirmé en direct, mais quelle fonction
   en amont a écrit ce zéro n'a pas encore été tracé -- deviner un
   correctif sans cette traçabilité violerait la discipline de preuve du
   projet (précédent r1111/r1113). La vraie boucle par image
   (`sub_821D7AE0`/`sub_821D7CD0`) ne s'exécute toujours pas (attendu,
   aucun code changé ce cycle). **Nommé pour r379** : (1) tracer en
   arrière depuis le champ `next` corrompu de `0x10082ac8` (réutiliser la
   technique de point d'observation-depuis-le-démarrage-du-processus de
   r371) pour trouver quelle fonction a écrit `0x00000000` au lieu d'un
   lien correct ; (2) appliquer et vérifier un correctif via le même
   script de correctif tracé déjà utilisé trois fois, une fois l'écrivain
   identifié ; (3) ne pas supposer que ce sera le dernier bug de ce genre
   -- quatre défauts de chaînage de freelist indépendants ont déjà été
   trouvés dans cet unique allocateur. Voir
   `reports/ac6-retail-native-codegen-gate2-r280-doc-deadlock-refuted-codec-starves-on-us-tbl-over-pal-pac-20260906.md`
   §4.99 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r378-newloop-control/`.

2. **(contexte, r378 — voir ci-dessus)**

3. **r378 — continuation normale (PAS un blocage qualifié).** r377
   (pas de correctif ce cycle) a caractérisé la fenêtre post-r376 avec
   une sonde d'échantillonnage périodique (SIGINT externe vers
   l'inférieur, 6 arrêts sur 95s) : (a) la poignée de main ouvrier
   (`sub_8233B5A0`/`sub_8233A620`) se déclenche désormais UNE FOIS,
   à moins d'1ms du démarrage -- jamais vu depuis r315, mais une seule
   occurrence, pas un régime stationnaire ; (b) les 6 échantillons
   (t≈12s à t≈84s) atterrissent TOUS dans une NOUVELLE boucle de
   recherche interne à `sub_821F8A00` (décalages `+489`/`+494`,
   confirmés par désassemblage), distincte de la boucle déjà corrigée
   de `sub_821F9E10`, se terminant à une sentinelle `+0x180` ; (c) la
   pile d'appel complète menant ici
   (`sub_821F9150 <- sub_821F92B8 <- sub_821F9E10 <- sub_821F7A88 <-
   sub_821F59E0 <- sub_821D74A8 <- sub_823B86D0 <- sub_823B8770 <-
   sub_823B0B48 <- sub_823A65A0 <- sub_8236E618`) est ENTIÈREMENT
   NOUVELLE au-dessus de `sub_821D74A8` -- jamais examinée par
   r311-r376. **Nommé pour r378** : (1) appliquer le contrôle
   registre-à-travers-plusieurs-arrêts de r369 à ce nouveau candidat
   AVANT de le traiter comme confirmé (6 échantillons au même décalage
   sur 72s est une preuve circonstancielle forte, pas encore une
   preuve directe comme celle de r369) ; (2) si confirmé, identifier
   le bucket/liste concerné et lire les fonctions `823A`/`823B` jamais
   examinées pour comprendre le sous-système demandeur ; (3) ne pas
   présumer qu'un correctif similaire (missing-Leave / control-flow
   gate) s'applique sans le vérifier en direct -- chaque bug de cette
   investigation a eu un mécanisme distinct. Voir
   `reports/ac6-retail-native-codegen-gate2-r280-doc-deadlock-refuted-codec-starves-on-us-tbl-over-pal-pac-20260906.md`
   §4.98 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r377-postfix-timeline/`.

2. **(contexte, r377 — voir ci-dessus)**

1. **r377 — continuation normale (PAS un blocage qualifié).** r376 a
   CONFIRMÉ EN DIRECT l'inférence de r375 (le nœud `0x1009fa10` est
   déjà la tête de la freelist surdimensionnée, pointée par la
   sentinelle `0x10000180`, AVANT même que le 4e appel à
   `sub_821F8A00` ne commence), puis a tracé les quatre appels
   (`sub_821F92B8 -> sub_821F8368 -> sub_821F85F8 -> sub_821F8A00`) en
   UNE SEULE passe (calibration de la disposition de `PPCContext` :
   `r3@ctx+0x0`, `r4@ctx+0x10`, valable pour toutes les fonctions
   partageant ce type). Résultat : `sub_821F85F8` reçoit un bloc
   FRAÎCHEMENT alloué (`0x100b0000`) mais renvoie une adresse
   complètement différente (`0x1009fa10`, le voisin arrière calculé en
   interne) à l'appelant. **VRAI CORRECTIF appliqué et vérifié EN
   DIRECT** : la 4e des quatre conditions de fusion-arrière de
   `sub_821F85F8` (la vérification de cohérence de r375, déjà prouvée
   CORRECTE) est la SEULE des quatre dont l'échec ne saute PAS vers
   `loc_821F880C` (« pas de fusion, `r30` de l'appelant inchangé ») --
   elle tombe à la place dans la logique « fusion acceptée », qui
   substitue inconditionnellement le voisin invalide via `r30 = r31`.
   Correctif (`tools/apply_sub_821f85f8_return_gate_fix.py`, nouveau,
   tracé, idempotent) : aligne les deux branches d'échec de cette 4e
   condition sur ses trois sœurs. Reconstruit, vérifié EN DIRECT : (1)
   `sub_821F8A00` reçoit maintenant la vraie valeur `0x100b0000`, plus
   un 5e appel authentiquement NOUVEAU (`0x2e780050`) qui n'existait
   pas avant ; (2) la boucle infinie de `sub_821F9E10` a DISPARU -- 20
   passages complets sur une fenêtre de 90 s, contre ~56 000+ passages
   et en croissance non bornée avant ce correctif. **Cependant** : le
   test de r360 sur la vraie boucle par image
   (`sub_821D7AE0`/`sub_821D7CD0`) montre toujours ZÉRO exécution sur
   la même fenêtre de 90 s -- le processus sort par le minuteur propre
   du harnais de sonde, pas par un blocage, mais pas non plus par le
   gameplay. **Nommé pour r377** : caractériser ce que fait maintenant
   le thread principal pendant cette fenêtre de 90 s (un échantillon
   d'état/pile pris à mi-fenêtre montrerait s'il progresse plus loin,
   se bloque ailleurs, ou termine légitimement son travail disponible)
   -- et NE PAS supposer qu'un éventuel nouveau blocage est un autre
   bug d'allocateur sans preuve directe, ce correctif touchant un
   mécanisme (portillon de flux de contrôle) fondamentalement différent
   des trois précédents (appels `Leave` manquants). Voir
   `reports/ac6-retail-native-codegen-gate2-r280-doc-deadlock-refuted-codec-starves-on-us-tbl-over-pal-pac-20260906.md`
   §4.97 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r376-return-gate-fix/README.md`.

2. **(contexte, r376 — voir ci-dessus)**


1. **r376 — continuation normale (PAS un blocage qualifié).** r375 a
   RÉFUTÉ la prémisse partagée de r371-r374 : capture EN DIRECT
   corrigée (première lecture au mauvais point d'instruction donnait un
   faux positif, corrigée dans le même cycle) montre que la
   vérification de cohérence fusion-arrière de `sub_821F85F8`
   (`r9==r7`) évalue à FALSE de façon CORRECTE (le voisin candidat
   `0x1009fa10` a `back=NULL`, `fwd=self` -- intrinsèquement
   incohérent) et refuse à juste titre de fusionner. `sub_821F85F8`
   n'est PAS le bug. La corruption est isolée entièrement dans la
   propre boucle de recherche de `sub_821F8A00` : son chemin
   grand/surdimensionné (`>=128`) parcourt une freelist triée SANS
   AUCUNE vérification que le candidat trouvé n'est pas le nœud en
   cours d'insertion -- l'explication la plus probable est que
   `0x1009fa10` était DÉJÀ LIÉ dans cette freelist au moment du 4e appel
   à `sub_821F8A00`. **Nommé pour r376** : (a) parcourir la freelist
   surdimensionnée EN DIRECT juste avant le 4e appel à `sub_821F8A00`
   pour confirmer directement que `0x1009fa10`/`0x1009fa18` y est déjà
   lié, plutôt que par inférence depuis la destination de l'écriture
   corrompue ; (b) si confirmé, tracer lequel des trois appels
   antérieurs à `sub_821F8A00` (ou sa propre logique de coalescence/
   allocation en amont) l'y a inséré sans retrait correspondant ; (c)
   n'appliquer un correctif natif qu'une fois cette asymétrie
   précisément vérifiée en direct. Voir
   `reports/ac6-retail-native-codegen-gate2-r280-doc-deadlock-refuted-codec-starves-on-us-tbl-over-pal-pac-20260906.md`
   §4.96 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r375-backward-merge-check/README.md`.

2. **(contexte, r375 — voir ci-dessus)**


1. **(contexte, r374)** r374 a
   testé et RÉFUTÉ deux hypothèses : (a) un chevauchement d'arrondi
   dans le stub natif `NtAllocateVirtualMemory` (trace EN DIRECT
   complète de tous les appels du run -- zéro chevauchement, zéro
   ajustement d'arrondi observé, hypothèse refermée) ; (b) la recherche
   interne secondaire de `sub_821F8368` nommée par r373 (capture EN
   DIRECT montre que sa propre valeur de retour pour l'appel corrompu
   est `0x100b0000`, pas `0x1009fa10` -- le candidat interne
   `0x1009fa10` ne sert qu'à sa comptabilité locale, jamais transmis :
   coïncidence d'adresse, pas un défaut). Relecture statique de
   `sub_821F85F8` contre son garde réel (`r6=0`) montre que le bloc de
   fusion-AVANT entier est sauté inconditionnellement pour cet appel --
   **seul le bloc de fusion-ARRIÈRE (`loc_821F8708`-`loc_821F8768`)
   peut être responsable.** Sa vérification de cohérence en deux
   parties (`r9==r7 && r9==r8`, dérivée de `*(r31+12)`/`*(r31+8)`) est
   maintenant la SEULE branche non auditée restante dans toute
   l'investigation. **PAS corrigé, PAS un blocage qualifié.** **Nommé
   pour r375** : (a) capturer en direct `r31`/`r11`/`r10`/`r9`/`r7`/`r8`
   à `loc_821F8708` de `sub_821F85F8` pour l'appel exact produisant
   `0x1009fa10`, pour déterminer si la vérification de cohérence passe
   (déchaînement exécuté, écartant ce chemin aussi) ou échoue
   (déchaînement sauté alors que la fusion/réinsertion continue quand
   même -- la forme exacte de bug qui expliquerait tout depuis r369) ;
   (b) n'appliquer un correctif natif qu'une fois un défaut précis
   vérifié en direct. Les décalages hôte des deux fonctions sont déjà
   sauvegardés dans
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r374-nav-hint-trace/disas_sub_821F85F8.txt`.
   Voir
   `reports/ac6-retail-native-codegen-gate2-r280-doc-deadlock-refuted-codec-starves-on-us-tbl-over-pal-pac-20260906.md`
   §4.95 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r374-nav-hint-trace/README.md`.

2. **(contexte, r373/r374 — voir ci-dessus)**


1. **(contexte, r372)** r372 a
   étendu la sonde de r371 avec une capture de registres EN DIRECT à
   l'instruction exacte de l'écriture auto-référentielle
   (`__imp__sub_821F8A00+209`) : `r8` (= `r4+8`, le nœud en cours
   d'épissure) et `r14_be` (= la valeur PPC r11 écrite, le point
   d'insertion trouvé par la recherche) sont identiques bit pour bit
   (`0x1009fa18` == `0x1009fa18`), **`VERDICT self-write-confirmed=True`**
   -- ce n'est plus une hypothèse structurelle mais un fait vérifié en
   direct. `sub_821F8A00` lui-même ne contient AUCUNE logique de
   déchaînement (relu en entier) ; le déchaînement manquant doit donc
   être en amont, dans `sub_821F85F8` (fonction de coalescence de blocs
   appelée par `sub_821F92B8` juste avant `sub_821F8A00`), qui effectue
   deux blocs de déchaînement inline (fusion-avant et fusion-arrière),
   chacun gardé par des conditions pas encore auditées branche par
   branche. **PAS ENCORE une localisation confirmée du déchaînement
   manquant, PAS un blocage qualifié.** **Nommé pour r373** : (a)
   compléter l'audit branche par branche des deux blocs de
   coalescence/déchaînement de `sub_821F85F8` (fusion-avant autour de
   `loc_821F8654`-`loc_821F86BC`-`loc_821F8708` ; fusion-arrière autour
   de `loc_821F8708`-`loc_821F8768`), la même technique que r356 a
   appliquée à `sub_821FA6F8`, pour trouver la condition exacte sous
   laquelle un voisin fusionnable -- ou le bloc lui-même -- n'est pas
   déchaîné avant que `sub_821F8A00` ne réinsère son épissure ; (b)
   vérifier en direct avec un point d'arrêt conditionnel avant de
   proposer tout correctif ; (c) n'appliquer un correctif natif qu'une
   fois le mécanisme vérifié en direct, pas seulement plausible. Voir
   `reports/ac6-retail-native-codegen-gate2-r280-doc-deadlock-refuted-codec-starves-on-us-tbl-over-pal-pac-20260906.md`
   §4.93 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r372-register-capture/README.md`.

2. **(contexte, r372 — voir ci-dessus)**


1. **(contexte, r371)** r371 a
   armé un point d'observation matériel depuis le PLUS TÔT possible
   (juste après le `mmap()` de `GuestAddressSpace::GuestAddressSpace()`,
   avant même ses boucles de pré-touch) plutôt que depuis l'entrée de
   `sub_821D5F48` (r369, trop tardif). Trois écritures EN DIRECT sur le
   champ figé capturées avec pile d'appels et désassemblage :
   `sub_821F9E10` écrit d'abord `0x10000180` (tête, correct), puis
   `0x10082ac8` (toujours plausible), puis **`sub_821F8A00`** — déjà
   nommée dans ce projet comme « chemin des blocs surdimensionnés » de
   `sub_821FA6F8` — écrase avec `0x1009fa18`, l'adresse du champ
   LUI-MÊME : destination et valeur écrite prouvées identiques, la
   création de l'auto-référence est capturée sur le fait. Lecture
   statique de `sub_821F8A00` (`ppc_recomp.27.cpp:11454-11627`) : il
   divise un bloc libre et réinsère le reste via le MÊME motif
   d'épissure doublement chaînée déjà confirmé correct dans
   `sub_821FA6F8` (r356) ; les quatre écritures d'épissure sont
   structurellement saines, donc l'auto-écriture ne peut avoir lieu que
   si la recherche du point d'insertion retourne le nœud en cours de
   division lui-même — probablement parce qu'il n'a jamais été
   déchaîné de la freelist avant réinsertion. Hypothèse structurellement
   fondée, PAS ENCORE vérifiée en direct. **PAS ENCORE un correctif
   confirmé, PAS un blocage qualifié.** **Nommé pour r372** : (a)
   capturer EN DIRECT les valeurs réelles de `r11`/`r9` au moment de
   l'écriture fautive et comparer à l'adresse du bloc divisé pour
   confirmer laquelle coïncide ; (b) remonter pour trouver si/où un
   appel de déchaînement manque avant cette épissure, en auditant la
   même façon que r356 a audité `sub_821FA6F8` ; (c) n'appliquer un
   correctif natif qu'une fois le mécanisme vérifié en direct, pas
   seulement plausible. Voir
   `reports/ac6-retail-native-codegen-gate2-r280-doc-deadlock-refuted-codec-starves-on-us-tbl-over-pal-pac-20260906.md`
   §4.92 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r371-self-ref-write-located/README.md`.

2. **(contexte, r371 — voir ci-dessus)**


2. **(contexte, r370 — voir ci-dessus)**

3. **r370 (historique, PAS un blocage qualifié).** r369 a
   confirmé EN DIRECT, par comptage de déclenchements + capture de
   registres à travers ~56 000 itérations et ~15s (pas de simples
   échantillons PC séparés dans le temps), que le curseur de balayage
   de `sub_821F9E10` (`rsi`) est figé BIT POUR BIT sur toutes les
   observations : une véritable boucle infinie non bornée, confirmée,
   pas un balayage long-mais-fini. Lecture mémoire en direct : l'entrée
   à l'adresse invitée `0x1009fa18` a son propre `next` qui pointe sur
   elle-même au lieu de la tête de liste (`0x10000180`) — la condition
   de terminaison ne peut jamais devenir vraie. Un point d'observation
   matériel armé dès la toute première entrée de `sub_821D5F48` (~2s,
   bien avant le blocage) et maintenu 40s pleines ne s'est JAMAIS
   déclenché : la valeur auto-référentielle était déjà présente avant
   ce chemin d'appel ce cycle — donnée statique/pré-init, pas une
   écriture au runtime sur ce chemin. **PAS ENCORE un blocage
   qualifié** : deux possibilités restent ouvertes — (1) défaut de
   chargement des données statiques propre à cette recompilation
   native, ou (2) donnée fidèlement expédiée par le retail, avec une
   omission en amont dans la logique du jeu original. **Nommé pour
   r370** : (a) extraire les données statiques du XEX retail aux
   adresses `0x1009fa18`/`0x10000180` depuis l'image disque qualifiée
   et comparer octet pour octet avec l'observation en direct ; (b) si
   fidèle, remonter ce qui devrait peupler/relier ce bucket en amont ;
   (c) si défaut de chargement mécaniquement corrigeable, appliquer et
   vérifier via le motif de correctif déjà autorisé — sinon nommer
   précisément un point de décision pour l'utilisateur. Voir
   `reports/ac6-retail-native-codegen-gate2-r280-doc-deadlock-refuted-codec-starves-on-us-tbl-over-pal-pac-20260906.md`
   §4.90 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r369-scan-cursor-check/README.md`.

2. **(contexte, r369 — voir ci-dessus)**

3. **r369 (historique, PAS un blocage qualifié).** r368 a
   revérifié EN DIRECT la revendication d'épuisement de graphe d'appel
   de r341/r349/r350 contre le binaire D'AUJOURD'HUI (deux fuites
   corrigées) et l'a trouvée obsolète dans son cadrage : nouvelle
   technique fiable de `SIGINT` envoyé DIRECTEMENT au PID de
   l'inférieur (pas `gdb.execute("interrupt")` en thread python,
   déjà prouvé non fiable r353), confirmée sur quatre exécutions.
   `sub_821D7DE0` (disas) ne peut sauter sa vraie boucle par image par
   aucune branche une fois atteinte — mais Sonde 1 (60s, neuf sites
   d'appel/retour) montre ZÉRO déclenchement au-delà de l'entrée de
   fonction : l'exécution ne revient JAMAIS de son premier appel,
   `sub_821D5F48`. Sonde 2 : rafale de 293 passages dans la chaîne
   allocateur déjà épuisée (`sub_821D5600`→...→`sub_821FA6F8`) en
   <0,5s, puis silence total 59s — inchangé par les deux fuites
   corrigées. Sonde 3 (backtrace des 25 threads via SIGINT) : le
   thread principal a pris une AUTRE branche du répartiteur
   `sub_82121308` que celle épuisée par r341-350, atteignant
   `sub_8236B3F8`→`sub_8236E868`→`sub_823801B8`→`sub_821F9E10` — et y
   EXÉCUTE EN DIRECT. Sonde 4 (3 échantillons PC espacés de 4s) :
   tous dans une plage de 9 octets (`+2857`/`+2866`/`+2857`) au sein
   d'une boucle de balayage de table à deux étages — preuve directe de
   plusieurs secondes réelles passées ici, mais PAS une preuve de
   boucle infinie (aucun registre comparé entre échantillons). **Ceci
   corrige le CADRAGE de r341/r349/r350 (pas leurs mesures)** :
   `sub_821D5F48` a une branche jamais parcourue, atteinte seulement
   maintenant que les deux fuites sont corrigées et que le processus
   survit assez longtemps. **PAS (encore) un blocage qualifié** — un
   emplacement candidat, pas une dépendance externe confirmée. **Nommé
   pour r369** : (a) breakpoint sur la cible de rebouclage
   (`sub_821F9E10+2996`) et vérifier si le curseur de balayage avance
   réellement entre les déclenchements, lire ce que représentent
   `r8d`/`rdx` (échantillonnés `8093`/`8093`) depuis l'objet invité ;
   (b) si borné et complet mais blocage persiste ensuite, reprendre le
   parcours linéaire sur la suite ; (c) si le balayage cherche une
   correspondance structurellement impossible dans ce bac à sable
   (contenu disque/asset manquant), nommer alors explicitement le
   blocage qualifié — pas avant. Voir
   `reports/ac6-retail-native-codegen-gate2-r280-doc-deadlock-refuted-codec-starves-on-us-tbl-over-pal-pac-20260906.md`
   §4.89 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r368-e20-loop-second-pass/README.md`.

2. **(contexte, r367-r368 — voir ci-dessus pour la suite)**

3. **r367 (historique, PAS un blocage qualifié).** r367 a
   instrumenté directement les points de décision de boucle de
   l'OUVRIER (`sub_8233A890`) au niveau désassemblage (deux offsets de
   branchement localisés : `+137` vérification initiale, `+343`
   vérification de continuation). Sonde 120s : l'ouvrier entre une
   fois, drapeau initial non nul, vrai travail par image entré une
   fois, mais la vérification de continuation ne se déclenche JAMAIS.
   Sonde 30s (chaque appel de l'itération instrumenté séparément) :
   `sub_8233DF90`/`sub_8233A830`(×2)/`sub_8233E2F0` chacun une fois ;
   `sub_82345CE0` (« attendre occupé ») exactement 3 fois, la 3e étant
   le SECOND appel d'attente de l'ouvrier lui-même juste après la fin
   du vrai travail — puis silence total sur ces 5 sites pour ~29s,
   pendant que `sub_82345C88` (primitive générique partagée) tourne à
   ~2800/s (processus vivant, pas planté). **Conclusion : le thread
   ouvrier exécute complètement sa première itération réelle, puis
   bloque LÉGITIMEMENT en attendant un second signal que le producteur
   (thread principal) n'envoie jamais — le côté OUVRIER est
   complètement EXONÉRÉ.** Ceci affine la découverte de r360/r366 côté
   producteur (le triplet `sub_8233B5A0`/`sub_8233A620`/`sub_8233A610`
   se déclenche UNE FOIS puis silence permanent jusqu'à 900s) : le
   producteur ne repose jamais la question une seconde fois. Question
   ouverte inchangée en nature, affinée en confiance : pourquoi
   `sub_821D7DE0` (boucle principale) n'exécute jamais une seconde
   itération, alors que r341/r349/r350 avaient déjà déclaré son graphe
   d'appel statique exhaustivement tracé et vide, bien avant la
   découverte des deux fuites de section critique (chaîne d'appel
   différente, depuis `sub_821D5F48`). **Nommé pour r368** : (a)
   revérifier la revendication d'épuisement de r341/r349 contre le
   binaire ACTUEL (deux correctifs de fuite déjà appliqués) plutôt que
   de faire confiance à une conclusion antérieure à ces correctifs ;
   (b) chercher si la CONDITION d'entrée de la boucle par image dépend
   d'une valeur de donnée à l'exécution (une cible d'appel indirect
   résolue différemment, ou une comparaison contre une valeur que ce
   bac à sable ne produit jamais) plutôt qu'un chemin de code manquant
   — r356 a déjà trouvé exactement cette forme de bug une fois ; (c) si
   cela aussi revient vide, nommer explicitement que l'instrumentation
   côté hôte approche la limite de ce qu'elle peut résoudre pour ce
   symptôme précis, et que continuer à y investir face à pivoter vers
   le câblage mort de `presented_frames` (r292, toujours non corrigé
   aujourd'hui) est une décision dont l'utilisateur pourrait vouloir
   être informé. Voir
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r367-worker-flag-check/README.md`.

2. **(contexte, r366-r367 — piste de fuite de section critique close ;
   voir ci-dessus pour la suite)**

3. **r366 (historique, PAS un blocage qualifié).** Sur choix
   explicite de l'utilisateur ("inventer un correctif maintenant"),
   r366 a appliqué et vérifié EN DIRECT le correctif INVENTÉ de
   `sub_821FA9E0` (`RtlLeaveCriticalSection` conditionnel sur `r23 & 1`,
   inséré juste avant l'unique retour de la fonction ; dérivé
   directement de la lecture du corps généré, pas deviné). Sonde
   globale de r364 (45s) : `sub_821FA9E0` maintenant **2/2, net 0**
   (était 3/0, net +3) ; résidu total `net=1` (uniquement le +1
   légitime déjà expliqué de `sub_821F9E10`) -- plus aucune fuite non
   identifiée sur ce verrou. **MAIS** re-exécution de la sonde EXACTE
   de r360 sur 300s post-correctif : `sub_821D7AE0`/`sub_821D7CD0` (la
   vraie boucle par image) ne se déclenche TOUJOURS JAMAIS, et le
   triplet de poignée de main (`sub_8233A620`/`sub_8233B5A0`/
   `sub_8233A610`) ne se déclenche qu'UNE SEULE FOIS puis se tait pour
   les ~299s restantes -- motif IDENTIQUE à r360, totalement INCHANGÉ
   par cette seconde correction réelle et vérifiée. **Conclusion
   centrale** : deux corrections de fuite de section critique
   indépendamment confirmées (r358, r366) n'ont fait avancer d'AUCUNE
   itération le test d'entrée dans la boucle par image -- la piste de
   fuite/blocage mutuel tracée depuis r311 se clôt ici à son terminus
   honnête (les deux fuites connues sur ce verrou sont maintenant
   corrigées et vérifiées, sa comptabilité est entièrement expliquée)
   SANS avoir répondu à la question posée par l'utilisateur
   ("pourquoi `presented_frames` reste à 0"). **Nommé pour r367** :
   pivoter vers l'instrumentation directe du côté OUVRIER du triplet de
   poignée de main (qu'est-ce qui change entre le succès de
   l'itération 1 et le silence de l'itération 2), en n'utilisant QUE
   des breakpoints locaux à la fonction ou des fenêtres longues sans
   gdb. **NOUVEAU risque d'instrumentation documenté** : le crochet
   global `RtlEnterCriticalSection`/`RtlLeaveCriticalSection` (utilisé
   depuis r364) a provoqué un SIGSEGV connu (course CREATE_SUSPENDED,
   r114/r280/r327/r339/r340) sur deux exécutions longues (45s et
   180s) ; la trace « verrou tenu, nouvelle pile d'appel » qui en a
   résulté est explicitement RÉTRACTÉE (capturée pendant/après le
   crash, pas en direct) -- confirmée dépendante de la durée par une
   exécution de contrôle à 20s qui sort proprement. **Ne plus utiliser
   ce crochet global au-delà d'environ 45s.** Voir
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r366-invented-fix/README.md`.

2. **(contexte, r365 — épuisé par r366 ci-dessus, décision suivante déjà prise et exécutée)**

3. **r365 (historique) — décision de l'utilisateur nommée
   par r365.** L'utilisateur a choisi l'option 1 de r364 ("continuer à
   remonter"). r365 a remonté la chaîne d'appel au-dessus de
   `sub_821FA9E0` jusqu'à épuisement : **correction à r364** -- les 4
   sites d'appel de `sub_823857E0` se partagent entre DEUX fonctions
   distinctes (`sub_8237FA48` : 2 sites, jamais appelée en direct dans
   ce scénario, confirmé par 225s cumulées de breakpoint EN DIRECT sans
   aucun coup ; `sub_8237FA50` : les 2 autres, confirmée comme le VRAI
   appelant par 3 backtraces identiques en direct sur `sub_821FA9E0`
   lui-même). Chaîne réelle : `sub_821FA9E0` <- `sub_823857E0` <-
   `sub_8237FA50` <- `sub_8237FB58` <- `sub_821F7B28` (marcheur
   GÉNÉRIQUE de table d'initialiseurs statiques du CRT -- deux plages
   de table fixes, appel indirect `bctrl` sur chaque entrée non nulle,
   ZÉRO section critique, zéro logique métier) <- `__imp___xstart`
   (machinerie hôte de création de thread) <- `std::thread` -- un
   thread OUVRIER dédié, pas la pile bloquée du thread principal.
   **Plus aucune chaîne d'appel invité à remonter** : la frontière
   invité/hôte est atteinte. Le candidat « frère » à motif partagé
   (`sub_821FB060`, dont les adresses retail chevauchent la queue de
   `sub_821FA9E0`) est un STUB DE CODEGEN INCOMPLET (`// ERROR
   821FB0B0` puis un `return` nu) -- pas une copie correcte à imiter.
   **L'option 1 est donc close sur ses PROPRES termes** (frontière
   atteinte, pas budget épuisé) : l'hypothèse « aucun niveau ne relâche
   ce verrou » est confirmée avec toute la profondeur possible.
   **Décision affinée nommée pour l'utilisateur** : (1) appliquer
   maintenant un correctif inventé dans `sub_821FA9E0` lui-même (choix
   réfléchi après épuisement de la remontée, pas un raccourci) ; (2)
   explorer un protocole armer/désarmer INTER-appels plutôt qu'une
   paire enter/exit du même appel (piste plus profonde, sans garantie
   de succès) ; ou (3) arrêter la fermeture du blocage par correctif
   natif et rediriger vers le câblage toujours mort de
   `presented_frames` (r292/r359) ou un autre angle. **NE PAS réarmer
   un `gdb.BP_WATCHPOINT` brut sur ce mutex** (crash de GDB, r362).
   Voir `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r365-upward-trace/README.md`.

2. **(contexte, r364 — épuisé par r365 ci-dessus)** Une sonde EN DIRECT globale filtrée par clé (une seule paire
   de breakpoints sur les points d'entrée hôte de
   `__imp__RtlEnterCriticalSection`/`__imp__RtlLeaveCriticalSection`,
   filtrée sur `*(uint32_t*)$rdi == 0x10000610`, comptée par adresse de
   retour de l'appelant -- au lieu du balayage statique des 242 sites
   nommé par r363) a **entièrement expliqué** le résidu `__count=4` de
   r361 : `sub_821F9E10` (575/574, net +1, structurellement incapable
   de fuir -- un appel légitimement en vol sur un autre thread) ;
   `sub_821FA6F8` (208/208, net **0** -- **le correctif r358 est
   confirmé PARFAITEMENT équilibré en direct**) ; `sub_821FA9E0` (3/0,
   net **+3** -- une SECONDE fuite Enter/Leave réelle, jamais
   corrigée). `1+0+3=4`, aucune source inconnue ne subsiste.
   `sub_821FA9E0` (1136 lignes générées) entre conditionnellement
   `*(r27+1408)` (même motif que `sub_821FA6F8` sur `*(r30+1408)`, même
   idiome de drapeau) mais **zéro** appel `RtlLeaveCriticalSection`
   nulle part dans la fonction, ni dans son unique appelant
   (`sub_823857E0`), ni dans les appelants de celui-ci (`sub_8237FA48`,
   tracés ce cycle). **Contrairement à `sub_821FA6F8`, aucun chemin de
   sortie frère déjà correct n'existe dans la même fonction pour servir
   de modèle** -- corriger ici exigerait d'INVENTER une logique de
   relâchement plutôt que d'en copier une déjà correcte, un changement
   d'une nature matériellement différente de celui autorisé pour
   r356/r358. **Décision nommée pour l'utilisateur** : (1) continuer à
   remonter la chaîne d'appel (`sub_8237FA48` et au-delà) pour trouver
   un site de relâchement authentique à imiter -- reste dans le motif
   déjà autorisé, mais peut prendre plusieurs cycles de plus, comme la
   chaîne à 13 fonctions de r354 ; (2) appliquer un correctif inventé
   par analogie (Leave sur le même bit de drapeau, au retour de la
   fonction) sans contrôle local prouvant que c'est le bon site --
   plus rapide, mais une décision d'une nature nouvelle ; ou (3)
   accepter l'état actuel (359943→4, un bug entièrement corrigé et
   vérifié) comme point final de cette piste et rediriger vers autre
   chose (p.ex. le câblage toujours mort de `presented_frames`,
   r292/r359). **NE PAS réarmer un `gdb.BP_WATCHPOINT` brut sur ce
   mutex** -- cela a fait planter GDB LUI-MÊME (r362) ; un breakpoint
   conditionnel simple (utilisé ce cycle) fonctionne bien. Voir
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r364-second-leak-hunt/README.md`.

2. **(contexte, r362-r364, déjà traité ci-dessus)**

3. **r361 — contexte historique.** r361 a
   CORRIGÉ la prémisse de r360 (« r316 jamais exécutée » était FAUX --
   r316-r325 avaient déjà entièrement achevé cette tâche, prouvant en
   direct un blocage circulaire AB-BA de manuel entre le thread
   principal et le worker sur ce verrou exact). r361 a re-exécuté la
   technique BYTE POUR BYTE de r325 sur le binaire POST-correctif r358
   : backtrace du détenteur IDENTIQUE à r325 (thread principal bloqué
   sur un futex en détenant `0x10000610`), **`__count=4` au lieu de
   `359943`** -- le correctif r358 a réduit l'AMPLEUR d'une contribution
   à la récursion détenue, mais PAS fermé le blocage : `__lock=1` tenu
   par un autre thread bloque le worker quel que soit le compte. Deux
   pistes à départager pour r362 : (a) une SECONDE paire Enter/Leave
   déséquilibrée, encore non corrigée, ailleurs dans les ~90 sites
   d'appel de l'allocateur (réutiliser les techniques statiques de
   r326-r333, restreintes maintenant à « qu'est-ce qui entre encore
   `0x10000610` sans Leave correspondant, post-correctif ») ; (b) un
   problème d'ORDONNANCEMENT structurel -- le propre Enter du thread
   principal n'est simplement jamais suivi de son Leave avant qu'il
   n'atteigne l'attente bloquante (`sub_8233B5A0`→`sub_82345CE0`). Ce
   sont deux bugs différents avec deux correctifs différents. Si (b),
   vérifier si le code retail Xbox 360 original emprunte ici un chemin
   plus étroit/différent avant de supposer qu'un correctif natif est
   même nécessaire -- ceci pourrait devenir un nouveau point de
   décision QUALIFIÉ. Ne PAS re-citer `presented_frames` ou le
   comportement de sortie du processus comme preuve que le blocage est
   résolu -- les deux sont maintenant des signaux confirmés non
   fiables. Voir §4.82 du rapport et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r361-postfix-holder-identify/README.md`.

## Historique (r360-r361, contexte précédent conservé)

1. **r360 — re-tester la méthode de r312-r314 (vraie boucle par image
   `loc_821D7E84` dans `sub_821D7DE0`, via `sub_821D7AE0`/
   `sub_821D7CD0`) POST-correctif r358, sur une fenêtre >=600s.**
   r359 a relu la lacune r311-r333 en entier et confirmé (r325/r326
   l'avaient déjà nommé) que le blocage circulaire résolu par r358 EST
   la continuation directe de la question r283-r297 (« pourquoi
   `presented_frames` reste à 0 »), pas un sujet séparé. r359 a aussi
   RECONFIRMÉ que `presented_frames` est câblé sur du code mort depuis
   r292 (`submit_ring()`, un seul appelant = un test unitaire),
   inchangé aujourd'hui -- **ne plus le citer comme signal de
   progrès** ; le signal correct est la trace `AC6_NATIVE_VD_TRACE`
   et/ou un décodage `PresentPacket`/`XE_SWAP`. Une sonde de 600s
   post-correctif a trouvé un NOUVEAU 6e lot d'anneau VD (jamais vu
   avant le correctif à AUCUNE fenêtre testée, 15s-180s) avec du
   contenu réellement neuf (write_index 19→25→31→37) -- preuve directe
   que le correctif a permis un progrès mesurable au-delà de tout ce
   qui était observé avant. Mais toujours zéro `PresentPacket` décodé,
   et le progrès s'arrête de nouveau après ce 6e lot -- question
   ouverte pour r360. r312-r314 avaient trouvé (entièrement AVANT le
   correctif) que le thread principal n'atteint JAMAIS la vraie boucle
   par image en 400s -- ceci n'a PAS encore été re-testé après le
   correctif et doit l'être en premier. Aucun blocage qualifié ce
   cycle. Voir §4.80 du rapport et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r359-postfix-longwindow/README.md`.

## Historique (r359, contexte précédent conservé)

1. **r359 — investiguer pourquoi `presented_frames` reste à 0 après le
   correctif de r358.** L'utilisateur a explicitement choisi
   « Apply the minimal native-side fix » (option 2 du point de
   décision qualifié r356/357). r358 a appliqué ce correctif via un
   NOUVEAU script TRACKÉ `tools/apply_sub_821fa6f8_leave_fix.py` :
   ajout d'un `RtlLeaveCriticalSection(*(r30+1408))` CONDITIONNEL
   (subordonné à `r25 != 0`) à `loc_821FA94C` (sortie du chemin B de
   `sub_821FA6F8`), reflétant la logique déjà correcte de
   `loc_821FA924`. Le bloc cible apparaissait deux fois (duplication
   par XenonRecomp d'un épilogue partagé avec `sub_821FA6F0`, même
   code retail, pas deux bugs) -- les deux occurrences sont patchées
   par cohérence. **VÉRIFIÉ EN DIRECT** via la cascade `A830Watch`
   déjà prouvée : `recursion=4` au lieu de `359943` au MÊME point de
   contrôle -- **la fuite confirmée depuis r324 est éliminée.** Un run
   propre de 90s se termine maintenant NORMALEMENT de lui-même
   (`generated entry terminated its own thread`) au lieu de rester
   bloqué -- le blocage mutuel circulaire confirmé (r315-338) ne se
   produit plus. **`ctest` 10/10, `pytest` 222/1 skip** -- aucune
   régression. **Cependant** : `presented_frames=0`,
   `entry_returned=0` toujours après ce même run de 90s -- le
   correctif résout le blocage SPÉCIFIQUE confirmé, mais ne démontre
   PAS à lui seul que le jeu atteint un gameplay effectivement rendu.
   Étant donné l'historique de ce projet (des centaines de cycles
   antérieurs à travers de nombreux sous-systèmes), il ne serait pas
   surprenant qu'un AUTRE goulot d'étranglement devienne maintenant le
   facteur limitant. Étapes pour r359 : — investiguer pourquoi `presented_frames` reste à 0 après le
   correctif de r358.** L'utilisateur a explicitement choisi
   « Apply the minimal native-side fix » (option 2 du point de
   décision qualifié r356/357). r358 a appliqué ce correctif via un
   NOUVEAU script TRACKÉ `tools/apply_sub_821fa6f8_leave_fix.py` :
   ajout d'un `RtlLeaveCriticalSection(*(r30+1408))` CONDITIONNEL
   (subordonné à `r25 != 0`) à `loc_821FA94C` (sortie du chemin B de
   `sub_821FA6F8`), reflétant la logique déjà correcte de
   `loc_821FA924`. Le bloc cible apparaissait deux fois (duplication
   par XenonRecomp d'un épilogue partagé avec `sub_821FA6F0`, même
   code retail, pas deux bugs) -- les deux occurrences sont patchées
   par cohérence. **VÉRIFIÉ EN DIRECT** via la cascade `A830Watch`
   déjà prouvée : `recursion=4` au lieu de `359943` au MÊME point de
   contrôle -- **la fuite confirmée depuis r324 est éliminée.** Un run
   propre de 90s se termine maintenant NORMALEMENT de lui-même
   (`generated entry terminated its own thread`) au lieu de rester
   bloqué -- le blocage mutuel circulaire confirmé (r315-338) ne se
   produit plus. **`ctest` 10/10, `pytest` 222/1 skip** -- aucune
   régression. **Cependant** : `presented_frames=0`,
   `entry_returned=0` toujours après ce même run de 90s -- le
   correctif résout le blocage SPÉCIFIQUE confirmé, mais ne démontre
   PAS à lui seul que le jeu atteint un gameplay effectivement rendu.
   Étant donné l'historique de ce projet (des centaines de cycles
   antérieurs à travers de nombreux sous-systèmes), il ne serait pas
   surprenant qu'un AUTRE goulot d'étranglement devienne maintenant le
   facteur limitant. Étapes pour r359 :
   (a) investiguer pourquoi `presented_frames` reste à 0 -- fenêtre de
       test plus longue et/ou instrumentation différente pour
       déterminer si un NOUVEAU goulot d'étranglement est devenu le
       facteur limitant, ou si plus de temps/un déclencheur différent
       est simplement nécessaire ;
   (b) envisager d'intégrer le script de correctif au pipeline de
       build habituel (invocation automatique après régénération du
       codegen) plutôt qu'une invocation manuelle après chaque
       `generate_native_guest.py` frais ;
   (c) ceci reste très plausiblement UNE PARTIE de la réponse à la
       question originelle de r283-r297 (« pourquoi `presented_frames`
       reste à 0 ») -- le blocage spécifique tracé depuis r311 est
       résolu, mais la question globale peut nécessiter d'autres
       correctifs encore non identifiés ;
   (d) NE PAS revenir sur le côté hôte déjà vérifié (sondeur VD,
       câblage de présentation, primitives d'attente bornées, ni la
       course `CREATE_SUSPENDED` déjà adressée r114/r280) sans preuve
       nouvelle et directe.
2. **Discipline** : resync `native` → `native-source` avant chaque build ;
   build sous cgroup ; ctest 10/10 ; pytest `tests/` ; pour poser un point
   d'arrêt sur une instruction précise, `break *(NOM_SYMBOLE+DÉCALAGE)` ;
   avant d'ajouter un nouveau verrou global à un site d'appel à haute
   fréquence, vérifier s'il peut être PAR CLÉ plutôt que global (piège
   introduit puis corrigé en r286/r287, sur le modèle déjà établi de
   `critical_section_for`) ; vérifier toute chaîne `\n` ajoutée dans
   `materialize_native_import_stubs.py` en régénérant et en lisant le
   fichier produit (`\\n` à deux caractères — et vérifier qu'aucun `#`
   n'apparaît par erreur en colonne 0 dans le texte C++ généré, ce qui
   casserait la compilation comme directive de préprocesseur invalide —
   piège rencontré et corrigé dans le MÊME cycle avant tout build) ; ne
   pas garder les traces brutes (réduire en extraits représentatifs) ;
   rapports + artefacts par cycle ; pas de commit tant que la gate racine
   échoue pour une cause étrangère au cycle — le dire dans le rapport.

## Contexte r310 (verrouillé) — corrige r309 : PAS un minuteur périodique, une course divergente

- Sur 150s : `sub_8233A610` reste FIXE à 2 occurrences (réfute le
  minuteur périodique) ; `sub_8233B378` continue à ~473/s (mise à
  l'échelle linéaire, PAS une rafale qui plafonne).
- Le compteur `+64` galope indéfiniment (>70 000 en 150s) — pas bloqué.
  La CIBLE de `sub_8233B5A0` doit donc croître plus vite que le
  compteur après un succès précoce — course divergente, pas minuteur.
- `sub_8233A890` reste vivant tout le temps (sondage `/proc` 90s) — pas
  de mort précoce.
- N'invalide PAS le constat de r309 sur r291 (10 minutes déjà testées
  sans effet sur `presented_frames`) — juste le MÉCANISME (course
  divergente, pas minuteur périodique).
- r311 doit lire directement cible et compteur aux invocations de
  `sub_8233B5A0` pour confirmer.

## Contexte r309 (verrouillé) — CLÔTURE r298-r308 ; PIVOT vers la vraie frontière de r297

- `objet+88`/`+96` de `sub_8233B378` sont son verrou PRIVÉ (Mutant/
  Événement générique), pas une ressource externe. La boucle est un
  compteur pur, borné uniquement par la cadence ~2ms des primitives
  hôte — explique exactement les ~459/s de r308.
- La cadence de ~15s de `sub_8233B5A0` (~6900 incréments) ressemble à
  un COMPTEUR/MINUTEUR LOGICIEL DÉLIBÉRÉ, pas un bug. TOUTE la chaîne
  (r283-r308) est maintenant confirmée fonctionner correctement.
- RÉCONCILIATION CRITIQUE avec r291 : 10 minutes déjà testées,
  ~180-420 cycles complétés, ZÉRO effet sur `presented_frames`.
  Répondre à la cadence CPU n'allait JAMAIS répondre à la présentation
  d'image (r294-r297 : le contenu d'anneau n'inclut jamais de vrai
  présent, quel que soit le nombre de cycles).
- r310 doit PIVOTER vers la seconde recommandation de r297 jamais
  achevée : localiser le code invité de CONSTRUCTION d'un
  `PresentPacket`, pas l'appel `VdSwap` lui-même.

## Contexte r308 (verrouillé) — corrige r303 ; la question devient quantitative, pas mécanique

- Relecture COMPLÈTE de `sub_8233B378` (82 lignes, r303 s'était arrêté à
  35) révèle une VRAIE boucle interne jamais vue par la mesure d'entrée
  de r303. Correction explicite nommée.
- Mesure directe (point d'arrêt sur le vrai site d'appel dans la
  boucle, `__imp__sub_8233B378+0xcd`) : ~459 itérations/seconde (13 761
  en 30s), tid=16.
- Malgré ceci, `sub_8233A610` (signal-occupé de `sub_8233B5A0`) ne se
  déclenche que 2 fois/30s — confirmation 1:1 avec les complétions de
  `sub_8233B5A0`.
- Le compteur `+64` avance VITE ; c'est la CIBLE de `sub_8233B5A0` qui
  exige des milliers d'incréments (~6900 estimé) par cycle. AUCUN
  mécanisme cassé ou lent trouvé nulle part — question maintenant
  QUANTITATIVE (combien de travail réel par cycle, pas pourquoi c'est
  lent par appel).
- r309 doit identifier l'unité de travail réelle de la boucle et ce qui
  fixe la cible.

## Contexte r307 (verrouillé) — le thread confirmé sonde légitimement, ni affamé ni bloqué

- TID OS réel de `sub_8233A890` capturé avec certitude via gdb
  (perturbation minimale, un seul arrêt ponctuel).
- Échantillonnage `/proc` de CE thread précis : 94% du temps en
  `hrtimer_nanosleep` LÉGITIME (sondage actif borné de `wait_mutant`,
  r288 — pas `futex_do_wait` comme r301 avait trouvé pour un AUTRE
  mécanisme/thread).
- ÉCARTE DÉFINITIVEMENT la famine OS ET une primitive bloquée pour ce
  thread précis. La question reste celle de r298/r299 : pourquoi la
  condition qu'il sonde prend-elle tant de tentatives (des dizaines de
  milliers à ~200µs chacune) ?
- r308 doit mesurer directement le rythme du signal-occupé de
  `sub_8233B5A0` sur la porte `+152` de `sub_8233A890` (jamais mesuré
  isolément) — même technique que r299.

## Contexte r306 (verrouillé) — corrige la prémisse : aucun réengendrement, retour au cadrage original de r298/r299

- Trace d'engendrement sur 90s IDENTIQUE OCTET POUR OCTET à celle de 30s
  de r305 — les 8 threads ouvriers sont créés UNE SEULE FOIS, tôt, JAMAIS
  réengendrés (même motif que r292-r294 pour l'anneau GPU, r298-r300
  pour la chaîne d'appel invitée).
- `sub_8233A890` n'est PAS réengendrée — c'est l'UNIQUE instance déjà
  caractérisée par r287-r299. L'angle de création de thread (r304-r306)
  est ÉPUISÉ.
- La question revient au cadrage ORIGINAL de r298/r299 avec pleine
  confiance : pourquoi la boucle interne de CETTE instance unique
  prend-elle 5-8s par itération.
- r307 doit identifier le VRAI TID OS de `sub_8233A890` et lui appliquer
  l'échantillonnage `/proc` de r301, ciblant cette fois le bon thread
  confirmé plutôt que deviné.

## Contexte r305 (verrouillé) — PLUS GRANDE CORRECTION JUSQU'ICI : `sub_8233A890` lui-même est engendré fraîchement, pas persistant

- Vrai répartiteur trouvé : `sub_823453E8` charge `task_function` depuis
  `*(descripteur+20)`, `task_argument` depuis `+24`, appelle
  indirectement.
- Sur 30s, 8 threads ont exécuté 6 fonctions de tâche DISTINCTES :
  `sub_8233B378` (confirme r303), `sub_8233B748` (nouvelle),
  **`sub_8233A890` LUI-MÊME**, `sub_823466C0` (×3), `sub_82344050`,
  `sub_8211C7A8`.
- CONFIRME ET AFFINE la correction de r304 : elle s'applique à
  `sub_8233A890`, l'objet central de toute l'investigation, pas
  seulement à `sub_8233B378`. N'annule PAS les mesures internes de
  r287-r299 (boucle propre à l'instance vivante), change seulement le
  CADRE (thread créé fraîchement, pas persistant).
- r306 doit tracer `ExCreateThread` filtré sur
  `task_function==0x8233a890` sur une fenêtre longue et trouver qui
  l'appelle — c'est la cible précise et correcte de « pourquoi 5-8s ».

## Contexte r304 (verrouillé) — `sub_821F8008` n'est PAS un répartiteur ; correction de la description de r283

- `sub_821F8008` : trampoline générique d'entrée de thread à usage
  unique (appelle le pointeur reçu, puis `ExTerminateThread`
  inconditionnellement). Aucune logique de répartition.
- `ExCreateThread` : 21 déclenchements en 15s, dont 8 SÉPARÉS avec
  `routine=0x823453e8` — PAS un pool fixe de 8 persistants créés une
  fois à l'amorçage. CORRECTION EXPLICITE de la description de r283.
- Modèle de création de thread PAR TÂCHE : un argument
  (`routine_argument`, `ctx.r8.u32`) détermine la tâche réelle.
- r305 doit capturer cet argument (variable dédiée, PAS
  `AC6_NATIVE_IMPORT_TRACE`) pour corréler quel argument mène à quelle
  sous-tâche, puis trouver qui décide de demander la tâche
  `sub_8233B378`.

## Contexte r303 (verrouillé) — écrivain du compteur trouvé, encore plus rare que tout le reste

- `sub_8233B378` (l'écrivain de `*(objet+64+16)`) trouvé en cherchant
  tous les appelants de `sub_82345C88`. Incrémente `*(objet+8)` sous
  section critique, écrit la nouvelle valeur via `sub_82345C88`.
- Balayage gdb : `sub_8233B378` — 1 SEULE occurrence en 15s. Appelée via
  `sub_823453E8` (routine ouvrier, r283) → `sub_821F8008`, un chemin
  DIFFÉRENT de celui vers `sub_8233A890`. Sur tid=16 (le thread « bruit »
  de r299 — pas du bruit pour CE mécanisme).
- Le pool d'ouvriers répartit vers des sous-tâches indépendamment
  rares — `sub_8233B378` pourrait être la VRAIE composante limitante,
  pas `sub_8233A890`.
- r304 doit lire `sub_821F8008` (le répartiteur) pour comprendre la
  condition de répartition.

## Contexte r302 (verrouillé) — les « sites jumeaux » attendent le MÊME objet, pas deux ouvriers différents

- `sub_8233B4D0`/`sub_8233B538` lus en entier : vraies boucles de
  relance (motif de `sub_82345CE0`, r298), TOUTES DEUX sur le MÊME objet
  `r31+64` (un troisième objet-porte, distinct de celui de r298/r299).
- Poignée de main en DEUX PHASES autour de `sub_8233E0A8`/VdSwap :
  `>=` cible (B4D0) → travail VdSwap → `>` cible rechargée (B538).
- CORRECTION EXPLICITE de l'hypothèse de travail de r301 : même objet,
  pas des ouvriers différents en séquence.
- r303 doit mesurer chaque phase indépendamment par gdb et trouver ce
  qui écrit `*(cet-objet+16)`.

## Contexte r301 (verrouillé) — échantillonnage OS : famine par l'ordonnanceur écartée comme explication probable

- ~15 des 29 threads : ~93% CPU en continu — probable source, par
  thread, du bruit agrégé de r289 (nombreux threads busy-pollant chacun
  leur propre objet-porte).
- AU MOINS UN thread : `futex_do_wait` sur TOUTE une fenêtre de 20s —
  réellement stationné, pas affamé par l'ordonnanceur.
- Écarte la famine OS comme explication probable pour la poignée de main
  suivie depuis r283 ; un thread stationné dans un vrai futex attend
  d'être RÉVEILLÉ (signal applicatif), pas de temps CPU.
- r302 doit lire `sub_8233B4D0`/`sub_8233B538` (jamais lus en entier) —
  la cadence de 5-8s pourrait être la SOMME de plusieurs attentes
  séquentielles sur différents ouvriers dans un seul appel.

## Contexte r300 (verrouillé) — la remontée dans le code invité est ÉPUISÉE (résultat négatif propre)

- `sub_82331E78` (43 lignes) : triviale, aucune attente propre.
- `sub_821D7DE0` (142 lignes, la vraie boucle de jeu, r283) : boucle
  INCONDITIONNELLE sans aucune attente/minuterie/pause, appelle
  `sub_82331E78` À CHAQUE itération.
- La chaîne ENTIÈRE (boucle principale → poignée de main → ouvrier) a
  été lue de bout en bout : aucun mécanisme de cadence délibéré dans le
  code invité examiné jusqu'ici.
- Candidats restants : famine du thread ouvrier par l'ordonnanceur OS,
  ou dépendance bloquante non examinée dans le vrai travail par image de
  l'ouvrier lui-même (`sub_8233DF90`/`sub_8233A830`/`sub_8233E2F0`,
  jamais lues en entier).
- r301 REDIRIGE vers l'échantillonnage d'état OS (`/proc`, sans gdb) —
  la lecture de code généré supplémentaire n'est plus la bonne méthode.

## Contexte r299 (verrouillé) — le vrai travail par image lui-même est l'événement rare

- Écriture exacte de `*(porte+16)` trouvée dans `sub_82345C88`
  (`PPC_STORE_U64(porte+16, valeur)`, second argument de l'appelant).
- `sub_8233DF90` (premier appel de vrai travail, un hit propre par
  itération d'ouvrier) : EXACTEMENT 2 hits en 15 s (~0,13 Hz).
- `sub_82345C88` : 6670 hits/15s, mais 6664 sur tid=16 (bruit sans
  rapport, jamais examiné — même leçon que r289 sur l'agrégat non
  filtré). Seulement 5 sur tid=2 (principal), 1 sur tid=18 (ouvrier).
- La question remonte d'un niveau : pourquoi `sub_82331E78` n'appelle
  `sub_8233B5A0` qu'environ une fois toutes les 5-8 secondes ? Rien dans
  `sub_8233B5A0`/`sub_82345CE0`/`sub_8233A890`/`sub_82345C88` n'explique
  une cadence de plusieurs secondes — l'écart doit être dans
  `sub_82331E78` lui-même ou un de ses appels non encore lus en entier.

## Contexte r298 (verrouillé) — confirmé et affiné par mesure directe + désassemblage

- Test de ratio 1:1 en direct (`sub_8233B5A0`:`sub_8233E0A8`, points
  d'arrêt simultanés) : chaque entrée dans `sub_8233B5A0` est
  immédiatement suivie d'une entrée dans `sub_8233E0A8`, même thread —
  confirme empiriquement la revendication causale de r297.
- `sub_8233B5A0` est en fait ENTIÈREMENT LINÉAIRE (aucune branche) —
  le blocage se trouve plus loin, dans `sub_82345CE0` (atteinte via
  `sub_8233A620`), qui contient une VRAIE boucle de relance côté INVITÉ :
  `sub_821F4128`/`sub_821F5868` répétés jusqu'à
  `*(porte+16) == cible`.
- Ceci N'EST PAS une correction de r297 — c'est l'identification précise
  d'OÙ et COMMENT le blocage se produit physiquement.
- r299 doit trouver l'instruction exacte, côté ouvrier, qui écrit
  `*(porte+16)`, et mesurer son propre rythme.

## Contexte r297 (verrouillé) — DÉCOUVERTE UNIFICATRICE : la porte CPU EST la chaîne VdSwap

- Pile d'appels gdb en direct : `sub_82331E78` → `sub_8233B5A0` (moitié
  productrice de la porte CPU, r283-r291) → `sub_8233E0A8` →
  `sub_8234F558` → `sub_82347158` → `sub_821F03B0` → `VdSwap`.
- `sub_8233B5A0` n'atteint `VdSwap` qu'APRÈS le succès (rare,
  ~0,3-0,7/s) de son attente-de-terminé sur la porte Mutant/événement.
- Réconcilie TOUT r283-r297 en UNE SEULE cause racine ; corrige la
  portée implicite de r293 (mesure du débit agrégé dominé par les
  échecs, pas les succès — pas de contradiction réelle).
- `presented_frames=0` n'est pas plusieurs blocages indépendants : le
  taux de succès de la porte CPU EST la question à résoudre (r298).

## Contexte r296 (verrouillé) — mécanisme entier expliqué ; `VdSwap` appelé mais périmé

- Trace d'entrée ajoutée à `publish_write_address()` : `VdSwap` EST
  appelé (deux fois/15s, bien câblé), mais ZÉRO commit — no-op périmé
  les deux fois.
- Diagnostic affiné : `discovered=1` et `write_index == already_read`
  EXACTEMENT (19==19, 31==31) — le sondeur autonome a déjà drainé la
  même valeur avant l'appel `VdSwap` de l'invité.
- Le mécanisme entier est maintenant expliqué sans spéculation hôte
  supplémentaire : sondeur correct (r293), câblage présentation correct
  (r295), contenu d'anneau sans jamais de vrai présent (r294), appel
  `VdSwap` réel mais toujours périmé (r296).
- L'investigation bascule ENTIÈREMENT côté INVITÉ pour r297.

## Contexte r295 (verrouillé) — CORRECTIF RÉEL appliqué ; ambiguïté résolue définitivement

- `bind_guest_vd()` construit maintenant un vrai `VulkanDevice` +
  `VulkanOffscreenTarget` et appelle `bind_offscreen()`. Confirmé
  fonctionnel EN DIRECT sur cet hôte (test dédié, exit 0, aucune note de
  repli ; `present_target_configured=1` dans chaque lot sur l'ISO US).
- Malgré le correctif, TOUJOURS zéro `PresentPacket` décodé,
  `presented_frames` toujours 0 — ÉLIMINE définitivement l'hypothèse
  « hôte ignorait silencieusement une présentation ». L'invité n'en émet
  simplement jamais avant l'arrêt de son anneau.
- L'investigation converge maintenant sur UNE SEULE question côté
  INVITÉ, recoupant r283-r295 : pourquoi l'exécution s'arrête-t-elle de
  progresser après un peu de travail initial, dans au moins trois
  sous-systèmes montrant ce motif identique.

## Contexte r294 (verrouillé) — de vrais dessins ont lieu ; second bug de câblage confirmé (`bind_offscreen()`)

- 28 `DrawPacket` réels + 8 `ImmediateShaderPacket` + synchronisation
  dans les lots 2/4/5 de la rafale VD — PAS juste de l'init PM4. Le jeu
  dessine réellement quelque chose.
- `present_target_configured=0` dans chaque lot, zéro `PresentPacket`
  décodé. `bind_offscreen()` (seul point d'écriture de `present_target_`)
  n'a AUCUN appelant réel dans tout `native/` — seul un test unitaire
  l'appelle. Même motif que `submit_ring()` (r292).
- Même si l'invité émettait un `PresentPacket`, le runtime réel ne
  pourrait actuellement ni l'observer ni agir dessus.
- r295 doit câbler `bind_offscreen()` (tâche d'intégration dédiée,
  construire un `VulkanOffscreenTarget` réel, l'appeler au bon moment
  dans `boot()`), PUIS relancer une fenêtre longue pour distinguer
  « l'invité n'émet jamais de présentation » de « l'hôte en ignorait
  une silencieusement ».

## Contexte r293 (verrouillé) — aucun couplage causal étroit entre les deux stalls ; le VD n'est pas bogué

- `discover_write_index_locked` relit directement le champ live de
  l'invité à chaque poll de 1 ms — pas de redécouverte qui pourrait
  manquer une mise à jour. Le silence GPU = l'invité n'écrit simplement
  plus, pas un bug hôte.
- Sonde combinée 30 s (CPU + GPU) : l'anneau GPU s'arrête en ~1 s, la
  porte CPU continue à taux constant (550-715k/s) sur TOUTE la fenêtre,
  sans changement au moment de l'arrêt GPU. Aucun couplage étroit
  observable — n'écarte pas une cause partagée de plus haut niveau
  (progression du jeu bloquée avant les deux), mais aucun lien direct
  entre les deux mécanismes.

## Contexte r292 (verrouillé) — `presented_frames` est câblé sur du code mort ; le vrai chemin GPU vit puis s'arrête, comme la porte CPU

- `NativeRuntime::submit_ring()` (seul écrivain de `presented_frames`)
  n'a AUCUN appelant réel — seul un test unitaire l'appelle. Le
  diagnostic ne peut jamais devenir non nul par le vrai chemin de code.
- Le vrai chemin, `NativeGuestVdService::drain_locked()`, piloté par un
  thread réel, EST vivant : 5 « drain accepted » réels en 15 s, aucun
  rejet. Sur 90 s : trace IDENTIQUE octet pour octet — l'activité GPU
  s'arrête complètement après le 5e drain, sans erreur.
- Motif IDENTIQUE à la porte CPU de r283-r291 (« progrès puis silence »)
  mais dans un sous-système DIFFÉRENT et sans lien causal établi —
  possibilité d'une cause racine partagée, non confirmée.
- `presented_frames=0` n'a jamais été un signal fiable depuis r280 — bug
  de diagnostic réel, séparé du blocage runtime recherché.

## Contexte r291 (verrouillé) — PIVOT : la porte n'est probablement pas la cause ; suivre le chemin GPU

- Fenêtre de 10 min sans gdb/trace : `presented_frames=0` inchangé,
  malgré ~180-420 itérations réussies de la porte estimées au taux de
  succès de r290. Écarte « il suffit d'attendre plus longtemps ».
- `presented_frames` reflète `backend_.present_count()` (Vulkan),
  incrémenté sur un `PresentPacket`/`XE_SWAP` — chemin de soumission GPU
  ENTIÈREMENT SÉPARÉ de la porte CPU (Mutant/événement) investiguée
  depuis r283. Aucun lien causal établi entre les deux.
- r292 doit suivre le chemin GPU (`draw_count`/`resolve_count`/
  `present_count`, puis `submit()`/`present_to_offscreen()`), PAS
  reprendre l'investigation de la porte CPU sans preuve nouvelle.

## Contexte r290 (verrouillé) — les deux hypothèses de r289 sont RÉFUTÉES : c'est un vrai spin avec un succès rare, pas un artefact

- Réutilisation de handle ÉCARTÉE (lecture de code, pas de build) :
  `g_next_handle` est un compteur unique strictement croissant, jamais
  recyclé ; `NtClose` n'efface même pas `g_mutants`. `0x120`/`0x121`
  désignent le MÊME objet pour toute la vie du process.
- Surcharge de gdb ÉCARTÉE (mesure contrôlée, même run) : rejoué le point
  d'arrêt de r286/r287 sur `sub_82345CE0` EN MÊME TEMPS que le heartbeat
  de r289. Résultat : 5 occurrences en 15 s (cohérent avec r286/r287),
  MAIS le débit `gate120`/`gate121` reste à 162 000-196 000/s à CHAQUE
  seconde — indiscernable de la mesure sans gdb. gdb n'est PAS la cause.
- CONCLUSION RÉELLE, confirmée côté moteur : ~190 000 tentatives/s, mais
  seulement ~0,3-0,7 succès PAR SECONDE (pas par 15s) — ratio d'environ 1
  sur plusieurs cent mille. C'est un spin/livelock réel, pas un artefact
  de mesure.
- CORRECTION EXPLICITE DE r287 (par son nom) : sa caractérisation
  « parfois ~100ms, parfois 1,4s+, aucune explication » décrivait
  l'écart entre succès rares comme si c'était la durée d'un appel
  bloquant. Ce n'en est pas un. Les autres conclusions de r287 (correctif
  de verrouillage par clé, capture du cycle en 120ms) restent correctes.

## Contexte r289 (verrouillé) — DÉCOUVERTE INITIALE : la porte ne s'arrête jamais, contredit r283-r287 (hypothèses tranchées en r290 ci-dessus)

- Compteur d'appels 1/s ajouté à `wait_event`/`wait_mutant`, filtré sur
  les handles exacts de la porte (`gate120`=Mutant, `gate121`=événement).
- RÉSULTAT : ~182 000 appels/s SOUTENUS sur ces deux handles, sur toute
  une fenêtre de ~79 s sans gdb, jamais un creux. `presented_frames`
  reste 0.
- CONTREDIT DIRECTEMENT r286/r287 : leur point d'arrêt gdb sur la MÊME
  instruction de comparaison avait mesuré ~4 occurrences/15s — six
  ordres de grandeur d'écart.
- Hypothèse dominante NON TRANCHÉE : la surcharge de gdb sur un point
  d'arrêt touché des centaines de milliers de fois/s pourrait avoir
  ralenti l'exécution réelle au point de faire paraître bloquée une
  boucle qui tourne en fait sans jamais s'arrêter. Alternative non
  écartée : réutilisation de handle. Si l'hypothèse gdb se confirme,
  ceci n'est plus un blocage mais un spin/livelock sans progression, et
  la caractérisation « variance élevée » de r287 doit être corrigée par
  son nom.

## Contexte r288 (verrouillé) — les deux primitives hôte sont innocentées, la variance est ailleurs

- `wait_event()`/`wait_mutant()` instrumentés (bornés, anomalie
  `elapsed_ms>5` seulement) — gardé, sur sa PROPRE variable dédiée
  `AC6_NATIVE_WAIT_TIMING_TRACE` (pas `AC6_NATIVE_IMPORT_TRACE`, qui
  réactive les vieilles traces par-appel r284/r286/r287 et flood — 487
  Mo/4,3 M lignes en <1 min mesuré directement ce cycle).
- RÉSULTAT SUR FENÊTRE PROPRE (75 s, US ISO, sans gdb) : exactement 2
  anomalies (14 ms, 6 ms), toutes `wait_event`, zéro `wait_mutant`. Ces
  deux primitives NE dépassent JAMAIS leur borne d'environ 2 ms.
  `presented_frames` reste 0.
- CONCLUSION : la variance de 1,4 s+ (r283-r287) N'EST PAS dans
  `wait_event`/`wait_mutant`. Reste à déterminer si elle est dans les
  écarts ENTRE appels (site d'appel invité `sub_8233A890` à instrumenter,
  r289) ou dans un sous-système non instrumenté (`NtReadFile`, non
  écarté) ou si `presented_frames` exige un jalon ultérieur distinct.

## Contexte r287 (verrouillé) — correctif réel gardé, symptôme non résolu, mais nouvelle compréhension

- Verrouillage par Mutant (pas un verrou global) — gardé, corrige un
  vrai convoi de verrou (un tiers thread sans rapport monopolisait le
  verrou partagé 6922 fois/15s), mais SANS EFFET sur le symptôme observé.
- DÉCOUVERTE CLÉ : le mécanisme peut compléter un cycle complet en moins
  de 120 ms (capturé directement) — ce n'est PAS un blocage fixe, c'est
  une VARIANCE non expliquée entre cycles rapides et cycles de 1,4 s ou
  plus. 90 s sans débogueur ne suffisent toujours pas à atteindre
  `presented_frames` > 0.

## Contexte r286/r285/r284/r283/r282/r281/r280 (verrouillés)

- Voir les rapports précédents (§4ter à §4septies du rapport r280) — tous
  les correctifs sont réels, gardés et confirmés individuellement, mais
  aucun n'a encore résolu le symptôme de la boucle de jeu.

## Résultat requis

Atteindre visiblement le début du gameplay Mission 01 avec le runtime natif,
sans substitution de frontbuffer, état synthétique, compteur injecté ni
fallback ReXGlue — sur l'identité US scellée (`6eefba42…` / `204c5e64…`),
disponible dans le bac à sable.
