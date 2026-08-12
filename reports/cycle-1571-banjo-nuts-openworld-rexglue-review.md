# Cycle 1571 — reNut/Banjo-Kazooie: Nuts & Bolts : monde, renderer et runtime

Date de qualification : **2026-08-12 22:49 CEST**. Cible de réutilisation :
AC6 PAL, Mission 01 uniquement. Aucun XEX, ISO, asset, dump de texture ou
binaire de release n'a été téléchargé ou exécuté. L'audit porte sur les arbres
Git publics, leurs métadonnées GitHub et le SDK public.

## Décision

Le seul projet public substantiel identifié est
[`masterspike52/reNut`](https://github.com/masterspike52/reNut). Il est utile à
AC6 comme **catalogue de frontières et de contre-exemples**, pas comme preuve
de fidélité :

- sa cible est uniquement le XEX US sans mise à jour, mais aucun SHA-256, Media
  ID, numéro de version, base d'image ou reçu de codegen ne scelle ce XEX ;
- `main`, la branche Linux publiée et `dev` ne scellent pas la même version du
  SDK et ne forment pas une chaîne de build propre reproductible ;
- le projet désactive ou contourne explicitement plusieurs effets Xenos,
  autorise des fetch constants invalides, altère cadence et input, et contient
  47 273 lignes de C++ PPC traduit à la main/recopié dans un « fix » ;
- aucun test, workflow de build ou contrôle de parité n'existe dans reNut ;
- aucune licence n'est fournie pour le code reNut. Aucun code ne doit donc être
  copié dans le produit AC6, indépendamment de sa qualité technique.

Trois résultats sont néanmoins directement exploitables après requalification
PAL :

1. un **probe post-normalisation** au seul bloc pad consommé par le jeu, en
   parallèle de l'injection brute au seam XAM, pour contrôler un replay
   synchronisé sans y injecter les valeurs de parité ;
2. un **corpus synthétique de fetch textures** couvrant espace virtuel/physique,
   endian, tiling, pitch 256 octets, base non résidente et mip tail, inspiré de
   la branche `dev` mais réécrit et testé ;
3. des tests de non-régression Linux pour ouverture read+write des sauvegardes
   et concurrence unregister/callback audio.

Il n'existe aucune sémantique `retail-qualified`, aucune lane M01 n'est fermée
et la jouabilité annoncée n'est utilisée nulle part comme preuve.

## Recherche et identités figées

Les recherches GitHub, moteurs publics, forks et historique par « Banjo-Kazooie
Nuts & Bolts », `reNut`, ReXGlue et recompilation ne trouvent qu'un projet
technique original. Les quatre forks publics observés ne constituent pas des
ports indépendants : trois sont derrière l'upstream et le plus récent pointe
sur un ancien HEAD sans commit propre. Le dépôt N64 `BanjoRecomp` est hors
périmètre Xbox 360.

Le HEAD de `main` a avancé durant l'audit ; les conclusions ci-dessous sont
figées sur la dernière révision observée, pas sur un nom de branche mobile.

| Arbre | Révision qualifiée | État utile |
|---|---|---|
| `main` | [`3b509f2fa97f0a1c7b49cbef5191bc69a714d2e4`](https://github.com/masterspike52/reNut/commit/3b509f2fa97f0a1c7b49cbef5191bc69a714d2e4), arbre `c16b38371e02bd43a3ba171c2c7f4d505680e144`, 202 commits, 36 fichiers, 3 492 705 octets | branche par défaut ; les deux derniers commits ne modifient que les instructions README Windows |
| `dev` | [`887aef79499df877486fe0576e63760077f6672f`](https://github.com/masterspike52/reNut/commit/887aef79499df877486fe0576e63760077f6672f), arbre `f6c92750f5174234066dbcd7e25d14bb52e816bd`, 205 commits, 44 fichiers | input MnK, carte GPU complète, dumper/replacer textures |
| `renut_linux` | [`e73ca783f2ea269e65c6b30dba52634cb0c86233`](https://github.com/masterspike52/reNut/commit/e73ca783f2ea269e65c6b30dba52634cb0c86233), arbre `7d335a4041b2be496a792b3ed1d22184921b4db1` | commit racine orphelin unique, snapshot Linux et packaging |
| `Linux` historique | [`b5224598afe6ff88c7e14c5135f1f6c99b848e20`](https://github.com/masterspike52/reNut/commit/b5224598afe6ff88c7e14c5135f1f6c99b848e20) | ancien bring-up SteamOS, dépassé par le snapshot |
| `netplay` | [`54f8dc9f957c486b27e3aeef4947a3d414e1d38a`](https://github.com/masterspike52/reNut/commit/54f8dc9f957c486b27e3aeef4947a3d414e1d38a) | aucune preuve de replay déterministe ; hors scope M01 |

Le README scelle seulement « US version, sans update »
([lignes 49–60](https://github.com/masterspike52/reNut/blob/3b509f2fa97f0a1c7b49cbef5191bc69a714d2e4/README.md#L49-L60)).
Le manifeste demande `assets/default.xex`, sans identité
([manifeste](https://github.com/masterspike52/reNut/blob/3b509f2fa97f0a1c7b49cbef5191bc69a714d2e4/renut_manifest.toml#L1-L10)).
Les 100 adresses de `main`, les 32 332 adresses de `dev`/Linux, les 6 971
adresses GPU et les 36 à 40 hooks ne sont donc reproductibles que contre un
XEX privé non qualifié. Leurs noms ne constituent pas une sémantique.

Empreintes utiles pour détecter une dérive future :

- `main/renut_config.toml` :
  `dd02259a8334f4101041401ad2a8f7714d0454dd2bb7b141aa1464b00f6ff417` ;
- `main/renut_manifest.toml` :
  `bdc2ca73d3296db5d02e1ab7205b370eecb69060f7f4b63477971cfd48bfef89` ;
- `main/README.md` :
  `5a705776e96940977eae590e626cb2a3079c27127fddb3124db07ef49a21ae16` ;
- `dev/src/renut_engine/texture_tools.cpp` :
  `8e403a380fe7ba3adddc0c3424ed1d8527ffc3c0f8ad6079f5de8cfaa1c7bbbf` ;
- `renut_linux/src/renut_engine/mnk_controls.cpp` :
  `9e81b4ca4049700498480b2966f43d11ca2210b8c472029dae8ea82f692bf2d4`.

## Dépendances et provenance exactes

### reNut n'a aucun gitlink

Le dépôt n'embarque ni ReXGlue, ni XenonRecomp, ni XenosRecomp, ni le
recompiler de rexdex. `main` versionne un bootstrap qui demande ReXGlue
**0.7.4 ou compatible**, tandis que son manifeste annonce **0.8.0**
([bootstrap, lignes 5–25](https://github.com/masterspike52/reNut/blob/3b509f2fa97f0a1c7b49cbef5191bc69a714d2e4/generated/rexglue.cmake#L5-L25),
[manifeste](https://github.com/masterspike52/reNut/blob/3b509f2fa97f0a1c7b49cbef5191bc69a714d2e4/renut_manifest.toml#L1-L4)).
`dev` annonce 0.9.0, mais son README demande encore le `dev` mobile de
`SolarRecomps/rexglue-ostentation`, alors que son historique dit être revenu au
SDK officiel. Il n'existe donc aucun pin SDK exact sur `main` ou `dev`.

Le snapshot `renut_linux` est le seul à pinner une source : tag
`nightly-20260809-f5c85215` →
[`f5c85215174c9dcd67b4c77227a979c4fc33197a`](https://github.com/rexglue/rexglue-sdk/commit/f5c85215174c9dcd67b4c77227a979c4fc33197a),
arbre `12076e884ca6b7974d5f4ddc4aa7b7876ce5ec1`, version publiée
`0.9.0.19-dev.gf5c8521`
([Dockerfile, lignes 90–103](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/packaging/container/Dockerfile#L90-L103)).
Le même arbre clone toutefois un `extract-xiso` précompilé depuis la branche
mobile `master`, sans SHA
([lignes 105–139](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/packaging/container/Dockerfile#L105-L139)).

### Lignée technique réellement vérifiable

Le SDK `f5c85215` est BSD-3-Clause et déclare explicitement être dérivé de
Xenia, inspiré de XenonRecomp et du recompiler rexdex
([présentation, lignes 17–24](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/README.md#L17-L24),
[crédits, lignes 52–60](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/README.md#L52-L60),
[licence](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/LICENSE)).
Des headers identifient plus précisément des wrappers et motifs SIMD issus de
XenonRecomp/UnleashedRecomp. **XenosRecomp n'est référencé nulle part dans cet
arbre**. Il n'existe donc aucune dépendance ou lignée XenosRecomp directement
qualifiable pour reNut.

Les vingt gitlinks du SDK exact sont :

```text
thirdparty/FFmpeg                  0604b464c7cb4ebc94940cf1f324a3b26b87717c
thirdparty/catch2                  88abf9bf325c798c33f54f6b9220ef885b267f4f
thirdparty/cli11                   bfffd37e1f804ca4fae1caae106935791696b6a9
thirdparty/fmt                     407c905e45ad75fc29bf0f9bb7c5c2fd3475976f
thirdparty/glslang                 f4f1d8a352ca1908943aea2ad8c54b39b4879080
thirdparty/imgui                   6d910d5487d11ca567b61c7824b0c78c569d62f0
thirdparty/inja                    7d1b4600b68595085a949743331c2e5673f511ea
thirdparty/libmspack               305907723a4e7ab2018e58040059ffb5e77db837
thirdparty/o1heap                  388a73fd9007300e5130c5fe352d9ce3288b6dde
thirdparty/sdl3                    8bf3b7215ad9fc3deb583c6a3a37c6c67f2e24e4
thirdparty/simde                   71fd833d9666141edcd1d3c109a80e228303d8d7
thirdparty/spdlog                  79524ddd08a4ec981b7fea76afd08ee05f83755d
thirdparty/spirv-headers           04f10f650d514df88b76d25e83db360142c7b174
thirdparty/spirv-tools             04d0b166dcd62e29509bf2aac3ca0c5ccdcb6929
thirdparty/tomlplusplus            30172438cee64926dc41fdd9c11fb3ba5b2ba9de
thirdparty/tracy                   05cceee0df3b8d7c6fa87e9638af311dbabc63cb
thirdparty/utfcpp                  63d64de49fd6b829f7c8694df5ab2ee625cb7134
thirdparty/volk                    0b17a763ba5643e32da1b2152f8140461b3b7345
thirdparty/vulkan-headers          49f1a381e2aec33ef32adf4a377b5a39ec016ec4
thirdparty/vulkan-memory-allocator 1d8f600fd424278486eade7ed3e877c99f0846b1
```

Les URLs correspondantes sont scellées dans
[`.gitmodules`](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/.gitmodules).
Le fork FFmpeg `wmarti/FFmpeg` est la seule source XMA/ASF explicitement
versionnée par ce port ; aucune mesure audio reNut ne qualifie ce commit.

### Licences et code dérivé

L'API GitHub et l'arbre reNut ne contiennent aucun `LICENSE`, `COPYING` ou
`NOTICE`. Le header SIMD/macros dit avoir été « yoinked » d'un autre port
([source](https://github.com/masterspike52/reNut/blob/3b509f2fa97f0a1c7b49cbef5191bc69a714d2e4/src/renut_engine/rex_macros.h#L1-L9))
et `Timer.h` cite seulement un gist. Le fichier
[`mullhwucrash.cpp`](https://github.com/masterspike52/reNut/blob/3b509f2fa97f0a1c7b49cbef5191bc69a714d2e4/src/renut_engine/fixes/mullhwucrash.cpp#L1-L35)
contient 47 273 lignes et 18 fonctions PPC brutes, sans en-tête de licence ; son
historique l'associe à des contournements `vsldoi128` puis aux animations.

Le tracked tree ne contient aucun XEX, ISO, XMA, vidéo ou conteneur retail ;
`assets/` et le codegen `generated/` sont ignorés. Cela ne rend pas le gros C++
PPC redistribuable ni acceptable pour AC6 : il est précisément du code dérivé
généré/manualisé que la preview doit refuser. Les binaires de release n'ont pas
été téléchargés, donc leur absence de bytes retail ou de notices n'est **pas**
affirmée.

## Build, CI et releases

reNut n'a aucun workflow de build/test versionné, aucun `add_test`, aucun test
source et aucun check sur le HEAD. GitHub expose seulement le reviewer Copilot
dynamique. Le SDK exact contient 258 `TEST_CASE` Catch2 dans 19 fichiers, mais
les tests sont désactivés par défaut
([option, lignes 13–18](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/CMakeLists.txt#L13-L18),
[activation, lignes 216–220](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/CMakeLists.txt#L216-L220))
et le workflow nightly configure puis installe sans les activer ni lancer
CTest
([workflow, lignes 70–93](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/.github/workflows/_build-platform.yaml#L70-L93)).
Une release SDK n'est donc pas un reçu de tests.

Un clean checkout reNut ne produit pas le jeu sans le XEX, ce qui est attendu,
mais la chaîne est en plus incohérente :

- `main` inclut du code Windows (`windows.h`, waitable timer) dans la liste
  commune des sources
  ([CMake](https://github.com/masterspike52/reNut/blob/3b509f2fa97f0a1c7b49cbef5191bc69a714d2e4/CMakeLists.txt#L13-L31),
  [frame limiter](https://github.com/masterspike52/reNut/blob/3b509f2fa97f0a1c7b49cbef5191bc69a714d2e4/src/renut_engine/frameHooks.cpp#L1-L15)) ;
- le CMake du snapshot Linux demande `include(generated/rexglue.cmake)`, mais ce
  fichier est absent de son arbre orphelin et ignoré par Git ; une source externe est nécessaire
  avant même la configuration ;
- le snapshot Linux pinne un SDK mais pas `extract-xiso`, et aucun digest
  d'image de build ou attestation ne lie le paquet final au snapshot ;
- le README actuel dit que Linux ne fonctionne pas sur AMD et recommande
  NVIDIA
  ([lignes 80–103](https://github.com/masterspike52/reNut/blob/3b509f2fa97f0a1c7b49cbef5191bc69a714d2e4/README.md#L80-L103)).

Les releases `finally` et `mnk` sont deux tags légers pointant tous deux sur
[`d33da27cc0207fca01a4924d95083e692bebf7bb`](https://github.com/masterspike52/reNut/commit/d33da27cc0207fca01a4924d95083e692bebf7bb),
antérieur au snapshot Linux. Les deux ZIP Windows ont le même digest
`b9fe172af5bc7d609e0bf4bb7be8ed12c9cf9f7edef623316444d7a087a2f6f0`,
mais les AppImage associés ont respectivement :

- `finally` :
  `810d22304fa275d5d4a30578799070a2dfab9061f4c65c74634951cf274a3407` ;
- `mnk`, asset remplacé le 12 août :
  `358babce5c306cabb3048e1db418808a4cf8e122ca1a30a43354d9ceae73f4b6`.

Les métadonnées sont publiques
([`finally`](https://github.com/masterspike52/reNut/releases/tag/finally),
[`mnk`](https://github.com/masterspike52/reNut/releases/tag/mnk)), mais aucun
workflow, SBOM, provenance SLSA, signature ou commit source distinct n'explique
les deux AppImage. Ces releases prouvent seulement qu'un artefact a été
téléversé.

## Taxonomie obligatoire

| Classe | Éléments reNut admis | Portée AC6 |
|---|---|---|
| `retail-qualified` | **aucun** | aucun XEX scellé, aucune capture oracle, aucun contrôle positif, aucun tick/cue qualifié |
| `provisional-rexglue` | structure de fetch et de mémoire du SDK `f5c85215`, seam XAM brut, layout pad observé par un hook de jeu, bug POSIX reproduit par lecture de code, race audio visible dans le SDK exact | source d'hypothèses et de tests synthétiques seulement |
| `divergent` | désactivation glow/shadows/CAO/MSAA/motion blur/particules, fetch invalides, memexport off, cadence hôte, 60 fps, injection float après deadzone, délai audio de grâce, interposition VFS, CRT HLE, remplacement texture | interdit pour fermer une lane ou alimenter le profil retail |
| `documented-unmatched` | noms `LoadShowdownTown`, `supportGameLoadNewScene`, `gameSaveSave`, XACT/ASF, commentaires d'adresses, tickets de crash/jouabilité, mapping GPU US | à recroiser sur bytes PAL AC6, projet Ghidra canonique, contrôle positif |

## Monde ouvert, streaming et transitions

La carte courte de `main` nomme `appScenePush`, `assetDbBundleLoadBundle`,
`assetDbUnifiedStorageInitPlayer`, `LoadShowdownTown`,
`supportGameLoadNewScene` et `gameSaveSave`
([lignes 51–108](https://github.com/masterspike52/reNut/blob/3b509f2fa97f0a1c7b49cbef5191bc69a714d2e4/config/renut_funcs.toml#L51-L108)).
La branche Linux contient des milliers de noms supplémentaires de streaming
XACT/ASF et de scènes, mais aucun hook de streaming monde, journal de lifetime,
test de transition, dump de graph ou reçu d'exécution. Ce sont des labels
`documented-unmatched`.

Les incidents publics renforcent la frontière sans fournir de solution : crash
à l'entrée de missions
([#16](https://github.com/masterspike52/reNut/issues/16),
[#19](https://github.com/masterspike52/reNut/issues/19)), softlock dépendant du
framerate
([#21](https://github.com/masterspike52/reNut/issues/21)) et sauvegarde Linux
initialement illisible
([#26](https://github.com/masterspike52/reNut/issues/26)). Leur fermeture ne
correspond pas à un test ou un commit de résolution systématique.

**Action AC6 M01 :** ne pas importer ces adresses. Ajouter à la trace M01 une
suite minimale `scene_id`, demande asset, completion asset, activation acteur,
transition et compteur retail, tous liés au tick. Une transition ne passe que
si la même séquence est observée par replay instrumenté et par le produit
natif ; les délais hôte et messages « jouable » restent exclus.

## Xenos, EDRAM, shaders et textures

### Frontières de divergence explicites

Le code de `main` décrit correctement plusieurs risques, mais les contourne :

- color et depth doivent partager le sample count ; le hook force les deux à
  non-MSAA tout en conservant dimensions et predicated tiling
  ([hooks, lignes 130–146](https://github.com/masterspike52/reNut/blob/3b509f2fa97f0a1c7b49cbef5191bc69a714d2e4/src/renut_engine/hooks.cpp#L130-L146)) ;
- un scratch surface à base EDRAM zéro aliaserait le framebuffer hôte, donc le
  glow est sauté par défaut
  ([lignes 155–173](https://github.com/masterspike52/reNut/blob/3b509f2fa97f0a1c7b49cbef5191bc69a714d2e4/src/renut_engine/hooks.cpp#L155-L173)) ;
- l'async shader compiler saute des draws tant que le pipeline n'est pas prêt ;
  le code propose une compilation synchrone
  ([frame hooks, lignes 166–194](https://github.com/masterspike52/reNut/blob/3b509f2fa97f0a1c7b49cbef5191bc69a714d2e4/src/renut_engine/frameHooks.cpp#L166-L194)).

Pourtant le README actuel recommande simultanément `sync_shader_compile=false`,
`gpu_allow_invalid_fetch_constants=true`, `native_2x_msaa=false`, les deux
readbacks memexport à `false`, et la désactivation des ombres, CAO, MSAA et
motion blur
([configuration, lignes 87–103](https://github.com/masterspike52/reNut/blob/3b509f2fa97f0a1c7b49cbef5191bc69a714d2e4/README.md#L87-L103)).
Le rendu affiché ne peut donc pas être présumé retail.

### Dumper/replacer de `dev`

Le meilleur artefact statique est le dumper texture de `dev`, branché à
`D3DDevice_SetTexture`. Il documente :

- les sept dwords du `D3DBaseTexture`, puis les six dwords du fetch ; la copie
  invitée est big-endian ; les objets sont virtuels, mais `base_address` et
  `mip_address` sont physiques
  ([lignes 1–40](https://github.com/masterspike52/reNut/blob/887aef79499df877486fe0576e63760077f6672f/src/renut_engine/texture_tools.cpp#L1-L40)) ;
- le miroir physique `0xE0000000` avec décalage d'une page de 4 KiB
  ([lignes 140–163](https://github.com/masterspike52/reNut/blob/887aef79499df877486fe0576e63760077f6672f/src/renut_engine/texture_tools.cpp#L140-L163)) ;
- DXT1, DXT3, DXT5/BC3, DXN/BC5, DXT5A et quelques formats entiers
  ([lignes 167–220](https://github.com/masterspike52/reNut/blob/887aef79499df877486fe0576e63760077f6672f/src/renut_engine/texture_tools.cpp#L167-L220)) ;
- untiling par `GetTiledOffset2D`, puis swaps `8in16`, `8in32` et `16in32`
  ([lignes 391–468](https://github.com/masterspike52/reNut/blob/887aef79499df877486fe0576e63760077f6672f/src/renut_engine/texture_tools.cpp#L391-L468)) ;
- mips, packed tail et base haute résolution non résidente
  ([lignes 870–928](https://github.com/masterspike52/reNut/blob/887aef79499df877486fe0576e63760077f6672f/src/renut_engine/texture_tools.cpp#L870-L928)).

Les limites sont aussi nettes : seulement 2D non-stacked, cubemaps et volumes
rejetés, formats inconnus sautés, détection de résidence par 64 probes épars et
trois binds stables, aucune image de contrôle, aucun test, aucun fetch brut
publié. Le remplacement réalloue une texture linéaire et réécrit son fetch
([lignes 750–844](https://github.com/masterspike52/reNut/blob/887aef79499df877486fe0576e63760077f6672f/src/renut_engine/texture_tools.cpp#L750-L844)) : c'est un outil de diagnostic
`provisional-rexglue`, puis une exécution `divergent`, pas une preuve de layout
retail.

**Actions AC6 M01 :**

1. créer des fixtures synthétiques BC3/Xenos couvrant les trois endian modes,
   pitch 256, tiled/linear, dimensions non multiples de 4, mips séparés et mip
   tail ; comparer hash et pixels à un décodeur indépendant ;
2. rejeter explicitement cubemap, stacked, volume et format inconnu tant que M01
   ne les a pas qualifiés ; ne jamais les traiter comme 2D ;
3. sur un fetch M01 PAL positivement identifié, enregistrer les six dwords,
   dimensions, offsets/mips et un hash de sortie borné, puis seulement ajouter
   un contrôle image positif. Ne copier ni le code reNut ni ses adresses US.

## Input, replay synchronisé et cadence

La branche Linux met au jour un seam intéressant. Après le scan
`XInputGetState`, elle peut créer un pad fantôme, faire réussir le poll, puis
écrire dans le bloc de 0x28 octets que le jeu s'apprête à committer : pressed,
released, held, raw, quatre sticks float et deux triggers
([layout, lignes 503–555](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/src/renut_engine/mnk_controls.cpp#L503-L555),
[hooks, lignes 750–855](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/src/renut_engine/mnk_controls.cpp#L750-L855)).
Elle recalcule les edges contre le frame précédent
([lignes 888–914](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/src/renut_engine/mnk_controls.cpp#L888-L914))
et traite séparément la queue `XInputGetKeystroke` des menus
([lignes 970–1015](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/src/renut_engine/mnk_controls.cpp#L970-L1015)).

Ce seam **ne doit pas remplacer** l'injection AC6 au wrapper XAM : le code dit
lui-même qu'il contourne XInput, la quantification 16 bits, deadzone et courbe
du jeu. Les répétitions menu reposent sur 400/110 ms de temps hôte et la caméra
transporte un reliquat souris entre frames. Tout cela est `divergent` pour un
replay retail.

Le SDK exact confirme que `XamInputGetState` ne fait que choisir l'utilisateur
et transmettre l'état du driver
([XAM, lignes 94–116](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/src/kernel/xam/xam_input.cpp#L94-L116)).
Le driver SDL incrémente le packet number au plus une fois par appel même si
plusieurs updates hôte sont arrivées
([SDL, lignes 194–223](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/src/input/sdl/sdl_input_driver.cpp#L194-L223)),
et la fusion de périphériques choisit les axes de plus grande magnitude
([merge, lignes 21–54](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/src/input/state_merge.cpp#L21-L54)).
Ces politiques ReXGlue ne sont pas des sémantiques retail Xbox.

**Action AC6 M01 :** conserver le replay brut et l'attestation cadence actuels,
puis ajouter un probe **lecture seule** immédiatement après le producteur PAL
normalisé correspondant au pump `0x821CA908`. Pour chaque tick : digest brut
injecté, compteur d'appels XAM, packet number, bloc normalisé, pressed/released
et premier tick divergent. Une égalité end-to-end sur une séquence contenant
axes, deadzone, triggers et edges peut remplacer plusieurs micro-exécutions de
transformation ; elle ne remplace pas la qualification des opcodes, exceptions
ou races hors de ce chemin.

La cadence reNut n'est pas transposable : `main` modifie l'intervalle guest et
dort au retour du draw selon `steady_clock`
([lignes 63–164](https://github.com/masterspike52/reNut/blob/3b509f2fa97f0a1c7b49cbef5191bc69a714d2e4/src/renut_engine/frameHooks.cpp#L63-L164)).
Le ticket #21 documente même un softlock au-dessus de 30 fps. AC6 doit garder la
simulation 60 Hz et dissocier strictement présentation et tick.

## XMA/ASF, audio et synchronisation

reNut ne contient aucun décodeur XMA/ASF spécifique ni test de cue. Les noms
XACT, `AudioCueInstance::Tick`, streaming wave bank et ASF de la carte de
fonctions ne qualifient aucune voix, vidéo ou synchro. La seule implémentation
bornée est celle du SDK ReXGlue/Xenia avec le fork FFmpeg au gitlink
`0604b464...` ; son succès supposé en jeu n'est pas une mesure.

Le snapshot Linux révèle néanmoins une race exacte du runtime `f5c85215` : le
worker copie le callback sous lock, libère le lock, puis l'exécute ; un
unregister concurrent peut détruire le driver avant `SubmitFrame`
([dispatch, lignes 95–155](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/src/audio/audio_system.cpp#L95-L155),
[lifecycle, lignes 251–283](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/src/audio/audio_system.cpp#L251-L283)).
reNut l'interpose avec un `shared_mutex`, une quiet period hôte de 30 ms, un
maximum de 250 ms et un drop de secours
([constantes, lignes 119–149](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/src/renut_engine/linuxfixes/audio_client_guard.cpp#L119-L149),
[interposition, lignes 181–249](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/src/renut_engine/linuxfixes/audio_client_guard.cpp#L181-L249)).
Il force aussi PulseAudio par défaut pour éviter un PipeWire muet
([backend](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/src/renut_engine/linuxfixes/audio_backend.cpp#L1-L31)).

Ces deux patches sont `divergent` et dépendent du temps hôte. Pour AC6, le
résultat durable est un stress-test : unregister pendant callback, aucun UAF,
aucun deadlock, ownership/génération explicite du client, et aucun délai magique.
La lane XMA/ASF reste ouverte jusqu'aux voix EN/JP, vidéo, cues ±20 ms et niveau
audio qualifiés sur M01.

## VFS, sauvegarde et Linux/Vulkan

Le snapshot Linux identifie un bug POSIX reproductible dans le SDK exact :
`O_RDONLY`, `O_WRONLY` et `O_RDWR` sont traités comme des bits combinables, ce
qui ouvre un save read+write en write-only ; `pread` échoue ensuite
([SDK, lignes 141–200](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/src/core/filesystem_posix.cpp#L141-L200)).
L'interposition reNut choisit un seul `O_ACCMODE` et remet le nombre d'octets à
zéro sur erreur
([fix, lignes 69–148](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/src/renut_engine/linuxfixes/posix_file_access.cpp#L69-L148)).
C'est une bonne garde générique, pas une preuve du format de sauvegarde AC6.

La même branche route config et logs vers XDG et migre les anciennes copies
([contrat, lignes 17–34](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/src/renut_engine/linuxfixes/xdg_paths.h#L17-L34),
[implémentation, lignes 62–124](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/src/renut_engine/linuxfixes/xdg_paths.h#L62-L124)).
Elle écrit toutefois les configs avec `std::ios::trunc`, sans fichier temporaire,
fsync ni rename atomique. AC6 doit conserver son cache/import et ses settings
atomiques, tester corruption/interruption et ne pas reprendre cette écriture.

Le SDK apporte Vulkan/XCB/SDL, mais reNut n'a ni validation Vulkan, ni CI AMD,
NVIDIA, X11 ou Wayland, ni recréation de swapchain testée. Le README exclut AMD
au moment de l'audit. Il n'apporte donc aucune qualification Linux/Vulkan pour
la preview AC6.

## VMX128, HLE et micro-exécutions

Le gros correctif PPC reNut confirme un risque déjà vu dans MCLA : des crashes
ont été attribués à l'absence de `vsldoi128`, puis des milliers de lignes VMX ont
été substituées pour corriger des animations. Sans XEX hash, oracle positif ni
test d'instruction, ce corpus ne peut pas être recroisé avec AC6.

La branche `dev` contient en revanche un avertissement très utile : inclure le
fichier CRT HLE supprime le PPC original aux adresses mappées et constitue un
changement de comportement majeur ; le groupe wide-char est laissé désactivé
car le helper natif lit le `int16_t` invité big-endian sans swap
([config, lignes 5–20](https://github.com/masterspike52/reNut/blob/887aef79499df877486fe0576e63760077f6672f/renut_config.toml#L5-L20),
[CRT, lignes 50–69](https://github.com/masterspike52/reNut/blob/887aef79499df877486fe0576e63760077f6672f/config/renut_crt.toml#L50-L69)).

Conséquence AC6 : la confiance `provisional-rexglue` peut accélérer
l'intégration, mais chaque substitution HLE/SIMD doit rester groupée par
sémantique, avec bytes PAL, contrôle positif et garde endian/ABI. Le replay
end-to-end peut réduire les micro-exécutions sur le chemin effectivement
couvert ; reNut montre précisément pourquoi il ne peut pas les supprimer pour
VMX128, CRT, atomiques ou timing.

## Actions AC6 M01 ordonnées

1. **Replay :** instrumenter en lecture seule le bloc pad normalisé du pump PAL,
   comparer au replay brut XAM sur 3 600 ticks, et produire le premier point de
   divergence structuré. Aucun temps hôte, keystroke repeat ou injection float
   post-deadzone dans la gate.
2. **Textures :** ajouter un corpus synthétique BC3/Xenos endian+tiled+mips et
   un rejet explicite cube/stacked/inconnu ; n'activer une branche supplémentaire
   qu'après recensement positif d'un fetch M01.
3. **Renderer :** créer deux tests de non-régression ciblés : alias EDRAM base
   zéro sans écraser le color target vivant, et pipeline non prêt qui ne peut
   silencieusement supprimer un `DrawPacket` retail.
4. **VFS/save :** tester read+write POSIX, erreur avec compteur d'octets zéro,
   write temporaire + fsync + rename, migration/corruption XDG. Aucun PAC relu
   après import.
5. **Audio :** stress-test lifecycle client/callback/unregister et backend dummy
   headless ; garder l'A/V M01 et les cues dans la clock de simulation, jamais
   dans une quiet period murale.
6. **Provenance :** ajouter reNut à la matrice publique avec `retail-qualified=0`,
   conserver les trois commits exacts et refuser tout code, adresse ou capture
   sans licence/XEX PAL.

## Validations de l'audit

- recherche web et GitHub, branches, quatre forks, 17 tags, 12 releases, 17
  issues et 16 PR inventoriés ;
- HEAD `main` revalidé après son avance en cours d'audit ; arbres, tailles,
  cartes de fonctions et hashes de fichiers recalculés ;
- aucun asset ou binaire de jeu/release téléchargé ou exécuté ;
- absence de gitlink, licence, test et workflow reNut vérifiée sur les trois
  arbres ;
- tag ReXGlue, arbre, version, 20 gitlinks, licence, tests et workflow nightly
  vérifiés à `f5c85215` ;
- permaliens source commit-pinnés contrôlés ;
- contrôle des espaces finaux par `rg` et `git diff --no-index --check` : aucun
  diagnostic (le rapport reste volontairement non indexé pour le parent).

## Risques résiduels

- le dépôt a avancé pendant l'audit et peut encore changer ; les SHA ci-dessus
  sont la frontière, jamais les noms de branches ;
- les AppImage n'ont pas été inspectés, conformément au périmètre : contenu,
  licences embarquées et lien réel au snapshot Linux restent inconnus ;
- aucune ISO Banjo n'était disponible ni nécessaire ; l'identité du XEX reNut
  et toutes ses adresses restent non qualifiées ;
- les commentaires détaillés de `dev` peuvent être justes, mais sans tests ni
  captures positives ils restent `provisional-rexglue` ou
  `documented-unmatched` ;
- **0 lane AC6 fermée** par cet audit.
