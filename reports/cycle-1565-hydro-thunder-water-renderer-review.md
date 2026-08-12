# Cycle 1565 — Hydro Thunder Hurricane : audit eau et renderer ReXGlue

Audit statique réalisé le 12 août 2026 sur les sources publiques GitHub. Aucun
XEX, package XBLA, actif Hydro Thunder ou code C++ recompilé n'a été téléchargé
ou exécuté. La seule release inspectée est un petit pack source public.

## Décision

Le projet public est un scaffold de dix fichiers dans le monorepo
`CrownParkComputing/xbox360-ports`, pas un renderer Hydro Thunder autonome. Il
ne contient aucun code d'eau, reflet, transparence, caméra, streaming, shader
ou lecteur d'actifs. Le renderer, XAM, XMA, VFS et les contrôleurs proviennent
entièrement du fork ReXGlue/Xenia lié.

Le dépôt affirme que le jeu est « user-verified » et « works fine ». Cette
affirmation reste `provisional-rexglue` : aucun hash XEX, log Hydro, capture,
trace RenderDoc, test, replay ou reçu de build ne l'accompagne, et Hydro n'est
même pas inclus dans le harness `retest.sh`.

Un apport technique précis ressort néanmoins : le fork corrige la lecture de
`exp_adjust` dans le mot 3 de la constante texture Xenos, alors que le chemin
SPIR-V lisait le mot 4. Le dépôt attribue à cette correction la disparition de
barils blancs dans Hydro Thunder. La correction est recoupée par un commit
Xenia antérieur, mais elle ne prouve rien sur l'eau elle-même et ne devient pas
une sémantique AC6 sans preuve PAL M01.

**Impact :** aucun JF/JV/JP/JG et aucune lane du checkpoint 2 ne sont fermés.
L'audit ajoute deux contrats de tests Xenos utiles et plusieurs gardes
négatives pour les oracles et paquets.

## Identité publique

| Élément | Valeur vérifiée |
|---|---|
| Dépôt | `CrownParkComputing/xbox360-ports` |
| Branche par défaut | `master` |
| HEAD | `0216dae319eb5b61a7f1553d74529ca9e4ad55c5` |
| Arbre HEAD | `5a3e93e33c4a03cea1be1ac26c5eac8b5ea8093a` |
| Historique | 26 commits ; Hydro ajouté par `69ca72f1214f42a086fe074865dc5c7a4d2554e1` |
| Chemin Hydro | 10 fichiers, 18 455 octets suivis |
| Tag public | `game-packs-v1` → `9ec1c7439930dc1bbc785136b71aaa04b5ae859c` ; arbre `e3941063db09f4db34c5acf5bef73c34fd0ef266` |
| Release Hydro | `hydrothunder-rexglue-pack.zip`, 11 683 octets, SHA-256 `5ee3bec397fcd98b591d7e08ffd91a43b9377d6995b20a14119c75acf004f8ed` |
| Sous-module SDK du monorepo | `34b11ee6aed9d4ef914e49e6d8a8a092b02ced36`, arbre `38f510e0f2ac37828241d565c114abf83f33d067` |
| Licence | aucune licence racine pour le monorepo ou le pack ; SDK BSD-3-Clause |
| Tests/CI du port | aucun test ; aucun workflow de build du jeu |

