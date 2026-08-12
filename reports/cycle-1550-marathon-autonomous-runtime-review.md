# Cycle 1550 — audit du runtime autonome MarathonRecomp

## Verdict

MarathonRecomp confirme qu'un port Xbox 360 abouti peut fonctionner sans
ReXGlue, mais pas qu'il existe un runtime Xbox générique plus simple. Le projet
réduit volontairement la surface à émuler en interceptant les fonctions
D3D/XDK liées statiquement à Sonic 2006, puis soumet des commandes typées à
Plume. PM4 n'est donc pas interprété et les nombreux hooks et correctifs restent
spécifiques au titre.

Ce choix valide une direction déjà prise par AC6 : faire porter au produit des
`DrawPacket` et des états de pipeline explicites, tout en gardant
XenonRecomp/XenosRecomp comme outils d'inventaire et de contrôle. Il ne qualifie
aucune sémantique PAL AC6. Le décodeur XMA, XAM/HID et les lecteurs ISO/XContent
de Marathon contiennent en outre des écarts qui interdisent leur reprise.

Aucune lane M01 n'est fermée par cet audit.

## Provenance et reproductibilité

| Élément | Révision qualifiée | Arbre / statut |
|---|---|---|
| `sonicnext-dev/MarathonRecomp` | `bd9c0bbd8a99bcc2c0fabdf9521462e75e0ae7d8` | arbre `5d547e0eba72dcd9744a7839c9895f3177189d5d` |
| XenonRecomp | `c3714b8d7d35d202df293c4965b52bd74ae9df02` | pin public résolu |
| XenosRecomp | `fb32631ee398e46f2a113d8f9103201dbaa000b4` | pin public résolu |
| FFmpeg XMA | `5089f001a5442308599fbab8e35282f5ad8ccf13` | pin public résolu |
| Plume | `4f556be1531698174a597e7e0a215c22d3238a24` | pin enregistré ; non qualifié davantage ici |

Le HEAD et son arbre ont été recoupés avec le remote public le 12 août 2026.
Le dépôt contient 606 fichiers suivis et est sous GPLv3. Cela autorise l'étude
des concepts, pas la copie de code dans le produit AC6.

Le checkout public n'embarque ni `private/default.xex`, ni les archives de
shaders, ni les 146 unités PPC générées, ni `shader_cache.cpp`. CMake génère ces
fichiers depuis une copie du jeu. La CI Linux, Windows, Flatpak et macOS copie
elle aussi un dépôt d'actifs fourni par secrets avant de construire. Elle est
donc utile comme matrice de compilation, mais n'est pas reproductible depuis
les seules sources publiques. Aucun test automatisé n'est déclaré ou exécuté.

Empreintes SHA-256 des sources centrales lues :

- renderer : `73c5ab1ab2ba3912e7ff8ba6820891236023708df3d49cc7cfe84ca01df9fb3f` ;
- XMA : `c07b18b9af824ab6e23e357f24da144a8ddb9473a1ca0545fa480e97ec12bd36` ;
- XAM : `0d4ea163b8f73908bd82831275456ccdeb83fc4382384420c2380a7810106024` ;
- SDL HID : `35eb7aa66ccf7e4f768f4231a902fe6440676e551e9b8f1d9f7bf460e67175a9` ;
- XContent : `ec8ca81e0b586c9c1c5e87cf15629196a51d7f188d42952b2c530bba26e395a8` ;
- installateur : `821c5ad76017c06faea13691231d59dc214d0524e13d613e9cd76284cb6db2d6` ;
- workflow : `032df930c7ba3823dc0f242ab63cbc1a30585ea4a25b0790ac3bfe3601e21ab9`.

## CPU et frontière de recompilation

CMake produit 146 unités C++ avec XenonRecomp, compilées avec
`-fno-strict-aliasing` et un modèle flottant strict (`/fp:strict` ou
`-ffp-model=strict`). La cible x86-64 commune est limitée à Sandy Bridge. Ces
choix sont de bons garde-fous de portabilité, mais ne démontrent ni l'ordre
d'arrondi VMX128, ni les estimations réciproques, ni les exceptions PPC.

Pour AC6, les flags stricts restent nécessaires mais insuffisants. Les
sémantiques SIMD doivent conserver leur statut actuel : `retail-qualified`
seulement après contrôle PAL borné ; sinon provisoires ou divergentes. Aucun
C++ généré Marathon ne doit entrer dans `reconstruction/ace-combat-6`.

## Renderer sans PM4

Le renderer ne remplace pas RexGlue par un autre command processor Xenos. Il
intercepte directement une large surface D3D/XDK : création et sélection des
textures et shaders, vertex declarations et streams, render states, tiling,
surfaces, draws indexés et non indexés. Les hooks convertissent ces appels en
`RenderCommand`, consommés par un thread de rendu.

XenosRecomp traduit hors ligne les shaders extraits de `shader.arc` et
`shader_lt.arc`. Le résultat généré contient une table triée par XXH64, les
offsets DXIL/SPIR-V/AIR, le masque de constantes de spécialisation et trois
caches compressés. À l'exécution, le backend décompresse le cache, retrouve un
shader par hash, normalise les états inutiles, puis construit les pipelines
Plume pour D3D12, Vulkan ou Metal.

Ce découpage fournit quatre patrons directement utiles à M01-B :

1. une frontière haute, propre au jeu, au lieu d'une émulation PM4 dans le
   produit ;
2. un paquet de draw qui transporte toutes ses dépendances et son transform ;
3. une normalisation déterministe des états avant la clé de pipeline ;
4. une identité de shader et de variante explicite, vérifiée avant création.

