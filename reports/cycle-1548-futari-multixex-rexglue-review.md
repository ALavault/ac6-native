# Cycle 1548 — Futari : multi-XEX et ReXGlue, bornés à AC6 Mission 01

Audit réalisé le 12 août 2026. Aucun contenu retail Futari ou AC6 n'a été
copié, ouvert ou exécuté pour cette revue.

## Décision pour M01

Futari apporte une bonne étude de cas publique du **hot-swap de plusieurs XEX
chargés à la même adresse invitée**. Il ne fournit pas une implémentation à
transplanter dans le produit AC6. Les seuls apports à retenir sont :

1. publier un module seulement après validation complète de son identité, de
   son image et de ses points d'entrée ;
2. attribuer fonctions et thunks au module appelant, puis tout retirer avant de
   réutiliser la même plage invitée ;
3. tester le cycle de vie avec des images synthétiques sans code retail.

La conséquence immédiate est de **ne pas écrire maintenant un chargeur
multi-XEX générique pour AC6**. Le corpus PAL qualifié et le cache natif
inventorient actuellement `default.xex`, `DATA.TBL` et les deux PAC ; aucune
preuve durable ne montre encore que le cône M01 atteint `XexLoadImage`. Un
census statique des imports/appels, puis un reçu du replay s'il existe un appel,
doivent précéder toute tranche C++.

Futari n'ajoute **aucun fait `retail-qualified`** à AC6. Ses affirmations de
boot, de menus et de gameplay qualifient son port ReXGlue, pas le comportement
du retail Xbox 360. Elles restent `provisional-rexglue`. Les stubs, no-op,
heuristiques titre et comportements dépendants du temps hôte sont
`divergent`.

Cela ne change pas les choix déjà faits pour M01 : replay poll-exact à la
frontière XAM, cache retail v2 atomique, décodage média borné et `DrawPacket`
typés soumis au renderer Vulkan manuscrit. Le command processor Xenos de
Futari reste un oracle provisoire, jamais une dépendance du produit.

## Provenance vérifiée

Le checkout d'audit était détaché dans `/tmp`, sans submodules initialisés :

