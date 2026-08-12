# Cycle 1574 — audit approfondi de Silent Hill: Downpour / ReXGlue

## Verdict

Downpour est utile à AC6 comme **catalogue de seams et de contre-exemples**, pas
comme preuve de fidélité retail. Le meilleur apport immédiat est le seam HLE
`XamInputGetState`: il permet d'injecter ou capturer l'état final retourné au
guest à chaque poll, sans savestate, sans toucher au C++ généré et sans supposer
une cadence par frame. C'est exactement la bonne frontière pour le replay
synchronisé AC6.

Les autres enseignements confirment les gates déjà choisies :

- les compteurs par phase et les clés de cache versionnées sont de bons patrons
  de diagnostic ;
- un scan statique des vtables et switch tables réduit fortement la boucle
  crash-ajout-rebuild, mais ne donne aucune sémantique ;
- le ring memexport retardé, les skips de PSO, le contournement EDRAM 7e3, les
  clocks 120/60 Hz et l'entrée souris pilotée par le nombre de polls sont des
  divergences de bring-up, pas des références retail ;
- XMA repose bien sur un flux FFmpeg borné, mais son ordonnanceur hôte et son
  état incomplet interdisent d'en déduire la synchronisation audiovisuelle ;
- le chemin Linux public du fork n'est pas qualifié : ses CI x86-64 et ARM64
  échouent, son UI force X11/Xwayland et aucun build du jeu n'est publié ;
- le modèle de paquet Downpour est incompatible avec AC6 : la documentation dit
  que le ZIP contient un exécutable issu du C++ recompilé et des shaders
  traduits. Aucun de ces artefacts ne doit entrer dans le TGZ AC6.

Résultat de classement : **0 élément `retail-qualified`**, plusieurs patrons
`provisional-rexglue`, et des frontières explicites `divergent` ou
`documented-unmatched`. Cet audit ne ferme aucune lane M01.

## Périmètre et méthode

Audit effectué le 12 août 2026 uniquement sur les arbres Git publics, leurs
métadonnées GitHub et les journaux Actions encore publiés. Aucun XEX, ISO,
STFS, actif de jeu, ZIP de release ou binaire opaque n'a été téléchargé ni
exécuté.

Les deux dépôts ont été clonés sans sous-modules et leurs objets ont été
recoupés par `git ls-remote`. Les 22 gitlinks du SDK ont été vérifiés un par un
par l'API commit de leur dépôt d'origine. Les permaliens ci-dessous sont tous
figés sur les commits audités.

Le classement employé est fermé : `retail-qualified` exige une identité retail
complète et un contrôle positif exécuté sur ces bytes ; `provisional-rexglue`
signifie que le mécanisme est présent dans la source pinnée mais non qualifié
pour les bytes AC6 ; `divergent` désigne un patch ou une approximation hôte qui
change délibérément la sémantique ; `documented-unmatched` couvre une
revendication documentaire sans code publié ou sans contrôle reproductible.
Une même observation n'est jamais promue entre classes par analogie.

Cette revue approfondit et corrige
[`cycle-1549`](cycle-1549-downpour-rexglue-source-audit.md). En particulier,
`downpour_config.toml` contient **692** affectations de fonctions, et non 439,
plus 32 tables `switch`. Le SHA-256 du fichier qualifié est
`6098aedb2a2e261eff178ac987f2e6359a0634ccb4e6018ec2ae19fa21d130d7`.

## Provenance exacte

