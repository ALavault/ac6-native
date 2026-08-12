# Cycle 1555 — audit ReOdyssey, renderer natif

Date de qualification : 12 août 2026.

## Résultat

ReOdyssey montre une migration nette de la présentation GPU ReXGlue vers des
hooks D3D Xbox 360 de haut niveau, un renderer Plume et des shaders traduits par
un fork de XenosRecomp. Cette architecture est un oracle utile pour découper un
renderer C++ manuscrit, mais elle ne qualifie aucune sémantique retail d’AC6.

La branche auditée ne contient ni preuve de parité retail, ni test renderer, ni
CI du projet, ni build Linux public reproductible. Plusieurs chemins sont
certainement incomplets ou divergents : cache SPIR-V SMOL-V non décodé, backend
Linux non linkable/utilisable tel quel, formats inconnus convertis par défaut,
mips et cubemaps guest absents, primitives de synchronisation neutralisées et
input dépendant des polls et du temps hôte.

Conclusion de gate : **aucune lane M01 n’est fermée par cet audit**. Les
éléments réutilisables restent classés provisional-rexglue jusqu’à une
qualification PAL AC6 indépendante.

## Périmètre et pins

Audit en lecture seule du dépôt
[sgertyh/ReOdyssey](https://github.com/sgertyh/ReOdyssey/tree/803294cb9d74e9509b3576e3c4c08de9bbe6a627),
sans octet retail et sans build lourd.

| Élément | Commit | Tree | Observation |
| --- | --- | --- | --- |
| ReOdyssey audité | [803294cb9d74e9509b3576e3c4c08de9bbe6a627](https://github.com/sgertyh/ReOdyssey/commit/803294cb9d74e9509b3576e3c4c08de9bbe6a627) | a83d300712035f97e8b6490d493af32a8881685e | HEAD/main observé, 2026-06-17, « Initial native rendering work » |
| Baseline ReXGlue | [fc6d242845b2e760e525be51cb561de39ac113fc](https://github.com/sgertyh/ReOdyssey/commit/fc6d242845b2e760e525be51cb561de39ac113fc) | a36281ba0f85d31745e8d825f7570c1c5f64e734 | commit initial, 2026-05-23 |
| XenosRecomp fork LO | [c1891538e9ec69819bb70fb3cc123cf65c5f6da2](https://github.com/rapidsamphire/XenosRecomp/commit/c1891538e9ec69819bb70fb3cc123cf65c5f6da2) | 31ddca2af6a73afc6515b9cc72d0d02c964c6df7 | parent fb32631ee398e46f2a113d8f9103201dbaa000b4 ; 809 insertions, 140 suppressions ; pin encore atteignable, main distant désormais a2bf15b41b5e369ed58a94ac94be5128f86eff7c |
| Plume | [561428b7d0499eaf96b17d04bd6aa594d3b1260f](https://github.com/renderbag/plume/commit/561428b7d0499eaf96b17d04bd6aa594d3b1260f) | 7808e539424bfba5829baf1bc34631ccc18c8ac4 | backend D3D12/Vulkan |
| smol-v | [9dd54c379ac29fa148cb1b829bb939ba7381d8f4](https://github.com/aras-p/smol-v/commit/9dd54c379ac29fa148cb1b829bb939ba7381d8f4) | df7893318055eec3ed0b524a41b4f9e6f59a87b7 | compression SPIR-V du générateur |
| unordered_dense | [7b55cab8418da1603496462ce3ccdb4cb1dc3368](https://github.com/martinus/unordered_dense/commit/7b55cab8418da1603496462ce3ccdb4cb1dc3368) | c47f43e7fa2f829160e16a8a6bd86d725188ca6b | dépendance outil |
| zstd | [5233c58e6ca0b1c4c6b353ad79649191ed195bdc](https://github.com/facebook/zstd/commit/5233c58e6ca0b1c4c6b353ad79649191ed195bdc) | 336d9076c71e9e664b12d1262efed8affb93adf6 | compression du cache |

Le dépôt ReOdyssey n’a que deux commits. Le baseline ne contient que le
squelette ReXGlue minimal — CMake généré, main, patches et app. Le second commit
le remplace par 30 fichiers et 9 097 insertions pour seulement 2 suppressions.
Il n’existe donc pas d’historique public permettant de
reconstituer ou de valider une migration progressive. L’auteur du commit est le
placeholder « Your Name <you@example.com> », ce qui affaiblit en plus la chaîne
de provenance.

Le manifeste déclare seulement un SDK ReXGlue 0.8.0.0 et
assets/default.xex ; il ne scelle ni SHA-256 du XEX, ni Title ID, ni Media ID :
[reodyssey_manifest.toml, lignes 1–12](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/reodyssey_manifest.toml#L1-L12).
Les répertoires assets, generated et out sont ignorés :
[.gitignore, lignes 2–4](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/.gitignore#L2-L4).

## Grille de qualification

| Classe | Sens dans cet audit | Éléments ReOdyssey |
| --- | --- | --- |
| **provisional-rexglue** | Architecture ou invariant réutilisable, sans preuve PAL AC6 | seam de cycle de vie, ABI guest typée, hooks D3D de haut niveau, rejet de pipeline structurable, identité/census de shaders, invariant fence/signal pending, séparation fetch/detile/endian/upload |
| **retail-qualified** | Couvert par bytes PAL qualifiés, contrôle positif et exécution déterministe | **aucun** |
| **divergent** | Stub, fallback, heuristique titre ou comportement hôte qui ne peut pas servir de sémantique retail | no-op GPU, heuristiques Lost Odyssey, input au temps/poll hôte, fallback de formats, omissions mips/cubes/MRT |
| **documenté-non-correspondant** | Documentation ou configuration annonce une capacité que le code public épinglé ne réalise pas | presets Linux face aux défauts Linux certains ; cache annoncé SPIR-V face au flux SMOL-V non décodé ; noms XAM/XMA/VFS générés face à l’absence de runtime public épinglé |

## Migration ReXGlue GPU vers D3D haut niveau et Plume

La bascule de propriété est explicite :

- OnPreSetup installe l’input custom puis remet config.graphics à zéro ;
  OnPreLaunchModule initialise Video/Plume une fois la fenêtre créée ;
  OnShutdown détruit Video :
  [reodyssey_app.h, lignes 31–46](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/reodyssey_app.h#L31-L46).
- Le build conserve le SDK ReXGlue généré, ajoute Plume et zstd, puis compile
  les hooks et un cache de shaders généré :
  [CMakeLists.txt, lignes 13–23](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/CMakeLists.txt#L13-L23) et
  [lignes 96–110](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/CMakeLists.txt#L96-L110).
- Les REX_HOOK interceptent création de ressources, états, draws, resolve et
  present au niveau des fonctions D3D du jeu, pas au niveau PM4 :
  [d3d_hooks.cpp, lignes 1019–1115](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/d3d_hooks.cpp#L1019-L1115).
- Les structures guest utilisent rex::be et des static_assert d’offset/taille,
  une discipline ABI directement réutilisable :
  [guest_device.h, lignes 16–55](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/guest_device.h#L16-L55).

Ce n’est pas une suppression de ReXGlue : le CPU recompilé, le cycle de vie et
les services hôte restent fournis par son SDK absent du dépôt. C’est un
remplacement complet du propriétaire GPU, sans coexistence ni fallback
interactif visible. Pour AC6, seule cette frontière d’architecture est
provisional-rexglue ; les signatures, offsets, formats et heuristiques restent
spécifiques à Lost Odyssey.

## Shaders, identité et cache

Le fork XenosRecomp documente lui-même sa portée titre-spécifique et son
incomplétude : il faut le modifier par jeu, le conteneur/reflection et le
contrôle de flux ne sont pas génériques, les constantes entières, certains
vertex fetches, cubemaps/samplers, memory export et point size sont incomplets :
[README, lignes 3–31](https://github.com/rapidsamphire/XenosRecomp/blob/c1891538e9ec69819bb70fb3cc123cf65c5f6da2/README.md#L3-L31),
[45–69](https://github.com/rapidsamphire/XenosRecomp/blob/c1891538e9ec69819bb70fb3cc123cf65c5f6da2/README.md#L45-L69),
[73–96](https://github.com/rapidsamphire/XenosRecomp/blob/c1891538e9ec69819bb70fb3cc123cf65c5f6da2/README.md#L73-L96).

Le principe d’identité est intéressant mais non scellé :

- le générateur parcourt une std::map ordonnée par hash XXH3 et produit pour
  chaque entrée hash, offsets/tailles DXIL et SPIR-V, masque de spécialisation
  et nom de source :
  [main.cpp, lignes 383–423](https://github.com/rapidsamphire/XenosRecomp/blob/c1891538e9ec69819bb70fb3cc123cf65c5f6da2/XenosRecomp/main.cpp#L383-L423) ;
- le cache public ne scelle ni hashes des entrées source/XEX/PAC, ni version et
  options DXC/Xenos, ni nombre attendu de shaders, ni SHA du cache, ni
  exhaustivité des misses ;
- le runtime ne contrôle ni bornes offsets/tailles, ni ordre/unicité, ni statut
  ou taille retournée par ZSTD_decompress :
  [pipeline.cpp, lignes 270–317](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/pipeline.cpp#L270-L317).

Un défaut certain rend le chemin Vulkan public incohérent. XenosRecomp encode
le SPIR-V avec smol-v
([main.cpp, ligne 136](https://github.com/rapidsamphire/XenosRecomp/blob/c1891538e9ec69819bb70fb3cc123cf65c5f6da2/XenosRecomp/main.cpp#L136))
puis compresse le flux SMOL-V avec zstd
([lignes 461–475](https://github.com/rapidsamphire/XenosRecomp/blob/c1891538e9ec69819bb70fb3cc123cf65c5f6da2/XenosRecomp/main.cpp#L461-L475)).
ReOdyssey ne fait que la décompression zstd et marque les octets obtenus
SPIR-V ; aucun décodage smol-v n’est compilé. Plume transmet ensuite ces octets
directement à vkCreateShaderModule :
[plume_vulkan.cpp, lignes 1285–1303](https://github.com/renderbag/plume/blob/561428b7d0499eaf96b17d04bd6aa594d3b1260f/plume_vulkan.cpp#L1285-L1303).

Autres divergences certaines :

- une absence de vertex shader provoque un rejet structuré, mais l’absence de
  pixel shader devient silencieusement nullptr :
  [render_state.cpp, lignes 1575–1599](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/render_state.cpp#L1575-L1599) ;
- les draws rejetés ont une raison et des hashes, mais le log global est limité
  aux 32 premiers, donc impropre à un census complet :
  [render_state.cpp, lignes 195–256](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/render_state.cpp#L195-L256) ;
- un cache miss écrit automatiquement le conteneur shader retail brut dans
  missed_shaders/<hash>.bin :
  [d3d_resource_hooks.cpp, lignes 447–479](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/d3d_resource_hooks.cpp#L447-L479).
  Ce comportement est interdit dans le produit et le paquet AC6.

## Textures, endian, tiling, mips et cubemaps

La séparation parse du fetch, detile/endian puis upload est un bon découpage
provisional-rexglue. Son contenu n’est cependant pas portable tel quel :

- les formats D3D inconnus avertissent puis deviennent RGBA8 au lieu d’échouer
  fermé :
  [d3d_resource_hooks.cpp, lignes 34–71](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/d3d_resource_hooks.cpp#L34-L71) ;
- CreateTexture distingue volume et 2D, mais pas cube ; il crée le nombre de
  mips demandé sans charger/verrouiller autre chose que la base :
  [lignes 189–254](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/d3d_resource_hooks.cpp#L189-L254) ;
- le fetch constant ne couvre que les formats 2, 6, 18, 19, 20, 26 et 32, avec
  largeur, hauteur, pitch, tiled, endian et packed :
  [lignes 541–600](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/d3d_resource_hooks.cpp#L541-L600) ;
- l’upload ne traite que le sous-resource zéro :
  [lignes 615–679](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/d3d_resource_hooks.cpp#L615-L679) ;
- la traduction guest crée systématiquement une texture 2D à un mip. Le cache
  est indexé par adresse de base et ne rafraîchit pas les données si la mémoire
  guest change :
  [lignes 687–783](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/d3d_resource_hooks.cpp#L687-L783) ;
- l’infrastructure de descripteur cube existe, mais aucune traduction de
  texture guest ne produit une vue cube :
  [render_state.cpp, lignes 2814–2849](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/render_state.cpp#L2814-L2849).

L’aide endian ne traite que les modes 1 et 2
([d3d_resource_hooks.cpp, lignes 603–613](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/d3d_resource_hooks.cpp#L603-L613)).
Le catalogue d’architecture canonique local,
/fastdata/lavaulta/auto-re-agent/.tools/knowledge-base/architecture-v1/xbox360/xenia-xenos.h
aux lignes 193–197 et 1052–1067, établit que le mode 3 est k16in32 et échange
les demi-mots. Son omission est donc certaine.

Le lecteur DDS a aussi deux défauts certains :

- il accepte un buffer de 128 octets puis, si le FourCC vaut DX10, lit à partir
  de l’offset 128 sans exiger les 148 octets nécessaires ;
- il ne valide pas les tailles dwSize/pixel format ni les produits de
  dimensions, et convertit les FourCC/DXGI inconnus en RGBA8.

Voir
[d3d_resource_hooks.cpp, lignes 942–1018](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/d3d_resource_hooks.cpp#L942-L1018).
LoadTextureFromMemory renvoie une texture 1×1 sur entrée invalide, crée la
texture GPU avant de détecter un payload tronqué, puis peut renvoyer cette
texture non initialisée comme valide. Il ne parcourt ni faces cube, ni arrays,
ni tranches depth :
[lignes 1023–1122](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/d3d_resource_hooks.cpp#L1023-L1122).

La présence de BC1/BC2/BC3/BC5/BC7 dans le chemin DDS n’est donc pas une
qualification Xenos, tiled/endian ou image positive.

## États pipeline, resolves et synchronisation

Les no-op sont explicites : BlockOnFence, BlockUntilIdle,
SynchronizeToPresentationInterval, SetPredication et SetShaderGPRAllocation :
[d3d_hooks.cpp, lignes 156–166](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/d3d_hooks.cpp#L156-L166).
Le present utilise acquire/signal/fence puis attend immédiatement le fence ; ce
flux donne un invariant pending/signal simple pour le bring-up déterministe,
mais sérialise entièrement les frames et ne prouve pas le timing retail :
[video.cpp, lignes 428–440](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/video.cpp#L428-L440).

Les autres limites à recenser avant tout port AC6 sont :

- les render targets d’index différent de zéro sont ignorées, donc pas de MRT :
  [render_state.cpp, lignes 3040–3044](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/render_state.cpp#L3040-L3044) ;
- Resolve ignore mip, slice, face et paramètres de clear :
  [d3d_hooks.cpp, lignes 835–853](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/d3d_hooks.cpp#L835-L853) ;
- de nombreux enums inconnus tombent sur des valeurs hôte par défaut :
  [render_state.cpp, lignes 1131–1394](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/render_state.cpp#L1131-L1394) ;
- les samplers ignorent anisotropie, LOD bias, min/max LOD et comparaison :
  [lignes 1828–1854](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/render_state.cpp#L1828-L1854) ;
- des hashes shader, déclarations de sommets, strides et interprétations de
  données sont codés en dur pour Lost Odyssey :
  [lignes 171–207](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/render_state.cpp#L171-L207),
  [2379–2575](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/render_state.cpp#L2379-L2575) et
  [3383–3469](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/render_state.cpp#L3383-L3469) ;
- le reverse-Z est déduit de D24FS8 :
  [lignes 1176–1195](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/render_state.cpp#L1176-L1195) ;
- seuls les 16 premiers booléens VS et PS sont chargés et les constantes
  entières dynamiques ne le sont pas :
  [lignes 1899–1918](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/render_state.cpp#L1899-L1918).
  Le fork Xenos utilise des définitions statiques ou le défaut (1,0,0,0), ce
  qui confirme l’absence du chemin dynamique :
  [shader_recompiler.cpp, lignes 2097–2162](https://github.com/rapidsamphire/XenosRecomp/blob/c1891538e9ec69819bb70fb3cc123cf65c5f6da2/XenosRecomp/shader_recompiler.cpp#L2097-L2162).

La durée de vie guest/hôte est incomplète : GuestResource possède un refCount,
mais aucun hook D3D Resource AddRef/Release n’est fourni, les descripteurs ne
sont jamais libérés et les textures/surfaces traduites sont conservées. Le
staging BeginVertices grandit en remplaçant l’ancienne adresse sans libération :
[d3d_hooks.cpp, lignes 104–120](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/d3d_hooks.cpp#L104-L120).

## Input, XAM, XMA et VFS

L’input custom n’est pas un replay déterministe :

- GetState consomme les deltas souris puis incrémente packet_number à chaque
  poll, même sans changement :
  [reodyssey_mnk_input.cpp, lignes 144–225](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/input/reodyssey_mnk_input.cpp#L144-L225) ;
- SetState ignore la vibration ; GetKeystroke dépile une queue dans laquelle
  aucun chemin ne pousse d’événement :
  [lignes 230–255](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/input/reodyssey_mnk_input.cpp#L230-L255) ;
- la perte de focus remet touches et deltas à zéro, invariant provisoire utile :
  [lignes 354–375](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/input/reodyssey_mnk_input.cpp#L354-L375) ;
- le support UI souris utilise HWND, GetTickCount64 et GetAsyncKeyState sous
  Windows ; ses équivalents Linux renvoient échec, zéro ou false :
  [mouse_support.cpp, lignes 187–235](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/input/mouse_support.cpp#L187-L235).

Il n’existe pas de hook XamInput direct dans ce code, ni de timeline/replay.
L’agrégation input réelle reste dans le SDK ReXGlue absent. AC6 doit donc
conserver son seam XAM PAL poll-exact, les entrées normalisées par tick et la
sémantique des packet numbers, sans temps hôte.

Il n’existe aucune implémentation publique custom de XAM, XMA ou VFS. Les noms
XAudio/XMA/XamContent présents dans la configuration générée sont des labels de
codegen, pas une preuve de service. OnConfigurePaths ne fait que choisir assets
comme racine :
[reodyssey_app.h, lignes 48–52](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/reodyssey_app.h#L48-L52).
Le commit exact du SDK runtime n’est pas publié. Rien ici ne permet donc
d’inférer une sémantique XAM/XMA/VFS pour AC6 ; le cache retail v2, l’absence de
relecture PAC et les flux FFmpeg bornés restent inchangés.

## Défauts Linux certains, build et tests

Quatre preuves empêchent de considérer le backend Linux public comme
fonctionnel :

1. Sous Linux par défaut, RenderWindow est une structure Xlib
   [Plume, lignes 44–52](https://github.com/renderbag/plume/blob/561428b7d0499eaf96b17d04bd6aa594d3b1260f/plume_render_interface_types.h#L44-L52),
   alors que Video::Init tente un reinterpret_cast depuis void*, ill-formed
   [video.cpp, ligne 111](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/video.cpp#L111).
2. Video prend inconditionnellement l’adresse de CreateD3D12Interface
   [video.cpp, lignes 115–120](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/video.cpp#L115-L120),
   mais Plume ne compile cette implémentation que sous WIN32
   [CMakeLists.txt, lignes 59–73](https://github.com/renderbag/plume/blob/561428b7d0499eaf96b17d04bd6aa594d3b1260f/CMakeLists.txt#L59-L73).
3. ReOdyssey compile les shaders de blit uniquement en DXIL
   [CMakeLists.txt, lignes 48–91](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/CMakeLists.txt#L48-L91)
   et les soumet comme DXIL
   [video.cpp, lignes 240–243](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/video.cpp#L240-L243),
   tandis que Plume Vulkan exige SPIR-V et transmet le blob tel quel.
4. Le dépôt propose des presets Linux mais ne contient ni CI propre, ni test.

Un configure CMake public ciblé échoue avant compilation : generated/rexglue.cmake
et le cache generated/shader_cache.cpp/.h ne sont pas publiés, les submodules
du checkout non récursif ne sont pas peuplés et DXC est absent. Un clone
récursif résout seulement les submodules ; il n’existe aucune règle CMake qui
génère le cache shader ignoré.

Plume possède sa propre CI, qui compile ses exemples Debug/Release sur Ubuntu
x86/ARM, Windows et macOS, mais sans test sémantique GPU ni assertion de
validation Vulkan. Cela ne constitue pas une CI ReOdyssey. Plume précise
également que son API n’est pas stable/production-ready tant que barriers et
transitions texture ne sont pas affinées :
[README, ligne 3](https://github.com/renderbag/plume/blob/561428b7d0499eaf96b17d04bd6aa594d3b1260f/README.md#L3).

## Licences et provenance

| Composant | Licence observée |
| --- | --- |
| ReOdyssey | BSD-3-Clause, copyright rexglue |
| XenosRecomp | MIT |
| Plume | MIT |
| unordered_dense | MIT |
| zstd | BSD |
| smol-v | double licence MIT / domaine public |

Le dépôt n’agrège pas de notices tierces. render_patches.cpp déclare seulement
« ported from Xenia Canary » sans commit, chemin ni notice amont précis :
[ligne 1](https://github.com/sgertyh/ReOdyssey/blob/803294cb9d74e9509b3576e3c4c08de9bbe6a627/src/render/render_patches.cpp#L1).
Les licences et révisions du SDK ReXGlue généré et des binaires DXC redistribués
ne sont pas présentes. Aucun audit de paquet ou release publique ne ferme ces
risques.

## Invariants et tests réutilisables pour AC6 M01

1. **Propriétaire GPU unique** : après le seam lifecycle, un seul backend
   présente ; un test doit interdire le fallback ReXGlue/CPU interactif.
2. **ABI guest explicite** : rex::be, pointeurs guest 32 bits, static_assert de
   taille/offset et tests de buffers connus.
3. **Cache déterministe** : entrées triées, hash et stage explicites, offsets
   monotones/bornés, tailles vérifiées, décompression à taille exacte et rejet
   de tout miss.
4. **Traduction texture en étages** : parse fetch, calcul layout, detile,
   endian, conversion et upload testables séparément.
5. **Rejet fermé des DrawPacket** : vertex shader, pixel shader, déclaration,
   texture et état doivent tous être présents ; le census conserve chaque motif
   et chaque identité unique sans plafond global.
6. **Fence observable** : une soumission signalée reste pending jusqu’au fence ;
   les tests vérifient ordre acquire/execute/present/wait sans prétendre au
   timing retail.
7. **Focus/input** : perte de focus neutralise l’état, mais les packet numbers
   et valeurs de replay dépendent du tick/poll guest, jamais du temps hôte.
8. **Durée de vie symétrique** : AddRef/Release guest, allocation/libération
   descriptor et remplacement de staging ont des tests de symétrie et de fuite.

## Tranches manuscrites recommandées pour AC6 M01

1. Conserver le renderer natif AC6. Utiliser ReOdyssey uniquement comme oracle
   d’architecture, jamais comme source de sémantique AC6.
2. Sceller, pour les preuves internes, un manifeste shader/outillage contenant
   hash de source et stage, hash/offset/taille de chaque entrée, pins et options
   exacts Xenos/DXC, version de schéma, SHA du cache, statut/taille ZSTD,
   sortedness et zéro miss M01. Ne pas publier de cache généré ou dérivé du
   retail dans la preview.
3. Ajouter des tests tabulaires pour fetch constants, endian 0–3, BC3
   tiled/linear, packed mips, chaînes de mips, cubemaps/3D, bornes/overflow et
   une image positive. Tout format inconnu échoue fermé.
4. Refuser le DrawPacket dès qu’un VS, PS, vertex declaration, texture ou état
   requis manque. Recenser tous les rejets uniques, sans plafond de 32.
5. Faire un census M01 avant toute neutralisation de fences, predication, MRT,
   memory export, point size ou allocation de registres shader.
6. Garder le replay au vrai seam XAM PAL : poll-exact, fixed tick, packet
   semantics et entrées normalisées ; bannir le temps hôte.
7. Ne rien inférer de ReOdyssey pour XMA/VFS. Conserver les flux FFmpeg bornés,
   le cache v2 atomique et l’interdiction de relire les PAC après import.
8. Rendre explicites et testées les durées de vie ressources/descripteurs.
   Désactiver toute écriture automatique de shader retail brut.
9. Ajouter un test lifecycle qui impose un propriétaire GPU unique et prouve
   l’absence de fallback CPU/ReXGlue dans le produit interactif.

Ces tranches accélèrent M01 sans requalifier RexGlue comme retail. Chacune reste
soumise aux bytes PAL qualifiés, au contrôle positif et à l’exécution
déterministe propres à AC6.
