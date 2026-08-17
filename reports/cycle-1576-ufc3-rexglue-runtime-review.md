# Cycle 1576 — Unchallenged 3 / UFC Undisputed 3 : revue runtime ReXGlue

Audit public arrêté au **12 août 2026**. Le projet examiné est
[`automoto/Unchallenged3`](https://github.com/automoto/Unchallenged3), avec son
sous-module ReXGlue public sur Codeberg et son miroir GitHub
[`automoto/rexglue-u3-fork`](https://github.com/automoto/rexglue-u3-fork).

Aucun XEX, ISO, save, code C++ généré depuis le jeu, asset de release ou paquet
opaque n'a été téléchargé ou exécuté. Les PNG suivis par le dépôt n'ont pas
été utilisés comme preuve visuelle. La revue porte sur les arbres Git publics,
leur historique, les métadonnées des forges, les workflows et le code source du
runtime.

## Verdict pour AC6 Mission 01

Le README annonce un jeu entièrement jouable et un multijoueur local avec les
manettes connectées
([source](https://github.com/automoto/Unchallenged3/blob/47921ade7678b4b5def6cd6935b9ade3f161841d/README.md#L123-L131)).
Ce sont des affirmations **`documented-unmatched`** : aucun SHA-256 XEX, Media
ID, région/révision, log de codegen, trace d'inputs, hash d'état, capture PCM,
mesure de cadence brute, test de save ou CI du jeu ne les relie à une exécution
reproductible.

Le dépôt est néanmoins une bonne carte des **jointures de runtime à
instrumenter** : quatre slots SDL/XAM, numéro de paquet d'input, attente GPU
`WAIT_REG_MEM`, fetch constants Xenos, texture tiled/endian/mips, trace du ring
buffer et de l'EDRAM, kick XMA et partitionnement save par titre/profil/XUID.
Ces formes sont **`provisional-rexglue`**. Elles ne qualifient ni le comportement
retail UFC3, ni celui d'AC6.

Trois conclusions négatives sont directement utiles :

1. le projet ne publie **aucun replay déterministe** ; son ancien injecteur
   d'inputs suivait le temps mural et la fenêtre Windows ;
2. la cadence guest dépend du temps hôte et les XThreads deviennent des threads
   hôte : le correctif « 60 FPS » améliore le pacing, pas le déterminisme ;
3. aucun code jeu généré n'est public : l'IA, les conditions, les événements et
   les transitions UFC3 sont totalement hors audit.

Il n'y a **aucun fait `retail-qualified`**. Cette revue ne ferme aucune des six
lanes AC6 M01 et ne justifie aucun compteur, aucune activation de cible ni
aucune sémantique VMX AC6.

## Échelle de qualification

| Classe | Usage dans cette revue |
|---|---|
| `provisional-rexglue` | structure publique vérifiable, utilisable comme hypothèse ou seam de test après réécriture AC6 |
| `retail-qualified` | résultat lié à un XEX exact par SHA-256 et validé contre le retail ; ensemble vide |
| `divergent` | fallback, bypass, option hôte ou ordonnancement qui remplace explicitement le comportement à qualifier |
| `documented-unmatched` | affirmation, adresse ou chemin présent sans artefact d'exécution déterministe correspondant |

## Dépôts, révisions, tags et releases

### Projet titre

Le HEAD public est le commit non signé
[`47921ade7678b4b5def6cd6935b9ade3f161841d`](https://github.com/automoto/Unchallenged3/commit/47921ade7678b4b5def6cd6935b9ade3f161841d),
arbre `d0aa23e393a10a7d6a12e030c1061bab28a9e65e`, sur l'unique branche
`main`. Il contient 27 blobs, 4 924 105 octets et un gitlink. GitHub ne publie
aucun tag, aucune release et aucun run Actions pour ce dépôt. La licence racine
est BSD-3-Clause.

Un scan des noms de l'arbre courant et de tous ses commits ne trouve aucun
container ou binaire évident portant une extension `.xex`, `.xexp`, `.iso`,
`.xiso`, `.exe`, `.dll`, `.lib`, `.zip`, `.7z` ou `.rar`. Le répertoire
`generated/` ne contient qu'un `.gitkeep`. Cela n'est pas une preuve d'absence
de toute matière dérivée.

Le dépôt suit en revanche trois captures PNG totalisant 4 893 360 octets, alors
que ses propres règles interdisent les screenshots de contenu protégé
([CONTRIBUTING](https://github.com/automoto/Unchallenged3/blob/47921ade7678b4b5def6cd6935b9ade3f161841d/CONTRIBUTING.md#L5-L10))
et que sa checklist demande qu'aucun screenshot ne soit stagé
([checklist](https://github.com/automoto/Unchallenged3/blob/47921ade7678b4b5def6cd6935b9ade3f161841d/docs/release-checklist.md#L1-L16)).
Les scripts `check-public-tree.ps1`, `build-u3.ps1` et `rungame.ps1` cités par
cette checklist sont absents du HEAD. L'hygiène annoncée est donc
`documented-unmatched` et ne doit pas servir de modèle au paquet AC6.

### Fork ReXGlue effectivement pinné

Le gitlink `tools/rexglue-sdk` pointe exactement sur
[`26ef1987eed990801775979e570b0614b3a162b8`](https://codeberg.org/GameRewrite/rexglue-u3-fork/commit/26ef1987eed990801775979e570b0614b3a162b8)
et `.gitmodules` nomme
`https://codeberg.org/GameRewrite/rexglue-u3-fork.git`
([source](https://github.com/automoto/Unchallenged3/blob/47921ade7678b4b5def6cd6935b9ade3f161841d/.gitmodules#L1-L3)).
Le HEAD Codeberg, `main` Codeberg et le miroir GitHub résolvent tous vers ce
même commit non signé, arbre
`c4b2e4c4d7f4fb7402502ef99ce5294ba59e548b`.

Le tag public [`v0.8.0`](https://codeberg.org/GameRewrite/rexglue-u3-fork/src/tag/v0.8.0)
pointe sur `2bdb97f95f154f32d281aaa08446ae007b8ca117`, arbre
`ff1c1b67f4dfae8f35269977bcd0570d1d174701`. Le merge non signé
`e8ce24fa73cd7c1ede80262c06f34893b7963dbe`, parent immédiat du patch UFC3,
a exactement le même arbre. Le fork ajoute ensuite :

| Commit | Effet public |
|---|---|
| [`27b71d73`](https://github.com/automoto/rexglue-u3-fork/commit/27b71d73e0b70ee93a0c822393f93e27d08939cd) | compatibilité UFC3, timer Windows haute résolution et services save |
| [`80604ead`](https://github.com/automoto/rexglue-u3-fork/commit/80604eadb8da84a5ab795b1915d5debacfb20877) | sources libmspack réelles dans CMake |
| [`26ef1987`](https://github.com/automoto/rexglue-u3-fork/commit/26ef1987eed990801775979e570b0614b3a162b8) | exclusions du scanner de migration |

Le delta `e8ce24f..26ef198` touche 14 fichiers : 978 insertions et 148
suppressions. L'arbre complet du fork contient 1 258 blobs, 163 255 213 octets
et 22 gitlinks.

Avec les tags de son remote Codeberg, le HEAD se décrit
`v0.8.0-7-g26ef198`. Le calcul de version du fork produit donc
`0.8.1.7-dev.g26ef198`
([algorithme](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/cmake/rex_version.cmake#L38-L69)),
alors que le manifeste titre reste estampillé `sdk_version = "0.8.1.6"`
([manifeste](https://github.com/automoto/Unchallenged3/blob/47921ade7678b4b5def6cd6935b9ade3f161841d/u3_manifest.toml#L1-L9)).
Le gitlink est l'autorité pour reconstruire ; le stamp manifeste n'est pas une
provenance exacte du SDK courant.

Le miroir GitHub du fork ne publie ni tag ni release. Codeberg expose 32 tags,
dont les tags versionnés jusqu'à `v0.8.0` et des nightlies ; aucun tag ne pointe
sur le HEAD UFC3. Aucun asset de release n'a été utilisé.

### Sous-modules du fork

Les 22 gitlinks de
[`.gitmodules`](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/.gitmodules)
sont pinnés comme suit :

| Dépendance | Pin exact |
|---|---|
| libmspack | [`305907723a4e7ab2018e58040059ffb5e77db837`](https://github.com/kyz/libmspack/commit/305907723a4e7ab2018e58040059ffb5e77db837) |
| glslang | [`f4f1d8a352ca1908943aea2ad8c54b39b4879080`](https://github.com/KhronosGroup/glslang/commit/f4f1d8a352ca1908943aea2ad8c54b39b4879080) |
| FFmpeg, branche déclarée `xenia-ffmpeg-canary-full` | [`0604b464c7cb4ebc94940cf1f324a3b26b87717c`](https://github.com/wmarti/FFmpeg/commit/0604b464c7cb4ebc94940cf1f324a3b26b87717c) |
| toml++ | [`30172438cee64926dc41fdd9c11fb3ba5b2ba9de`](https://github.com/marzer/tomlplusplus/commit/30172438cee64926dc41fdd9c11fb3ba5b2ba9de) |
| SIMDe | [`71fd833d9666141edcd1d3c109a80e228303d8d7`](https://github.com/simd-everywhere/simde/commit/71fd833d9666141edcd1d3c109a80e228303d8d7) |
| xxHash | [`e626a72bc2321cd320e953a0ccf1584cad60f363`](https://github.com/Cyan4973/xxHash/commit/e626a72bc2321cd320e953a0ccf1584cad60f363) |
| spdlog | [`79524ddd08a4ec981b7fea76afd08ee05f83755d`](https://github.com/gabime/spdlog/commit/79524ddd08a4ec981b7fea76afd08ee05f83755d) |
| fmt | [`407c905e45ad75fc29bf0f9bb7c5c2fd3475976f`](https://github.com/fmtlib/fmt/commit/407c905e45ad75fc29bf0f9bb7c5c2fd3475976f) |
| Catch2 | [`88abf9bf325c798c33f54f6b9220ef885b267f4f`](https://github.com/catchorg/Catch2/commit/88abf9bf325c798c33f54f6b9220ef885b267f4f) |
| Snappy | [`6af9287fbdb913f0794d0148c6aa43b58e63c8e3`](https://github.com/google/snappy/commit/6af9287fbdb913f0794d0148c6aa43b58e63c8e3) |
| utfcpp | [`63d64de49fd6b829f7c8694df5ab2ee625cb7134`](https://github.com/nemtrif/utfcpp/commit/63d64de49fd6b829f7c8694df5ab2ee625cb7134) |
| volk | [`0b17a763ba5643e32da1b2152f8140461b3b7345`](https://github.com/zeux/volk/commit/0b17a763ba5643e32da1b2152f8140461b3b7345) |
| Vulkan-Headers | [`49f1a381e2aec33ef32adf4a377b5a39ec016ec4`](https://github.com/KhronosGroup/Vulkan-Headers/commit/49f1a381e2aec33ef32adf4a377b5a39ec016ec4) |
| VulkanMemoryAllocator | [`1d8f600fd424278486eade7ed3e877c99f0846b1`](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/commit/1d8f600fd424278486eade7ed3e877c99f0846b1) |
| Dear ImGui | [`6d910d5487d11ca567b61c7824b0c78c569d62f0`](https://github.com/ocornut/imgui/commit/6d910d5487d11ca567b61c7824b0c78c569d62f0) |
| SPIR-V Tools | [`04d0b166dcd62e29509bf2aac3ca0c5ccdcb6929`](https://github.com/KhronosGroup/SPIRV-Tools/commit/04d0b166dcd62e29509bf2aac3ca0c5ccdcb6929) |
| SPIR-V Headers | [`04f10f650d514df88b76d25e83db360142c7b174`](https://github.com/KhronosGroup/SPIRV-Headers/commit/04f10f650d514df88b76d25e83db360142c7b174) |
| CLI11 | [`bfffd37e1f804ca4fae1caae106935791696b6a9`](https://github.com/CLIUtils/CLI11/commit/bfffd37e1f804ca4fae1caae106935791696b6a9) |
| o1heap | [`388a73fd9007300e5130c5fe352d9ce3288b6dde`](https://github.com/pavel-kirienko/o1heap/commit/388a73fd9007300e5130c5fe352d9ce3288b6dde) |
| SDL3, branche déclarée `release-3.4.x` | [`8bf3b7215ad9fc3deb583c6a3a37c6c67f2e24e4`](https://github.com/libsdl-org/SDL/commit/8bf3b7215ad9fc3deb583c6a3a37c6c67f2e24e4) |
| inja | [`7d1b4600b68595085a949743331c2e5673f511ea`](https://github.com/pantor/inja/commit/7d1b4600b68595085a949743331c2e5673f511ea) |
| Tracy | [`05cceee0df3b8d7c6fa87e9638af311dbabc63cb`](https://github.com/wolfpld/tracy/commit/05cceee0df3b8d7c6fa87e9638af311dbabc63cb) |

Le fork suit aussi 37 blobs sous `tools/binutils`, totalisant 136 537 551
octets : des binaires GNU binutils Windows/Cygwin et Linux, plus leur fichier
GPLv3
([arbre](https://github.com/automoto/rexglue-u3-fork/tree/26ef1987eed990801775979e570b0614b3a162b8/tools/binutils)).
Ils représentent 83,6 % des octets de blobs du SDK. Un paquet AC6 ne doit pas
les hériter transitivement : ses outils de build et leurs notices doivent être
audités séparément de son TGZ runtime.

## CI et reproductibilité publique

Le projet titre n'a ni workflow ni test. Le fork contient 241 `TEST_CASE`
Catch2 et 166 entrées assembleur PPC, mais `REXGLUE_BUILD_TESTS` vaut `OFF` par
défaut
([CMake](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/CMakeLists.txt#L13-L20))
et aucun workflow ne lance CTest.

Le miroir du fork compte 43 runs Actions sur le HEAD : 42 nightlies et un
lint, tous en échec. Le
[nightly le plus récent](https://github.com/automoto/rexglue-u3-fork/actions/runs/31574073002)
échoue au checkout : le workflow demande la branche `development`
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/.github/workflows/nightly.yaml#L12-L31)),
mais le fork public n'expose que `main`; les jobs Windows, Linux amd64 et Linux
arm64 sont alors sautés. Le seul
[run lint](https://github.com/automoto/rexglue-u3-fork/actions/runs/28539045681)
échoue à `clang-format --dry-run --Werror`. Il n'existe donc aucun build CI
vert du pin public.

Le CMake titre exige `generated/rexglue.cmake`, absent avant codegen
([source](https://github.com/automoto/Unchallenged3/blob/47921ade7678b4b5def6cd6935b9ade3f161841d/CMakeLists.txt#L1-L23)).
La source ne peut pas produire un exécutable sans bytes utilisateur, ce qui est
normal, mais aucun manifeste de fichiers/hashes générés ni log ne permet de
rejouer le build annoncé.

## Identité retail et fermeture de codegen

Le dépôt ne publie aucun SHA-256 XEX, Title ID, Media ID, région, version,
timestamp ou certificat. Le manifeste ne dit que `game/default.xex`. Le script
d'extraction accepte toute image avec une partition XDVDFS, puis vérifie
seulement l'existence et la taille de `default.xex`
([source](https://github.com/automoto/Unchallenged3/blob/47921ade7678b4b5def6cd6935b9ade3f161841d/scripts/extract_iso.py#L163-L212)).
Une autre révision ou un autre titre pourrait donc atteindre le codegen avant
qu'une divergence ne soit remarquée.

Le fichier d'overrides contient 21 adresses guest : 19 cibles annoncées comme
observées par validation, puis deux cibles rencontrées uniquement avec un
render scale supérieur à 1
([source](https://github.com/automoto/Unchallenged3/blob/47921ade7678b4b5def6cd6935b9ade3f161841d/config/u3_rexglue_overrides.toml#L1-L72)).
Il ne fournit ni bytes, ni frontières PDATA/Ghidra, ni log d'appel indirect, ni
micro-exécution. Toutes ces adresses sont `documented-unmatched`. Leur seule
leçon transférable est la méthode : la fermeture dynamique doit varier les
modes qui ouvrent de nouveaux chemins, tandis que les comparaisons retail
restent sous profil fixe.

## Filiation Xenon/Xenos et SIMD

La seule dépendance directe du titre est le fork ReXGlue. Le README du SDK dit
explicitement que ReXGlue repose largement sur Xenia et s'inspire de
XenonRecomp et du recompiler de rexdex
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/README.md#L19-L23)).
Le contexte PPC se dit fondé sur XenonRecomp/UnleashedRecomp et inclut SIMDe
SSE/AVX
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/include/rex/ppc/context.h#L1-L25));
le parseur TOML crédite la même filiation
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/codegen/config.cpp#L1-L26)).

Les presets amd64 compilent avec `-march=x86-64-v3`
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/CMakePresets.json#L8-L52)).
C'est une preuve que le C++ portable/SIMDe peut devenir SSE/AVX natif, pas une
preuve de sémantique VMX128 correcte pour AC6.

Il n'existe **aucun gitlink ni pin** XenonRecomp, XenonAnalyse, XenosRecomp,
rexdex/recompiler ou Xenia. `XenonAnalyse` et `XenosRecomp` ne sont même pas
nommés dans l'arbre. Le renderer audité est la voie Xenia/ReXGlue, pas
XenosRecomp. Ces projets restent des oracles/corpus distincts dans AC6 ; la
filiation textuelle ne remplace pas leur provenance exacte.

## Manettes XAM et replay synchronisé

### Ce qui existe

Le driver SDL alloue exactement quatre slots et conserve pour chacun un état,
un drapeau de changement et un numéro de paquet
([header](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/include/rex/input/sdl/sdl_input_driver.h#L24-L28),
[stockage](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/include/rex/input/sdl/sdl_input_driver.h#L48-L99)).
À la connexion, il honore un index joueur SDL libre ou choisit le premier slot
libre ; au-delà de quatre, la manette est ignorée
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/input/sdl/sdl_input_driver.cpp#L453-L485)).
Le hotplug efface le slot à la déconnexion
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/input/sdl/sdl_input_driver.cpp#L488-L499)).

`GetState` draine les événements, incrémente `packet_number` au plus une fois
au prochain poll si l'état ou le focus a changé, puis copie l'état ; hors focus,
les boutons/axes rendus au guest sont nuls
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/input/sdl/sdl_input_driver.cpp#L168-L201)).
`XamInputGetState` transmet l'index 0–3 au système d'input, mais force
`ANY_USER`/`0xFF` vers le slot 0
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/kernel/xam/xam_input.cpp#L94-L116)).
Ce dernier choix est un comportement ReXGlue, pas une sémantique XAM retail
qualifiée.

Quatre slots d'input ne signifient pas quatre utilisateurs XAM. Le patch
accepte les indices 0–3 et `0xFF`, mais `XamUserGetXUID`, signin, nom et gamertag
renvoient tous le **même singleton** `user_profile()`
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/kernel/xam/xam_user.cpp#L39-L132)).
Ce profil a un XUID et un nom hôte constants, se dit toujours connecté et se
déclare local/online
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/system/xam/user_profile.cpp#L24-L34),
[interface](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/include/rex/system/xam/user_profile.h#L289-L306)).
Les écritures de settings de tout index accepté modifient ce singleton
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/kernel/xam/xam_user.cpp#L293-L312)).
Le runtime accorde aussi toute vérification de privilège, annonce l'online actif
et renvoie un tier Gold
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/kernel/xam/xam_user.cpp#L401-L453)).

Le multijoueur local annoncé peut donc fonctionner par aliasing des profils et
bypass permissifs. Ce bloc est **`divergent`**, pas une implémentation
multi-utilisateur XAM à reprendre. Pour AC6 M01, le replay doit conserver
l'index user réel et le frontend retail ne doit exposer que les profils dont la
sémantique a été qualifiée.

Si plusieurs backends répondent pour le même slot, `InputSystem` fusionne les
boutons par OR, les triggers par maximum, chaque stick par magnitude maximale
et le numéro de paquet par maximum
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/input/input_system.cpp#L77-L127)).
Cette fusion est utile pour le confort hôte, mais **`divergent`** pour un replay :
elle perd l'identité du producteur et son ordre d'événements.

### Ce qui n'existe pas

Le HEAD ne contient aucun format de replay, backend d'input scripté, compteur
de poll scellé, hash de tick ou contrôle de première divergence. La documentation
dit que la capture headless et ses compteurs vivent dans un fork séparé
`rex-glue-debug`
([source](https://github.com/automoto/Unchallenged3/blob/47921ade7678b4b5def6cd6935b9ade3f161841d/docs/development.md#L16-L28)),
mais aucun dépôt public correspondant n'est lié dans le projet.

L'historique contient un ancien
[`run-u3-input-capture.ps1`](https://github.com/automoto/Unchallenged3/blob/924321d706e8c3485a46a05f537060f3b6d01d1a/scripts/run-u3-input-capture.ps1).
Il pilote `SendInput`, force le focus, attend par `Start-Sleep`, choisit les
touches selon `Stopwatch.Elapsed.TotalSeconds`
([plan](https://github.com/automoto/Unchallenged3/blob/924321d706e8c3485a46a05f537060f3b6d01d1a/scripts/run-u3-input-capture.ps1#L195-L247)),
et capture le bureau toutes les vingt secondes
([boucle](https://github.com/automoto/Unchallenged3/blob/924321d706e8c3485a46a05f537060f3b6d01d1a/scripts/run-u3-input-capture.ps1#L274-L318)).
C'est un smoke test Windows `documented-unmatched`, pas un replay synchronisé.

### Patron à retenir pour AC6

Le seam public confirme qu'il faut enregistrer/rejouer **au bord
`XamInputGetState`**, après normalisation mais avant toute logique de jeu. Pour
chaque appel AC6, le record minimal doit contenir :

```text
poll_ordinal, sim_tick, guest_thread, user_index, flags,
result, packet_number, buttons, triggers, four_stick_axes
```

Le playback doit être l'unique backend actif et refuser immédiatement un index,
des flags, un résultat ou un nombre de polls différent. `sim_tick` sert à
localiser la divergence ; **`poll_ordinal` est l'ordre autoritaire**, car le jeu
peut appeler XAM zéro, une ou plusieurs fois par tick.

Ainsi, « 3 600 ticks » ne signifie ni 3 600 tests ni nécessairement 3 600
records d'input. Il faut sceller les 3 600 ticks de simulation et le flux de
polls réel correspondant, puis produire au premier écart un record structuré
avec hash d'état, dernier input consommé et événement mission.

## Cadence, scheduler, IA et événements

Le seul patch de cadence spécifique UFC3 remplace les courts `Sleep` Windows :
moins de 100 µs fait un yield, 100 µs à 20 ms utilise un waitable timer haute
résolution, puis le code revient au `Sleep` Win32
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/core/threading_win.cpp#L96-L146)).
La boucle `WAIT_REG_MEM` du command processor emploie cette primitive lorsque
le vsync est actif
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/graphics/command_processor.cpp#L1116-L1189)).
Le README attribue à ce patch le live match à 60 FPS
([source](https://github.com/automoto/Unchallenged3/blob/47921ade7678b4b5def6cd6935b9ade3f161841d/README.md#L133-L143)),
mais aucune série de timestamps/frame times n'est publiée.

Ce patch ne crée pas un pas fixe. L'horloge guest est calculée depuis les ticks
hôte et protégée par mutex
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/core/clock.cpp#L20-L88)).
Chaque XThread est lancé sur un thread hôte
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/system/xthread.cpp#L419-L478))
et ses délais deviennent des sleeps/yields hôte
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/system/xthread.cpp#L982-L1018)).
Le scheduler et le pacing sont donc `divergent` pour une preuve de replay
déterministe.

Le dispatcher public sait mapper une adresse guest vers une fonction C++ et
piéger une cible indirecte absente
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/system/function_dispatcher.cpp#L37-L67)).
Il ne fournit pas une trace des appels directs entre fonctions générées. Sans
le C++ jeu, il n'existe aucune preuve publique pour le scheduler gameplay,
l'IA, les compteurs, les triggers, les conditions de round ou les transitions
UFC3.

Pour AC6, une analyse sémantique statique des fonctions déjà trouvées reste
rentable si elle part d'une frontière Ghidra/bytes PAL qualifiée et se termine
par un invariant testable. ReXGlue peut réduire le nombre de micro-exécutions
pour les services hôte déjà couverts, mais ne permet pas de supprimer celles
qui tranchent une sémantique PPC/VMX ou un premier écart gameplay. La stratégie
efficace est : replay large pour localiser, analyse statique pour expliquer,
micro-exécution bornée seulement pour départager.

## Renderer Xenos, EDRAM et textures

UFC3 n'a pas de renderer natif manuscrit : il utilise le command processor
Xenia/ReXGlue. Le projet titre force deux cvars pour son chemin « jouable » :
accepter les fetch constants invalides et désactiver le submit D3D12 en fin de
buffer primaire
([source](https://github.com/automoto/Unchallenged3/blob/47921ade7678b4b5def6cd6935b9ade3f161841d/src/u3_app.h#L27-L46)).
Ces deux choix sont `divergent`, pas des faits Xenos retail.

Le runtime documente lui-même deux chemins EDRAM : render targets hôte avec
transferts de tiles, qualifiés de potentiellement « irreparably inaccurate »,
et pixel shader interlock avec contrôle par pixel/échantillon
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/include/rex/graphics/pipeline/render_target/cache.h#L33-L82)).
Le succès visuel revendiqué ne permet donc pas de choisir une sémantique
EDRAM AC6 sans contrôle positif.

Les structures utiles comme vocabulaire sont bien présentes :

- le fetch constant produit base/mip, dimensions, tiling, packing, format et
  endian
  ([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/graphics/pipeline/texture/cache.cpp#L898-L988)) ;
- `TextureInfo` conserve format, endianness, dimension, mémoire base/mip et
  packed mips
  ([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/include/rex/graphics/pipeline/texture/info.h#L153-L217)) ;
- la conversion implémente les swaps 8-in-16, 8-in-32, 16-in-32 et le calcul
  tiled 2D
  ([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/graphics/pipeline/texture/conversion.cpp#L24-L44),
  [untile](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/graphics/pipeline/texture/conversion.cpp#L80-L135)).

Lorsque le fetch est invalide et que le bypass forcé est actif, Vulkan aligne
son comportement sur le null SRV D3D12 avec une texture RGBA nulle 1×1
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/graphics/vulkan/texture_cache.cpp#L96-L103),
[fallback](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/graphics/vulkan/texture_cache.cpp#L2920-L2951)).
AC6 doit au contraire rejeter déterministement un format/fetch inconnu et
garder le cas comme fixture, jamais fabriquer une texture noire silencieuse.

Le command processor peut enregistrer une frame ou un stream de paquets
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/graphics/command_processor.cpp#L163-L197))
et le protocole sait inclure les lectures/écritures mémoire et un snapshot EDRAM
de taille Xenos
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/graphics/trace_writer.cpp#L205-L275)).
C'est un oracle packet/EDRAM `provisional-rexglue`, pas un replay gameplay ni
un savestate qualifié.

Pour M01, on peut reprendre le **schéma** des clés texture et la segmentation
des traces afin de produire des fixtures AC6. Le produit doit toujours convertir
les assets importés en `DrawPacket` retail typés et les soumettre directement au
backend Vulkan natif ; aucun command processor ReXGlue ne doit entrer dans le
runtime ou le TGZ.

## XMA, audio et vidéo

Le sous-système audio lie SDL3, `libavcodec` et `libavutil`, mais pas
`libavformat`
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/audio/CMakeLists.txt#L1-L27)).
Un contexte XMA recherche le codec FFmpeg `AV_CODEC_ID_XMAFRAMES`
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/audio/xma_context.cpp#L58-L92)).
Le decoder mappe les registres XMA à `0x7FEA0000`, alloue les contextes et
lance un worker hôte
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/audio/xma_decoder.cpp#L89-L165)).

Le kick active les contextes, réveille le worker puis attend leur fin avant de
rendre la main au guest
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/audio/xma_decoder.cpp#L271-L304)).
Cette barrière est un seam intéressant pour une fixture : hash du contexte avant
kick, hash après kick et hash PCM produit. Elle ne prouve ni bit-exactitude XMA,
ni niveau audio, ni timing de cue.

Le worker de sortie audio est lui aussi un thread hôte
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/audio/audio_system.cpp#L76-L110))
et le shutdown peut terminer de force un callback guest bloqué
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/audio/audio_system.cpp#L185-L203)).
Il n'existe donc pas de preuve de synchronisation A/V déterministe.

La couche vidéo XAM ne fournit que le mode/capabilities et laisse
`XamLoadExtraAVCodecs2`/`XamUnloadExtraAVCodecs2` en stubs
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/kernel/xam/xam_video.cpp#L25-L44)).
Le README conseille d'ailleurs de sauter les vidéos d'intro/menu
([source](https://github.com/automoto/Unchallenged3/blob/47921ade7678b4b5def6cd6935b9ade3f161841d/README.md#L75-L82)).
UFC3 n'apporte donc aucune fermeture ASF/vidéo pour AC6.

## VFS, import et sauvegardes

Le script `extract_iso.py` localise XDVDFS puis extrait directement chaque
fichier dans `game/`. Il ne calcule aucun digest, ne scelle aucun manifest,
n'utilise pas de staging/rename atomique et ne vérifie pas que les composants
XDVDFS restent sous la racine de sortie
([parse/extraction](https://github.com/automoto/Unchallenged3/blob/47921ade7678b4b5def6cd6935b9ade3f161841d/scripts/extract_iso.py#L88-L150)).
Le runtime est ensuite lancé avec `--game_data_root=game`
([source](https://github.com/automoto/Unchallenged3/blob/47921ade7678b4b5def6cd6935b9ade3f161841d/README.md#L63-L79)).
Il relit donc les fichiers retail extraits à l'exécution. Ce chemin est
`divergent` par rapport à l'invariant AC6 cache v2 : import atomique puis aucune
relecture PAC/container retail par `play`.

Le patch save fournit un partitionnement hôte intelligible :
`content_root/xuid/title_id/content_type/name`, avec XUID commun pour le contenu
marketplace
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/system/xam/content_manager.cpp#L117-L165)),
et `content_root/title_id/profile/xuid` pour les settings du profil
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/system/xam/content_manager.cpp#L472-L479)).
Cette séparation est `provisional-rexglue`.

Les écritures ne sont toutefois pas transactionnelles : le header est créé
puis écrit directement par `fwrite`
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/system/xam/content_manager.cpp#L220-L251))
et un setting de profil est tronqué/écrit directement sans checksum, version,
fsync ou migration
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/src/system/xam/user_profile.cpp#L132-L172)).
Aucun test public ne couvre corruption, crash en cours d'écriture, profils
multiples ou reprise. Pour AC6, seul le partitionnement logique est à retenir ;
la persistance doit rester atomique et versionnée sous XDG.

## Linux, Vulkan et packaging

Le projet titre documente uniquement Windows, Clang dans `C:\Program Files`,
Visual Studio 2022 et un exécutable `.exe`
([source](https://github.com/automoto/Unchallenged3/blob/47921ade7678b4b5def6cd6935b9ade3f161841d/README.md#L14-L25)).
Ses presets sont conditionnés exclusivement à Windows
([source](https://github.com/automoto/Unchallenged3/blob/47921ade7678b4b5def6cd6935b9ade3f161841d/CMakePresets.json#L1-L37)).

Le SDK générique choisit Vulkan par défaut hors Windows et prévoit un RPATH
relatif pour ses installs Linux
([source](https://github.com/automoto/rexglue-u3-fork/blob/26ef1987eed990801775979e570b0614b3a162b8/CMakeLists.txt#L29-L66)).
Cela prouve une intention cross-platform du SDK, pas un build UFC3 Linux : il
n'existe ni preset titre Linux, ni artifact, ni run Vulkan, ni validation X11/
Wayland, AMD/Mesa ou NVIDIA.

Le dépôt titre ne contient aucune règle `install()`, CPack, TGZ, SBOM ou
manifest de licences runtime. Il ne publie aucune release. La politique source
interdit correctement XEX, ISO et C++ généré
([source](https://github.com/automoto/Unchallenged3/blob/47921ade7678b4b5def6cd6935b9ade3f161841d/docs/legal.md#L1-L9)),
mais l'absence des scripts d'audit cités et la présence des PNG empêchent de
qualifier sa discipline de packaging.

## Actions AC6 M01

| Priorité | Action bornée | Source UFC3 utilisée | Gate AC6 requise |
|---|---|---|---|
| P0 | Ajouter au harnais oracle un flux XAM **poll-exact** et un backend replay exclusif | quatre slots, `packet_number`, bord `XamInputGetState` | XEX PAL SHA-256, 3 600 ticks, flux de polls scellé, erreur au premier poll/tick divergent |
| P0 | Enregistrer `thread/user/flags/result/state` et non les événements SDL physiques | la fusion multi-backend montre la perte d'identité | deux replays identiques bit à bit, invariance résolution, aucun input hôte concurrent |
| P0 | Refuser l'aliasing implicite de quatre indices vers un profil/XUID unique | le patch UFC3 rend tous les users connectés sur un singleton | user 0 qualifié pour M01 ; tout index supplémentaire explicitement absent ou validé |
| P0 | Tracer aux bornes AC6 qualifiées activation cible, dommage/destruction, compteur, condition mission et transition | absence totale de logique titre UFC3 | JV/JG : événements ±1 tick et aucun compteur synthétique |
| P0 | Utiliser le replay complet pour localiser les divergences, puis la micro-exécution seulement sur la première fonction/VMX ambiguë | le dispatcher attrape les cibles indirectes mais ne trace pas les appels directs | bytes PAL + contrôle positif + test de non-régression par sémantique portée |
| P1 | Construire un corpus texture avec clé `{format,endian,dimension,tiled,base,mip,packed}` | modèle fetch/texture ReXGlue | BC3 positif, mips/cubemap, unknown rejeté, corpus M01 complet |
| P1 | Capturer paquets GPU/EDRAM uniquement comme oracle, puis produire les `DrawPacket` natifs | trace ring buffer/EDRAM ReXGlue | backend Vulkan 1.3 direct ; aucun fallback CPU/ReXGlue interactif |
| P1 | Borner chaque kick XMA par hashes contexte/PCM et horodatage de cue | fence kick → worker terminé | EN/JP, niveau ±1 dB, cues ±20 ms ; ASF/vidéo validé séparément |
| P1 | Durcir `import` plutôt que reprendre l'extracteur UFC3 | lacunes XEX/hash/atomicité/containment | cache v2 atomique, digests, path containment, aucune PAC relue après import |
| P1 | Garder saves/configs transactionnels et versionnés sous XDG | partition titre/profil utile, écritures directes rejetées | crash/corruption/migration/reprise et install relogeable |
| P2 | Ne tirer aucune conclusion Linux du fork générique | seul le SDK expose Vulkan Linux | build/CTest Python/audits puis Vulkan validation sous X11 et Wayland |

Pour valider l'activation/destruction d'une cible en gameplay, le replay doit
porter un chemin reproductible jusqu'au contact, tandis que le jeu natif émet
des événements structurés depuis les producteurs retail portés. Un input humain
ou le tutoriel peut amorcer **l'enregistrement** ; il ne devient pas la preuve.
La preuve est le même flux poll-exact rejoué dans l'oracle qualifié et dans le
natif, avec concordance de l'identité cible, du tick d'activation, des dommages,
du tick de destruction, du compteur et de la transition suivante.

## Limites et risques résiduels

- Aucun build UFC3 n'a été reproduit faute de XEX qualifié et de codegen
  publiable ; aucune affirmation de jouabilité n'a été confirmée.
- Les adresses UFC3, les options de rendu et les chemins save sont liés à un
  XEX inconnu. Ils ne doivent jamais entrer dans les cartes AC6.
- Le correctif de timer est Windows-only et son effet annoncé n'a ni trace ni
  test. Il ne résout pas l'ordonnancement déterministe.
- Le renderer et l'audio héritent de Xenia/ReXGlue sans pin Xenia. Leur filiation
  est documentée, leur base historique exacte ne l'est pas.
- Le fork `rex-glue-debug` mentionné n'est pas une dépendance publique du projet ;
  aucune capacité qui lui est attribuée n'a été comptée.
- Aucun fait issu de screenshots, de vidéos, de commentaires communautaires ou
  d'une release binaire n'entre dans la qualification.

**Bilan :** UFC3 valide l'idée d'une instrumentation XAM poll-exact et fournit
des structures de test utiles pour textures, EDRAM, XMA et saves. Il ne fournit
ni oracle retail qualifié, ni replay, ni sémantique gameplay transférable, ni
preuve Linux. La stratégie AC6 reste donc : ReXGlue comme oracle provisoire,
trace synchronisée pour réduire l'espace de recherche, puis validation PAL
retail différée sans relâcher les gates JF/JV/JP/JG.