| Élément | Révision | Arbre / identité | Statut |
|---|---|---|---|
| [`LittleBitUA/DownpourRecomp`](https://github.com/LittleBitUA/DownpourRecomp/tree/66c075d9fe9cbf712ac1694a7b108ae630a0e06a) | `66c075d9fe9cbf712ac1694a7b108ae630a0e06a`, branche `main`, tag léger `v1.1.6` | arbre `dfb815ecb2f9a99bead585a7cbc0e7a4e6730b82`, 26 fichiers | source hôte BSD-3-Clause |
| [`LittleBitUA/rexglue-sdk-dpour`](https://github.com/LittleBitUA/rexglue-sdk-dpour/tree/03b3282fd1263c5642f5925ba625b3ba0f6940c9) | `03b3282fd1263c5642f5925ba625b3ba0f6940c9`, branche `dpour-main`, tag annoté `v1.0` | arbre `415f53ca5242cb712e09e7bfc48879cda8d49065`, 1 329 fichiers | fork ReXGlue/Xenia BSD-3-Clause |
| ReXGlue upstream courant | `cb58065c793429aa92895d778af58d12e9d26d8f` au jour de l'audit | **non pinné par Downpour** | hors corpus fonctionnel |

Le dépôt jeu n'a ni sous-module ni verrou de dépendances. Son README demande de
cloner le ReXGlue upstream mobile
([instructions](https://github.com/LittleBitUA/DownpourRecomp/blob/66c075d9fe9cbf712ac1694a7b108ae630a0e06a/README.md#L786-L840)),
alors qu'une autre section déclare que la DLL livrée provient du fork `v1.0`
([compagnon SDK](https://github.com/LittleBitUA/DownpourRecomp/blob/66c075d9fe9cbf712ac1694a7b108ae630a0e06a/README.md#L860-L872)).
Ce lien n'est que documentaire.

La fermeture de version est contradictoire :

- le manifeste dit générateur `v0.8.1.7-dev.g14275e8` et requiert
  `sdk_version = "0.8.2.19"`
  ([manifeste](https://github.com/LittleBitUA/DownpourRecomp/blob/66c075d9fe9cbf712ac1694a7b108ae630a0e06a/downpour_manifest.toml#L1-L19)) ;
- le fork se déclare CMake `0.8.0`
  ([CMake](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/CMakeLists.txt#L1-L18)) ;
- l'unique asset du
  [SDK `v1.0`](https://github.com/LittleBitUA/rexglue-sdk-dpour/releases/tag/v1.0)
  se nomme `rexglue-sdk-0.8.0.0-dev.g03b3282-win-amd64.zip`.

Il n'existe donc pas de recette publique immuable qui relie générateur,
runtime, C++ généré et release jeu.

### Gitlinks du fork SDK

Les 22 pins ci-dessous sont ceux de
[`03b3282…/.gitmodules`](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/.gitmodules).
Tous les commits existent sur leur remote déclaré. Le manifeste canonique,
trié bytewise par chemin et formé de lignes `chemin SHA remote` séparées par un
espace, a pour SHA-256
`26df1ac5f4b61741260154442ae524f86661257fd7172d4a37bde83791ab293f`.

| Chemin SDK | Remote déclaré | Commit pinné |
|---|---|---|
| `thirdparty/libmspack` | `kyz/libmspack` | `305907723a4e7ab2018e58040059ffb5e77db837` |
| `thirdparty/glslang` | `KhronosGroup/glslang` | `f4f1d8a352ca1908943aea2ad8c54b39b4879080` |
| `thirdparty/FFmpeg` | `wmarti/FFmpeg` | `0604b464c7cb4ebc94940cf1f324a3b26b87717c` |
| `thirdparty/tomlplusplus` | `marzer/tomlplusplus` | `30172438cee64926dc41fdd9c11fb3ba5b2ba9de` |
| `thirdparty/simde` | `simd-everywhere/simde` | `71fd833d9666141edcd1d3c109a80e228303d8d7` |
| `thirdparty/xxHash` | `Cyan4973/xxHash` | `e626a72bc2321cd320e953a0ccf1584cad60f363` |
| `thirdparty/spdlog` | `gabime/spdlog` | `79524ddd08a4ec981b7fea76afd08ee05f83755d` |
| `thirdparty/fmt` | `fmtlib/fmt` | `407c905e45ad75fc29bf0f9bb7c5c2fd3475976f` |
| `thirdparty/catch2` | `catchorg/Catch2` | `88abf9bf325c798c33f54f6b9220ef885b267f4f` |
| `thirdparty/snappy` | `google/snappy` | `6af9287fbdb913f0794d0148c6aa43b58e63c8e3` |
| `thirdparty/utfcpp` | `nemtrif/utfcpp` | `63d64de49fd6b829f7c8694df5ab2ee625cb7134` |
| `thirdparty/volk` | `zeux/volk` | `0b17a763ba5643e32da1b2152f8140461b3b7345` |
| `thirdparty/vulkan-headers` | `KhronosGroup/Vulkan-Headers` | `49f1a381e2aec33ef32adf4a377b5a39ec016ec4` |
| `thirdparty/vulkan-memory-allocator` | `GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator` | `1d8f600fd424278486eade7ed3e877c99f0846b1` |
| `thirdparty/imgui` | `ocornut/imgui` | `6d910d5487d11ca567b61c7824b0c78c569d62f0` |
| `thirdparty/spirv-tools` | `KhronosGroup/SPIRV-Tools` | `04d0b166dcd62e29509bf2aac3ca0c5ccdcb6929` |
| `thirdparty/spirv-headers` | `KhronosGroup/SPIRV-Headers` | `04f10f650d514df88b76d25e83db360142c7b174` |
| `thirdparty/cli11` | `CLIUtils/CLI11` | `bfffd37e1f804ca4fae1caae106935791696b6a9` |
| `thirdparty/o1heap` | `pavel-kirienko/o1heap` | `388a73fd9007300e5130c5fe352d9ce3288b6dde` |
| `thirdparty/sdl3` | `libsdl-org/SDL` | `8bf3b7215ad9fc3deb583c6a3a37c6c67f2e24e4` |
| `thirdparty/inja` | `pantor/inja` | `7d1b4600b68595085a949743331c2e5673f511ea` |
| `thirdparty/tracy` | `wolfpld/tracy` | `05cceee0df3b8d7c6fa87e9638af311dbabc63cb` |

Xenia est la base déclarée du runtime. XenonRecomp et
`rexdex/recompiler` ne sont que crédités comme inspirations
([README SDK](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/README.md#L19-L23));
aucun des deux n'est un gitlink. `XenosRecomp` et `XenonAnalyse` sont absents.
Le manifeste mentionne un `xex_merge.exe` dérivé de `XexPatcher`, mais ne
publie ni source, ni pin, ni hash de cet outil. Le commentaire
`sp00nznet/360tools find_missing_vtable_funcs.py` n'est pas davantage pinné.

## Identité retail et fermeture codegen

Le dépôt donne le Title ID `4B4E0823`, le Media ID `7D387D17` et un « base XEX
hash » `7A3D5809776EE6AB`. Il ne précise pas l'algorithme de ce hash et ne publie
ni SHA-256 du XEX, ni taille, région, version, timestamp, image base ou entry
point. Surtout, le codegen vise `assets/default.tu1.xex`, un XEX fusionné hors
dépôt, dont aucune identité n'est publiée
([manifest](https://github.com/LittleBitUA/DownpourRecomp/blob/66c075d9fe9cbf712ac1694a7b108ae630a0e06a/downpour_manifest.toml#L8-L19)).

Le TU1 est mieux borné : l'installateur exige un payload `default.xexp` de
1 652 736 octets et SHA-256
`8732e3266301347b4494f2ab885c0ddf6a1ab5d03403a950263f90eeb8425122`
([whitelist](https://github.com/LittleBitUA/DownpourRecomp/blob/66c075d9fe9cbf712ac1694a7b108ae630a0e06a/src/downpour_title_update_installer.cpp#L39-L78)).
Cela qualifie le payload après extraction, pas le XEX fusionné ni le codegen.

Le fichier de configuration expose 692 entrées `[functions]` et 32 switch
tables. Il documente une stratégie en deux temps :

1. ajouter les cibles indirectes à mesure des crashes et hangs ;
2. scanner en masse les pointeurs de vtables pour éviter le cycle
   crash-ajout-rebuild
   ([début du census](https://github.com/LittleBitUA/DownpourRecomp/blob/66c075d9fe9cbf712ac1694a7b108ae630a0e06a/downpour_config.toml#L7-L52),
   [passe TU1](https://github.com/LittleBitUA/DownpourRecomp/blob/66c075d9fe9cbf712ac1694a7b108ae630a0e06a/downpour_config.toml#L391-L414)).

Le premier mécanisme prouve seulement qu'un appel a eu lieu. Le second donne
des débuts plausibles, pas des frontières, ABI ou sémantiques qualifiées. Aucun
byte dump, export Ghidra, log exécutable, hash d'arbre généré ou contrôle
positif ne permet de reproduire les 692 décisions. Elles restent
`documented-unmatched` pour AC6.

Le dépôt exclut `assets/` et `generated/`; `generated/rexglue.cmake`, requis dès
la ligne 11 du CMake, est absent. Le checkout source seul ne peut donc pas être
configuré, ce qui est attendu avant codegen mais interdit de parler d'un build
reproductible sans le XEX et l'outil de fusion exacts.

## Runtime et seam de replay

ReXGlue enregistre ses exports noyau/XAM par wrappers C++ typés. `REX_EXPORT`
marshalle les registres PPC vers une fonction hôte et l'inscrit au registre des
exports
([API de hook](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/include/rex/hook.h#L27-L82)).
`XamInputGetState_entry` valide le type de périphérique, normalise « any user »
vers l'utilisateur 0, puis délègue une fois à `InputSystem::GetState`
([export XAM](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/kernel/xam/xam_input.cpp#L94-L116)).

C'est une frontière bien meilleure qu'une capture SDL : elle voit exactement
ce que le guest reçoit, au nombre et dans l'ordre exacts de ses polls. Pour AC6,
la patch instrumentée doit ajouter un observer/injecteur hôte autour de ce seam,
sans modifier le code généré. Chaque enregistrement doit porter au minimum :

- index monotone du poll ;
- user, flags, code retour et les 16 octets pertinents de `X_INPUT_STATE` ;
- thread guest, LR/callsite et marker phase ;
- identité complète du XEX et hash du code au marker.

Il faut injecter **après** le merge ou rendre le driver replay exclusif.
`InputSystem::GetState` combine actuellement tous les drivers par OR/max et
choisit l'axe de plus grande magnitude
([merge](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/input/input_system.cpp#L106-L167)).
Ajouter un driver replay ordinaire laisserait donc une manette physique modifier
la trace.

Les deux drivers publics ont par ailleurs des sémantiques de packet différentes :

- SDL incrémente `packet_number` seulement si l'activité ou l'état change
  ([SDL GetState](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/input/sdl/sdl_input_driver.cpp#L258-L291)) ;
- clavier/souris incrémente à chaque poll et applique smoothing/decay par appel,
  sans `dt`
  ([MnK GetState](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/input/mnk/mnk_input_driver.cpp#L250-L275),
  [transformation](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/input/mnk/mnk_input_driver.cpp#L336-L421)).

La remise à zéro complète à la perte de focus est un bon invariant hôte : elle
vide touches, deltas, stick lissé, molette, répétitions et queue de keystrokes
([focus](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/input/mnk/mnk_input_driver.cpp#L790-L810)).
Elle ne doit toutefois pas intervenir dans un replay retail.

Aucun recorder, player, format de trace contrôleur, checkpoint gameplay ou
savestate reproductible n'existe dans le fork. Les occurrences de « replay »
concernent les caches GPU ou la remise en jeu de clears. Downpour qualifie donc
le **seam**, pas le replay.

## Cadence : pourquoi le poll-exact est obligatoire

La cadence publiée de Downpour est un triplet volontairement non retail :

- vblank guest annoncé à 120 Hz ;
- patch guest censé plafonner le jeu à un demi-vblank ;
- limiteur de présentation D3D12 hôte à 60 Hz
  ([valeurs publiées](https://github.com/LittleBitUA/DownpourRecomp/blob/66c075d9fe9cbf712ac1694a7b108ae630a0e06a/README.md#L576-L588),
  [limiteur hôte](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/ui/d3d12/d3d12_presenter.cpp#L57-L81)).

Le C++ généré contenant le patch n'est pas publié ; la revendication n'est donc
pas vérifiable. Le runtime lie aussi la libération de crédits audio à
`steady_clock` hôte, avec une marge de 1 %, explicitement pour empêcher que le
jeu — et « video playback » — avance trop vite
([pacing SDL](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/audio/sdl/sdl_audio_driver.cpp#L28-L42),
 [crédits](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/audio/sdl/sdl_audio_driver.cpp#L261-L290)).

Conclusion AC6 : ne jamais reconstruire les inputs depuis les presents ou une
horloge hôte. Capturer chaque appel XAM, conserver l'ordre des polls et ne faire
le zero-order hold 30→60 qu'une seule fois dans la projection native. La
sélection d'observations aux ticks 2, 4, … n'est pas un second hold.

## Renderer Xenos, EDRAM et memexport

Le fork conserve un command processor Xenos issu de Xenia. Ce n'est ni
XenosRecomp, ni le backend Vulkan natif à `DrawPacket` visé par AC6.

### Deux modèles D3D12

La documentation distingue correctement :

- RTV : render targets D3D12 et passes explicites de resolve/conversion ;
- ROV : EDRAM émulée dans un `RWByteAddressBuffer` ordonné depuis les pixel
  shaders
  ([description](https://github.com/LittleBitUA/DownpourRecomp/blob/66c075d9fe9cbf712ac1694a7b108ae630a0e06a/docs/v1.0-performance.md#L20-L50)).

Les nombres 36/58 FPS ne proviennent que d'un RTX 5070 Windows et ne sont pas
des tests de fidélité. La politique `pso_missing_policy = skip` accepte en outre
des géométries absentes pendant 1–2 frames pour éviter un stall
([tradeoff](https://github.com/LittleBitUA/DownpourRecomp/blob/66c075d9fe9cbf712ac1694a7b108ae630a0e06a/docs/v1.0-performance.md#L52-L77)).
Cette politique est `divergent` pour toute capture de parité.

### Ring memexport

Le chemin D3D12 utilise trois slots et recopie en mémoire guest le slot écrit
deux tours auparavant. Un premier miss déclenche encore le chemin synchrone
complet ; un slot ancien non terminé attend sa submission précise
([implémentation](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/graphics/d3d12/command_processor.cpp#L3453-L3525)).

Le commentaire source reconnaît que le guest relit memexport pour des indirect
draw counts, offsets de vertex, matrices de skinning, etc. Le délai de deux
tours est donc une optimisation de compatibilité observée, pas une sémantique
Xenos prouvée. Pour AC6 :

- autorisé dans un oracle ReXGlue uniquement sous l'étiquette
  `provisional-rexglue` ;
- interdit pour qualifier un compteur ou une transition à ±1 tick ;
- inutile dans le produit C++ natif, qui doit produire ses `DrawPacket`
  directement.

### Contournements Downpour

Le fork contient plusieurs choix spécifiques au jeu :

- `force_gameplay_state_active`, y compris dans le chemin Vulkan, force le GPU
  à croire la scène active ;
- `gpu_allow_invalid_fetch_constants = false` corrige un artefact observé mais
  coûte 5–8 % selon le commentaire ;
- le chemin D3D12 est pinné ROV par défaut ;
- `skip_depth_color_7e3_aliasing_transfers` saute uniquement les transferts
  depth→`k_2_10_10_10_FLOAT`
  ([cvar et justification](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/graphics/pipeline/render_target/cache.cpp#L42-L66)).

Le dernier mécanisme est particulièrement instructif : sa version symétrique
cassait les fontes du menu. C'est un excellent test négatif, mais il supprime un
événement EDRAM que le modèle hôte a créé à tort ; ce n'est pas une règle retail
transférable. Classement : `divergent`.

## Textures, tiling, endian et BC3

Le runtime commun possède bien des briques explicites de swap `8in16`, `8in32`,
`16in32` et d'untile 2D par blocs
([conversion](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/graphics/pipeline/texture/conversion.cpp#L24-L141)).
Cela vaut comme lecture `provisional-rexglue` de l'architecture Xenos, pas comme
test de bytes AC6.

Le document DXT5/BC3 Downpour est explicitement une hypothèse inachevée. Il
suppose un défaut d'extraction des indices alpha, liste quatre causes possibles
et demande encore de comparer au chemin Xenia puis d'ajouter un test unitaire
([forensic](https://github.com/LittleBitUA/DownpourRecomp/blob/66c075d9fe9cbf712ac1694a7b108ae630a0e06a/docs/v1.0-dxt5-alpha-bug.md#L22-L53),
[travail restant](https://github.com/LittleBitUA/DownpourRecomp/blob/66c075d9fe9cbf712ac1694a7b108ae630a0e06a/docs/v1.0-dxt5-alpha-bug.md#L125-L143)).
Aucun correctif ni vecteur positif n'est publié. Classement :
`documented-unmatched`.

Action AC6 inchangée : BC3 ne passe qu'avec un bloc synthétique connu, bytes
retail M01 bornés et contrôle image positif après untile+endian, puis mips et
cubemaps testés séparément.

## XMA, audio et vidéo

Le chemin public est techniquement intéressant mais non déterministe :

- ReXGlue lie `libavcodec` et `libavutil` et ouvre
  `AV_CODEC_ID_XMAFRAMES`
  ([XMA FFmpeg](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/audio/xma_context.cpp#L26-L99)) ;
- 320 contextes XMA sont exposés via la plage MMIO `0x7FEA0000..FFFF` ;
- un worker au-dessus de la priorité normale scanne tous les contextes et, sans
  travail, se réveille toutes les 2 ms
  ([worker](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/audio/xma_decoder.cpp#L92-L176)) ;
- le défaut `xma_use_old_decoder = true` est justifié par un crackle de
  **Skate 3**, pas par Downpour ni AC6 ;
- la sortie SDL copie les frames et libère les crédits selon le callback audio
  et une horloge hôte
  ([sortie SDL](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/audio/sdl/sdl_audio_driver.cpp#L136-L255)).

`AudioSystem::Save/Restore` ne sérialise que les clients/callbacks
([état sauvegardé](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/audio/audio_system.cpp#L352-L424)) ;
l'état interne des contextes FFmpeg/XMA, les subframes restantes et les buffers
ne sont pas sauvegardés. Le fork ne fournit donc ni savestate audio ni reprise
A/V déterministe.

Le sous-système vidéo se limite aux modes d'affichage. Les exports
`XamLoadExtraAVCodecs2` et `XamUnloadExtraAVCodecs2` sont des stubs
([XAM vidéo](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/kernel/xam/xam_video.cpp#L25-L44));
aucun décodeur Bink/XMV/WMV ou pipeline de sous-titres n'est publié. Une vidéo
éventuellement décodée par le code guest absent n'est pas une implémentation
hôte vérifiable. La vidéo, les cues et la synchro Downpour sont
`documented-unmatched`.

Pour AC6, FFmpeg/XMA peut accélérer le bring-up provisoire, mais la gate reste
la trace M01 : langue EN/JP, énergie ±1 dB, cue ±20 ms, événement ±1 tick et
timeline déterminée par la simulation, pas par `steady_clock`.

## VFS, import et sauvegardes

Le runtime monte le répertoire de jeu sur
`\Device\Harddisk0\Partition1`, lie `game:` et `d:`, monte éventuellement
`update:` en lecture seule et fait réussir artificiellement certains accès
raw/cache via `NullDevice`
([montages](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/system/runtime.cpp#L273-L336)).
Les sauvegardes sont des dossiers hôte sous
`content_root/xuid/title_id/content_type/name`; leurs headers sont écrits
directement
([layout](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/system/xam/content_manager.cpp#L83-L136)).

Le shell Downpour redirige par défaut données utilisateur et cache à côté de
l'exécutable
([portable layout](https://github.com/LittleBitUA/DownpourRecomp/blob/66c075d9fe9cbf712ac1694a7b108ae630a0e06a/src/downpour_app.h#L48-L72)).
Ce choix facilite un ZIP portable, mais diverge de la cible XDG AC6 et mélange
saves, cache dérivable et installation.

Les bonnes idées à conserver sont :

- normaliser les chemins guest avant lookup ;
- séparer le game root en lecture seule du user root writable ;
- vérifier `taille + SHA-256` du payload exact avant publication.

Les mécanismes à ne pas reprendre sont :

- cache négatif consulté avant résolution du symlink mais alimenté après, et
  non invalidé par une mutation directe du filesystem hôte
  ([cache](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/filesystem/virtual_file_system.cpp#L268-L304)) ;
- court-circuit d'un dossier writable vide, qui masque une création externe
  ([fast path](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/filesystem/devices/host_path_device.cpp#L64-L101)) ;
- écriture du TU par `ofstream(..., trunc)` sans temp+fsync+rename
  ([staging](https://github.com/LittleBitUA/DownpourRecomp/blob/66c075d9fe9cbf712ac1694a7b108ae630a0e06a/src/downpour_title_update_installer.cpp#L125-L145)) ;
- parseur STFS sans authentification, `visited set` ou plafond de chaîne par
  payload
  ([lecteur minimal](https://github.com/LittleBitUA/DownpourRecomp/blob/66c075d9fe9cbf712ac1694a7b108ae630a0e06a/src/downpour_title_update_installer.cpp#L150-L355)) ;
- backup annoncé en v1.1.6 mais absent de toute source publique.

AC6 conserve son import/cache v2 atomique et son stockage XDG. Le cache retail
reste immuable après import ; aucun PAC ne doit être relu pendant `play`.

## Linux, Vulkan, X11/Wayland et tests

Le dépôt jeu possède des presets `linux-amd64`/`linux-arm64` avec Clang 20
([presets](https://github.com/LittleBitUA/DownpourRecomp/blob/66c075d9fe9cbf712ac1694a7b108ae630a0e06a/CMakePresets.json#L21-L124)),
mais son README ne documente qu'un build Windows/D3D12. Il n'a aucun workflow
Actions.

Le fork SDK active Vulkan par défaut hors Windows et propose une CI Linux. La
révision `03b3282…` a toutefois produit :

- Windows AMD64 :
  [succès](https://github.com/LittleBitUA/rexglue-sdk-dpour/actions/runs/28298920518) ;
- Linux AMD64 : échec à l'étape `Build and install`
  ([run 28298920531](https://github.com/LittleBitUA/rexglue-sdk-dpour/actions/runs/28298920531)) ;
- Linux ARM64 :
  [échec](https://github.com/LittleBitUA/rexglue-sdk-dpour/actions/runs/28298920529).

Les logs détaillés ont expiré ; il est donc impossible d'attribuer honnêtement
la cause. Le workflow ne lance jamais CTest et laisse
`REXGLUE_BUILD_TESTS=OFF`
([workflow](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/.github/workflows/_build-platform.yaml#L28-L130),
[option tests](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/CMakeLists.txt#L13-L38)).

Enfin, Linux utilise GTK3 + X11-XCB et fixe explicitement
`GDK_BACKEND=x11` pour obtenir Xwayland sous Wayland
([entrypoint POSIX](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/src/ui/windowed_app_main_posix.cpp#L22-L45)).
Ce n'est pas une preuve Wayland natif. Le chemin Vulkan est une implémentation
séparée, sans capture Downpour qualifiée et sans le fix D3D12 ROV. Classement
Linux gameplay : `documented-unmatched`.

Cela explique pourquoi Wine/D3D12 peut être plus avancé pour l'oracle
Downpour/AC6-recomp, mais ne change pas la cible produit AC6 : SDL3 + Vulkan 1.3
sur X11 et Wayland natifs, validés séparément.

## Paquet, licences et frontière retail

Le dépôt Git suivi ne contient aucun `.xex`, `.xexp`, ISO, STFS, codegen ou
conteneur de jeu. Il contient cependant sept captures PNG de gameplay, malgré
un `.gitignore` qui dit que les screenshots ne doivent jamais être distribués.
Ce ne sont pas des bytes exécutables retail, mais c'est un rappel que les
extensions seules ne suffisent pas à un audit de contenu.

Les releases n'ont pas été téléchargées. GitHub publie pour
[`v1.1.6`](https://github.com/LittleBitUA/DownpourRecomp/releases/tag/v1.1.6)
un unique asset `DownpourRecomp-v1.1.6.zip`, 37 247 075 octets, digest
`sha256:a006951941bb8afb0a8bce0d1f84091db236cc8a6ecf0fc604567adea3045911`.
Cette identité permet de refuser un asset différent, pas d'en attester le
contenu.

La documentation officielle du projet dit que le ZIP v1.0 contient :

- `downpour.exe`, « statically-recompiled UE3 game runtime », 105,6 Mio ;
- `cache/shaders/shareable/`, des descriptions PSO et du bytecode DXBC
  ([inventaire](https://github.com/LittleBitUA/DownpourRecomp/blob/66c075d9fe9cbf712ac1694a7b108ae630a0e06a/docs/v1.0-release-overview.md#L23-L37)).

Le même README affirme ailleurs que le ZIP n'est qu'un host shell sans code ni
actif Konami, puis reconnaît que `generated/default/` est dérivé du binaire et
n'est pas couvert par la licence BSD
([frontière légale](https://github.com/LittleBitUA/DownpourRecomp/blob/66c075d9fe9cbf712ac1694a7b108ae630a0e06a/README.md#L894-L902)).
Ces déclarations sont incompatibles sans manifeste binaire et analyse de
provenance. Elles interdisent d'utiliser le paquet Downpour comme modèle.

Le fork SDK installe par ailleurs FFmpeg, SIMDe, ImGui, glslang, SPIR-V Tools et
autres bibliothèques, mais ses règles d'installation ne copient ni le LICENSE
racine ni un bundle de notices tierces
([règles](https://github.com/LittleBitUA/rexglue-sdk-dpour/blob/03b3282fd1263c5642f5925ba625b3ba0f6940c9/cmake/rexglue_install.cmake#L13-L130)).
Un audit de licences reste donc nécessaire même pour un paquet SDK générique.

Pour le TGZ AC6 : allowlist de fichiers, inspection des magics et gros blobs,
scan de chaînes/adresses, absence de code généré, de `.xsh`/shader dérivé, de
ReXGlue/Xenia/Xbox, de capture retail et de cache local ; licences et hashes
doivent être produits depuis le staging final.

## Réduction des micro-exécutions pour AC6

Downpour confirme une stratégie praticable, à condition de séparer découverte
de frontières et preuve sémantique :

1. **Fermeture statique large** : pointeurs de vtables, tables de fonctions,
   switch tables et tail targets, recoupés entre Ghidra canonique et codegen
   ReXGlue revision-pinné. Cela remplace des centaines de crash-runs.
2. **Qualification par bytes** : chaque fonction retenue porte XEX SHA-256,
   project name, module, start/end, hash des bytes et au moins un caller ou une
   table source. Une adresse Downpour ou US ne devient jamais PAL par étiquette.
3. **Analyse sémantique ciblée** : abstract interpretation et tests C++ pour les
   feuilles pures, VMX/VMX128 et parseurs. Les noms générés n'apportent aucune
   sémantique.
4. **Replay poll-exact synchronisé** : l'oracle instrumenté fournit les inputs
   et observations aux vrais polls XAM ; la simulation native rend son premier
   point de divergence structuré.
5. **Micro-exécution résiduelle seulement** : opcodes VMX ambigus, alias mémoire,
   arrondis/FPSCR et branches dont deux modèles statiques restent plausibles.

Cette route évite la majorité des micro-exécutions sans abaisser la gate : les
tests remplacent les exécutions une fois l'invariant extrait, et le replay
remplace les sessions humaines pour le gameplay répétable.

## Matrice finale et actions M01

| Élément Downpour | Classe AC6 | Décision |
|---|---|---|
| seam HLE `XamInputGetState` | `provisional-rexglue` | instrumenter capture/injection poll-exact hors généré |
| merge des drivers et packet semantics | `divergent` pour replay | bypass exclusif ; enregistrer l'état final retourné |
| clear complet à la perte de focus | `provisional-rexglue` | conserver pour QoL, jamais dans profil retail |
| scanner vtables/switch/tail targets | `provisional-rexglue` | automatiser avec bytes et frontières PAL qualifiées |
| compteurs CPU/GPU par phase | `provisional-rexglue` | adapter au backend Vulkan et au first-divergence report |
| ring memexport à deux tours | `divergent` | oracle bring-up seulement ; exclure des preuves ±1 tick |
| skip PSO / force gameplay state | `divergent` | interdire dans les captures de parité |
| skip depth→7e3 | `divergent` | test négatif uniquement, ne pas porter |
| untile/endian générique | `provisional-rexglue` | recouper par vecteurs BC3 AC6 et image positive |
| correctif BC3 annoncé | `documented-unmatched` | aucun code/test publié ; gate AC6 inchangée |
| XMA via FFmpeg | `provisional-rexglue` | flux borné utile, synchronisation et langues à qualifier |
| vidéo/sous-titres/A-V | `documented-unmatched` | aucune implémentation hôte publiée |
| VFS host et whitelist TU | `provisional-rexglue` | ne garder que séparation RO/RW et taille+SHA |
| portable saves / backup v1.1.6 | `divergent` / `documented-unmatched` | conserver XDG et écriture atomique AC6 |
| Linux/Vulkan Downpour | `documented-unmatched` | CI échouée, X11 forcé, aucun gameplay qualifié |
| paquet avec codegen et shaders | `divergent` | modèle explicitement interdit pour le TGZ AC6 |

Ordre d'action proposé pour M01 :

1. terminer le prototype ReXGlue moderne au seam XAM avec identité NTSC-U/J
   exacte, marker séparé, compteur de polls et mode injection exclusif ;
2. produire 3 600 ticks / 1 800 états source sans double zero-order hold et
   attester le premier/dernier poll ;
3. utiliser le même log structuré pour cible activée, dégâts, destruction et
   compteurs, plutôt qu'un test humain continu ;
4. n'employer les patrons renderer/audio Downpour que pour diagnostiquer le
   bring-up ; les gates Vulkan/BC3/XMA restent fondées sur AC6 PAL ;
5. auditer le staging TGZ depuis zéro avec une allowlist, pas depuis un dossier
   d'exécution ou de cache.

## Validations de l'audit

- `git ls-remote` : HEAD/tags/branches recoupés pour les deux dépôts ;
- `git rev-parse` : commits et arbres ci-dessus ;
- 22/22 gitlinks résolus par l'API commit de leur remote déclaré ;
- 692 entrées fonctions et 32 switch tables recomptées dans le fichier qualifié ;
- GitHub Actions : Windows succès, Linux AMD64/ARM64 échecs à `03b3282…` ;
- GitHub Releases : noms, tailles et digests lus comme métadonnées seulement ;
- scan des chemins Git : aucun XEX/ISO/XEXP/STFS/codegen suivi dans le dépôt jeu ;
- aucun contenu retail ni asset de release téléchargé ou exécuté ;
- permaliens source contrôlés contre les objets Git clonés ;
- `git diff --check` exécuté sur ce rapport.

Risque résiduel principal : les releases binaires v1.1–v1.1.6 peuvent contenir
des implémentations non publiées. Sans source, manifeste de build, SBOM et audit
du ZIP, elles restent hors preuve et ne changent aucun classement ci-dessus.
