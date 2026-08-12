# Cycle 1557 — Forza Horizon : revue multi-XEX ReXGlue v0.2.2

Audit public réalisé le 12 août 2026 au commit Forza
[`80a25bed26ef231ea086a87235cd46aedae38120`](https://github.com/NerunSmarts/ForzaRecomp/commit/80a25bed26ef231ea086a87235cd46aedae38120).
Aucun XEX, ISO, STFS, asset retail ou paquet de jeu n'a été téléchargé ou
exécuté. Les unités de recompilation publiques n'ont été ni compilées ni
exécutées ; leur revue est bornée aux noms, tailles et manifestes. Le code
ciblé lu est l'intégration manuscrite et le SDK public qualifié ci-dessous.

## Décision pour AC6 Mission 01

Forza est un **cas négatif particulièrement utile**, pas un modèle multi-XEX.
Le HEAD ne recompile que `default.xex`, initialise une seule table de fonctions
et précharge deux XEX secondaires comme images invitées. Il ne fournit ni code
hôte pour ces deux modules, ni liaison de leurs imports, ni `DllMain`, ni TLS
DLL, ni registre de plages sûr. Le dernier commit ne change qu'une ligne du
README pour dire que le multi-XEX reste absent de l'upstream ; aucun code n'a
changé depuis le commit racine.

La revue révèle une frontière plus forte que le simple « `LoadUserModule` ne
suffit pas » déjà retenu au cycle 1546 : dans ReXGlue v0.2.2, chaque chargement
XEX réinitialise les métadonnées d'allocation de **tout** le heap invité
`0x80000000–0x8fffffff`, tandis que la table d'appels indirects invitée est
bornée au code du XEX principal et que le setup public n'enregistre que ses
mappings. Le préchargement Forza peut donc rendre une image lisible et
enregistrée sans rendre son code recompilé appelable, tout en perdant la
connaissance des allocations antérieures.

Conséquences M01 :

1. ne pas écrire de chargeur multi-XEX produit avant le census PAL M01 de
   `XexLoadImage`/`XexGetProcedureAddress` déjà demandé par l'audit Futari ;
2. si ce census devient positif, exiger une transaction par module : identité
   XEX, plages, fonctions, imports, TLS et attach validés avant publication ;
3. conserver les tests sur images synthétiques et le refus des chevauchements,
   alias de chemin, loads concurrents et sorties hors table ;
4. ne reprendre aucun stub, backend ou comportement Forza dans AC6.

Forza n'ajoute **aucun fait `retail-qualified`**, pour Forza comme pour AC6.
Aucune sémantique AC6 PAL n'est qualifiée par transitivité.

## Échelle de qualification

| Classe | Usage dans cette revue |
|---|---|
| `provisional-rexglue` | forme publique vérifiable utile comme invariant ou fixture, à requalifier localement avant emploi |
| `retail-qualified` | résultat rattaché à un XEX exact par SHA-256 et validé contre son exécution retail ; ensemble vide ici |
| `divergent` | no-op, succès fictif, perte de cycle de vie, backend hôte ou comportement connu comme non équivalent |
| `documented-unmatched` | affirmation, preset ou fichier présent sans chemin public construit/testé qui l'établisse |

## Provenance vérifiée

### Dépôt Forza

| Élément | Résultat |
|---|---|
| dépôt | [`NerunSmarts/ForzaRecomp`](https://github.com/NerunSmarts/ForzaRecomp), public |
| branche par défaut | `master`; `git ls-remote --symref` donne le même HEAD que le pin d'audit |
| HEAD | `80a25bed26ef231ea086a87235cd46aedae38120`, commit GitHub vérifié, arbre `79e96d0c2b721962b15d616b86a2ee150adacc94` |
| parent | [`5942d30c547ff8e66e25f78e79d024abf9201fef`](https://github.com/NerunSmarts/ForzaRecomp/commit/5942d30c547ff8e66e25f78e79d024abf9201fef), commit racine non signé, arbre `5bda0e7d28d401dffe3cb1203d9691a28dcfea6b` |
| historique | deux commits, aucun tag ; le HEAD modifie seulement [`README.md`](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/README.md#L7-L11) |
| sous-modules Forza | aucun `.gitmodules`, aucun gitlink `160000`; 1 302 blobs ordinaires `100644` |

L'arbre suivi totalise 673 073 710 octets non compressés :

| Sous-arbre | Fichiers | Octets | Observation |
|---|---:|---:|---|
| `FH1/generated` | 157 | 254 525 194 | 153 unités `recomp.N.cpp`, un `init.cpp`, headers et manifeste |
| `lib/` | 91 | 402 127 084 | 64 bibliothèques Windows `.lib` plus fichiers CMake/pkg-config |
| `include/` | 1 023 | 16 243 399 | ReXGlue et dépendances aplatis |
| reste | 31 | 178 033 | intégration, presets, logo, CMake et documentation |

Aucun chemin suivi ne se termine par `.xex`, `.xexp`, `.iso`, `.stfs`, `.exe`,
`.dll`, `.pak` ou `.pac`, et aucun `assets/` n'est suivi. Les extensions seules
ne prouvent cependant pas l'absence de matière dérivée : la configuration
nomme [`assets/default.xex`](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/FH1/forza_horizon_1_config.toml#L5-L7)
comme entrée des 254,5 Mo de C++ généré.

### SDK aplati et upstream exact

Le paquet installé déclare ReXGlue `0.2.2` dans
[`rexglueConfigVersion.cmake`](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/lib/cmake/rexglue/rexglueConfigVersion.cmake#L12-L18).
Le tag léger upstream [`v0.2.2`](https://github.com/rexglue/rexglue-sdk/releases/tag/v0.2.2)
pointe sur le commit GitHub vérifié
[`72cfaaf28c49a2dcafaa46f2a98e181e0aae3057`](https://github.com/rexglue/rexglue-sdk/commit/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057),
arbre `0cdb048ed8edf64959e6c664a1a16c5169cc462d`.

La release officielle publie :

- `rexglue-sdk-win-amd64.zip`, 87 488 417 octets,
  SHA-256 `095af1261ec8cf9143126e6cfb0db357f6e248ce67dbec15e3a2f8a336695c75` ;
- `rexglue-sdk-linux-amd64.zip`, 140 649 260 octets,
  SHA-256 `d39eaad9b196c7a10a301b5dbc6e6b80dab9d956d046ad1dd843865c5a6dbeb1`.

Le ZIP Windows a été vérifié contre son digest GitHub puis comparé au SDK
Forza, sans lire `FH1/generated` : 1 132 chemins sont communs, 420 sont
identiques octet pour octet, 711 ne diffèrent que par fins de ligne, et un seul
fichier diffère sémantiquement,
[`share/rexglue/windowed_app_main_win.cpp`](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/share/rexglue/windowed_app_main_win.cpp#L31-L165).
Le fichier upstream correspondant est
[`src/ui/windowed_app_main_win.cpp`](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/ui/windowed_app_main_win.cpp#L26-L124).
Le paquet Forza omet en outre l'unique chemin `bin/rexglue.exe` du ZIP.

Les 286 entrées `include/rex/**` ont exactement les mêmes blobs Git que le tag
upstream. `rex_app.cpp` et l'entrypoint POSIX sont également identiques. Cela
qualifie la source v0.2.2 comme comparatif littéral du runtime aplati, mais ne
constitue ni une attestation source→binaire des `.lib`, ni une preuve retail.

Le tag upstream possède 19 gitlinks. Les URLs de
[`.gitmodules`](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/.gitmodules)
et chaque endpoint `commits/{pin}` ont été vérifiés :

| Dépendance | Pin |
|---|---|
| [`xenia-project/FFmpeg`](https://github.com/xenia-project/FFmpeg/commit/15ece0882e8d5875051ff5b73c5a8326f7cee9f5) | `15ece0882e8d5875051ff5b73c5a8326f7cee9f5` |
| [`catchorg/Catch2`](https://github.com/catchorg/Catch2/commit/88abf9bf325c798c33f54f6b9220ef885b267f4f) | `88abf9bf325c798c33f54f6b9220ef885b267f4f` |
| [`CLIUtils/CLI11`](https://github.com/CLIUtils/CLI11/commit/bfffd37e1f804ca4fae1caae106935791696b6a9) | `bfffd37e1f804ca4fae1caae106935791696b6a9` |
| [`fmtlib/fmt`](https://github.com/fmtlib/fmt/commit/407c905e45ad75fc29bf0f9bb7c5c2fd3475976f) | `407c905e45ad75fc29bf0f9bb7c5c2fd3475976f` |
| [`KhronosGroup/glslang`](https://github.com/KhronosGroup/glslang/commit/f4f1d8a352ca1908943aea2ad8c54b39b4879080) | `f4f1d8a352ca1908943aea2ad8c54b39b4879080` |
| [`ocornut/imgui`](https://github.com/ocornut/imgui/commit/6d910d5487d11ca567b61c7824b0c78c569d62f0) | `6d910d5487d11ca567b61c7824b0c78c569d62f0` |
| [`kyz/libmspack`](https://github.com/kyz/libmspack/commit/305907723a4e7ab2018e58040059ffb5e77db837) | `305907723a4e7ab2018e58040059ffb5e77db837` |
| [`libsdl-org/SDL`](https://github.com/libsdl-org/SDL/commit/5d249570393f7a37e037abf22cd6012a4cc56a71) | `5d249570393f7a37e037abf22cd6012a4cc56a71` |
| [`simd-everywhere/simde`](https://github.com/simd-everywhere/simde/commit/71fd833d9666141edcd1d3c109a80e228303d8d7) | `71fd833d9666141edcd1d3c109a80e228303d8d7` |
| [`google/snappy`](https://github.com/google/snappy/commit/6af9287fbdb913f0794d0148c6aa43b58e63c8e3) | `6af9287fbdb913f0794d0148c6aa43b58e63c8e3` |
| [`gabime/spdlog`](https://github.com/gabime/spdlog/commit/79524ddd08a4ec981b7fea76afd08ee05f83755d) | `79524ddd08a4ec981b7fea76afd08ee05f83755d` |
| [`KhronosGroup/SPIRV-Headers`](https://github.com/KhronosGroup/SPIRV-Headers/commit/04f10f650d514df88b76d25e83db360142c7b174) | `04f10f650d514df88b76d25e83db360142c7b174` |
| [`KhronosGroup/SPIRV-Tools`](https://github.com/KhronosGroup/SPIRV-Tools/commit/04d0b166dcd62e29509bf2aac3ca0c5ccdcb6929) | `04d0b166dcd62e29509bf2aac3ca0c5ccdcb6929` |
| [`marzer/tomlplusplus`](https://github.com/marzer/tomlplusplus/commit/30172438cee64926dc41fdd9c11fb3ba5b2ba9de) | `30172438cee64926dc41fdd9c11fb3ba5b2ba9de` |
| [`nemtrif/utfcpp`](https://github.com/nemtrif/utfcpp/commit/63d64de49fd6b829f7c8694df5ab2ee625cb7134) | `63d64de49fd6b829f7c8694df5ab2ee625cb7134` |
| [`zeux/volk`](https://github.com/zeux/volk/commit/0b17a763ba5643e32da1b2152f8140461b3b7345) | `0b17a763ba5643e32da1b2152f8140461b3b7345` |
| [`KhronosGroup/Vulkan-Headers`](https://github.com/KhronosGroup/Vulkan-Headers/commit/49f1a381e2aec33ef32adf4a377b5a39ec016ec4) | `49f1a381e2aec33ef32adf4a377b5a39ec016ec4` |
| [`GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator`](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/commit/1d8f600fd424278486eade7ed3e877c99f0846b1) | `1d8f600fd424278486eade7ed3e877c99f0846b1` |
| [`Cyan4973/xxHash`](https://github.com/Cyan4973/xxHash/commit/e626a72bc2321cd320e953a0ccf1584cad60f363) | `e626a72bc2321cd320e953a0ccf1584cad60f363` |

Forza aplatit ces dépendances : aucun de ces pins n'est conservé dans son
arbre. La reconstruction doit donc partir du tag upstream et du digest de
release ci-dessus, jamais des seuls dossiers `include/` et `lib/`.

## Chemin d'exécution réellement public

Le chemin construit par les sources suivies est :

```text
ReXApp::OnInitialize
  -> Runtime::Setup(table unique de default.xex)
  -> Runtime::LoadXexImage("game:\\default.xex")
  -> ForzaHorizon1App::OnPostSetup
       -> LoadUserModule("game:\\XMediaFacade_default.xex", false)
       -> LoadUserModule("game:\\SpeechFacade_default.xex", false)
  -> création fenêtre/presenter
  -> Runtime::LaunchModule(default.xex)
```

Ce séquencement vient de
[`rex_app.cpp`](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/ui/rex_app.cpp#L118-L206)
et de l'override Forza
[`main.cpp`](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/FH1/src/main.cpp#L27-L83).
Les deux modules sont qualifiés d'optionnels : un échec ne bloque pas le
lancement. `false` supprime explicitement l'attach, et l'appel intervient après
l'initialisation mémoire/XAM/XMA/GPU mais avant l'entrée principale.

Le seam `OnPostSetup` est `provisional-rexglue` comme point de transaction
pré-lancement. Son usage ici est `divergent` : il publie des images non
recompilées sans init ni preuve d'identité et continue en cas d'échec.

## Pourquoi le multi-XEX n'existe pas au HEAD

### Une seule image de code et une seule table

Le projet passe exactement les constantes du XEX principal au constructeur :

- image `0x82000000–0x8361ffff` ;
- code `0x823e0000–0x831ef59b` ;
- table de pointeurs hôte calculée à `0x83620000`, taille `0x01c1eb38`, fin
  `0x8523eb37`.

Les bornes sont suivies dans
[`forza_horizon_1_config.h`](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/FH1/generated/forza_horizon_1_config.h#L6-L9).
`Runtime::Setup` initialise une fois cette table et enregistre seulement
`PPCFuncMappings`
([source](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/system/runtime.cpp#L152-L191)).
Le `Processor` refuse une seconde initialisation ; `Memory::SetFunction` ignore
les adresses hors du code principal
([processor](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/system/processor.cpp#L241-L280),
[table mémoire](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/system/xmemory.cpp#L587-L670)).

`LoadUserModule` charge le XEX secondaire en mémoire et l'ajoute à
`user_modules_`, mais n'accepte aucun mapping hôte, aucune bibliothèque native
et aucune seconde plage de code
([source](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/system/kernel_state.cpp#L392-L440)).
Même un `GetProcAddress` réussi ne renverrait qu'une adresse invitée ; le
dispatcher secondaire resterait absent. C'est `divergent`.

### Le chargement oublie tout le heap XEX

`XexModule::ReadImage` appelle `LookupHeap(base_address_)->Reset()` avant de
réserver l'image
([source](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/system/xex_module.cpp#L449-L480)).
Toutes les adresses `0x80000000–0x8fffffff` partagent `v80000000`, et
`BaseHeap::Reset` met toute sa table de pages à zéro
([sélection du heap](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/system/xmemory.cpp#L304-L332),
[reset](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/system/xmemory.cpp#L881-L886)).

Le reset n'efface pas immédiatement les octets mappés, mais il oublie les
réservations, protections et propriétaires de l'image principale **et** de sa
table de fonctions, elle-même allouée dans ce heap. Un `AllocFixed` ultérieur
peut alors accepter un chevauchement que le runtime ne connaît plus et
réécrire les octets concernés. Chaque module secondaire répète l'opération.
Cette conception mono-XEX est `divergent` et interdit toute conclusion à
partir d'un simple log « module loaded ».

Invariant AC6 si le besoin devient réel : le chargement d'un module synthétique
ne doit modifier aucune allocation étrangère ; toutes ses plages doivent être
possédées, rejetées en cas de collision et retirées atomiquement au dernier
unload.

### Imports, publication, chemin et concurrence

La source v0.2.2 dit que les imports de fonctions sont résolus au codegen ; au
runtime, elle ne patche que les variables noyau et conserve les thunks comme
métadonnées
([source](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/system/xex_module.cpp#L1058-L1138)).
Il n'existe donc pas de linker d'import fonction utilisateur→XEX secondaire.

Le registre présente en outre quatre défauts déterministes :

1. il compare le chemin demandé (`game:\\...`) au chemin absolu stocké après
   résolution (`\\Device\\Harddisk0\\Partition1\\...`) sans canonicaliser ;
2. la comparaison est sensible à la casse alors que la VFS résout les enfants
   sans casse ;
3. le verrou est relâché pendant le load puis repris sans second contrôle, donc
   deux loads concurrents peuvent publier deux instances ;
4. `Runtime::LoadXexImage` définit l'exécutable mais ne l'ajoute pas à
   `user_modules_`, alors que `TerminateTitle` prétend décharger la liste « y
   compris l'exécutable »
   ([load principal](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/system/runtime.cpp#L289-L318),
   [registre](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/system/kernel_state.cpp#L176-L260),
   [arrêt](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/system/kernel_state.cpp#L464-L539)).

`DllMain(PROCESS_ATTACH/DETACH)` n'est qu'un warning non implémenté ; Forza le
saute explicitement
([source](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/system/kernel_state.cpp#L430-L462)).
`UnloadUserModule` retire aussi registre et handle sans appeler directement
`UserModule::Unload` : une référence restante peut maintenir l'image après son
unload logique. `TerminateTitle` prend une autre voie, appelle `Unload` sur la
liste, mais sans detach invité. Ce décalage de cycle de vie est `divergent`.
Le TLS statique d'un thread provient uniquement du module exécutable
([source](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/system/xthread.cpp#L284-L318)).
Il n'existe ni TLS DLL, ni notifications `DLL_THREAD_ATTACH/DETACH`, ni
rollback d'un attach. Tous ces comportements sont `divergent`.

## VFS et XContent

La base ReXGlue calcule trois racines distinctes : jeu, utilisateur et update.
Forza ne remplace que la racine jeu et conserve le profil utilisateur de la
plateforme ; l'update reste opt-in
([base](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/ui/rex_app.cpp#L55-L87),
[override](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/FH1/src/main.cpp#L39-L54)).
La séparation est `provisional-rexglue`, mais l'override privilégie trois
dossiers `assets` existants même si un chemin positionnel explicite a été
fourni ; le CLI peut donc être silencieusement masqué.

Le runtime monte le jeu en lecture seule sous
`\\Device\\Harddisk0\\Partition1` avec alias `game:` et `d:`, puis l'update
séparément en lecture seule
([source](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/system/runtime.cpp#L224-L266)).
Cette séparation est utile. En revanche, `Partition0`, `Cache0` et `Cache1`
passent par un `NullDevice` qui simule le succès sans stockage, tandis que
`cache:` reste absent
([source](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/system/runtime.cpp#L268-L284)).
Le succès fictif est `divergent` et ne doit pas entrer dans le cache v2 AC6.

Le `ContentManager` v0.2.2 ne monte pas un STFS authentifié. Il transforme le
contenu en répertoires hôte
`user_root/title_id/content_type/file_name`, crée un `HostPathDevice` écrivable
et l'expose sous le nom racine demandé
([construction](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/system/xam/content_manager.cpp#L28-L77),
[create/open](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/system/xam/content_manager.cpp#L110-L192)).
Il n'y a ici ni signature, ni chaîne de hashes, ni licence de contenu, ni
staging atomique. Forza n'ajoute aucun installateur, manifest DLC, digest ou
test XContent. La forme de chemin est `provisional-rexglue`; toute prétention
STFS/licence reste `documented-unmatched`.

## Input, XAM, XMA et GPU

### Input et replay

Forza remplace le backend SDL par `NopInputDriver` avant le setup
([source](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/FH1/src/main.cpp#L56-L62)).
Ce pilote annonce un contrôleur complet connecté pour l'utilisateur 0, renvoie
toujours un état neutre, accepte la vibration sans effet et ne produit jamais
de keystroke
([source](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/input/nop/nop_input_driver.cpp#L25-L82)).

L'entrypoint Windows local désactive en plus WGI, XInput et HIDAPI, force les
logs `trace` après lecture du CLI/environnement et écrit un log bootstrap
append-only
([source](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/share/rexglue/windowed_app_main_win.cpp#L71-L115)).
Le `main.cpp` désactive WGI une seconde fois. Cette voie est `divergent` : elle
peut faire franchir une attente « contrôleur connecté », mais ne mesure ni poll,
hotplug, paquet, vibration ou input réel. Elle n'apporte rien au replay
poll-exact AC6.

### Stubs XAM/kernel

[`import_shims.cpp`](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/FH1/src/import_shims.cpp#L7-L123)
contient 99 macros no-op et un shim explicite : 41 XAM, 18 réseau, 18
kernel/I/O, 7 crypto/clefs, 5 XAudio, 3 LDI et 7 autres. Les 99 no-op ne
définissent même pas `r3` ; s'ils étaient appelés, la valeur de retour serait
l'état antérieur du contexte invité. Le shim
`NtQueryVolumeInformationFile` est le seul à renseigner résultat et sortie avec
`STATUS_NOT_SUPPORTED`, et borne ses warnings aux cinq premiers appels.

Cependant ce fichier n'est présent ni dans
[`FORZAHORIZON1_SOURCES`](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/FH1/CMakeLists.txt#L25-L34)
ni dans
[`generated/sources.cmake`](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/FH1/generated/sources.cmake#L1-L162).
Son effet runtime est donc `documented-unmatched`; ses succès sans contrat sont
`divergent`. Seule la forme fail-closed du dernier shim est un patron
`provisional-rexglue`, à réécrire par ABI observée si un appel AC6 est atteint.

### XMA/audio

Le chemin générique crée `SDLAudioSystem`, qui possède un worker audio et un
décoder XMA ; un échec de setup audio ne fait qu'émettre un warning puis le
runtime continue sans audio
([setup](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/system/runtime.cpp#L86-L125)).
Forza n'ajoute aucun code XMA, fixture, capture PCM, mesure de ring, test de
contexte ou configuration `SDL_AUDIODRIVER=dummy`. Les cinq `XAudioGetDucker*`
du fichier mort n'ont aucun effet.

La présence des `.lib` XMA/FFmpeg et un éventuel franchissement des warnings
sont `documented-unmatched`, pas une qualification audio. AC6 conserve son A/B
audio headless et ses preuves de contexte/ring propres.

### GPU/Xenos

Le `ReXApp` upstream choisit D3D12 ou Vulkan à la compilation et traite un échec
GPU comme fatal
([source](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/src/ui/rex_app.cpp#L122-L188)).
Le paquet aplati Forza est la release Windows : ses targets définissent
`REX_HAS_D3D12=1` et lient D3D12/DXGI
([source](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/lib/cmake/rexglue/rexglueTargets.cmake#L75-L121)),
tandis que son package config a Vulkan désactivé
([source](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/lib/cmake/rexglue/rexglueConfig.cmake#L35-L54)).

Forza ne contient aucun hook renderer titre, shader attendu, fixture PM4,
capture, test de format ou comparaison Xenos. Les headers et bibliothèques ne
qualifient donc aucune commande GPU ; cette voie reste
`documented-unmatched` pour le retail et `provisional-rexglue` comme simple
frontière d'injection.

## Build Linux, tests et reproductibilité

Le checkout n'est reproductible ni sous Linux ni, sans SDK externe, sous
Windows :

1. `REXSDK_DIR` vaut par défaut un chemin absolu auteur
   `X:\\Game Development\\FH1Recomp\\win-amd64`; puisqu'il est non vide,
   CMake appelle toujours `add_subdirectory` sans tester son existence
   ([source](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/FH1/CMakeLists.txt#L6-L23)) ;
2. le `main.cpp` appelle `_putenv_s` sans garde plateforme, ce qui n'est pas une
   API POSIX
   ([source](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/FH1/src/main.cpp#L20-L24)) ;
3. vider `REXSDK_DIR` exige encore de fournir `CMAKE_PREFIX_PATH`; si ce prefix
   désigne la racine suivie, il trouve le package aplati Windows, composé de
   `.lib`, D3D12 et APIs Win32 ; celui-ci référence en plus
   `bin/rexglue.exe`, omis du dépôt, et le fichier d'import vérifie fatalement
   chaque chemin
   ([target absent](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/lib/cmake/rexglue/rexglueTargets-debug.cmake#L168-L175),
   [garde](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/lib/cmake/rexglue/rexglueTargets.cmake#L221-L253)) ;
4. les presets Linux ne règlent ni `REXSDK_DIR`, ni `CMAKE_PREFIX_PATH`, et ne
   font que choisir Clang 20
   ([source](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/FH1/CMakePresets.json#L20-L69)) ;
5. [`make.txt`](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/make.txt#L1)
   prépare Visual Studio puis construit `ninja-clang-debug`, nom absent des
   presets suivis ;
6. `import_shims.cpp` n'est pas compilé.

La lane Linux est donc `documented-unmatched`. Aucun build n'a été lancé dans
cet audit : la preuve statique ferme déjà les préconditions et un build complet
demanderait les sources générées dérivées du jeu.

Le dépôt Forza ne possède ni `.github/`, ni test, `enable_testing`, `add_test`,
sanitizer ou script de smoke test. L'upstream v0.2.2 possède sept fichiers de
tests unitaires et des tests PPC, mais `REXGLUE_BUILD_TESTS` est désactivé par
défaut
([CMake](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/CMakeLists.txt#L12-L15),
[unitaires](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/tests/unit/CMakeLists.txt#L1-L34)).
Le workflow release Linux construit et installe sans activer les tests ni
lancer CTest
([workflow](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/.github/workflows/build-linux-amd64.yaml#L38-L59)).
Aucun de ces tests ne cible multi-XEX, VFS/XContent, XAM/input, XMA ou GPU.

## Octets retail, provenance et licences

Le README affirme exclure le contenu copyrighté et ignore `assets/`, sorties,
exécutables et logs
([politique](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/README.md#L13-L25),
[ignore](https://github.com/NerunSmarts/ForzaRecomp/blob/80a25bed26ef231ea086a87235cd46aedae38120/.gitignore#L1-L25)).
Cette garde empêche le XEX brut suivi, mais pas le C++ généré : 153 fichiers de
recompilation, 254,5 Mo, sont produits depuis `assets/default.xex`. Il n'existe
ni SHA-256 XEX, ni Media ID/version/certificate, ni pin du codegen, ni digest du
config ou de l'arbre généré. On ne peut donc ni identifier la révision retail,
ni reproduire le codegen, ni distinguer une dérive d'outil.

Même sans container ou XEX brut, une traduction instructionnelle volumineuse
peut constituer du code retail dérivé. C'est un risque de droits et de
redistribution à traiter explicitement, pas une conclusion juridique. Pour une
revue externe AC6, conserver la règle actuelle : manifeste d'identité et
tranches bornées seulement ; jamais les PAC, XEX complet ou corpus généré.

Le dépôt n'a aucune `LICENSE`, `COPYING`, `NOTICE` ou SBOM racine. Le seul
fichier de licence dédié est `licenses/SDL2/LICENSE.txt`, alors qu'il
redistribue ReXGlue, 64 bibliothèques `.lib` totalisant 402 052 400 octets et de
nombreux headers tiers. L'upstream ReXGlue est BSD-3-Clause
([licence](https://github.com/rexglue/rexglue-sdk/blob/72cfaaf28c49a2dcafaa46f2a98e181e0aae3057/LICENSE)),
mais ce texte n'est pas embarqué. Les notices partielles dans des headers ne
remplacent pas un inventaire et les licences complètes des binaires. Risque
résiduel élevé de conformité, même si les blobs SDK sont rattachables à la
release v0.2.2.

## Matrice stricte et action AC6

| Observation vérifiée | Classe | Action M01 |
|---|---|---|
| `OnPostSetup` avant entrée principale | `provisional-rexglue` | conserver seulement l'idée d'une transaction pré-lancement |
| deux `LoadUserModule(..., false)` sans code hôte | `divergent` | ne jamais confondre image chargée et module recompilé exécutable |
| table de fonctions unique, bornée au XEX principal | `divergent` | registre et table par module ou domaine non chevauchant si F0 devient positif |
| reset de tout `v80000000` à chaque XEX | `divergent` | test snapshot/ownership : zéro allocation étrangère modifiée |
| chemin non canonicalisé et race de publication | `divergent` | identité canonicalisée exacte, single-flight, recheck avant publication |
| imports fonction résolus seulement au codegen | `divergent` | reçu imports/exports par module, inconnu fail-closed |
| absence DllMain et TLS DLL | `divergent` | attach/TLS avant publication, rollback complet, notifications thread testées |
| jeu/update read-only et user root séparé | `provisional-rexglue` | garder la séparation, requalifier les paths AC6 |
| raw cache en `NullDevice` succès | `divergent` | conserver cache v2 content-addressed et erreurs explicites |
| schéma XContent en répertoires hôte séparés | `provisional-rexglue` | retenir seulement la séparation de racines |
| XContent sans authenticité ni staging | `divergent` | aucune entrée STFS sans signature/hash/confinement/staging |
| contrôleur Nop connecté et neutre | `divergent` | ne pas utiliser comme oracle ; replay poll-exact inchangé |
| fichier de shims absent des sources CMake | `documented-unmatched` | ne lui attribuer aucun effet runtime |
| 99 shims qui laissent le résultat indéfini | `divergent` | stub atteint = ABI, outputs et erreur déterministe obligatoires |
| XMA et GPU seulement dans le SDK générique | `documented-unmatched` | aucune qualification audio/Xenos ; harness AC6 inchangé |
| presets Linux sans SDK Linux ni code portable | `documented-unmatched` | exiger pin source/release, build propre et CTest |
| README multi-XEX WIP | `documented-unmatched` | ne ferme aucun gate M01 |
| sémantique retail Forza ou AC6 | aucun `retail-qualified` | census PAL et validation exécutée restent souverains |

Si le census M01 atteint réellement un chargement de module, compléter les
fixtures F0–F3 du cycle 1548 avec cinq gardes synthétiques :

1. deux alias et deux casses du même chemin produisent une instance et un
   `load_count` contrôlé ; deux threads concurrents obtiennent le même résultat ;
2. un module hors plage principale peut enregistrer puis retirer fonctions
   directes, indirectes et thunks sans toucher la table principale ;
3. un chevauchement image/code/table/TLS échoue avant la moindre écriture ;
4. attach `FALSE`, import inconnu ou TLS DLL non supporté laisse zéro module,
   handle, page, fonction ou thunk publié ;
5. unload du dernier utilisateur puis arrêt titre retire executable, DLL,
   threads et TLS dans un ordre déterministe, avec second arrêt idempotent.

En l'absence d'appel PAL qualifié, ces tests restent une spécification de
frontière et aucune tranche C++ produit n'est justifiée.

## Validation reproductible

Contrôles effectués, sans contenu retail :

```text
git ls-remote --symref https://github.com/NerunSmarts/ForzaRecomp.git
git clone --filter=blob:none --no-checkout <origin> <tmp>
git rev-parse HEAD^{commit} HEAD^{tree}
git rev-list --count HEAD
git ls-tree -r HEAD                 # modes, gitlinks, chemins
GitHub git/trees/<tree>?recursive=1 # tailles, SHAs, arbre non tronqué
GitHub git/commits/<sha>            # parents et signatures
git ls-remote rexglue-sdk refs/tags/v0.2.2
sha256sum rexglue-sdk-win-amd64.zip
diff -qr --strip-trailing-cr <sdk-forza> <release-v0.2.2>
GitHub repos/<dependency>/commits/<gitlink> pour les 19 pins
```

Les permaliens `blob/<commit>` et `commit/<sha>` cités ont été contrôlés. Le
rapport seul est ajouté au worktree AC6 ; aucun build, codegen, test retail,
VNC, Xenia, XEX ou binaire du dépôt Forza n'a été exécuté.

## Risques résiduels

- Sans SHA-256 du `default.xex` source, le corpus généré ne peut être rattaché à
  aucune révision retail précise ni régénéré de façon probante.
- La comparaison au ZIP rattache les fichiers distribués à la release v0.2.2,
  pas les `.lib` à une reconstruction source reproductible ou attestée.
- Les adresses, bases et imports exacts des deux facades restent inconnus sans
  leurs XEX ; ils n'étaient ni nécessaires ni autorisés pour fermer ce cas
  négatif.
- Aucun reçu de boot public, log signé, test automatisé ou capture comparative
  ne permet de distinguer « franchit les warnings » de « exécute correctement ».

Le risque principal serait de promouvoir le préchargement Forza en solution
multi-XEX. Le code public démontre l'inverse et renforce le gate « census PAL,
identité, transaction et tests synthétiques avant implémentation ».