Le [README](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/README.md#L21-L29)
dit correctement que les XEX, actifs et sorties générées sont absents. Le
[statut Hydro](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/STATUS.md#L7-L16)
ne fournit toutefois que la phrase « works fine » et la correction des
surfaces blanches.

Le dépôt principal épingle bien son fork SDK par gitlink ; le
[`.gitmodules`](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/.gitmodules#L1-L4)
nomme aussi la branche mouvante `development`. Le commit exact SDK est
[celui-ci](https://github.com/CrownParkComputing/rexglue-sdk/commit/34b11ee6aed9d4ef914e49e6d8a8a092b02ced36).
Il n'est associé à aucun tag et son seul check public est un échec de format ;
les workflows de build du SDK ne s'exécutaient alors que sur les tags.

## XEX, région et frontières

Le port ne publie ni SHA-256 du XEX, ni région, ni media ID, ni taille/entry
point qualifiés. Pire, les identifiants suivis sont des restes de scaffolding :

- [`main.cpp`, lignes 1–15](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/games/hydrothunder/project/src/main.cpp#L1-L15)
  annonce OutRun, le Title ID `58410968` et conserve une classe `OutRunApp` ;
- [`ppc_config.h`, lignes 5–15](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/games/hydrothunder/ppc/ppc_config.h#L5-L15)
  annonce Daytona, Title ID `58410B1D`, et ses bornes d'image ;
- la configuration Hydro ne donne que
  [22 adresses sans symbole](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/games/hydrothunder/config/ht_rexglue.toml#L1-L27),
  de `0x82134BF8` à `0x82792238`. Quatre sont hors de la fausse plage Daytona
  `0x82120000..0x824FFFFF`.

Avec ReXGlue 0.8, les bornes effectives sont régénérées depuis le XEX dans
`hydrothunder_init.h`; le [template exact](https://github.com/CrownParkComputing/rexglue-sdk/blob/34b11ee6aed9d4ef914e49e6d8a8a092b02ced36/resources/templates/codegen/init_h.inja#L20-L24)
est autonome. Les vieux headers PPC sont donc morts dans ce build, mais ils
montrent que le scaffold public n'est pas un reçu d'identité fiable.

Une source Xenia publique indépendante donne Title ID `5841096A`, media ID
`1F4C5091` et série `XA-2410`
([métadonnées](https://github.com/xenia-canary/xenia-canary.github.io/blob/23601d7b08fb1d22e4958773591632ce7de75680/_games/HydroThunderHurricane.md#L11-L16)).
Le patch Xenia indique un hash de module `3BA269908365D78E`
([lignes 1–4](https://github.com/xenia-canary/game-patches/blob/84d6682caf1b75b2fdb7adcd197c6559c09b2ed4/patches/5841096A%20-%20Hydro%20Thunder%20Hurricane.patch.toml#L1-L4)).
Ce hash n'est pas un SHA-256 et aucun artefact du port ne permet de le relier
au XEX utilisé : ces données restent `documented-unmatched`, région comprise.

## Build et reproductibilité

Le manifeste déclare seulement `sdk_version = "0.8.0"` et accepte tout fichier
nommé `assets/default.xex`
([manifeste](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/games/hydrothunder/config/ht_manifest.toml#L1-L7)).
Il n'existe aucune garde de hash, Title ID, région ou révision.

Le chemin de build public comporte plusieurs contradictions :

- le CMake dit que Daytona exige une branche `daytonaxbla`, alors que le
  monorepo épingle un autre commit ;
- sa cible `hydrothunder_codegen` appelle le manifeste inexistant
  `or_manifest.toml` et son commentaire parle encore de Daytona
  ([lignes 80–85](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/games/hydrothunder/project/CMakeLists.txt#L80-L85)) ;
- le pack contourne cette cible en appelant directement `ht_manifest.toml`,
  mais clone `development` sans commit ni tag ; son option `SDK_SRC` n'est pas
  transmise à `REXGLUE_SDK_SOURCE_DIR` lors de la configuration ;
- le codegen est toujours lancé avec `--force`, donc les erreurs d'analyse ne
  constituent pas un gate ;
- le lancement active `--gpu_allow_invalid_fetch_constants=true`, une voie
  fail-open qui doit rester diagnostique.

Le [harness](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/retest.sh#L37-L82)
ne parcourt que `games.conf`. Or le
[fichier de jeux](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/games.conf#L10-L17)
n'inclut pas Hydro Thunder. Le projet n'a ni CTest, ni tests unitaires, ni
workflow qui reconstruise le port. Les checks verts du HEAD sont ceux du site
GitHub Pages, pas ceux du jeu.

La release est donc reconstruisible en principe à partir d'un jeu possédé,
mais non reproductible au sens AC6 : entrée invitée et SDK ne sont pas scellés,
et aucun résultat de build ou d'exécution n'est attesté.

## Apport renderer réellement démontré

### `exp_adjust` : contrat utile, mais pas preuve de l'eau

Le champ Xenos est un signé de six bits à `dword_3 + 13`, tandis que
`lod_bias` est dans `dword_4`
([définition](https://github.com/CrownParkComputing/rexglue-sdk/blob/34b11ee6aed9d4ef914e49e6d8a8a092b02ced36/include/rex/graphics/xenos.h#L1231-L1255)).
Le fork charge désormais le mot 3
([lecture](https://github.com/CrownParkComputing/rexglue-sdk/blob/34b11ee6aed9d4ef914e49e6d8a8a092b02ced36/src/graphics/pipeline/shader/spirv_translator_fetch.cpp#L1421-L1438))
et en extrait les bits 13–18 avant multiplication du résultat
([application](https://github.com/CrownParkComputing/rexglue-sdk/blob/34b11ee6aed9d4ef914e49e6d8a8a092b02ced36/src/graphics/pipeline/shader/spirv_translator_fetch.cpp#L2059-L2072)).

Le commit fork
[`5ecb564b`](https://github.com/CrownParkComputing/rexglue-sdk/commit/5ecb564bc3a0290ae9af0f3a84c8ec4401a62780)
attribue les barils blancs Hydro à l'ancienne lecture dans `lod_bias`. Le commit
Xenia indépendant
[`32889f51`](https://github.com/xenia-project/xenia/commit/32889f51beafc207ef3c4e73c807d363dfa133c3)
effectue la même correction. C'est une forte corroboration de la forme Xenos,
mais toujours `provisional-rexglue` pour AC6.

Test AC6 à retenir : décoder séparément un `exp_adjust` négatif, nul et positif
depuis le mot 3, faire varier `lod_bias` dans le mot 4 sans changer le résultat,
et refuser la promotion tant qu'un draw M01 PAL utilisant un biais non nul n'a
pas un contrôle image positif. Cette garde cible précisément le défaut
blanc/noir ; elle ne nécessite pas de reprendre le code du fork.

### Render targets et resolves : hypothèses génériques seulement

Le même SDK épinglé contient deux corrections potentiellement importantes pour
l'eau, les reflets et les passes HDR :

- `k_2_10_10_10` → `VK_FORMAT_A2B10G10R10_UNORM_PACK32`, au lieu de tronquer
  chaque canal à huit bits
  ([commit](https://github.com/CrownParkComputing/rexglue-sdk/commit/d5919b793d607c235610b259bdb96e68594754e5)) ;
- resolve vide/inversé comme no-op, et pitch/hauteur alignés à 32 avant le
  calcul d'adresse tiled
  ([commit](https://github.com/CrownParkComputing/rexglue-sdk/commit/eb955feebef989c8851d3c9ada63eaba0d0f0a9c)).

Mais les messages de ces commits citent Choplifter/SoulCalibur II, pas un A/B
Hydro. Aucun fichier Hydro ne recense ses render targets, formats, mips,
cubemaps, EDRAM, resolves, depth ou blend. Ces deux règles sont des candidats
de tests partagés, pas des faits Hydro et encore moins des faits AC6.

### Ce que le projet ne fournit pas

- aucun shader invité ou traduit, cache shader ou `DrawPacket` typé ;
- aucun dump de fetch constants, texture tiled/endian, BC3, mip ou cubemap ;
- aucun inventaire des passes eau/reflet, transparence, depth ou culling ;
- aucune capture RenderDoc, image de référence ou comparaison Xenia/recomp ;
- aucun hook caméra, streaming monde, visibilité ou transition ;
- aucun renderer natif réutilisable : le port appelle le plugin Xenos dérivé
  de Xenia fourni par ReXGlue.

Le fait qu'un jeu de course nautique atteigne prétendument le gameplay ne
permet donc pas d'inférer l'algorithme de l'eau ni sa fidélité.

## Input, cadence et replay

Le driver local est une copie Daytona/Crazy Taxi. Sous Linux,
`MapKeyToButton`, `OnKeyDown` et `OnKeyUp` sont vides, puis `GetState` retourne
un pad connecté entièrement neutre
([lignes 55–61](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/games/hydrothunder/project/src/keyboard_driver.cpp#L55-L61),
[139–181](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/games/hydrothunder/project/src/keyboard_driver.cpp#L139-L181)).
Le SDK ajoute séparément SDL et clavier/souris puis fusionne leurs états
([source exacte](https://github.com/CrownParkComputing/rexglue-sdk/blob/34b11ee6aed9d4ef914e49e6d8a8a092b02ced36/src/input/input_system.cpp#L162-L192)).
Le pad neutre n'interdit donc pas nécessairement le gameplay, mais n'ajoute
aucune instrumentation déterministe.

Il n'existe ni capture d'inputs, ni séquence poll-exact, ni replay, ni premier
point de divergence. Le seul outil de cadence lit les timestamps hôte des
messages `PRESENT`; il ne relie ni vblank, ni tick guest, ni XAM poll, ni audio.
Aucun log Hydro n'est suivi. Le lancement n'impose pas de cadence propre à
Hydro et le commit SDK épinglé précède les patches ultérieurs `frame_limit` du
fork.

Le pack force enfin `GDK_BACKEND=x11`; il ne valide pas Wayland. Rien ici ne
remplace le replay AC6 à l'entrée normalisée de `XamInputGetState` et à horloge
60 Hz scellée.

## XAM, XMA, VFS et sauvegarde

`stubs.cpp` ne contient qu'un include. Aucun hook Hydro ne couvre XAM, XMA,
VFS, sauvegarde, progression ou réseau. Le lancement fournit seulement la
racine des données, déverrouille la licence et augmente la file audio à 64
frames. Tout comportement dépend donc du runtime ReXGlue exact, sans test
spécifique au titre.

Il n'existe aucune preuve publique de :

- fin de course, progression, pause/restart ou sauvegarde/reprise ;
- synchro XMA/vidéo, absence de drop ou latence bornée ;
- chemins VFS, écriture atomique ou reprise après corruption ;
- ordre de polls, hotplug, vibration ou comportement sans contrôleur.

Les 22 adresses manuelles ne portent aucun nom ou contrôle positif : elles ne
constituent pas une analyse sémantique du gameplay.

## Pack public, contenu retail et licence

La [release `game-packs-v1`](https://github.com/CrownParkComputing/xbox360-ports/releases/tag/game-packs-v1)
contient un ZIP Hydro de 19 entrées. Après extraction statique :

- 12 fichiers texte ; les deux répertoires de destination `assets/` et
  `gamedata/` sont vides ;
- aucun XEX, STFS, binaire, shader compilé, image, son ou C++ généré ;
- aucun chemin absolu, `..` ou lien symbolique ;
- les dix fichiers communs sont identiques octet par octet au tag et au HEAD ;
- le digest local reproduit exactement le digest GitHub.

Le [`.gitignore`, lignes 1–34](https://github.com/CrownParkComputing/xbox360-ports/blob/0216dae319eb5b61a7f1553d74529ca9e4ad55c5/.gitignore#L1-L34)
refuse XEX, assets, sorties générées, captures et builds. Le scan du pack et
des fichiers Hydro confirme l'absence de contenu retail brut évident. Il ne
peut pas prouver l'absence de toute information dérivée ; les adresses de
fonctions sont elles-mêmes issues d'une analyse d'un XEX non identifié.

Le pack n'embarque aucun LICENSE/NOTICE et le dépôt n'a pas de licence
déclarée. Le SDK séparé est BSD-3-Clause
([licence](https://github.com/CrownParkComputing/rexglue-sdk/blob/34b11ee6aed9d4ef914e49e6d8a8a092b02ced36/LICENSE)),
mais cela ne licence pas automatiquement le scaffold. Aucun code n'est donc à
copier ; seuls les invariants réimplémentés et tests négatifs sont retenus.

## Taxonomie AC6

| Élément | Classe | Conséquence |
|---|---|---|
| « Hydro works fine » et barils corrigés | `provisional-rexglue` | indication mainteneur sans reçu AV ni XEX |
| `exp_adjust` mot 3, bits 13–18 signés | `provisional-rexglue` | test générique à ajouter ; PAL M01 encore requis |
| mapping RT 10:10:10:2 et alignement resolve | `provisional-rexglue` | candidats seulement si les paquets PAL les utilisent |
| Title/media/module Xenia publics | `documented-unmatched` | aucune liaison au XEX du port |
| région, révision, eau/reflets, campagne/save | `documented-unmatched` | aucune preuve publique bornée |
| `--force` et fetch constants invalides autorisées | `divergent` | interdits dans les gates et le produit AC6 |
| vieux PPC fail-open s'il redevenait actif | `divergent` | trap/OOR/null ne doivent jamais retourner zéro silencieusement |
| input local neutre, cadence `PRESENT`, X11 forcé | `divergent` comme oracle de parité | ne scellent ni polls, ni ticks, ni plateforme |
| sémantique AC6 `retail-qualified` | **aucune** | autre titre, autre XEX, aucune exécution croisée |

## Actions retenues pour Mission 01

1. Ajouter une garde Xenos sur `exp_adjust` : mot 3 signé, indépendance vis-à-vis
   de `lod_bias`, cas négatif/nul/positif et contrôle image sur un draw PAL M01.
2. Si le census M01 trouve `k_2_10_10_10`, un resolve vide ou une destination
   tiled, tester format 10 bits, alignement 32, endian, bornes et no-op sans
   promouvoir l'hypothèse avant le contrôle retail.
3. Exiger dans tout reçu oracle XEX SHA-256, projet/module, SDK commit/tree,
   binaire hôte, GPU/driver, flags et zéro erreur masquée par `--force` ou
   `gpu_allow_invalid_fetch_constants`.
4. Conserver l'entrée replay poll-exact AC6 ; ne pas dériver la simulation des
   timestamps `PRESENT` ou de la fréquence d'écran.
5. Ne rien reprendre du scaffold Hydro. Obtenir l'eau, les reflets et la caméra
   depuis les paquets/scènes M01 PAL et des contrôles images qualifiés.
6. Maintenir l'audit du TGZ : rejet de XEX/STFS, code généré, runtime
   ReXGlue/Xenia, shaders ou captures retail, plus manifeste de licences.

## Validations de l'audit

- recherche GitHub puis clone du monorepo ; remote, HEAD, arbre, 26 commits,
  branche, tag, release et gitlink recoupés par API ;
- checkout statique du SDK exact `34b11ee6`, sans initialiser ni exécuter de
  jeu ; provenance ReXGlue/Xenia/XenonRecomp et absence de XenosRecomp vérifiées ;
- ZIP Hydro téléchargé depuis la release, SHA-256 reproduit, inventaire et
  extraction statiques, chemins et liens contrôlés ;
- deux TOML, le JSON de presets et le shell du pack parsés ; fichiers communs
  comparés au checkout ;
- recherche bornée des signatures XEX/PE et des familles generated/retail ;
- commits Xenia `32889f51` et fork `5ecb564b` comparés sur le champ
  `exp_adjust` ;
- permaliens GitHub du rapport contrôlés ;
- `git diff --no-index --check /dev/null
  reports/cycle-1565-hydro-thunder-water-renderer-review.md` propre.

## Risques résiduels

- les artefacts locaux ayant servi à la déclaration « works fine » ne sont pas
  publics ; une branche ou un build privé peut être plus complet ;
- faute de XEX SHA-256, les 22 frontières ne sont pas reproductibles et leur
  région reste inconnue ;
- le jeu, le runtime et l'archive n'ont pas été exécutés ; aucune fidélité eau,
  audio, input, cadence ou sauvegarde n'est donc qualifiée dynamiquement ;
- le scan de paquet exclut les conteneurs et signatures évidents, pas toute
  similarité dérivée à des octets retail ;
- les contrats Xenos retenus restent à confronter aux bytes et aux draws M01
  du PAL canonique avant toute fermeture de lane.
