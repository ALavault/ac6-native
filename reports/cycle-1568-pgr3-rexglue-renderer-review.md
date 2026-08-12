# Cycle 1568 — PGR3/ReXGlue : renderer, cadence et paquet Linux

Date de qualification : **2026-08-12**. Cible de réutilisation : AC6 PAL,
Mission 01 uniquement. Aucun XEX, ISO, asset ou code recompilé PGR3 n'a été
téléchargé ; aucun binaire de jeu n'a été exécuté.

## Décision

Le seul projet public substantiel identifié est le monorepo
[`CrownParkComputing/xbox360-ports`](https://github.com/CrownParkComputing/xbox360-ports).
Son port PGR3 est un scaffold de dix fichiers : manifeste ReXGlue, quatre
frontières ajoutées à l'analyse, shell hôte et driver input générique. Ce n'est
pas un renderer natif PGR3. Le PPC est recompilé en C++, mais PM4, shaders,
textures, EDRAM, resolves et XMA restent exécutés par le plugin Vulkan
`rexgpu-xenos` dérivé de Xenia.

Le résultat renderer utile est une **frontière de divergence**, pas une
solution : le chemin host render targets/FBO dessine le monde correctement
selon le mainteneur, mais laisse ciel noir, reflet bruité et verre « confetti » ;
le chemin FSI rend le verre propre, mais corrompt fortement arbres, foule et
barrières. Aucun des deux chemins n'est donc une référence complète. La capture
RenderDoc citée, les images, les logs et le XEX exact ne sont pas publics.

Le projet apporte à AC6 des tests négatifs précis autour de l'EDRAM 2xMSAA,
du depth/sample coverage, des resolves vers cubemap et des placeholders shader.
Il n'apporte aucune sémantique `retail-qualified`, ne ferme aucune lane M01 et
ne justifie ni epsilon alpha, ni fetch invalide, ni limitation HLE de cadence
dans le produit.

## Recherche et identité publique

Les recherches GitHub par nom, README, `PGR3`, titre complet, Title ID
`4D5307D1`, ReXGlue et recompilation ne trouvent qu'un port de recompilation.
Les autres résultats sont des réglages/patches Xenia ou des dépôts homonymes
sans rapport. Ils ne sont pas traités comme projets de recompilation.

| Élément | Valeur vérifiée |
|---|---|
| Dépôt | `CrownParkComputing/xbox360-ports` |
| Branche par défaut | `master` |
| HEAD | [`0216dae319eb5b61a7f1553d74529ca9e4ad55c5`](https://github.com/CrownParkComputing/xbox360-ports/commit/0216dae319eb5b61a7f1553d74529ca9e4ad55c5) |
| Arbre HEAD | `5a3e93e33c4a03cea1be1ac26c5eac8b5ea8093a` |
| Historique | 26 commits publics ; PGR3 ajouté par [`278b383d5554f68cc5bd0cdcb8961715b4820762`](https://github.com/CrownParkComputing/xbox360-ports/commit/278b383d5554f68cc5bd0cdcb8961715b4820762) |
| PGR3 au HEAD | 10 fichiers, 21 062 octets ; identique au tag de release |
| Tag léger | `game-packs-v1` → [`9ec1c7439930dc1bbc785136b71aaa04b5ae859c`](https://github.com/CrownParkComputing/xbox360-ports/commit/9ec1c7439930dc1bbc785136b71aaa04b5ae859c), arbre `e3941063db09f4db34c5acf5bef73c34fd0ef266` |
| Release PGR3 | `pgr3-rexglue-pack.zip`, 13 993 octets, SHA-256 `a8768cc9a78f080ae739f9437214e9e84d5f813e6cbbe28ebe32e3abb17ec79d` |
| Licence du port | aucune licence GitHub, racine ou pack |
| Tests/CI du port | aucun test et aucun workflow de build ; seulement le déploiement Pages automatique |

Le [README épinglé](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/README.md#L66-L92)
classe PGR3 « Plays » avec cadence 30 fps et artefacts verre/reflets. Le
[script réellement publié](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/play.sh#L68-L117)
documente les symptômes et les contournements ; aucun artefact versionné ne les
reproduit.

## XEX, région et frontières de code

Le port ne publie ni SHA-256 du XEX, ni région, Media ID, version, taille et
base d'image qualifiées, ni digest des imports. Le Title ID `4D5307D1` apparaît
seulement dans un commentaire du lanceur. Le manifeste accepte tout fichier
nommé `default.xex` et ne scelle que le nom du projet et un stamp SDK
([manifeste](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/games/pgr3/config/pgr_manifest.toml#L1-L8)).

La configuration ajoute un couple `setjmp/longjmp`, deux seuils d'analyse et
quatre cibles de branches non résolues
([config](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/games/pgr3/config/pgr_rexglue.toml#L1-L17)).
Elle ne donne aucun symbole gameplay, renderer, caméra ou streaming. Les
adresses ne sont reproductibles que contre le XEX privé utilisé par l'auteur.

Deux dépôts Xenia publics illustrent l'ambiguïté sans la résoudre : ils donnent
le même Title ID mais deux hashes de module, `CCDD4477E074DBBC` pour v9 ou
antérieur et `1DBBA44F305E61A9` pour v10 ou postérieur, avec plusieurs Media ID
USA/Europe possibles
([v9](https://github.com/xenia-canary/game-patches/blob/84d6682caf1b75b2fdb7adcd197c6559c09b2ed4/patches/4D5307D1%20-%20Project%20Gotham%20Racing%203%20%28v9.0%20or%20lower%29.patch.toml#L1-L8),
[v10](https://github.com/xenia-canary/game-patches/blob/84d6682caf1b75b2fdb7adcd197c6559c09b2ed4/patches/4D5307D1%20-%20Project%20Gotham%20Racing%203%20%28v10.0%20or%20higher%29.patch.toml#L1-L7)).
Ce sont des métadonnées `documented-unmatched`, pas un reçu du port.

Les commentaires du scaffold sont restés ceux d'autres jeux : `main.cpp`
annonce OutRun et son Title ID, tandis que `ppc_config.h` annonce Daytona et
ses bornes d'image
([main](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/games/pgr3/project/src/main.cpp#L1-L16),
[PPC](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/games/pgr3/ppc/ppc_config.h#L5-L15)).
Le header d'init ReXGlue régénère ses vraies bornes depuis le XEX, donc ces
macros ne déterminent pas l'image dans ce build. Elles confirment néanmoins que
le scaffold n'est pas une preuve d'identité.

## ReXGlue exact : trois pins incompatibles et aucun reçu d'exécution

### État des dépôts

| Rôle | Commit/arbre | Qualification |
|---|---|---|
| Gitlink SDK du monorepo | [`34b11ee6aed9d4ef914e49e6d8a8a092b02ced36`](https://github.com/CrownParkComputing/rexglue-sdk/commit/34b11ee6aed9d4ef914e49e6d8a8a092b02ced36), arbre `38f510e0f2ac37828241d565c114abf83f33d067` | pin source explicite du HEAD port |
| Base ReXGlue officielle du fork | [`29eaa8ab72a235be997ab93cbc2f3d85d3b66582`](https://github.com/rexglue/rexglue-sdk/commit/29eaa8ab72a235be997ab93cbc2f3d85d3b66582) | merge-base avec l'upstream actuel |
| Tag fork `v0.8.0-dev` | [`f9b695e67ef03d117c2b488ce276df6a81c87d26`](https://github.com/CrownParkComputing/rexglue-sdk/commit/f9b695e67ef03d117c2b488ce276df6a81c87d26), arbre `5371ea01c7d6a09d2a42d667b2e4ce32fcf46248` | release Linux publiée |
| Candidat au moment du pack PGR3 | [`983a8e6daf91a0881308f42309d1e914e36185aa`](https://github.com/CrownParkComputing/rexglue-sdk/commit/983a8e6daf91a0881308f42309d1e914e36185aa), arbre `0a18a79ae26f9ca73ee2a7663ddc9ab7946ff372` | dernier HEAD public avant le pack ; inférence, pas pin |
| HEAD actuel du fork | [`f4bac2bd02ae89220841b9dfb03db8ac2e058cf3`](https://github.com/CrownParkComputing/rexglue-sdk/commit/f4bac2bd02ae89220841b9dfb03db8ac2e058cf3), arbre `989c43a81d05abb11698caa0a1c4b801372d54e2` | 36 commits après la base ; non utilisé comme reçu PGR3 |
| ReXGlue upstream actuel | [`df2743b069d0db19f8ecad2688eecb14e23e1565`](https://github.com/rexglue/rexglue-sdk/commit/df2743b069d0db19f8ecad2688eecb14e23e1565) | 36 commits de l'autre côté de la base ; upstream v0.9 divergent |

Le [gitlink](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/.gitmodules#L1-L4)
est trop ancien pour le PGR3 publié : `frame_limit` n'arrive qu'en
[`3f2e2f6c`](https://github.com/CrownParkComputing/rexglue-sdk/commit/3f2e2f6ca7a331d50e268d3961a3c7c94f63caf1),
le diagnostic RenderDoc en
[`d8f3358d`](https://github.com/CrownParkComputing/rexglue-sdk/commit/d8f3358d2abada8ef9693a798f8677596719ae3d),
le cache de descripteurs en
[`4d18953f`](https://github.com/CrownParkComputing/rexglue-sdk/commit/4d18953fc7a3942b67c6a5d1179a94f0d8d4917c),
et la cadence liée au tick vblank en `983a8e6d`. Les commits port et SDK ont des
horodatages synchronisés, ce qui rend ces révisions plausibles pour les essais,
mais aucun log ou binaire hôte ne les scelle.

Le stamp `sdk_version = "0.8.1"` n'est pas un hash. Le pack et le site demandent
de cloner la branche mouvante `development`. Le petit toolchain recommandé dans
la release `v0.8.0-dev` aggrave le problème : le TAR public a le SHA-256
`cd4c7a4d120dd2fb1eb6ce7b252882507b8ad22173d056a8822f0dd18a3b5bef`, mais
son CLI contient la version `0.8.1.95-dev.g0b73bb4`. Ses bibliothèques exposent
`frame_limit` et le diagnostic RenderDoc, donc elles ne correspondent pas non
plus à l'arbre propre `0b73bb4`; elles n'exposent pas
`vulkan_reuse_texture_descriptors`, pourtant activé par le pack. Ce build est
fonctionnellement non attribuable à un commit public exact
([release SDK](https://github.com/CrownParkComputing/rexglue-sdk/releases/tag/v0.8.0-dev)).

Le fork contient 22 gitlinks tiers exacts. Les dépendances pertinentes au
présent audit sont FFmpeg `0604b464c7cb4ebc94940cf1f324a3b26b87717c`,
SDL3 `8bf3b7215ad9fc3deb583c6a3a37c6c67f2e24e4`, glslang
`f4f1d8a352ca1908943aea2ad8c54b39b4879080`, SPIRV-Tools
`04d0b166dcd62e29509bf2aac3ca0c5ccdcb6929`, SPIRV-Headers
`04f10f650d514df88b76d25e83db360142c7b174`, Vulkan-Headers
`49f1a381e2aec33ef32adf4a377b5a39ec016ec4`, VMA
`1d8f600fd424278486eade7ed3e877c99f0846b1` et SIMDe
`71fd833d9666141edcd1d3c109a80e228303d8d7`. Aucun gitlink ne pointe vers
XenonRecomp, XenonAnalyse, XenosRecomp ou rexdex. XenonRecomp et rexdex sont
seulement crédités dans le README ; XenosRecomp n'est ni utilisé ni piné.

## Frontière renderer : Xenos dynamique, pas renderer PGR3

Le CMake lie explicitement le plugin `xenos`
([cible](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/games/pgr3/project/CMakeLists.txt#L34-L55))
et le lanceur passe `--gpu_plugin xenos`. Il n'existe dans le port ni
`DrawPacket`, ni source shader, ni cache shader, ni table de formats, ni
implémentation EDRAM/resolve. Le plugin `rexgpu-native` ajouté après les essais
PGR3 au HEAD du fork est un prototype générique et n'est pas sélectionné.

La preuve publique porte donc sur un renderer d'émulation Xenia-derived
Vulkan : shaders invités traduits à l'exécution, RT hôte ou EDRAM en buffer,
cache texture/pipeline et présentation SDL. Elle aide à tracer une frontière ;
elle ne fournit pas le backend Vulkan direct d'AC6.

## Fetch, shaders et pipeline

### Fetch invalides : voie fail-open

Tous les jeux lancés par le harness reçoivent
`--gpu_allow_invalid_fetch_constants=true`. Sans ce CVar, un fetch marqué
`kInvalidTexture` reste invalide et la texture n'est pas liée ; avec le CVar,
le runtime le traite comme une texture normale
([texture](https://github.com/CrownParkComputing/rexglue-sdk/blob/983a8e6daf91a0881308f42309d1e914e36185aa/src/graphics/pipeline/texture/cache.cpp#L899-L927),
[vertex](https://github.com/CrownParkComputing/rexglue-sdk/blob/983a8e6daf91a0881308f42309d1e914e36185aa/src/graphics/vulkan/command_processor.cpp#L4120-L4139)).
Le port ne publie ni constante fautive, ni draw, ni justification PGR3. Cette
option est `divergent` et interdite dans un gate AC6.

### Epsilon alpha et placeholders

`--use_fuzzy_alpha_epsilon=true` remplace les comparaisons alpha Xenos par une
fenêtre fixe `1e-3`, y compris pour equal/not-equal/less/greater
([traduction](https://github.com/CrownParkComputing/rexglue-sdk/blob/983a8e6daf91a0881308f42309d1e914e36185aa/src/graphics/pipeline/shader/spirv_translator_rb.cpp#L519-L581)).
Le mainteneur dit que cela stabilise feuillages, foule et barrières sur NVIDIA.
C'est un contournement visuel hôte, sans dérivation du format ou de la précision
Xenos : `divergent`, pas sémantique de comparaison réutilisable.

La compilation asynchrone marque toute frame ayant utilisé un pipeline
placeholder. Le défaut la saute ; PGR3 force le contraire et présente donc une
frame où certains draws ont pu être remplacés par un shader de discard
([décision present](https://github.com/CrownParkComputing/rexglue-sdk/blob/983a8e6daf91a0881308f42309d1e914e36185aa/src/graphics/vulkan/command_processor.cpp#L2413-L2432)).
Cela explique le compromis « mouvement continu contre pop-in ». Ni skip ni
placeholder ne sont admissibles dans une validation image AC6.

Le cache de descripteurs compare layout, nombres et tableaux
`VkDescriptorImageInfo` puis réutilise le set précédent dans la même frame
([commit exact](https://github.com/CrownParkComputing/rexglue-sdk/commit/4d18953fc7a3942b67c6a5d1179a94f0d8d4917c)).
Le port revendique 3–4 k draws par frame et un smoke test en course, mais le
commit n'ajoute aucun test. C'est une optimisation `provisional-rexglue`, pas
une preuve shader/fetch et pas un pin reproductible dans le pack.

Les corrections `exp_adjust` mot 3 et RT `k_2_10_10_10` à dix bits existent
dans le fork avant PGR3, mais le projet n'en donne aucun A/B PGR3. Elles restent
les contrats génériques déjà retenus lors de l'audit Hydro, pas une nouvelle
preuve PGR3.

## EDRAM, resolves, depth, verre et reflets

Le fork décrit lui-même les deux chemins Vulkan : FBO utilise render targets,
blend et depth/stencil fixes mais a un support de formats moins fidèle ; FSI
packe, blend et teste depth/stencil manuellement dans l'EDRAM et vise la plus
haute fidélité
([sélection](https://github.com/CrownParkComputing/rexglue-sdk/blob/983a8e6daf91a0881308f42309d1e914e36185aa/src/graphics/vulkan/render_target_cache.cpp#L40-L63),
[fallback matériel](https://github.com/CrownParkComputing/rexglue-sdk/blob/983a8e6daf91a0881308f42309d1e914e36185aa/src/graphics/vulkan/render_target_cache.cpp#L206-L242)).

La documentation PGR3 rapporte :

- FBO : monde propre, mais ciel noir et bruit sur la reflection map de la
  voiture ;
- FSI : verre propre, mais corruption lourde des alpha-tested surfaces ;
- scène : `k_2_10_10_10_FLOAT_AS_16_16_16_16`, RGBA16F hôte et 2xMSAA ;
- verre : draws plein écran, motif de lattice présent dans le depth buffer, et
  un sous-draw produisant la couleur de carrosserie ;
- A/B négatifs : `alpha_to_mask=false`, `gamma_render_target_as_unorm16=false`,
  `vulkan_dynamic_rendering=false` et `direct_host_resolve=false` ne corrigent
  pas la classe observée
  ([audit mainteneur](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/STATUS.md#L92-L126)).

Le mapping source de ce format est bien
`VK_FORMAT_R16G16B16A16_SFLOAT`
([mapping](https://github.com/CrownParkComputing/rexglue-sdk/blob/983a8e6daf91a0881308f42309d1e914e36185aa/src/graphics/vulkan/render_target_cache.cpp#L1973-L1986)).
Mais la capture `pgr3race_frame126545.rdc`, le dump EDRAM et les images FBO/FSI
sont absents et explicitement ignorés par le dépôt. Il n'est donc pas possible
de contrôler le format, l'échantillon, l'event 9707, la face/mip du cubemap ou
la chaîne resolve→fetch→present.

La conclusion la plus étroite est `documented-unmatched` : le désaccord
localise la famille **RB output / per-sample coverage / ownership EDRAM /
resolve / display**, mais ne désigne pas la faute exacte et ne prouve pas que
FSI soit retail-correct. Le ciel/reflet peut se séparer du verre ; aucune
causalité commune n'est démontrée.

### Comparaison utile avec PGR4

| PGR4 déjà audité | PGR3 présent | Conséquence AC6 |
|---|---|---|
| D3D12 ReXGlue 0.8.0 exact, binaire seulement | Vulkan fork source, mais révision exécutée non scellée | aucun des deux n'est reproductible de bout en bout |
| UI noire corrigée par estimation CPU de l'extent d'un VS non clippé | monde/verre se contredisent entre FBO et FSI | tester séparément extent/aliasing puis RB/depth/resolve |
| frontière EDRAM adjacent-range/ownership | frontière 2xMSAA, per-sample, HDR, cubemap/reflet | deux gardes complémentaires, pas deux renderers à copier |
| aucune preuve retail | aucune preuve retail | statut inchangé |

## Caméra, streaming et gameplay

Le port ne contient aucun hook caméra, matrice, frustum, LOD, secteur, requête
I/O, décompression ou transition de ville. « Attract », écran de chargement et
course ne donnent pas la séquence de streaming ni la provenance des transforms.

La première publication affirmait seulement des écrans de chargement de course
à environ 46 fps ; les commits suivants parlent d'une courte session en course,
291 nouveaux pipelines et 3–4 k draws. Le README final dit « Plays ». Aucun log,
replay, screenshot, durée, tour terminé, IA, caméra ou transition n'est suivi.

Plus grave, le lanceur de jeu conserve
`--unregistered_function_nonfatal=true`. Le runtime documente ce mode comme
outil de découverte « not for play » : une fonction indirecte manquante est
ignorée, `r3` reçoit zéro et l'exécution suivante est indéfinie
([implémentation](https://github.com/CrownParkComputing/rexglue-sdk/blob/983a8e6daf91a0881308f42309d1e914e36185aa/src/system/function_dispatcher.cpp#L30-L71)).
Sans journal démontrant zéro `[UNREGFN]` sur le parcours, les claims gameplay,
caméra et streaming restent `documented-unmatched`.

## Input, replay et cadence

Le driver PGR3 est une copie Crazy Taxi/Daytona. Il ajoute un driver connecté
au système déjà peuplé par SDL, MnK et NOP. Sur Linux, le clavier ne produit que
boutons et deux triggers numériques ; tous les sticks restent à zéro. Son
`packet_number` n'augmente que lorsque le masque **boutons** change, pas lors
d'un changement de trigger ; vibration retourne succès sans action et
`GetKeystroke` renvoie vide
([driver](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/games/pgr3/project/src/keyboard_driver.cpp#L187-L247)).

Le runtime fusionne les drivers par OR des boutons, max des triggers, axe de
plus grande magnitude et max des packet numbers
([fusion](https://github.com/CrownParkComputing/rexglue-sdk/blob/983a8e6daf91a0881308f42309d1e914e36185aa/src/input/input_system.cpp#L143-L206)).
Il n'existe ni capture à `XamInputGetState`, ni ordre de polls, périphériques
neutralisés, état normalisé, hotplug ou replay déterministe.

Pour la cadence, PGR3 désactive IMMEDIATE et MAILBOX afin d'obtenir FIFO puis
force `frame_limit=30`. La première implémentation dormait selon l'horloge hôte
dans `VdSwap`; le candidat `983a8e6d` attend tous les deux ticks du worker
vblank 60 Hz, avec timeout de sécurité
([gate VdSwap](https://github.com/CrownParkComputing/rexglue-sdk/blob/983a8e6daf91a0881308f42309d1e914e36185aa/src/kernel/xboxkrnl/xboxkrnl_video.cpp#L529-L554)).
Une source Xenia indépendante décrit aussi PGR3 comme 30 fps, mais elle ne lie
pas le XEX du port
([métadonnées](https://github.com/xenia-canary/xenia-canary.github.io/blob/23601d7b08fb1d22e4958773591632ce7de75680/_games/ProjectGothamRacing3.md#L193-L205)).

Les observations 1600+ `VdSwap`/s, capacité ~46 fps et amélioration perceptive
ne sont pas accompagnées de séries de timestamps. Dormir dans un export HLE
modifie directement l'ordonnancement du thread invité : utile pour jouer,
`divergent` comme oracle AC6. PGR3 renforce le besoin de séparer tick guest,
poll, simulation, swap, submit et present dans le replay M01.

## XMA et audio

PGR3 n'ajoute aucun hook audio. Le lanceur force seulement
`--audio_maxqframes=64`, soit le maximum du runtime au lieu du défaut 8. Le
runtime générique crée les contextes XMA, traite leurs MMIO et décode via le
codec FFmpeg `AV_CODEC_ID_XMAFRAMES`
([XMA/FFmpeg](https://github.com/CrownParkComputing/rexglue-sdk/blob/983a8e6daf91a0881308f42309d1e914e36185aa/src/audio/xma_context.cpp#L24-L75),
[file audio](https://github.com/CrownParkComputing/rexglue-sdk/blob/983a8e6daf91a0881308f42309d1e914e36185aa/src/audio/audio_system.cpp#L27-L64)).

Aucune voix PGR3, paquet, loop, hash PCM, cue moteur, latence, underrun ou
synchro audio/vidéo n'est publié. Augmenter la file peut masquer une starvation
au prix de la latence ; cela ne qualifie pas XMA. La mécanique runtime est
`provisional-rexglue`, toute fidélité PGR3/AC6 est `documented-unmatched`.

## Build, CI, Linux et paquet

### Le pack public est propre en contenu, mais cassé comme recette

La [release `game-packs-v1`](https://github.com/CrownParkComputing/xbox360-ports/releases/tag/game-packs-v1)
contient 21 entrées et 27 326 octets décompressés. Les dix fichiers PGR3 sont
bit-identiques au tag et au HEAD ; `assets/` et `gamedata/` sont vides. Aucun
XEX, STFS, ISO, binaire, shader compilé, image, son, capture ou C++ généré n'a
été trouvé. Aucun lien symbolique ni chemin traversant n'est présent. Le digest
local reproduit celui de GitHub.

La recette échoue cependant avant toute compilation :

- le README du pack demande `assets/default.xex`, mais le manifeste copié lit
  `config/../extracted/default.xex`, répertoire absent ;
- le README et `SDK_SRC` clonent le SDK dans
  `project/thirdparty/rexglue-sdk`, alors que le CMake par défaut cherche
  `project/../thirdparty/rexglue-sdk` ;
- `SDK_SRC` n'est jamais transmis comme
  `-DREXGLUE_SDK_SOURCE_DIR=...` au CMake ;
- le build appelle toujours le codegen avec `--force` ;
- le README annonce CMake ≥3.21, le projet exige 3.25 et le SDK candidat 3.27 ;
- le preset Linux impose `-march=x86-64-v3`, donc AVX2 comme baseline
  ([preset](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/games/pgr3/project/CMakePresets.json#L8-L25)).

Ces contradictions sont visibles dans le
[générateur du pack](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/tools/make_gamepack.sh#L62-L84)
et son [README généré](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/tools/make_gamepack.sh#L100-L125).
Une configuration CMake statique du ZIP reproduit l'erreur et le chemin SDK
inattendu. Le shell est syntaxiquement valide ; c'est le contrat de chemins qui
est faux.

Le monorepo n'a aucun workflow de build/test. Son seul check vert au HEAD est
GitHub Pages. Le fork SDK a cinq workflows et 166 fixtures assembleur plus des
unit tests, mais `REXGLUE_BUILD_TESTS` est `OFF` et aucun workflow ne lance
`ctest`. Au 12 août, le nightly du HEAD construit Linux amd64/arm64, échoue sur
Windows et ne publie rien ; le format check du HEAD échoue également
([nightly](https://github.com/CrownParkComputing/rexglue-sdk/actions/runs/31569888975),
[format](https://github.com/CrownParkComputing/rexglue-sdk/actions/runs/29651995147)).
Cela valide au mieux le SDK Linux courant, jamais PGR3, son pack ou ses claims.

Le port et le ZIP n'ont aucune licence. Le SDK source est BSD-3-Clause, mais le
petit toolchain précompilé n'embarque ni `LICENSE`, ni `NOTICE`, seulement cinq
ELF et un README. Aucune signature, provenance de build, SBOM ou attestation
n'est publiée. Le paquet AC6 ne doit reprendre ni code du scaffold sans licence,
ni runtime/plugin/généré retail ; seuls des invariants réimplémentés et leurs
tests peuvent être conservés.

## Taxonomie AC6

| Élément PGR3 | Classe | Conséquence |
|---|---|---|
| plugin Xenos Vulkan, FBO/FSI, cache texture/pipeline, XMA/FFmpeg | `provisional-rexglue` | source consultable, aucune qualification PAL M01 |
| cache de descripteurs intra-frame | `provisional-rexglue` | optimisation à retester avec invalidations exhaustives |
| attract/race, 30 fps, 291 pipelines, 3–4 k draws | `documented-unmatched` | claims sans XEX, log, métrique ou replay public |
| verre propre en FSI, monde propre en FBO, reflet/ciel cassés | `documented-unmatched` | capture et images absentes ; aucune référence retail |
| caméra, streaming, progression, IA, save, audio correct | `documented-unmatched` | aucune preuve publique |
| fetch invalides autorisés, epsilon alpha `1e-3` | `divergent` | interdits dans le produit et les gates AC6 |
| fonctions manquantes ignorées avec `r3=0` | `divergent` | invalide les claims de parcours sans journal zéro faute |
| sleep/gate HLE dans `VdSwap`, drivers fusionnés | `divergent` comme oracle | ne fournit ni cadence ni replay AC6 |
| frames placeholder présentées ou sautées | `divergent` | toute validation image doit préchauffer ou échouer fermée |
| sémantique `retail-qualified` | **aucune** | autre titre, image inconnue, aucun contrôle console |

## Actions retenues pour AC6 Mission 01

1. Ajouter au reçu renderer M01 les fetch constants brutes, type, base/mip,
   format/endian, sampler, shader hash et draw ordinal. Refuser un gate dès le
   premier fetch `invalid`, pipeline placeholder ou erreur masquée ; garder
   `use_fuzzy_alpha_epsilon=false`.
2. Si le census PAL atteint un RT
   `k_2_10_10_10_FLOAT(_AS_16_16_16_16)` 2xMSAA, créer un golden test
   per-sample : pack 7e3/alpha, sample locations, depth/stencil, write mask,
   blend, alpha-to-mask et resolve. Le même paquet doit donner un résultat
   identique cache chaud/froid.
3. Instrumenter une chaîne bornée
   `draw → plage EDRAM → ownership/aliasing → resolve destination
   (pitch/format/face/mip) → fetch → shader → present`, avec digest à chaque
   frontière. Combiner cette garde per-sample PGR3 avec la garde extent/EDRAM
   non clippée issue de PGR4.
4. Pour toute réutilisation de descripteurs native, ajouter un test différentiel
   cache ON/OFF qui change séparément image view, layout, sampler, fetch,
   éviction, frame, shader stage et pipeline layout. Une seule mutation doit
   invalider exactement le bon set.
5. Précompiler ou bloquer sur les pipelines nécessaires au replay M01 ; ne
   jamais valider une frame avec discard placeholder et ne pas supprimer une
   frame pour cacher la compilation.
6. Conserver le journal poll-exact à `XamInputGetState`, périphériques hôte
   neutralisés. Sceller poll ordinal, résultat XAM, packet number, triggers,
   sticks et tick guest ; ne rien reprendre de la fusion multi-driver PGR3.
7. Mesurer séparément vblank, tick simulation, poll, swap, submit et present.
   Ne pas imposer 30 fps ni dormir dans `VdSwap` tant que la trace PAL M01 ne
   démontre pas ce rapport.
8. Ne créer aucun composant caméra/streaming depuis PGR3. Les matrices,
   secteurs, priorités et transitions viennent uniquement des bytes et traces
   du PAL canonique lorsqu'ils sont atteints par M01.
9. Garder les fixtures XMA offline déjà prévues : paquets bornés, hash PCM,
   nombre d'échantillons, boucle/EOF et timestamps indépendants de la file
   audio hôte. `audio_maxqframes=64` ne devient jamais une attente de fidélité.
10. Tester le TGZ AC6 depuis une extraction vierge en CI avec SDK, compilateur,
    baseline CPU et licences scellés ; rejeter XEX/PAC retail, C++ généré,
    runtime ReXGlue/Xenia, shaders/captures retail et chemin absolu.

**Impact checkpoint :** aucun JF/JV/JP/JG et aucune des six lanes du checkpoint
2 ne sont fermés. Le bénéfice est une garde renderer plus discriminante entre
extent EDRAM, résultat per-sample, resolve/cubemap et présentation.

## Validations de l'audit

- recherche Web/GitHub puis recherche code par Title ID et manifeste ; seul le
  monorepo CrownPark est une recompilation PGR3 substantielle ;
- clone des deux dépôts, HEAD/tree/historique/branches/tags/releases/licences,
  gitlink et merge-base upstream vérifiés ;
- API GitHub contrôlée pour assets, digests, workflows et runs ;
- ZIP PGR3 téléchargé, SHA-256 reproduit, extraction et inventaire statiques,
  fichiers communs comparés au tag/HEAD, chemins et liens vérifiés ;
- petit toolchain Linux téléchargé et inspecté statiquement : digest, membres,
  ELF Build IDs, version embarquée, CVars et absence de notices ; aucun ELF
  exécuté ;
- inspection source ciblée du codegen, dispatcher fail-open, fetch, SPIR-V,
  FBO/FSI, EDRAM/resolves, input, vblank et XMA aux commits épinglés ;
- TOML et JSON parsés ; `bash -n` propre pour le lanceur et le pack ; CMake
  statique du ZIP reproduisant le mauvais chemin SDK ;
- aucun binaire de jeu, asset retail ou sortie C++ recompilée téléchargé ou
  exécuté ;
- `git diff --no-index --check /dev/null
  reports/cycle-1568-pgr3-rexglue-renderer-review.md` sans diagnostic de
  whitespace (code 1 attendu pour un fichier nouveau).

## Risques résiduels

- les captures, logs, builds et XEX privés du mainteneur pourraient rendre
  certaines observations plus solides, mais ils ne sont pas auditables ;
- l'absence de SHA-256 XEX laisse version, région et frontières de fonctions
  indéterminées ;
- le binaire toolchain public ne se rattache pas exactement à son tag et le
  pack ne pinne pas la branche utilisée : aucun résultat n'est reproductible à
  l'octet ;
- aucun rendu PGR3 ou AC6 n'a été exécuté durant cet audit ; les causalités
  verre/reflet/depth/resolve restent des hypothèses localisées ;
- le scan exclut le contenu retail brut évident du pack, pas toute information
  dérivée : les adresses de fonctions proviennent d'un XEX inconnu ;
- les nouveaux contrats Xenos restent à confronter aux draws PAL M01 avant
  toute promotion ou fermeture de lane.