La limite est tout aussi importante : les adresses de hooks, les corrections
de shaders, les hashes spéciaux et les décisions de resolve sont spécifiques à
Sonic. Ils sont `divergent` pour AC6 tant qu'un appel ou un payload PAL ne les
recoupe pas. XenosRecomp reste un oracle hors produit ; ses shaders générés ne
sont ni redistribués ni utilisés comme substitut à un shader AC6 qualifié.

L'audit renforce les gardes du chemin Vulkan monde en cours : refuser un état
de sampler, blend, culling ou constante non pris en charge vaut mieux que le
rendre silencieusement avec un état hôte choisi.

## XMA

Le décodeur reprend des mécanismes Xenia intéressants : paquets de 2 048
octets, header de 4 octets, frames traversant deux paquets, double buffer
d'entrée, ring PCM, boucles et `AV_CODEC_ID_XMAFRAMES`. Ces formes peuvent
servir à construire des fixtures, pas une implémentation AC6.

Les écarts observés sont bloquants :

- une frame scindée sans paquet suivant déclenche `__builtin_debugtrap` ;
- les échecs `avcodec_send_packet` et `avcodec_receive_frame` sont journalisés,
  puis le traitement continue ;
- le chemin suppose 512 samples, des plans `float`, au plus deux canaux dans
  son buffer fixe, et fixe les sous-frames restantes à `4 * channelCount` ;
- `FlushData` et `Destroy` n'ont aucun effet ; les requêtes de boucle restante
  et de position courante déclenchent un trap ;
- le calcul de samples disponibles décale le nombre d'octets par
  `channelCount`, ce qui n'est pas un calcul général de bytes-per-frame ;
- tous les hooks sont liés à des adresses Sonic et aucun test XMA n'existe.

La lane audio AC6 conserve donc son lecteur borné FFmpeg. Une fixture de frame
XMA traversant deux paquets pourra être ajoutée seulement si un flux M01
qualifié présente ce cas ; erreurs codec, canaux, taille de frame, boucle et
flush devront rester fail-closed.

## XAM, HID et replay

La fonction `XamInputGetState` de Marathon accepte seulement l'utilisateur 0,
ignore `flags`, déréférence `state` avant toute garde de nullité, masque le code
de retour HID et retourne finalement toujours succès. Le clavier est fusionné
après le gamepad. Le driver SDL incrémente son numéro de paquet à chaque poll,
inverse Y par complément binaire et gère le hotplug, mais n'enregistre aucune
timeline.

Cette route est plus permissive que le contrat AC6 et ne doit pas devenir un
oracle. Le replay AC6 reste au seam XAM poll-exact, avec résultat, pointeur
invité, user, flags, LR, marqueur et identité scellés avant toute projection
vers les entrées natives.

## ISO, XContent et installateur

L'installateur sait lire un dossier, une ISO, STFS et SVOD, puis compare chaque
fichier attendu à une liste de XXH3-64. La séparation VFS/import et le journal
de progression sont utiles comme architecture. Plusieurs gardes manquent :

- le lecteur ISO borne les records de répertoire, mais pas l'étendue
  `sector + length` ajoutée à la table ; `load` fait ensuite le `memcpy` sans
  revalider la fin du fichier ;
- les arbres ISO/SVOD n'ont ni ensemble de nœuds visités ni limite de nœuds ou
  de profondeur ;
- STFS borne les blocs de données, mais calcule et déréférence certaines
  entrées de table de hash sans borner préalablement leur offset ; aucune
  chaîne de hash ou signature du container n'est authentifiée ;
- les fichiers sont chargés entièrement en mémoire sans plafond et le total
  cumulé ne possède pas de garde explicite d'overflow ;
- l'écriture tronque directement la cible, sans fichier temporaire, `fsync` ou
  renommage atomique ; un chemin préexistant est ajouté à `createdFiles`, puis
  peut être supprimé par `rollback` après un échec ;
- `skipHashChecks` autorise explicitement une installation non identifiée ;
  XXH3-64 n'est de toute façon pas un sceau cryptographique.

Le cache retail v2 AC6 est déjà plus strict : SHA-256, écritures atomiques,
index scellé et absence de relecture PAC après import. Aucun lecteur
ISO/XContent ne sera ajouté à la preview M01 sans besoin utilisateur concret,
plafonds, détection de cycles, validation des extents et tests de corruption.

## Décisions AC6

| Mécanisme Marathon | Classe AC6 | Décision |
|---|---|---|
| Hooks D3D haut niveau vers commandes typées | architecture seulement | reprendre le découpage, requalifier chaque seam PAL |
| Cache shader DXIL/SPIR-V/AIR généré | oracle seulement | inventaire XenosRecomp, aucun byte généré dans le produit |
| modèle flottant strict | garde nécessaire | conserver, sans promouvoir la sémantique SIMD |
| framing XMA et split packet | provisoire | fixture seulement après observation M01 |
| XAM/HID | divergent | conserver le replay poll-exact AC6 |
| ISO/STFS/SVOD | divergent en l'état | ne pas importer ; le cache v2 reste autorité |
| code source Marathon | GPLv3 | étude conceptuelle, aucune copie |

La prochaine action issue de cet audit est locale et vérifiable : terminer le
chemin Vulkan monde avec transform par draw, profondeur et états explicites,
puis ajouter des rejets pour toute liaison texture/matériau ou état pipeline
que le backend ne représente pas. La parité visuelle reste soumise aux
captures M01 ; la maturité de Marathon ne remplace aucune capture retail.