| Élément | Pin vérifié | Observation |
|---|---|---|
| dépôt | [`springah/futari_recomp`](https://github.com/springah/futari_recomp) | origin public |
| `HEAD` | [`b5281bcaa9c9fdc0cdaabdf14a189814c615297b`](https://github.com/springah/futari_recomp/commit/b5281bcaa9c9fdc0cdaabdf14a189814c615297b) | `git ls-remote origin HEAD` identique ; commit du 11 août 2026 |
| tag `v1.5.2` | objet `d90fe19968c8304bb61de5ae3ac76209c8aa6c2b`, commit [`dd34bb79bbdfd9c4c9603a391236c293a4d4bd26`](https://github.com/springah/futari_recomp/commit/dd34bb79bbdfd9c4c9603a391236c293a4d4bd26) | `HEAD` est le restaging documentaire post-tag |
| vendoring SDK | [`3d6ffe373e7ef9ee0012b5df0c3f805768b17a5d`](https://github.com/springah/futari_recomp/commit/3d6ffe373e7ef9ee0012b5df0c3f805768b17a5d) | remplace l'ancien submodule par 1 283 fichiers SDK |
| origine SDK déclarée | fork `springah/rexglue-sdk`, tag `v0.9.3.1`, commit `040d5bca3de2f2970e9e29b4bf789739a48f5301` | déclaré dans [`VENDORED.md`](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/VENDORED.md#L1-L14) |
| ReXGlue upstream observé | [`cb58065c793429aa92895d778af58d12e9d26d8f`](https://github.com/rexglue/rexglue-sdk/commit/cb58065c793429aa92895d778af58d12e9d26d8f) | `HEAD` public au jour de l'audit |

Le fork déclaré est aujourd'hui privé ou supprimé : `git ls-remote` échoue et
l'API GitHub répond 404. Le pin `040d5b…` n'est pas récupérable dans l'upstream
public. Le commit de vendoring affirme que tous les blobs correspondent au tag,
sauf les binutils omis, le fichier de version et l'enregistrement des
submodules ; cette affirmation est une preuve de mainteneur, pas une
reproduction indépendante possible aujourd'hui. Depuis ce vendoring, le SDK
embarqué a encore changé sur 12 fichiers, notamment le mécanisme de chunks et
la sauvegarde des réglages. L'identité utile est donc **l'arbre embarqué au
commit Futari**, pas le nom `v0.9.3.1`.

Les 22 gitlinks et leurs URLs viennent de [`.gitmodules`](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/.gitmodules).
Chaque endpoint primaire GitHub `repos/{owner}/{repo}/commits/{pin}` a répondu
200 pendant l'audit :

| Dépendance | Commit | URL |
|---|---|---|
| FFmpeg | `0604b464c7cb4ebc94940cf1f324a3b26b87717c` | [`wmarti/FFmpeg`](https://github.com/wmarti/FFmpeg/commit/0604b464c7cb4ebc94940cf1f324a3b26b87717c), branche déclarée `xenia-ffmpeg-canary-full` |
| Catch2 | `88abf9bf325c798c33f54f6b9220ef885b267f4f` | [`catchorg/Catch2`](https://github.com/catchorg/Catch2/commit/88abf9bf325c798c33f54f6b9220ef885b267f4f) |
| CLI11 | `bfffd37e1f804ca4fae1caae106935791696b6a9` | [`CLIUtils/CLI11`](https://github.com/CLIUtils/CLI11/commit/bfffd37e1f804ca4fae1caae106935791696b6a9) |
| fmt | `407c905e45ad75fc29bf0f9bb7c5c2fd3475976f` | [`fmtlib/fmt`](https://github.com/fmtlib/fmt/commit/407c905e45ad75fc29bf0f9bb7c5c2fd3475976f) |
| glslang | `f4f1d8a352ca1908943aea2ad8c54b39b4879080` | [`KhronosGroup/glslang`](https://github.com/KhronosGroup/glslang/commit/f4f1d8a352ca1908943aea2ad8c54b39b4879080) |
| Dear ImGui | `6d910d5487d11ca567b61c7824b0c78c569d62f0` | [`ocornut/imgui`](https://github.com/ocornut/imgui/commit/6d910d5487d11ca567b61c7824b0c78c569d62f0) |
| inja | `7d1b4600b68595085a949743331c2e5673f511ea` | [`pantor/inja`](https://github.com/pantor/inja/commit/7d1b4600b68595085a949743331c2e5673f511ea) |
| libmspack | `305907723a4e7ab2018e58040059ffb5e77db837` | [`kyz/libmspack`](https://github.com/kyz/libmspack/commit/305907723a4e7ab2018e58040059ffb5e77db837) |
| o1heap | `388a73fd9007300e5130c5fe352d9ce3288b6dde` | [`pavel-kirienko/o1heap`](https://github.com/pavel-kirienko/o1heap/commit/388a73fd9007300e5130c5fe352d9ce3288b6dde) |
| SDL3 | `8bf3b7215ad9fc3deb583c6a3a37c6c67f2e24e4` | [`libsdl-org/SDL`](https://github.com/libsdl-org/SDL/commit/8bf3b7215ad9fc3deb583c6a3a37c6c67f2e24e4), branche déclarée `release-3.4.x` |
| SIMDe | `71fd833d9666141edcd1d3c109a80e228303d8d7` | [`simd-everywhere/simde`](https://github.com/simd-everywhere/simde/commit/71fd833d9666141edcd1d3c109a80e228303d8d7) |
| Snappy | `6af9287fbdb913f0794d0148c6aa43b58e63c8e3` | [`google/snappy`](https://github.com/google/snappy/commit/6af9287fbdb913f0794d0148c6aa43b58e63c8e3) |
| spdlog | `79524ddd08a4ec981b7fea76afd08ee05f83755d` | [`gabime/spdlog`](https://github.com/gabime/spdlog/commit/79524ddd08a4ec981b7fea76afd08ee05f83755d) |
| SPIR-V Headers | `04f10f650d514df88b76d25e83db360142c7b174` | [`KhronosGroup/SPIRV-Headers`](https://github.com/KhronosGroup/SPIRV-Headers/commit/04f10f650d514df88b76d25e83db360142c7b174) |
| SPIR-V Tools | `04d0b166dcd62e29509bf2aac3ca0c5ccdcb6929` | [`KhronosGroup/SPIRV-Tools`](https://github.com/KhronosGroup/SPIRV-Tools/commit/04d0b166dcd62e29509bf2aac3ca0c5ccdcb6929) |
| toml++ | `30172438cee64926dc41fdd9c11fb3ba5b2ba9de` | [`marzer/tomlplusplus`](https://github.com/marzer/tomlplusplus/commit/30172438cee64926dc41fdd9c11fb3ba5b2ba9de) |
| Tracy | `05cceee0df3b8d7c6fa87e9638af311dbabc63cb` | [`wolfpld/tracy`](https://github.com/wolfpld/tracy/commit/05cceee0df3b8d7c6fa87e9638af311dbabc63cb) |
| utfcpp | `63d64de49fd6b829f7c8694df5ab2ee625cb7134` | [`nemtrif/utfcpp`](https://github.com/nemtrif/utfcpp/commit/63d64de49fd6b829f7c8694df5ab2ee625cb7134) |
| volk | `0b17a763ba5643e32da1b2152f8140461b3b7345` | [`zeux/volk`](https://github.com/zeux/volk/commit/0b17a763ba5643e32da1b2152f8140461b3b7345) |
| Vulkan Headers | `49f1a381e2aec33ef32adf4a377b5a39ec016ec4` | [`KhronosGroup/Vulkan-Headers`](https://github.com/KhronosGroup/Vulkan-Headers/commit/49f1a381e2aec33ef32adf4a377b5a39ec016ec4) |
| Vulkan Memory Allocator | `1d8f600fd424278486eade7ed3e877c99f0846b1` | [`GPUOpen/VMA`](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/commit/1d8f600fd424278486eade7ed3e877c99f0846b1) |
| xxHash | `e626a72bc2321cd320e953a0ccf1584cad60f363` | [`Cyan4973/xxHash`](https://github.com/Cyan4973/xxHash/commit/e626a72bc2321cd320e953a0ccf1584cad60f363) |

## Échelle de qualification

- `retail-qualified` : prouvé contre les bytes PAL AC6 qualifiés et leur
  contrôle positif/exécuté. **Aucun apport Futari n'atteint ce niveau.**
- `provisional-rexglue` : comportement implémenté et inspectable dans ce pin,
  utile à l'oracle ou comme patron de test, sans présumer le retail.
- `divergent` : stub, heuristique titre, comportement temps hôte, omission ou
  choix incompatible avec le produit natif AC6.

## Architecture multi-XEX réelle

Le shell est `default.xex`. Huit modules de mode sont déclarés :
`reco_normal`, `reco_abnormal`, `palm_normal`, `palm_abnormal`,
`hirow_normal`, `hirow_abnormal`, `kiniro_normal` et `kiniro_abnormal`.
Le manifest consigne leurs chemins `game:\media\Module\*.xex`, une base commune
`0x88000000` et l'entrée observée `0x88070588` ; ce sont des déclarations de
projet, pas des bytes publics à recontrôler
([manifest](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/futari_manifest.toml#L13-L62)).

```text
shell -> XexLoadImage(chemin invité)
      -> charge le XEX possédé par l'utilisateur en mémoire invitée
      -> trouve le descripteur host par chemin
      -> charge libfutari_<mode>.so / futari_<mode>.dll
      -> vérifie symboles + base/taille
      -> enregistre fonctions/thunks
      -> DllMain(PROCESS_ATTACH)
      -> exécution
      -> DllMain(PROCESS_DETACH) -> unregister -> plage réutilisable
```

### Génération isolée : bon diagnostic, mauvais contrat produit

Les huit images se recouvrent. L'analyseur ReXGlue corrompait l'analyse croisée
lorsqu'elles partageaient un manifest. `regen.sh` lance donc huit codegens à un
seul module, puis fabrique manuellement un registre et huit bibliothèques
partagées
([source](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/scripts/regen.sh#L7-L12),
[assemblage](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/scripts/regen.sh#L49-L81)).

Le script de hints compare XenonRecomp au set ReXGlue, injecte tous les
candidats, élimine ceux qui ne décodent aucun bloc, puis exige zéro stub et zéro
unresolved. Il a produit 345 entrées, réparties
46/46/45/44/42/42/40/40
([méthode](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/scripts/build_module_hints.sh#L1-L25),
[script](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/scripts/build_module_hints.sh#L81-L122)).

Classification : `provisional-rexglue` comme **générateur de candidats** et
`divergent` comme preuve de frontière ou de sémantique. « Un bloc se décode »
n'établit ni l'ABI ni le comportement. Le faux pointeur `0x82081778`, finalement
reclassé comme chunk intérieur, démontre précisément le risque de ce scan
([historique](https://github.com/springah/futari_recomp/commit/bebb3536301ee112ad1bf7f7bde9ddf73363f911)).
Pour AC6, un diff de cartes peut grouper le travail statique et éviter les
micro-exécutions une par une, mais chaque frontière reste soumise à Ghidra,
bytes PAL, contrôle positif et test natif.

### Chargement et publication

`LoadUserModule` :

- résout un nom relatif sous le répertoire du XEX principal ;
- sérialise un chemin en cours de chargement ;
- charge le XEX avant la bibliothèque hôte ;
- exige `ReXModule_Register` et `ReXModule_GetImageInfo` ;
- compare exactement `image_base` et `image_size` entre XEX et bibliothèque ;
- initialise la table, enregistre les fonctions et ne publie le module qu'après
  succès de ce câblage hôte
  ([source](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/system/kernel_state.cpp#L697-L815)).

Le câblage avant publication et le rejet des plages chevauchantes sont
`provisional-rexglue` et réutilisables comme invariants. La publication précède
cependant encore `DllMain(PROCESS_ATTACH)` ; le module est retiré si l'attach
retourne `FALSE`, mais il peut être observable pendant cet intervalle. Cette
partie reste `divergent` : une vraie publication en deux phases doit attendre
l'attach réussi. L'ensemble est aussi insuffisant pour AC6 : aucune SHA-256
XEX, version d'ABI hôte, identité du code, digest des imports/TLS ou
correspondance `code_base/code_size` avec les sections XEX n'est scellée. Le
chargement POSIX tente d'abord `dlopen` sur un nom nu avant le chemin à côté de
l'exécutable ; une oracle qualifiée devrait résoudre un chemin absolu sous un
arbre scellé
([source](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/core/dynlib_posix.cpp#L29-L55)).

Le second appel concurrent du même chemin ne patiente pas : il reçoit `nullptr`.
Hors thread invité, `DllMain(PROCESS_ATTACH)` est simplement sauté. Un retour
`FALSE` sur un attach exécuté déclenche bien un rollback
([source](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/system/kernel_state.cpp#L708-L732),
[attach](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/system/kernel_state.cpp#L816-L835)).
Les deux premiers comportements sont `divergent`; le rollback est
`provisional-rexglue`.

### Déchargement, mêmes adresses et thunks

Le dispatcher refuse les plages image/code recouvrantes, crée un pool de thunks
par module appelant, mémorise un thunk par fonction, puis retire fonctions,
thunks et table mémoire au déchargement. La même plage peut ensuite être
réinitialisée
([plages](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/system/function_dispatcher.cpp#L193-L243),
[thunks et retrait](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/system/function_dispatcher.cpp#L296-L416)).
Les tests synthétiques couvrent deux modules, le refus d'un appelant inconnu et
le retrait/réemploi d'une même plage
([tests](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/tests/unit/system/function_dispatcher_test.cpp#L37-L180)).
Ces invariants sont `provisional-rexglue` et constituent le meilleur apport du
dépôt.

À l'unload, le runtime invalide aussi les caches de thunks noyau. Il conserve en
revanche chaque bibliothèque hôte déchargée dans
`deferred_unload_libraries_` jusqu'à la destruction du `KernelState`, pour ne
pas exécuter du code déjà `dlclose`
([source](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/system/kernel_state.cpp#L838-L890),
[drain final](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/system/kernel_state.cpp#L163-L220)).
C'est une mitigation UAF `provisional-rexglue`, mais une sémantique de reload
`divergent` : statiques et handles hôte s'accumulent jusqu'à l'arrêt.

`XexLoadImage` incrémente le `load_count` sous verrou et le dernier
`XexUnloadImage` déclenche le retrait. Aucun garde explicite ne précède
`--load_count`; l'underflow doit être testé et refusé, pas reproduit
([source](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/kernel/xboxkrnl/xboxkrnl_modules.cpp#L86-L149)).

Enfin, le registre rejette un `guest_path` dupliqué mais ajoute au lookup un
fallback Futari par basename, insensible à la casse. Il suppose les noms de
fichier uniques afin de réparer une divergence entre chemin demandé et chemin
stocké
([source](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/system/kernel_state.cpp#L892-L945)).
Ce fallback est `divergent` et interdit pour AC6 : deux répertoires peuvent
porter le même basename. L'identité canonicalisée doit être stockée directement
sur l'instance chargée.

## Imports, exports et TLS

Les imports de fonctions noyau sont résolus au codegen. Au runtime, seules les
variables importées sont patchées ; une variable connue mais non implémentée
reçoit une valeur sentinelle `0xD000BEEF…`
([source](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/system/xex_module.cpp#L1059-L1120)).
Une fonction dont l'ordinal ne se résout pas est générée sous un nom de repli et
reste une erreur de codegen
([source](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/codegen/phase_register.cpp#L449-L507)).

`XexGetProcedureAddress` accepte nom ou ordinal. Pour un export utilisateur
recompilé, l'ordinal retourne un thunk dans le pool du module appelant ; un nom
retourne l'adresse invitée brute
([source](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/kernel/xboxkrnl/xboxkrnl_modules.cpp#L151-L198),
[module utilisateur](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/system/user_module.cpp#L239-L255)).
Le dépôt ne démontre pas un linker générique d'imports entre XEX utilisateur ;
le shell charge et appelle ses modes dynamiquement. Toute généralisation est
`divergent`. Pour AC6, un import inconnu doit échouer avec son module, ordinal,
callsite et phase ; jamais continuer avec une sentinelle.

Le TLS statique provient exclusivement du **module exécutable** lors de
l'initialisation du processus et d'un thread
([processus](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/system/kernel_state.cpp#L588-L625),
[thread](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/system/xthread.cpp#L346-L380)).
Il n'existe ni TLS statique par DLL ni `DLL_THREAD_ATTACH/DETACH`. Le succès des
huit modes ne qualifie donc pas un XEX secondaire AC6 portant du TLS. Ce cas est
`divergent` et doit faire échouer le census tant qu'il n'est pas porté.

## VFS, cache et contenu

Le runtime monte le répertoire de jeu en lecture seule sous
`\Device\Harddisk0\Partition1`, avec alias `game:` et `d:`, et un update séparé
en lecture seule. Cette séparation est `provisional-rexglue`
([source](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/system/runtime.cpp#L292-L335)).

Il monte ensuite un `NullDevice` pour `Partition0`, `Cache0` et `Cache1` qui
réussit lectures et écritures sans octet transféré, tout en laissant `cache:`
absent
([montage](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/system/runtime.cpp#L337-L353),
[no-op](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/filesystem/devices/null_file.cpp#L25-L47)).
Ces succès fictifs sont `divergent`. Le cache v2 AC6, borné, content-addressed,
écrit puis synchronisé avant publication, est déjà plus fort et ne doit pas
être remplacé.

Le lecteur GoD Python vérifie la chaîne SHA-1 SVOD avant extraction, rejette les
noms XDVDFS contenant séparateurs, `:` ou `..`, et détecte les lectures courtes
([hash tree](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/scripts/extract_iso.py#L70-L179),
[noms et tailles](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/scripts/extract_iso.py#L360-L410)).
Le principe « vérifier puis publier » est `provisional-rexglue`. Le code n'est
pas à reprendre : `--no-verify` existe, le header est lu sans borne, les layouts
inhabituels continuent bien que non testés, il n'y a ni quotas profondeur/nombre
d'entrées, ni staging atomique, et un DLC déjà présent est comparé par sa seule
taille.

L'installateur STFS runtime est plus faible : `ReadHeaderAndVerify` vérifie la
taille et le magic, pas signature ni contenu
([source](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/filesystem/devices/stfs_container_device.cpp#L192-L221)).
`ExtractEntry` ignore le statut de `ReadSync`, les écritures courtes et retourne
succès ; la présence du `.header` suffit ensuite à sauter toute révalidation
([source](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/system/xam/content_manager.cpp#L529-L593)).
Les noms STFS/SVOD sont joints au chemin puis utilisés comme destination sans
garde explicite de confinement. Toute cette voie est `divergent` et hors scope
M01/DLC.

La sauvegarde de `futari.ini` utilise un fichier frère `.tmp` puis `rename`,
mais sans `fsync`, sync du dossier, temp unique, récupération de corruption ou
remplacement portable garanti
([source](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/src/main.cpp#L414-L433)).
Le merge de texte est testable et préserve les lignes inconnues, mais AC6 ne
doit en reprendre ni le writer ni le format avant la phase QoL.

## Entrée et replay

`XamInputGetState_entry` normalise `ANY_USER` vers zéro et délègue directement
à l'input system
([source](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/kernel/xam/xam_input.cpp#L94-L116)).
La frontière confirme `XamInputGetState` comme seam `provisional-rexglue`, mais
n'apporte aucun replay.

En dessous, plusieurs drivers sont fusionnés : OR des boutons, maximum des
triggers, axe de plus grande magnitude et plus grand packet number
([source](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/input/input_system.cpp#L77-L126)).
SDL met les événements en file depuis un thread hôte, les draine au poll et
n'incrémente le packet number que d'une unité malgré plusieurs changements
([source](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/input/sdl/sdl_input_driver.cpp#L187-L226),
[queue](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/input/sdl/sdl_input_driver.cpp#L420-L450)).
Cette cadence asynchrone et cette fusion sont `divergent` pour un replay
déterministe.

Le bon choix AC6 reste donc le journal poll-exact déjà conçu : ordinal global
du poll, marqueur/tick, appelant, arguments, code retour, packet number et bytes
retournés. Futari ajoute seulement des cas négatifs utiles : pointeur nul,
`ANY_USER`, déconnexion, plusieurs polls dans un tick et plusieurs drivers. Il
ne justifie ni un replay SDL ni l'enregistrement du périphérique physique.

## XMA/audio et renderer

Le XMA est la voie générique Xenia/ReXGlue avec FFmpeg. Il expose 320 contextes,
un worker qui balaie les contextes, et décode aussi inline sur un kick pour
éviter 50–100 ms d'attente
([source](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/audio/xma_decoder.cpp#L93-L165),
[kick](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/audio/xma_decoder.cpp#L271-L327)).
Le registre `CurrentContextIndex` renvoie volontairement un index tournant pour
ne pas paraître bloqué. Le callback SDL consomme la queue au rythme hôte,
injecte du silence si elle est vide puis libère le sémaphore qui réveille le
callback invité
([registre](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/audio/xma_decoder.cpp#L237-L265),
[SDL](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/audio/sdl/sdl_audio_driver.cpp#L153-L213)).
Il n'existe aucun golden PCM, hash de flux, test de timing ou mesure A/V public.
Le décodage FFmpeg est `provisional-rexglue`; l'index artificiel, la cadence
hôte et toute prétention de parité sont `divergent`. AC6 conserve son lecteur
FFmpeg borné et doit valider offline PCM, niveau et cues avant toute présentation
temps réel.

Futari n'a pas de renderer titre manuscrit : son unique cible demande le plugin
`xenos`
([CMake](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/CMakeLists.txt#L34-L56)).
Le plugin, explicitement porté de Xenia, lit les rings PM4, traduit les shaders
et choisit Vulkan sur Linux ou D3D12 sur Windows
([architecture](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/graphics/CMakeLists.txt#L1-L56),
[ring PM4](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/src/graphics/command_processor.cpp#L691-L764)).
Il est `provisional-rexglue` comme oracle de commandes/captures et `divergent`
comme architecture produit. Il n'ajoute rien au passage AC6 vers des
`DrawPacket` retail typés et un backend Vulkan direct.

## Tests, CI, provenance et licences

Le workflow actif du dépôt ne build pas le jeu. Il vérifie extensions/chemins
de game data, ShellCheck et identité Git
([workflow](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/.github/workflows/guards.yaml#L1-L74)).
Les workflows placés dans `thirdparty/rexglue-sdk/.github` ne sont pas exécutés
par GitHub pour le dépôt racine. Dans le SDK, les unit tests sont OFF par défaut
à cause d'un double-link connu ; les tests PPC exigent les binutils précisément
omis du vendoring
([CMake tests](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/tests/CMakeLists.txt#L1-L19),
[omission](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/thirdparty/rexglue-sdk/VENDORED.md#L16-L34)).
Il n'y a aucun test public d'intégration attach/detach, concurrence, underflow,
collision de basename, TLS DLL, import inter-module, croissance sur reload,
audio ou image.

La racine est BSD-3-Clause. Le NOTICE exclut explicitement le C++ généré, le
screenshot et l'icône dérivés du jeu ; l'icône est même intégrée aux builds
Windows
([NOTICE](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/NOTICE.md#L13-L40)).
Les règles de package reconnaissent que les huit DLL contiennent le programme
retail traduit et que certains fichiers de staging ne sont pas suivis
([PORTS](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/PORTS.md#L82-L121)).
Ce modèle est `divergent` pour la preview AC6, qui interdit bytes retail, code
généré et artwork retail dans le TGZ.

`THIRD-PARTY-NOTICES.txt` documente FFmpeg/libmspack statiques sous LGPL-2.1,
mais renvoie à l'upstream ReXGlue alors que l'arbre exact vient d'un fork devenu
inaccessible, et reconnaît plusieurs textes de licence absents
([notice](https://github.com/springah/futari_recomp/blob/b5281bcaa9c9fdc0cdaabdf14a189814c615297b/THIRD-PARTY-NOTICES.txt#L13-L66)).
Ce constat n'est pas un avis juridique. Pour AC6, l'audit doit porter sur les
objets réellement liés, leurs pins, textes et obligations de relink, dans le
staging final ; aucune source Futari/ReXGlue n'est à copier implicitement.

## Tranches C++ manuscrites et tests réutilisables

Ces tranches sont toutes conditionnelles au census M01. Elles ne contiennent ni
C++ généré, ni instruction traduite, ni byte retail.

| Priorité | Tranche bornée | Qualification et gate |
|---|---|---|
| F0 | **Aucun chargeur produit.** Émettre d'abord un reçu oracle `ModuleLoadReceipt` contenant séquence, tick/poll, appelant, opération, chemin canonicalisé, SHA-256 XEX, base/taille image+code, entrée, digest imports/TLS, code retour et load count. | instrumentation manuscrite `provisional-rexglue`; si aucun appel M01 n'est atteint sur deux replays identiques, fermer le sujet pour M01 |
| F1 | `ExactModuleRegistry` réservé au harness oracle : clé = chemin invité canonicalisé exact ; chemin hôte absolu sous racine scellée ; SHA/ABI/layout vérifiés avant publication ; aucun basename fallback. | seulement si F0 observe un module ; `provisional-rexglue` jusqu'au contrôle PAL |
| F2 | `ModuleLifecycleModel` pur C++ avec fausses image, bibliothèque, dispatcher et DllMain. | invariant `provisional-rexglue`, test asset-independent ; ne lie ni ReXGlue ni code guest |
| F3 | Reçu `ImportTlsInventory` : modules, ordinals/noms, variables, TLS statique et notifications thread. Un inconnu ou un TLS DLL non porté échoue explicitement. | fail-closed `provisional-rexglue`; aucune sentinelle ni no-op |
| F4 | Étendre les tests du replay XAM existant avec `ANY_USER`, null, déconnexion, multi-poll/tick et deux drivers contradictoires. | seam `provisional-rexglue`, tests seulement ; Futari ne change pas le format de replay AC6 |
| F5 | Fixtures audio offline : paquets XMA bornés synthétiques/libres, hash PCM, nombre d'échantillons, canaux, drain EOF et timestamps ; simulation indépendante du callback SDL. | `provisional-rexglue` jusqu'aux mesures PAL ; réutilise le lecteur AC6 existant |

La matrice minimale de F2/F3 doit couvrir :

- deux chemins différents avec même basename ;
- même base/taille mais SHA différente ;
- chevauchement simultané refusé, puis même base acceptée après detach ;
- attach `FALSE` sans module publié ni fonction/thunk résiduel ;
- appel hors thread invité refusé, jamais « attach sauté mais succès » ;
- deux loads, deux unloads, puis troisième unload refusé sans underflow ;
- import inconnu avec diagnostic exact ;
- module TLS refusé tant que ses allocations et notifications ne sont pas
  implémentées ;
- 1 000 cycles load/unload sans croissance de handles, tables ou bibliothèques ;
- digest identique de reçus sur deux replays de 3 600 ticks, tous polls inclus.

Ne pas reprendre : le registre généré de Futari, le fallback basename, les
hints, le loader STFS, le `NullDevice`, le writer INI, le driver replay, le
worker audio ou le command processor Xenos. Ils resteraient soit dépendants de
ReXGlue, soit trop génériques, soit incompatibles avec la preview M01 native.

## Validations effectuées

- pin origin, tag et commit vérifiés avec `git ls-remote` ;
- 22 URLs et 22 commits de submodules vérifiés auprès de l'API GitHub primaire ;
- fork SDK déclaré confirmé inaccessible, upstream public épinglé séparément ;
- inspection ciblée du manifest, codegen, registre, lifecycle, dispatcher,
  imports/exports, TLS, VFS/STFS, XAM, SDL, XMA, renderer, CMake, tests, CI,
  notices et historique pertinent ;
- `python3 -m py_compile scripts/*.py` : PASS ;
- `bash -n` et `shellcheck --severity=error` sur les scripts racine suivis :
  PASS ;
- garde « aucun XEX/ISO/media/DLC suivi » : PASS selon son expression exacte ;
- test C++23 standalone de `ini_merge.h` : préservation, remplacement,
  insertion, idempotence et marqueur endommagé : PASS ;
- aucune initialisation de submodule, génération ou exécution retail.

Le build jeu n'est pas reproductible depuis ce checkout seul : `generated/`
est volontairement absent et requiert les neuf XEX de l'utilisateur. Les tests
SDK complets n'ont pas été lancés : les dépendances ne sont pas initialisées,
les unit tests sont désactivés par défaut et la toolchain PPC documentée est
omise. Les affirmations « huit modules bootent » et « gameplay » restent donc
des attestations de mainteneur, notamment les commits
[`be2db37`](https://github.com/springah/futari_recomp/commit/be2db3743bc74ef1baa9c943ef550c1744a083cb),
[`6002b67`](https://github.com/springah/futari_recomp/commit/6002b678f1869cb8e411da14ac46baf7038f776c) et le soak de 17 minutes
[`1e7ecd4`](https://github.com/springah/futari_recomp/commit/1e7ecd4cf5073ae129f2ba55ee7ea9afa6e640bf).

## Risque résiduel

Le risque principal serait de confondre « Futari commute huit modules dans
ReXGlue » avec « AC6 M01 a besoin d'un runtime multi-XEX ». Rien ne l'établit.
Cette revue raccourcit la route en plaçant un gate négatif peu coûteux avant
toute implémentation. Si F0 observe un module, Futari fournit alors des
invariants et des tests utiles ; sinon, il ne doit consommer aucune capacité de
la verticale M01.
