# Cycle 1569 — Diablo III / Souls of the Reaper : revue release ReXGlue

Audit public arrêté au **12 août 2026**. Les recherches GitHub par nom,
description, README, titre ID et manifeste ne trouvent qu'un projet public
pertinent :
[`Boron853/Souls-of-the-Reaper`](https://github.com/Boron853/Souls-of-the-Reaper).
L'API GitHub ne lui attribue aucun fork public à cette date.

Aucun XEX, ISO, CPK, save retail, installateur de release ou exécutable jeu n'a
été téléchargé ou exécuté. La revue porte sur les 20 blobs source du dépôt, les
métadonnées GitHub, le SDK public qualifié et l'application statique du patch
sur une copie temporaire du SDK. Les assets de release ne sont qualifiés que
par leurs métadonnées GitHub.

## Verdict pour AC6 Mission 01

Le dépôt revendique un port « playable and stable », deux chemins D3D12, le
jeu local à quatre et des sauvegardes fonctionnelles
([README](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/README.md#L1-L5)).
Ces revendications restent **`documented-unmatched`** : aucun SHA-256 XEX,
Media ID, version/certificate XEX, log de codegen, capture, replay, hash de
frame/PCM, test de campagne ou CI du port n'est publié.

La source publiée ne reconstruit même pas la release annoncée. Le patch se
rejoue sur le nightly ReXGlue déclaré par le manifeste, mais ajoute deux
includes vers `rex/input/slot_arbiter.h`, fichier absent du patch, du dépôt et
de l'upstream. Le C++ généré, le SDK binaire, l'outil d'extraction et plusieurs
dépendances du launcher sont également absents. Une release installable peut
donc être jouable sans que son état source soit reproductible.

Il n'y a **aucun fait `retail-qualified`**, ni pour Diablo III ni par
transitivité pour AC6 PAL. Aucune lane M01 n'est fermée.

L'intérêt pour AC6 est méthodologique : le projet expose des frontières utiles
à transformer en fixtures AC6 — création du premier save, identité par joueur,
fin de vie des tâches XAM, ordre d'attribution des pads, mesure de cadence et
aliasing EDRAM couleur/profondeur. Les implémentations Diablo restent des
correctifs titre/runtime non transposables.

## Échelle de qualification

| Classe | Usage dans cette revue |
|---|---|
| `provisional-rexglue` | forme ou invariant public vérifiable, utile seulement après réécriture et validation AC6 |
| `retail-qualified` | résultat lié à un XEX exact par SHA-256 et validé contre l'exécution retail ; ensemble vide |
| `divergent` | succès fabriqué, état hôte, bypass, mutation du guest ou remplacement explicite du flux retail |
| `documented-unmatched` | affirmation ou code présent sans artefact public déterministe qui en établit l'effet |

## Dépôt, tags et releases

Le HEAD public est le commit racine non signé
[`11650aec28bc1d86c221da4992bff3b4b5778ccb`](https://github.com/Boron853/Souls-of-the-Reaper/commit/11650aec28bc1d86c221da4992bff3b4b5778ccb),
arbre `342f3327faac8a4c820d535df8000dccc2d59056`, sur `master`. Il contient 20
blobs, 3 294 620 octets. Aucun gitlink, `.gitmodules`, `.github/`, test,
`LICENSE`, `COPYING`, `NOTICE` ou SBOM n'est suivi. Un scan des noms ne trouve
aucun `.xex`, `.iso`, `.xiso`, `.cpk`, `.exe`, `.dll`, `.lib`, `.zip` ou `.7z`.
Cela exclut les containers évidents de l'arbre source, pas toute matière
dérivée ou tout asset incorporé.

Les quatre tags sont légers. `beta-1.3` est le nouveau commit racine ; il n'est
pas descendant de `beta-1.2`. L'historique courant ne permet donc pas d'auditer
la release 1.3 comme une évolution linéaire de 1.2.

| Tag | Commit / arbre | Asset GitHub (non téléchargé) |
|---|---|---|
| `beta-1.3` | `11650aec28bc1d86c221da4992bff3b4b5778ccb` / `342f3327faac8a4c820d535df8000dccc2d59056` | `SoulsOfTheReaper_Setup.exe`, 23 435 514 octets, SHA-256 `51d902e7fddfab16246403a4fa17998a05f593f3217079cf513fc4c175b0f353` |
| `beta-1.2` | [`0d70981532d593a4cd0d606b979714d06224273c`](https://github.com/Boron853/Souls-of-the-Reaper/commit/0d70981532d593a4cd0d606b979714d06224273c) / `cc6f3a1ef29905810b0293e66d3ae21ac2a0d5c1` | `SotR_Beta1.2.zip`, 22 498 753 octets, SHA-256 `421fd4aeb4b6d1644d197c4633955e2271aa4eca5cb35878702d66f7fa5dd9fd` |
| `beta-1.1` | [`fd433575428edc595d2e901933144c86a72ad906`](https://github.com/Boron853/Souls-of-the-Reaper/commit/fd433575428edc595d2e901933144c86a72ad906) / `228094031b1d11322342d1d18aa1003014872c1d` | `SotR_Beta1.1.zip`, 21 714 464 octets, SHA-256 `fa54de16f062af2cc8ae1c0555e0e1d327a76a975ccc6af374561281733bffe7` |
| `1.0` | [`642765417be7565ffbcbde198566413f41030a29`](https://github.com/Boron853/Souls-of-the-Reaper/commit/642765417be7565ffbcbde198566413f41030a29) / `5692833e51f790ec7456bca96204d510f3856d76` | `SotR_Beta1.0.7z`, 14 073 653 octets, SHA-256 `052a83a00663d776beeb8a15bf3f5df6eeb52715b4393057f5b35e24027a7158` |

Les releases sont
[`beta-1.3`](https://github.com/Boron853/Souls-of-the-Reaper/releases/tag/beta-1.3),
[`beta-1.2`](https://github.com/Boron853/Souls-of-the-Reaper/releases/tag/beta-1.2),
[`beta-1.1`](https://github.com/Boron853/Souls-of-the-Reaper/releases/tag/beta-1.1)
et [`1.0`](https://github.com/Boron853/Souls-of-the-Reaper/releases/tag/1.0).
GitHub fournit leur digest, mais aucune attestation source → asset. Le HEAD et
les deux commits beta intermédiaires sont non signés ; le commit `1.0` final a
une signature GitHub valide.

L'installateur source embarque `diablo3.exe`, `rexruntimerd.dll`, le launcher
et `extract-xiso.exe`
([liste](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/installer/SoulsOfTheReaper.iss#L278-L284)).
Le premier est du code hôte généré depuis un XEX retail inconnu. L'absence de
CPK/XEX brut dans le dépôt ou dans la liste prévue du paquet ne supprime donc
ni la lacune de provenance du binaire généré, ni le risque de redistribution
de code dérivé. Ce constat n'est pas une conclusion juridique.

## ReXGlue, codegen et fermeture de source

### Révision exacte retrouvée

Le manifeste est la meilleure preuve de version : il se dit généré par
`ReXGlue v0.8.1.32-dev.gf22cd9d`, demande `sdk_version = "0.8.1"` et nomme
`../game/default.xex`
([source](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/port/diablo3_manifest.toml#L1-L11)).
Le suffixe correspond exactement au commit upstream non signé
[`f22cd9dc360dda5700358f7452230af24c2c2e69`](https://github.com/rexglue/rexglue-sdk/commit/f22cd9dc360dda5700358f7452230af24c2c2e69),
arbre `4eb143036c7767c70489048542ecf5b8903d1fd1`, tag léger
[`nightly-20260605-f22cd9dc`](https://github.com/rexglue/rexglue-sdk/releases/tag/nightly-20260605-f22cd9dc).

Cette release officielle publie trois ZIPs :

| Plateforme | Octets | SHA-256 GitHub |
|---|---:|---|
| Windows amd64 | 43 975 007 | `1c3effeaec2bb8ad047df8fc62e15ea58a1b2fc2bbefeee99034923b94abf195` |
| Linux amd64 | 149 623 000 | `549cc013914ba4de258a57de1031e8ffb0799aa918e20ccc5a600f4214573277` |
| Linux arm64 | 143 090 884 | `7d9bda81ef447612690288f7ee93330796531b15022c08317da9eecdc2bd20fa` |

Le patch Diablo, SHA-256
`dc6facbbab26e74ff0edfd8fa0607292bc4166d0a0030a277bd7526662c6f375`,
passe `git apply --check` sur `f22cd9dc…`. Le fichier additionnel D3D12 demandé
par le README a pour SHA-256
`79a836c56b61380ac49d9bd6ae6b6f04b355ef499821ec403003aa520ad97c91`
([étape](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/README.md#L30-L40)).

Ce cross-match qualifie le nightly comme **base littérale probable** du runtime
release. Il ne transforme pas l'instruction README « clone ReXGlue » en pin :
le dépôt n'a ni submodule, ni script de fetch, ni digest du ZIP. Le patch échoue
sur `v0.9.0`, le HEAD `main` et le nightly du 12 août 2026.

| Upstream au 12 août 2026 | Commit / arbre | Application du patch Diablo |
|---|---|---|
| nightly déclaré | `f22cd9dc360dda5700358f7452230af24c2c2e69` / `4eb143036c7767c70489048542ecf5b8903d1fd1` | PASS |
| stable `v0.9.0` | [`3eb9b511b4140d2769e27be63eae57d41bfa2afa`](https://github.com/rexglue/rexglue-sdk/commit/3eb9b511b4140d2769e27be63eae57d41bfa2afa) / `a8b23cc4b2ed36ca9ea04a152b14874b43c9ed45` | FAIL |
| `main` | [`cb58065c793429aa92895d778af58d12e9d26d8f`](https://github.com/rexglue/rexglue-sdk/commit/cb58065c793429aa92895d778af58d12e9d26d8f) / `a8b23cc4b2ed36ca9ea04a152b14874b43c9ed45` | FAIL |
| nightly courant | [`df2743b069d0db19f8ecad2688eecb14e23e1565`](https://github.com/rexglue/rexglue-sdk/commit/df2743b069d0db19f8ecad2688eecb14e23e1565) / `d9943ef164eb7f620eb0583a45f8ffad573fd48b` | FAIL |

### Closure cassée

Le patch possède 42 entrées diff, dont un faux changement de gitlink
`305907…` → `305907…-dirty`; une application ordinaire modifie réellement 41
fichiers. Surtout, il inclut
`<rex/input/slot_arbiter.h>` dans les deux drivers
([MnK](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/patches/rexglue-sdk.patch#L1684-L1694),
[SDL](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/patches/rexglue-sdk.patch#L1979-L1989))
et appelle `SlotArbiter::ClaimNextSlot/ReleaseSlot`, sans publier ce header.
Aucune révision upstream vérifiée ne le contient. Le build source décrit est
donc incomplet avant même le code jeu.

Le projet n'inclut pas non plus `port/generated/rexglue.cmake`, pourtant requis
par CMake
([source](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/port/CMakeLists.txt#L1-L23)).
Son absence est attendue avant codegen, mais aucun hash de l'arbre généré ou du
log `cg.log` ne permet de vérifier le résultat. Le script post-codegen remplace
deux fonctions par recherche textuelle ; un fichier ou une ancre absente ne
produit qu'un warning puis un succès de script
([source](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/port/apply_generated_patches.ps1#L38-L104)).

La closure exacte est donc : nightly identifiable + patch identifiable + un
fichier D3D12 identifiable, **moins** `slot_arbiter`, généré, log, SDK binaire
et postconditions. Elle n'est pas reconstructible.

### Sous-modules du nightly

Les 22 gitlinks de
[`f22cd9dc…/.gitmodules`](https://github.com/rexglue/rexglue-sdk/blob/f22cd9dc360dda5700358f7452230af24c2c2e69/.gitmodules)
ont tous été recoupés avec l'endpoint commit de leur remote : 22/22 résolus.

| Dépendance | Pin vérifié |
|---|---|
| FFmpeg | [`0604b464c7cb4ebc94940cf1f324a3b26b87717c`](https://github.com/wmarti/FFmpeg/commit/0604b464c7cb4ebc94940cf1f324a3b26b87717c) |
| Catch2 | [`88abf9bf325c798c33f54f6b9220ef885b267f4f`](https://github.com/catchorg/Catch2/commit/88abf9bf325c798c33f54f6b9220ef885b267f4f) |
| CLI11 | [`bfffd37e1f804ca4fae1caae106935791696b6a9`](https://github.com/CLIUtils/CLI11/commit/bfffd37e1f804ca4fae1caae106935791696b6a9) |
| fmt | [`407c905e45ad75fc29bf0f9bb7c5c2fd3475976f`](https://github.com/fmtlib/fmt/commit/407c905e45ad75fc29bf0f9bb7c5c2fd3475976f) |
| glslang | [`f4f1d8a352ca1908943aea2ad8c54b39b4879080`](https://github.com/KhronosGroup/glslang/commit/f4f1d8a352ca1908943aea2ad8c54b39b4879080) |
| Dear ImGui | [`6d910d5487d11ca567b61c7824b0c78c569d62f0`](https://github.com/ocornut/imgui/commit/6d910d5487d11ca567b61c7824b0c78c569d62f0) |
| inja | [`7d1b4600b68595085a949743331c2e5673f511ea`](https://github.com/pantor/inja/commit/7d1b4600b68595085a949743331c2e5673f511ea) |
| libmspack | [`305907723a4e7ab2018e58040059ffb5e77db837`](https://github.com/kyz/libmspack/commit/305907723a4e7ab2018e58040059ffb5e77db837) |
| o1heap | [`388a73fd9007300e5130c5fe352d9ce3288b6dde`](https://github.com/pavel-kirienko/o1heap/commit/388a73fd9007300e5130c5fe352d9ce3288b6dde) |
| SDL | [`8bf3b7215ad9fc3deb583c6a3a37c6c67f2e24e4`](https://github.com/libsdl-org/SDL/commit/8bf3b7215ad9fc3deb583c6a3a37c6c67f2e24e4) |
| SIMDe | [`71fd833d9666141edcd1d3c109a80e228303d8d7`](https://github.com/simd-everywhere/simde/commit/71fd833d9666141edcd1d3c109a80e228303d8d7) |
| Snappy | [`6af9287fbdb913f0794d0148c6aa43b58e63c8e3`](https://github.com/google/snappy/commit/6af9287fbdb913f0794d0148c6aa43b58e63c8e3) |
| spdlog | [`79524ddd08a4ec981b7fea76afd08ee05f83755d`](https://github.com/gabime/spdlog/commit/79524ddd08a4ec981b7fea76afd08ee05f83755d) |
| SPIR-V Headers | [`04f10f650d514df88b76d25e83db360142c7b174`](https://github.com/KhronosGroup/SPIRV-Headers/commit/04f10f650d514df88b76d25e83db360142c7b174) |
| SPIR-V Tools | [`04d0b166dcd62e29509bf2aac3ca0c5ccdcb6929`](https://github.com/KhronosGroup/SPIRV-Tools/commit/04d0b166dcd62e29509bf2aac3ca0c5ccdcb6929) |
| toml++ | [`30172438cee64926dc41fdd9c11fb3ba5b2ba9de`](https://github.com/marzer/tomlplusplus/commit/30172438cee64926dc41fdd9c11fb3ba5b2ba9de) |
| Tracy | [`05cceee0df3b8d7c6fa87e9638af311dbabc63cb`](https://github.com/wolfpld/tracy/commit/05cceee0df3b8d7c6fa87e9638af311dbabc63cb) |
| utfcpp | [`63d64de49fd6b829f7c8694df5ab2ee625cb7134`](https://github.com/nemtrif/utfcpp/commit/63d64de49fd6b829f7c8694df5ab2ee625cb7134) |
| volk | [`0b17a763ba5643e32da1b2152f8140461b3b7345`](https://github.com/zeux/volk/commit/0b17a763ba5643e32da1b2152f8140461b3b7345) |
| Vulkan Headers | [`49f1a381e2aec33ef32adf4a377b5a39ec016ec4`](https://github.com/KhronosGroup/Vulkan-Headers/commit/49f1a381e2aec33ef32adf4a377b5a39ec016ec4) |
| Vulkan Memory Allocator | [`1d8f600fd424278486eade7ed3e877c99f0846b1`](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/commit/1d8f600fd424278486eade7ed3e877c99f0846b1) |
| xxHash | [`e626a72bc2321cd320e953a0ccf1584cad60f363`](https://github.com/Cyan4973/xxHash/commit/e626a72bc2321cd320e953a0ccf1584cad60f363) |

Le port ne pinne **aucun** XenonRecomp, XenonAnalyse, XenosRecomp ou
`rexdex/recompiler`. L'upstream ReXGlue les crédite comme inspirations, sans
gitlink ni révision
([README SDK](https://github.com/rexglue/rexglue-sdk/blob/f22cd9dc360dda5700358f7452230af24c2c2e69/README.md#L17-L23)).
Le renderer est le chemin Xenia/ReXGlue, pas XenosRecomp.

## Identité retail et codegen

Le dépôt ne publie aucun SHA-256, Media ID, version XEX, timestamp PE, région ou
certificat. Le titre ID `394F07D4` apparaît seulement dans le chemin de save
([README](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/README.md#L17-L27)).
L'installateur cherche récursivement le premier `Common.cpk` et le premier
`Default.xex`; l'absence de XEX n'est pas fatale avant publication de la config
([source](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/installer/SoulsOfTheReaper.iss#L699-L775)).

Le seul rapport utilisateur public de démarrage indique que l'édition de base
crashait et que l'**Ultimate Evil Edition** démarrait
([issue #1](https://github.com/Boron853/Souls-of-the-Reaper/issues/1)). Ce
rapport qualifie au mieux le choix d'édition, pas un build retail : région,
révision et bytes restent inconnus. Le README demande par ailleurs
`Default_decrypted.exe`, alors que le manifeste et l'installateur utilisent
`default.xex`/`Default.xex`.

Le fichier d'overrides contient **439** adresses, dont trois tailles forcées.
Ses commentaires déclarent des cibles indirectes et continuations découvertes
au runtime
([début](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/port/diablo3_overrides.toml#L1-L18)),
mais aucun dump de bytes, frontière Ghidra, log de fonction manquante ou C++
généré ne permet de les recouper. Elles sont `documented-unmatched` et aucune
adresse ne doit entrer dans la carte AC6.

Le post-codegen remplace aussi un couple `setjmp/longjmp` déclaré mal traduit
par une table `thread_local` hôte et un save manuel des GPR/FPR
([source](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/port/apply_generated_patches.ps1#L106-L189)).
Il ne publie ni bytes de l'instruction VMX dite non supportée, ni conservation
des registres VMX/CR/FPSCR attendus, ni test de longjmp imbriqué ou inter-thread.
Ce remplacement est `divergent`, pas une correction Xenon transférable.

## Shell hors ligne, import et paquetage

Le shell est Windows/WebView2/PowerShell et l'installation Inno Setup demande
une ISO utilisateur. La séparation launcher → config → processus jeu est une
forme `provisional-rexglue`, mais l'import n'est pas une transaction qualifiée :

- validation par noms `Common.cpk`/`Default.xex`, sans identité ni hash ;
- choix du premier match récursif, sans manifeste de fichiers attendu ;
- suppression de l'ancien dossier avant validation finale ;
- poursuite possible sans XEX ;
- aucun hash post-copie, marqueur versionné ou rollback ;
- sélection de langue répétée dans plusieurs tables et contradictoire entre
  scan disque, langue installer et liste fixe du launcher.

L'ancien `setup.ps1` illustre les mêmes limites : il copie tous les CPK voisins,
le premier XEX trouvé et écrit un simple timestamp `.setup_done`
([source](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/setup.ps1#L175-L265)).
Il sait aussi copier un starter save au XUID fixe si un payload local absent du
dépôt existe
([source](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/setup.ps1#L267-L287)).

La fermeture package est également incomplète :

- release `beta-1.3`, mais `AppVersion = 1.1.0` dans Inno
  ([source](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/installer/SoulsOfTheReaper.iss#L25-L47))
  et launcher compilé en `1.0.0.0`
  ([source](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/launcher/build_launcher.ps1#L43-L54)) ;
- ps2exe, WebView2, Inno Setup, `extract-xiso`, LLVM, Visual Studio et WinSDK
  non pinnés ; plusieurs binaires/icônes sont explicitement exclus
  ([README](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/README.md#L56-L65)) ;
- aucune release Linux ; le README et `build.ps1` sont Windows-only
  ([build](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/port/build.ps1#L1-L30)) ;
- les presets Linux/ARM64 existent
  ([source](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/port/CMakePresets.json#L23-L54)),
  mais aucune commande, CI, package ou validation port ne les exerce.

Linux est `documented-unmatched`. AC6 ne doit reprendre ni ce détecteur de
contenu ni ses marqueurs ; son import v2 avec SHA-256, écritures atomiques et
manifestes bornés reste supérieur.

## Save, campagne, XAM et restart

### Cache et contenu

Le port remplace `\Device\Harddisk0` par un périphérique synthétique. Il sert
`Josh` seulement pour une lecture commençant exactement à `0x800`, annonce un
volume de 1 Gio et accepte puis jette toutes les écritures
([source](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/port/src/cache_device.h#L28-L80)).
Deux hooks forcent ensuite `IoDismountVolume` à réussir et toute signature
console à être valide
([source](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/port/src/kernel_overrides.cpp#L1-L28)).
Ce sont des bypass `divergent`, même si la séparation cache jetable/save
persistant est un invariant produit raisonnable.

Pour un profil vierge, le patch transforme un `OPEN_EXISTING` manquant de type
`kSavedGame` en création de container, écrit un header, le ferme puis le rouvre
([source](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/patches/rexglue-sdk.patch#L2207-L2329)).
Le commit beta 1.2 affirme qu'un profil frais atteint Hero Creation et que les
saves multi-héros ne régressent pas. Aucun save synthétique, log, test de
corruption, crash/power-loss, conflit, quota ou écriture atomique n'est publié.
Surtout, `OPEN_EXISTING` n'a plus sa sémantique API. L'observation « prévoir le
container au bon stade du shell » est `provisional-rexglue`; l'auto-création
dans l'API est `divergent`.

L'énumérateur agrégé remappe un XUID tronqué 32 bits vers le profil complet et
le dispatcher rend le vrai compte/terminateur
([source](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/patches/rexglue-sdk.patch#L2159-L2398)).
Cela suggère deux tests AC6 utiles — extension explicite du XUID et terminaison
exacte d'énumérateur — mais le type, les bytes et la séquence Diablo ne sont pas
retail-qualifiés.

### Identités locales

Les joueurs 2–4 sont auto-signés comme `Guest2..4` et reçoivent les XUIDs
constants `B13EBABEBABEBABF`, `…BAC0`, `…BAC1`. `XamShowSigninUI` active le
prochain slot, fabrique les notifications UI/sign-in et partage les réglages de
profil de P1
([source](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/patches/rexglue-sdk.patch#L2796-L3161)).
Le mapping n'est pas dérivé du profil courant, ne modélise ni sélection,
sign-out, collision, reprise de session ou contrôle parental. Il est
`divergent`; seul l'invariant « identité et namespace save distincts par joueur
normalisé » est conservable.

### Restart masqué

Le deuxième patch généré remplace la routine déclarée « return to title
screen » par `RequestClose`, avec `exit(0)` en fallback
([guest patch](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/port/apply_generated_patches.ps1#L191-L218),
[hook hôte](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/patches/rexglue-sdk.patch#L2648-L2677)).
Le flux chaud session → titre → nouvelle session n'est donc pas validé ; la
sortie du processus réinitialise opportunément invités, arbiter et statiques.
Le projet ne prouve qu'un cold launch allégué, pas un restart campagne.

Décision AC6 : tester séparément fresh profile, reprise checkpoint, retour
titre dans le même processus et relance froide. Ne jamais substituer la sortie
processus au restart retail.

## Tâches XAM et cycle de vie

Le patch crée les tâches `XamTaskSchedule` suspendues, marque leur thread comme
fire-and-forget puis le reprend. À l'exit, il libère stack, scratch, TLS, KPCR,
fiber et parfois l'objet natif ; le commentaire allègue environ 64 tâches/s et
un OOM en quelques minutes
([schedule](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/patches/rexglue-sdk.patch#L2730-L2776),
[teardown](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/patches/rexglue-sdk.patch#L3296-L3417)).

L'invariant est utile : une tâche sans handle guest doit avoir un propriétaire
et une complétion explicites, et son fiber doit être détruit par son thread
propriétaire. L'implémentation modifie cependant `XThread::Exit` pour tous les
threads, sans test de wait/APC/exit status, terminate concurrent ou
use-after-free. Elle reste `provisional-rexglue` comme scénario de stress,
`documented-unmatched` comme correctif.

## Entrée, replay et cadence

Le patch veut attribuer un slot au premier bouton/clavier réellement pressé,
plutôt qu'à l'ordre d'énumération USB
([source](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/patches/rexglue-sdk.patch#L1979-L2158)).
La politique est raisonnable pour un shell local, mais son arbiter manque. Les
XUIDs restent attachés au numéro de slot, le clavier ne publie pas de chemin de
release et aucun replay ne vérifie reconnect, focus, simultanéité ou quatre
joueurs. La claim « local co-op up to 4 » reste `documented-unmatched`.

Le driver MnK incrémente `packet_number` à chaque poll, même sans transition,
et sa méthode `EnqueueKeystroke` n'a aucun appelant dans l'arbre patché. Le port
ajoute en parallèle une injection debug par fichier : lecture toutes les 30 ms
selon `steady_clock`, état tenu ou arêtes synthétiques, chemin auteur par défaut
`D:\Diablo3_ReXGlue\port\input_inject.txt`
([source](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/patches/rexglue-sdk.patch#L2435-L2647)).
Les scripts censés la piloter sont ignorés
([ignore](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/.gitignore#L39-L64)).
Ce n'est ni un format replay, ni un échantillonnage déterministe.

La cadence est limitée par un sleep/spin au `XE_SWAP` et mesurée par une
fenêtre hôte d'environ cinq secondes
([source](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/patches/rexglue-sdk.patch#L414-L562)).
Le commentaire appelle 30 fps la cible console, tandis que le README appelle
ROV 60 fps « faithful » et le launcher choisit RTV 120 fps par défaut. Il n'y a
ni timestamp vblank/present, ni corrélation input/audio, ni capture de variance.
La métrique host est `provisional-rexglue`; toute cadence retail alléguée est
`documented-unmatched`.

AC6 conserve son replay au seam XAM et doit enregistrer pour chaque frame :
index logique, résultat XAM exact, timestamp guest, soumission GPU, PRESENT et
callback audio. Un sleep hôte ne vaut pas preuve de cadence console.

## XMA, audio et vidéo

Le patch Diablo ne touche aucun fichier audio, XMA, FFmpeg ou vidéo. Le
nightly ReXGlue contient un décodeur générique XMA/FFmpeg avec MMIO, contexts et
worker
([setup](https://github.com/rexglue/rexglue-sdk/blob/f22cd9dc360dda5700358f7452230af24c2c2e69/src/audio/xma_decoder.cpp#L89-L165),
[décodage](https://github.com/rexglue/rexglue-sdk/blob/f22cd9dc360dda5700358f7452230af24c2c2e69/src/audio/xma_context.cpp#L455-L511))
et une sortie SDL
([source](https://github.com/rexglue/rexglue-sdk/blob/f22cd9dc360dda5700358f7452230af24c2c2e69/src/audio/sdl/sdl_audio_driver.cpp#L38-L101)).
La présence du code et du gitlink FFmpeg ne prouve pas son chemin Diablo.

Aucun contexte, paquet, PCM, cue, ring depth, underrun, latence ou A/V sync
n'est publié. Les exports extra AV codecs sont encore des stubs
([source](https://github.com/rexglue/rexglue-sdk/blob/f22cd9dc360dda5700358f7452230af24c2c2e69/src/kernel/xam/xam_video.cpp#L25-L44)),
et le mécanisme d'injection dit explicitement servir à **sauter** la cinématique
d'intro. Audio/XMA/vidéo sont `documented-unmatched`.

Décision AC6 : aucune modification de la lane XMA. Conserver l'A/B
`SDL_AUDIODRIVER=dummy` uniquement pour headless, puis exiger hashes PCM,
timeline cues et capture vidéo qualifiée pour toute preuve M01.

## Renderer ReXGlue/Xenos

Le README appelle ROV « faithful » et RTV « fully playable » jusqu'à 240 fps
([source](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/README.md#L114-L121)).
Il n'existe pourtant aucune capture avant/après, frame hash, shader hash,
fixture PM4, RenderDoc publié, comparaison Xenia/console ou test renderer.

Le patch est une modification profonde du cache de render targets ReXGlue. Le
mode 8 maintient deux cartes d'ownership couleur/profondeur, des séquences de
claim, mémorise un writer de clear et transfère au resolve les tiles considérées
plus récentes
([contrat](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/patches/rexglue-sdk.patch#L26-L153),
[implémentation](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/patches/rexglue-sdk.patch#L967-L1632)).
Le second cvar exécute certains vertex shaders unclipped sur CPU pour réduire
l'étendue EDRAM réclamée. C'est une hypothèse de backend titre, pas une
sémantique Xenos démontrée.

La forme testable est néanmoins utile à AC6 :

1. fixture synthétique `color draw → depth clear aliasé → color resolve` ;
2. assertion que seuls les tiles réellement écrits changent de propriétaire ;
3. timeline monotone des claims et provenance du dernier writer ;
4. cas négatifs format/pitch/MSAA/layout incompatibles ;
5. comparaison du résultat au chemin EDRAM canonique avant toute optimisation.

Cette fixture est `provisional-rexglue`. Le mode 8, ROV « fidèle », RTV et la
performance 240 fps restent `documented-unmatched`; aucun code ne doit être
copié dans le renderer AC6.

Le patch embarque aussi des triggers production non bornés à un build debug :
screenshot, logs RT, dump jusqu'à 64 Mio et chargement d'un fichier arbitraire
dans la mémoire physique guest via des fichiers `.req` sous un chemin auteur
fixe
([source](https://github.com/Boron853/Souls-of-the-Reaper/blob/11650aec28bc1d86c221da4992bff3b4b5778ccb/patches/rexglue-sdk.patch#L579-L727)).
Le chemin `memload` modifie l'état observé et ne valide pas explicitement la
plage adresse+taille. Il est `divergent` et impropre à un package ou une preuve
déterministe.

## Tests, CI et licences

| Surface | Résultat |
|---|---|
| tests du port | aucun fichier/target/test vector |
| CI du port | aucune `.github/`, aucun check publié |
| preuve campagne | messages de commits seulement ; aucun log/save/replay |
| preuve renderer/audio | aucune capture ou hash publié |
| SDK nightly | 17 sources unitaires et 166 programmes PPC asm |
| exécution tests SDK | `REXGLUE_BUILD_TESTS=OFF` par défaut ; les workflows build/install ne l'activent pas et ne lancent pas CTest |
| patch Diablo | aucun test ajouté aux suites SDK |

Les options SDK sont visibles dans
[`CMakeLists.txt`](https://github.com/rexglue/rexglue-sdk/blob/f22cd9dc360dda5700358f7452230af24c2c2e69/CMakeLists.txt#L13-L20)
et l'activation conditionnelle dans
[`CMakeLists.txt`](https://github.com/rexglue/rexglue-sdk/blob/f22cd9dc360dda5700358f7452230af24c2c2e69/CMakeLists.txt#L187-L218).
Le workflow commun ne fait que configure/build/install
([source](https://github.com/rexglue/rexglue-sdk/blob/f22cd9dc360dda5700358f7452230af24c2c2e69/.github/workflows/_build-platform.yaml#L81-L93)).

ReXGlue contient un texte BSD-3-Clause
([licence](https://github.com/rexglue/rexglue-sdk/blob/f22cd9dc360dda5700358f7452230af24c2c2e69/LICENSE)).
Le dépôt Diablo n'a aucune licence racine et l'installateur ne copie aucune
notice, alors qu'il redistribue le runtime, FFmpeg/SDL et d'autres dépendances,
WebView2, ps2exe, `extract-xiso` et du code jeu généré. Les images base64 du
launcher n'ont pas non plus de provenance/licence publiée. Risque résiduel de
conformité élevé ; aucune conclusion juridique n'est formulée.

## Classement AC6

| Mécanisme | Classe | Décision AC6 |
|---|---|---|
| manifeste codegen avec version `gf22cd9d` | `provisional-rexglue` | garder la forme, ajouter SHA-256 XEX/config/généré |
| claims playable/stable/4 joueurs | `documented-unmatched` | aucune promotion sans replay/captures/tests |
| 439 starts/overrides Diablo | `documented-unmatched` | ne transférer aucune adresse ou frontière |
| remplacement setjmp/longjmp | `divergent` | corriger l'instruction/codegen et tester ABI Xenon |
| cache synthétique, writes jetés, signature forcée | `divergent` | séparer cache/save sans succès fictif |
| précréation du save | invariant `provisional-rexglue`, implémentation `divergent` | provision explicite au shell, pas dans `OPEN_EXISTING` |
| XUID tronqué et fin d'énumérateur | `provisional-rexglue` | ajouter fixtures ABI/terminaison AC6 |
| invités/XUIDs/sign-in fabriqués | `divergent` | state machine locale explicite, reset et namespaces testés |
| tâches XAM fire-and-forget | `provisional-rexglue` | stress test ownership/fiber/APC avant implémentation |
| exit à la place du retour titre | `divergent` | conserver un vrai warm restart |
| first-input slot assignment | `provisional-rexglue` | normaliser avant XAM et rejouer disconnect/reconnect |
| injection fichier entrée | `divergent` | format replay frame-indexé AC6 seulement |
| limiteur sleep/spin et histogramme hôte | `provisional-rexglue` | instrumentation multi-clock, aucune preuve retail |
| XMA/vidéo SDK générique | `documented-unmatched` | garder les gates PCM/cues/vidéo AC6 |
| alias EDRAM mode 8 | fixture `provisional-rexglue`, implémentation `documented-unmatched` | test synthétique, pas de copie renderer |
| dump/load mémoire physique en package | `divergent` | interdire hors harness borné et authentifié |
| presets Linux | `documented-unmatched` | exiger build/test/package Linux réel |
| source et licences de release | `documented-unmatched` | provenance/SBOM/notices avant publication |

## Actions AC6 vérifiables

1. Étendre le manifeste de toute recompilation comparative avec XEX SHA-256,
   projet Ghidra, module, Media ID/version, commit+tree outil, config digest,
   nombre de fonctions et hash de l'arbre généré.
2. Ajouter une gate de closure : checkout propre, tous gitlinks résolus, aucun
   include absent, patch post-codegen fail-closed, puis rebuild à deux reprises
   avec mêmes hashes.
3. Créer une matrice save M01 : profil vierge, save existant, save corrompu,
   quota/écriture interrompue, warm restart et cold restart. Vérifier namespace,
   disposition, compte d'énumérateur et commit atomique.
4. Tester XAM utilisateur comme state machine : attribution de slot, sign-in,
   notification équilibrée, disconnect/reconnect, sign-out, quatre pads et
   reset au retour titre, sans XUID ou privilège fabriqué.
5. Ajouter un stress test borné des tâches XAM : allocations guest/host,
   handles, fiber owner, event/APC et exit status stables sur au moins le débit
   maximal observé M01.
6. Conserver l'entrée au seam XAM : replay frame-indexé des retours exacts,
   packet number seulement sur changement, focus et hotplug inclus. Ne pas
   utiliser le polling d'un fichier host comme preuve.
7. Corréler cadence guest, input, soumission, PRESENT et audio ; publier
   percentiles et hash de timeline. Un limiteur hôte reste un mécanisme QoL.
8. Ajouter la fixture EDRAM aliasée décrite ci-dessus à XenosRecomp/native, avec
   un oracle canonique et des cas négatifs, avant toute hypothèse M01.
9. Garder XMA/vidéo comme frontière explicite jusqu'à captures PCM/cues/A-V
   qualifiées sur AC6 PAL.
10. Exiger pour le package natif AC6 un build Linux exécuté, une closure de
    dépendances, SBOM/notices, versions cohérentes et attestation source → asset.

## Validations de l'audit

- HEAD/branche/arbre, commits, tags légers, releases, assets et signatures
  recoupés via Git et l'API GitHub le 12 août 2026 ;
- arbre Diablo : 20 blobs, 3 294 620 octets, zéro gitlink et zéro extension
  retail/binaire évidente ;
- patch : `git apply --check` PASS sur `f22cd9dc…`, FAIL sur `3eb9b511…`,
  `cb58065c…` et `df2743b…` ;
- closure statique : `slot_arbiter.h` absent du port et de toutes les révisions
  SDK testées ; `generated/rexglue.cmake` absent ;
- overrides : 439 entrées, trois tailles forcées ;
- SDK nightly : commit/arbre/release vérifiés, 22/22 pins de submodule résolus ;
- suites SDK : 17 fichiers unitaires, 166 asm PPC, aucune exécution CI ;
- catalogue architecture local `architecture-reference-corpus/v1`, récupéré le
  17 juillet 2026 : hashes `xenia-xenos`, `xenonrecomp-README` et
  `xenosrecomp-README` conformes au catalogue. Il confirme la nature des
  frontières Xenon/Xenos, jamais une sémantique Diablo/AC6.

## Risques résiduels

Le principal risque est l'absence d'artefact exécuté ou reproductible reliant
les affirmations à un XEX exact. Le contenu réel de l'installateur n'a pas été
inspecté ; son digest ne prouve ni sa composition, ni sa correspondance au
HEAD. Sans bytes retail, généré, logs privés, captures RenderDoc et scripts de
navigation ignorés, il est impossible de distinguer correctif stable,
contournement d'un écran précis et régression masquée par relance processus.

Aucune session VNC/controller n'est demandée : elle ne fermerait pas la
frontière statique actuelle. Le prochain artefact nécessaire serait un bundle
public borné et sans retail contenant identité XEX, versions/outils, logs de
validation, replay normalisé, hashes de frames/PCM et résultats fresh-save /
warm-restart.
