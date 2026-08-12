# Cycle 1575 — delta GoldenEye 007 XBLA / ReXGlue pour AC6 M01

Date de qualification : 2026-08-12. Sources primaires publiques uniquement.
Aucun asset de release n'a été téléchargé ou exécuté et aucun contenu retail
n'a été copié.

## Verdict

Ce rapport ne remplace pas
[l'audit 1559](cycle-1559-goldeneye-rexglue-instrumentation-review.md), qui
reste la référence pour `SunJaycy/GoldenEye-Recomp` à
`fdee4d1f750aff4c3b5c6ba3d60f20281c21447d` : hooks, watchdog GPU, patches CE
et risque historique de release. Le delta utile est le fork actif de Jeffory,
son port Linux/Android et, surtout, son runtime désormais public.

Le gain pour AC6 est réel mais limité à des **formes d'instrumentation et de
bring-up** : point de capture XAM, file XMA bornée, compteurs EDRAM, contrôle de
capacités SDL et écriture temporaire d'un cache Vulkan. Il n'apporte aucune
preuve retail pour AC6, aucun replay déterministe et aucun pipeline vidéo.

Taxonomie stricte de ce cycle :

| classe | résultat |
| --- | --- |
| `retail-qualified` | **aucun élément** : aucun SHA-256 de XEX GoldenEye, aucune identité média/version, aucune trace liée à ce XEX et, a fortiori, aucune preuve PAL AC6 |
| `provisional-rexglue` | interfaces et compteurs génériques explicitement repris ci-dessous, utilisables comme hypothèses ou patrons testables |
| `divergent` | correctifs par adresses GoldenEye dans le kernel, délais en temps hôte, fusion arbitraire des périphériques, renderer Xenos émulé et paquet contenant du code PPC recompilé |
| `documented-unmatched` | affirmation « jouable », XUI, sauvegarde de campagne, replay/cadence, vidéo/ASF, Wayland natif, parité audiovisuelle et propreté retail des paquets |

Conclusion de gate : **0 lane M01 fermée**. La mention publique « entièrement
jouable » décrit l'état déclaré du projet ; elle ne vaut pas qualification de
fidélité dans notre taxonomie.

## Sources exactes et chaîne de provenance

### Dépôts et révisions

| rôle | révision qualifiée | constat |
| --- | --- | --- |
| jeu de référence 1559 | [`SunJaycy/GoldenEye-Recomp@fdee4d1`](https://github.com/SunJaycy/GoldenEye-Recomp/tree/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d) | déjà audité en 1559 |
| jeu actif | [`jeffory/GoldenEye-Recomp@6f8ac48`](https://github.com/jeffory/GoldenEye-Recomp/tree/6f8ac486c289a833222a4a0173bc3c855acab07d), arbre `ceb04c77dabd314467f13e5e56832096542baa8c` | tag annoté `v1.6.1` (objet `d32b92f67b2616ff3ddbc20f590ffcf96a4c9e24`), branche `develop`, 2026-08-04 ; le `main` par défaut était encore `82f4c7511a75b1ab2502defd04a19b52b74c53ec` |
| runtime SunJaycy | racine [`8b5de8a`](https://github.com/SunJaycy/GoldenEye-Recomp-rexglue/commit/8b5de8a2f51d0d99369919e7085135a53304b87a), HEAD [`2824a80`](https://github.com/SunJaycy/GoldenEye-Recomp-rexglue/commit/2824a80ecaa541084e1e1fa3e16c77b0d2f6d9e4) | quatre commits, aucun tag ni release |
| runtime actif | [`jeffory/GoldenEye-Recomp-rexglue@479dc8e`](https://github.com/jeffory/GoldenEye-Recomp-rexglue/tree/479dc8e3f5397a3ace1b16e7ea9d8e38ad11ef91), arbre `f3f929ae3682db18bf328ec24c6c8d40627060ce` | 70 commits, aucun tag ni release ; commit épinglé par les notes de `v1.6.1` |
| ReXGlue officiel déclaré | [`rexglue-sdk@v0.8.0`](https://github.com/rexglue/rexglue-sdk/commit/2bdb97f95f154f32d281aaa08446ae007b8ca117) | le manifeste du jeu ne donne que la chaîne `sdk_version = "0.8.0.0"` |

Le fork du jeu est nominalement un fork GitHub de SunJaycy, mais sa release
`6f8ac48` et le HEAD SunJaycy `fdee4d1` divergent après
`79d6c4cab3e54babc7a2594d6cd8df4203126c39`; le second n'est pas ancêtre du
premier. Le runtime Jeffory et le runtime SunJaycy divergent, eux, dès leur
racine commune `8b5de8a`; `2824a80` n'est pas ancêtre de `479dc8e`.

Plus important, la racine `8b5de8a` est un import de sources sans parent Git.
Elle n'a aucun merge-base avec le tag officiel `v0.8.0`; une comparaison
d'arbres donne 49 fichiers changés, 1 970 insertions et 484 suppressions. Le
[manifeste](https://github.com/jeffory/GoldenEye-Recomp/blob/6f8ac486c289a833222a4a0173bc3c855acab07d/ge_manifest.toml#L1-L13)
n'épingle donc pas la provenance du runtime : cette filiation reste
`documented-unmatched`.

La [release publique `v1.6.1`](https://github.com/jeffory/GoldenEye-Recomp/releases/tag/v1.6.1)
note seulement les SHA courts `6f8ac48` et `479dc8e`. Ses métadonnées GitHub,
consultées sans télécharger les archives, donnent :

| asset | taille | digest GitHub |
| --- | ---: | --- |
| `GoldenEye-Recomp-v1.6.1-android-arm64.apk` | 34 238 574 | `sha256:ecfa910e7a803d090ce7d8698bb125fec08331a0af8aac35723af6cb2e344449` |
| `GoldenEye-Recomp-v1.6.1-linux-amd64.tar.gz` | 51 575 834 | `sha256:b8b34674358f387746a98b4bfa08d354387c71ace34578cd62adb450ca6538fd` |

### Sous-modules et outils réellement employés

Le runtime actif possède 22 gitlinks précis. Les pins qui touchent directement
AC6 sont :

| composant | commit |
| --- | --- |
| FFmpeg, fork `wmarti`, branche déclarée `xenia-ffmpeg-canary-full` | `0604b464c7cb4ebc94940cf1f324a3b26b87717c` |
| SDL3 | `8bf3b7215ad9fc3deb583c6a3a37c6c67f2e24e4` |
| SIMDe | `71fd833d9666141edcd1d3c109a80e228303d8d7` |
| glslang | `f4f1d8a352ca1908943aea2ad8c54b39b4879080` |
| SPIR-V Tools / Headers | `04d0b166dcd62e29509bf2aac3ca0c5ccdcb6929` / `04f10f650d514df88b76d25e83db360142c7b174` |
| Vulkan Headers / volk / VMA | `49f1a381e2aec33ef32adf4a377b5a39ec016ec4` / `0b17a763ba5643e32da1b2152f8140461b3b7345` / `1d8f600fd424278486eade7ed3e877c99f0846b1` |

Les treize autres gitlinks sont également fixés :

| composant | commit |
| --- | --- |
| Catch2 | `88abf9bf325c798c33f54f6b9220ef885b267f4f` |
| CLI11 | `bfffd37e1f804ca4fae1caae106935791696b6a9` |
| fmt | `407c905e45ad75fc29bf0f9bb7c5c2fd3475976f` |
| ImGui | `6d910d5487d11ca567b61c7824b0c78c569d62f0` |
| inja | `7d1b4600b68595085a949743331c2e5673f511ea` |
| libmspack | `305907723a4e7ab2018e58040059ffb5e77db837` |
| o1heap | `388a73fd9007300e5130c5fe352d9ce3288b6dde` |
| snappy | `6af9287fbdb913f0794d0148c6aa43b58e63c8e3` |
| spdlog | `79524ddd08a4ec981b7fea76afd08ee05f83755d` |
| toml++ | `30172438cee64926dc41fdd9c11fb3ba5b2ba9de` |
| Tracy | `05cceee0df3b8d7c6fa87e9638af311dbabc63cb` |
| utfcpp | `63d64de49fd6b829f7c8694df5ab2ee625cb7134` |
| xxHash | `e626a72bc2321cd320e953a0ccf1584cad60f363` |

Les URLs déclarées sont dans
[`.gitmodules`](https://github.com/jeffory/GoldenEye-Recomp-rexglue/blob/479dc8e3f5397a3ace1b16e7ea9d8e38ad11ef91/.gitmodules) ; le gitlink, et non le
nom de branche, est le pin reproductible.

Recherche source complète hors tiers :

- `XenosRecomp` et `XenonAnalyse` ne sont ni présents, ni invoqués, ni épinglés ;
- `XenonRecomp` et `rexdex/recompiler` ne figurent que dans les crédits et dans
  quelques commentaires d'origine, sans sous-module ni commande de build
  ([README runtime, lignes 39–43 et 76–79](https://github.com/jeffory/GoldenEye-Recomp-rexglue/blob/479dc8e3f5397a3ace1b16e7ea9d8e38ad11ef91/README.md#L39-L79)) ;
- le codegen et l'hôte utilisés sont ceux de ce fork ReXGlue ; le renderer est
  un backend Xenos/Vulkan dérivé de Xenia inclus dans l'arbre, pas
  XenosRecomp ; aucune révision Xenia n'est épinglée.

## Delta runtime pertinent

### Kernel, XAM et XUI

Le runtime public permet enfin d'inspecter le contrat, mais révèle une frontière
à ne pas franchir. `xeKeWaitForSingleObject` contient des adresses et structures
GoldenEye en dur : threads `0x821A4A68` et `0x82366628`, pointeur guest
`0x8308EC34`, jeton à `AO+300`, et polling hôte de 12 ms
([source](https://github.com/jeffory/GoldenEye-Recomp-rexglue/blob/479dc8e3f5397a3ace1b16e7ea9d8e38ad11ef91/src/kernel/xboxkrnl/xboxkrnl_threading.cpp#L840-L967)).
C'est `divergent`, même si cela répare un lost wakeup observé : AC6 ne doit ni
le copier dans son kernel générique, ni traiter son succès comme une sémantique
retail. Un éventuel correctif de bring-up doit vivre dans un adaptateur titre,
porter un reçu adresse/XEX et produire une trace avant/après.

`XamInputGetState` force l'utilisateur « any » vers le slot 0
([source](https://github.com/jeffory/GoldenEye-Recomp-rexglue/blob/479dc8e3f5397a3ace1b16e7ea9d8e38ad11ef91/src/kernel/xam/xam_input.cpp#L94-L116)).
L'hôte fusionne tous les drivers par OR des boutons, maximum des gâchettes,
axe de plus grande magnitude et packet number maximum
([source](https://github.com/jeffory/GoldenEye-Recomp-rexglue/blob/479dc8e3f5397a3ace1b16e7ea9d8e38ad11ef91/src/input/input_system.cpp#L80-L129)).
Le point d'interception est intéressant pour AC6 ; cette règle de fusion ne
l'est pas et reste `divergent`.

La couche UI implémente quelques dialogues hôte différés, avec complétion
`X_OVERLAPPED`, mais se décrit elle-même comme WIP et laisse une longue liste
d'exports en stubs
([source](https://github.com/jeffory/GoldenEye-Recomp-rexglue/blob/479dc8e3f5397a3ace1b16e7ea9d8e38ad11ef91/src/kernel/xam/xam_ui.cpp#L38-L51),
[stubs](https://github.com/jeffory/GoldenEye-Recomp-rexglue/blob/479dc8e3f5397a3ace1b16e7ea9d8e38ad11ef91/src/kernel/xam/xam_ui.cpp#L573-L694)).
Les exports XUI eux-mêmes restent largement des stubs, notamment
[`XuiLoadFromBinary`](https://github.com/jeffory/GoldenEye-Recomp-rexglue/blob/479dc8e3f5397a3ace1b16e7ea9d8e38ad11ef91/src/kernel/xam/xam_misc.cpp#L925-L940).
GoldenEye prouve uniquement le chemin UI qu'il consomme ;
XUI pour AC6 est `documented-unmatched`.

### Input, replay et cadence

Le fork actif conserve le hook après le poll XAM déjà analysé en 1559, mais il
n'existe aucun enregistreur/lecteur de replay, aucun flux de 3 600 polls, aucune
relation prouvée « poll = tick », aucun premier point de divergence et aucun
test d'indépendance à la résolution. Les watchdogs et délais s'appuient en plus
sur `steady_clock`, `REX_QUERY_TIMEBASE` et des signaux de présentation hôte.
La distinction `present#` / `rendered#` de l'audit 1559 reste une bonne sonde,
pas une horloge de gameplay.

L'enseignement `provisional-rexglue` est donc seulement le placement : capturer
un `X_INPUT_STATE` normalisé **après** la fusion du périphérique hôte et le
réinjecter **avant** sa première consommation guest, indexé par ordinal de poll
et tick qualifié. Le replay AC6 devra sceller SHA-256 PAL, adresse du site,
commit, hash du cache importé, profil, état initial et packet number. Une entrée
ne doit jamais être avancée par `PRESENT`, par frame rendue ou par millisecondes
hôte.

### VFS, import et sauvegarde

ReXGlue monte directement le dossier fourni en `game:` et `d:`, retombe sur le
dossier jeu quand `user_data_root` est vide, et fait réussir artificiellement
les accès bruts `Partition0/Cache0/Cache1` via `NullDevice`
([source](https://github.com/jeffory/GoldenEye-Recomp-rexglue/blob/479dc8e3f5397a3ace1b16e7ea9d8e38ad11ef91/src/system/runtime.cpp#L258-L319)).
Il ne fournit pas l'import/cache immuable exigé pour AC6.

Le fork jeu ajoute un bon diagnostic de forme : une passe récursive compare
1 800 chemins en minuscules et produit la liste manquante
([source](https://github.com/jeffory/GoldenEye-Recomp/blob/6f8ac486c289a833222a4a0173bc3c855acab07d/src/ge_asset_check.cpp#L45-L75),
[diagnostic](https://github.com/jeffory/GoldenEye-Recomp/blob/6f8ac486c289a833222a4a0173bc3c855acab07d/src/ge_asset_check.cpp#L93-L156)).
Mais le manifeste ne contient ni taille, ni hash, ni identité de XEX ; une
option permet même de poursuivre malgré les absences. C'est un contrôle UX
`provisional-rexglue`, pas une qualification de source.

`XamContentCreate`, énumération, fermeture, miniature et suppression ont des
implémentations partielles, tandis que `XamContentResolve` renvoie « not found »,
`XamContentOpenFile` renvoie « file not found » et de nombreuses opérations de
paquet sont des stubs
([resolve](https://github.com/jeffory/GoldenEye-Recomp-rexglue/blob/479dc8e3f5397a3ace1b16e7ea9d8e38ad11ef91/src/kernel/xam/xam_content.cpp#L47-L57),
[exports](https://github.com/jeffory/GoldenEye-Recomp-rexglue/blob/479dc8e3f5397a3ace1b16e7ea9d8e38ad11ef91/src/kernel/xam/xam_content.cpp#L419-L468)).
Le runtime possède des sérialiseurs bas niveau mémoire/objets/threads, mais
aucune commande titre ni preuve publique ne montre un savestate CPU+GPU+audio
cohérent
([kernel save/restore](https://github.com/jeffory/GoldenEye-Recomp-rexglue/blob/479dc8e3f5397a3ace1b16e7ea9d8e38ad11ef91/src/system/kernel_state.cpp#L1217-L1320)).
Ni la sauvegarde de campagne, ni corruption/migration, ni reprise déterministe
ne sont qualifiées.

### XMA, SIMD et vidéo

Le XMA est décodé via le fork FFmpeg épinglé. Le commit
[`752fbda`](https://github.com/jeffory/GoldenEye-Recomp-rexglue/commit/752fbda5bc94d121422305d42dfd07df78f14327)
priorise les contextes fraîchement kickés et borne à 33 ms, par écriture MMIO,
l'attente du thread guest
([source](https://github.com/jeffory/GoldenEye-Recomp-rexglue/blob/479dc8e3f5397a3ace1b16e7ea9d8e38ad11ef91/src/audio/xma_decoder.cpp#L57-L62),
[attente](https://github.com/jeffory/GoldenEye-Recomp-rexglue/blob/479dc8e3f5397a3ace1b16e7ea9d8e38ad11ef91/src/audio/xma_decoder.cpp#L298-L355)).
La file bornée est un patron utile ; la limite « environ une frame 30 fps » en
`steady_clock` est `divergent` tant que les règles XMA retail et les cues AC6 ne
la qualifient pas.

La conversion des frames possède des chemins SSE x86, NEON ARM64 et scalaire
([source](https://github.com/jeffory/GoldenEye-Recomp-rexglue/blob/479dc8e3f5397a3ace1b16e7ea9d8e38ad11ef91/src/audio/xma_context.cpp#L712-L821)).
C'est une confirmation pratique qu'un même C++ peut employer SIMD explicitement
ou via une abstraction portable. Aucun test source n'établit ici l'identité
octet pour octet des trois chemins : AC6 devra conserver un oracle scalaire et
tester SSE/AVX/NEON contre lui sur arrondis, saturation, endian et buffers
aléatoires avant activation.

Les seuls fichiers vidéo exposent le mode d'affichage (`xam_video` /
`xboxkrnl_video`). Aucun décodeur ASF/WMV, aucune synchronisation A/V, aucun
sous-titre et aucun contrôle ±1 dB / ±20 ms n'ont été trouvés. GoldenEye ne
ferme donc rien dans la lane XMA/ASF AC6.

### Xenos, EDRAM, textures et Vulkan/Linux

Le commit
[`6db4d1b`](https://github.com/jeffory/GoldenEye-Recomp-rexglue/commit/6db4d1b0e9c65892f35c4ca8e2589011ff20a61d)
ajoute, par soumission, les compteurs de passes/transferts EDRAM, draws,
stockages depth, reconfigurations et no-op évités
([source](https://github.com/jeffory/GoldenEye-Recomp-rexglue/blob/479dc8e3f5397a3ace1b16e7ea9d8e38ad11ef91/src/graphics/vulkan/render_target_cache.cpp#L104-L145)).
Le commit
[`4fb54f3`](https://github.com/jeffory/GoldenEye-Recomp-rexglue/commit/4fb54f3c5e4218ab102be5b91faebc20657b81ff)
regroupe les barrières hors de boucles de slices de textures. Ces deux idées
sont `provisional-rexglue` comme sondes/performance seulement : elles ne
qualifient ni BC3 tiled/endian, ni mips/cubemaps, ni image positive, et une
soumission Xenos n'est pas un tick de gameplay.

Le cache `VkPipelineCache` écrit un `.tmp`, vérifie le short write puis renomme
([source](https://github.com/jeffory/GoldenEye-Recomp-rexglue/blob/479dc8e3f5397a3ace1b16e7ea9d8e38ad11ef91/src/graphics/vulkan/pipeline_cache.cpp#L398-L446)).
Il manque `fsync` du fichier et du répertoire : forme utile, mais contrat
atomique insuffisant pour les données importées ou sauvegardes AC6.

Le runtime déclare Vulkan 1.3 comme version maximale utilisée
([source](https://github.com/jeffory/GoldenEye-Recomp-rexglue/blob/479dc8e3f5397a3ace1b16e7ea9d8e38ad11ef91/include/rex/ui/vulkan/device.h#L41-L51)),
mais son frontend Linux est GTK3/XCB. La création de surface ne prend que XCB
et porte encore `TODO: Wayland surface`
([source](https://github.com/jeffory/GoldenEye-Recomp-rexglue/blob/479dc8e3f5397a3ace1b16e7ea9d8e38ad11ef91/src/ui/window_gtk.cpp#L786-L806)).
La release vise donc X11/XWayland, pas Wayland natif, et n'est pas le contrat
SDL3-window + Vulkan 1.3 d'AC6.

## Build, paquet et provenance retail

La CI publique reconnaît qu'elle ne peut pas construire le binaire final sans
le C++ PPC généré depuis le XEX ; elle ne fait qu'un `-fsyntax-only` d'une partie
des sources manuscrites, excluant notamment `ge_hooks.cpp` et `main.cpp`
([workflow](https://github.com/jeffory/GoldenEye-Recomp/blob/6f8ac486c289a833222a4a0173bc3c855acab07d/.github/workflows/build.yml#L3-L13)).
Elle checkout en outre le runtime sans `ref` fixe
([workflow](https://github.com/jeffory/GoldenEye-Recomp/blob/6f8ac486c289a833222a4a0173bc3c855acab07d/.github/workflows/build.yml#L26-L53)).

Le script de release exige `generated/` et dit explicitement que les artefacts
embarquent le code PPC généré depuis le XEX
([source](https://github.com/jeffory/GoldenEye-Recomp/blob/6f8ac486c289a833222a4a0173bc3c855acab07d/scripts/cut-release.sh#L1-L6),
[précondition](https://github.com/jeffory/GoldenEye-Recomp/blob/6f8ac486c289a833222a4a0173bc3c855acab07d/scripts/cut-release.sh#L75-L90)).
Le TGZ copie le binaire et les `.so` résolues, sans le dossier d'assets
([assemblage](https://github.com/jeffory/GoldenEye-Recomp/blob/6f8ac486c289a833222a4a0173bc3c855acab07d/scripts/cut-release.sh#L178-L215)).
Cela peut convenir à la politique GoldenEye, mais viole explicitement le gate
AC6 « aucun code généré / aucun octet retail dans la preview ».

Le contrôle Linux est utile mais étroit : ABI, `ldd` sous Ubuntu 24.04, puis
présence des backends ALSA/PulseAudio/udev. Le smoke test précise qu'il ne lance
pas le jeu
([source](https://github.com/jeffory/GoldenEye-Recomp/blob/6f8ac486c289a833222a4a0173bc3c855acab07d/scripts/smoke-test-bundle.sh#L1-L7)).
Le retour d'expérience le plus actionnable est qu'une image SDL3 sans backends
audio a produit un deadlock de boot, et que `SDL_AUDIODRIVER=dummy` l'a débloqué
([Dockerfile](https://github.com/jeffory/GoldenEye-Recomp/blob/6f8ac486c289a833222a4a0173bc3c855acab07d/docker/linux-release.Dockerfile#L23-L41)).
Cela confirme directement l'A/B audio obligatoire du harness headless AC6 ;
cela ne remplace pas un lancement Vulkan réel.

Enfin, le fork actif **commite et embarque dans l'APK** deux caches de shaders
issus d'un appareil. Sa propre spécification décrit le `.xsh` comme du
« Xenos microcode + pipeline descriptions » et la procédure ADB demande de
tirer les fichiers puis de les committer
([spécification](https://github.com/jeffory/GoldenEye-Recomp/blob/6f8ac486c289a833222a4a0173bc3c855acab07d/docs/superpowers/specs/2026-07-11-shader-seed-bundling-design.md#L20-L40),
[script](https://github.com/jeffory/GoldenEye-Recomp/blob/6f8ac486c289a833222a4a0173bc3c855acab07d/scripts/refresh-shader-seed.sh#L15-L32)).
Le dépôt ne fournit pas de preuve établissant que ces données sont
redistribuables. Leur provenance est donc `documented-unmatched`; AC6 ne doit
jamais livrer un cache contenant du microcode guest. Un cache de pipelines
strictement hôte peut être recréé après import, sous réserve de prouver son
absence de bytes retail.

## Actions retenues pour AC6 PAL Mission 01

1. **Replay synchronisé en priorité.** Qualifier le site AC6 PAL
   `XamInputGetState` par SHA-256 XEX, Ghidra, contrôle positif et trace ; capturer
   un état normalisé par ordinal de poll et tick, puis injecter exactement au
   même point. Sceller contenu/import/build/profil et produire le premier écart
   structuré. Ne jamais cadencer sur `PRESENT`, frame rendue ou temps hôte.
2. **Importer, ne pas monter le retail.** Étendre le manifeste AC6 v2 à
   `chemin + taille + SHA-256`, valider le XEX PAL et chaque payload nécessaire,
   construire atomiquement le cache XDG, puis interdire toute relecture PAC.
   Garder le diagnostic GoldenEye comme inspiration UX seulement ; séparer
   strictement cache, saves et dossier source.
3. **Isoler les correctifs provisoires.** Aucun test d'adresse titre dans
   xboxkrnl/XAM générique. Toute exception M01 éventuelle doit être un adaptateur
   explicitement divergent, désactivable, avec compteur et preuve avant/après.
4. **Borner XMA sans changer l'horloge guest.** Reprendre l'idée file
   kickée/worker, tracer kick-décode-ready-timeout, puis dériver les limites de
   ticks retail ou d'une preuve AC6, pas de 33 ms arbitraires. Tester le chemin
   SIMD bit-exact contre le scalaire et conserver les gates audio/vidéo séparés.
5. **Instrumenter le Vulkan natif.** Ajouter aux `DrawPacket` M01 des compteurs
   typés de transfert EDRAM, resolve, upload texture, compilation pipeline et
   soumission. Ils localisent un coût ou une divergence ; ils ne définissent ni
   frame gameplay ni réussite. Ne pas importer le command processor émulé.
6. **Durcir cache, save et TGZ.** Écriture `temp + flush + fsync + rename +
   fsync(dir)`, tests de kill/corruption/migration, inventaire de paquet et scan
   anti-retail/anti-codegen. Épingler SHA complets, arbres et gitlinks, produire
   SBOM/licences, puis tester le TGZ par lancement réel sous Xvfb avec
   `SDL_AUDIODRIVER=dummy` et A/B audio, NVIDIA/AMD, X11 et Wayland natif.

Ces actions accélèrent M01 sans transformer ReXGlue en oracle retail. La
qualification finale reste PAL AC6 et chaque écart provisoire doit disparaître
ou être confirmé avant JF/JV/JP/JG.

## Validation de l'audit

- clones blobless et API GitHub utilisés en lecture seule ; aucun asset de
  release téléchargé ;
- commits, arbres, tags, branches, merge-bases et 22 gitlinks vérifiés
  localement ;
- recherche ciblée de `XenonRecomp`, `XenosRecomp`, `XenonAnalyse`, `rexdex`,
  replay, vidéo et savestate ;
- aucun fichier hors de ce rapport modifié par ce cycle ; aucun commit/push.
