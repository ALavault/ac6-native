# Cycle 1561 — Army of Two : audit du runtime XenonRecomp historique

Audit réalisé le 12 août 2026, borné au dépôt public vérifiable. Aucun octet
retail Army of Two n'a été copié dans le dépôt AC6, aucun binaire invité n'a
été exécuté et aucun code généré n'a été importé.

## Décision

Le seul dépôt public retrouvé pour Army of Two est un ancien bring-up
XenonRecomp, pas une migration ReXGlue vérifiable. Son HEAD contient une copie
très incomplète du runtime Unleashed, 249 unités C++ PPC générées, une couche
D3D haute partielle, aucune chaîne shader utilisable, aucune entrée manette
active, aucun audio et aucun test. Le README le dit bloqué sur un format de
texture inconnu.

Il ne fournit donc aucun fait `retail-qualified`, aucun composant à porter et
ne ferme aucune lane Mission 01. Sa valeur est principalement négative : il
montre exactement les défauts que les gates AC6 doivent détecter avant une
preview — contenu retail suivi, code généré embarqué, identité XEX non liée,
surface d'API abondante mais non routée, `assert` qui devient fail-open en
Release, cadence hôte non déterministe et statut public surestimé.

La ligne de l'inventaire « Oui, migration depuis XenonRecomp » reste
`documented-unmatched` : la recherche GitHub publique n'a retrouvé aucun autre
dépôt ni source ReXGlue Army of Two. Le dépôt audité correspond seulement à
l'ancien état XenonRecomp.

## Identité et reproductibilité publique

| Élément | Valeur observée |
|---|---|
| Dépôt | `Jellybaby34/ArmyofTwoRecomp` |
| Branche | `master` |
| HEAD | `75432a71565cc4a33b12a10a092b67ede3f1aaa4` |
| Arbre | `cdfd4b6e11caeeba39960552e66972bb96365455` |
| Historique | 12 commits, 14 mars au 2 juillet 2025, aucun tag |
| Licence racine | GPL-3.0 |
| Sous-modules | 14 gitlinks, non initialisés dans le clone de contrôle |
| Tests/CI | aucun `add_test`, aucun test suivi, aucun workflow GitHub |
| Dernier statut auteur | WIP, assertion sur format de texture inconnu |

Le [README, lignes 1–33](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/README.md#L1-L33)
dit explicitement que le projet est une copie d'Unleashed avec les fichiers
PPC Army of Two, que les instructions ajoutées au XenonRecomp local n'ont pas
été testées et que renderer, son, entrée, sauvegarde et réseau restent à faire.

Les presets déclarent Windows et Linux, mais un checkout sans sous-modules ne
se configure pas : vcpkg, fmt, xxHash et tomlplusplus manquent. Cette panne est
attendue pour un projet à sous-modules non initialisés ; elle ne prouve pas
qu'un checkout récursif échouerait. En revanche, l'absence de tests et de CI
reste factuelle. Les [presets Linux](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/CMakePresets.json)
ne constituent donc pas une preuve de build reproductible.

## Provenance et contenu retail

Le dépôt suit directement :

- `ArmyofTwoRecompResources/default.xex`, 19 349 504 octets ;
- SHA-256 `989f047de87aaa686680af90d68af61dfaac97a8bbcdf3f78b73debd04bfec0d` ;
- format reconnu localement comme XEX Xbox 360, media ID `38595BF0`, région USA ;
- blob Git `f2262d2abed52dd8cfdad9d070fe01f85cd2aca5` ;
- ajouté au commit HEAD.

Aucun lien de téléchargement direct n'est reproduit ici. Ce constat suffit à
classer le dépôt comme impropre à toute reprise ou redistribution. Son
[`.gitignore`](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/.gitignore)
est le modèle Visual Studio générique et ne refuse ni XEX ni sorties de
recompilation.

Le dépôt suit également 249 `ppc_recomp.N.cpp`, environ 270 070 705 octets,
plus une table de 63 255 fonctions. Le
[CMake de la bibliothèque PPC, lignes 3–29](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecompLib/CMakeLists.txt#L3-L29)
compile explicitement ces unités. Même sans le XEX, ce corpus généré est hors
frontière du produit C++ manuscrit AC6.

Conséquence AC6 : l'audit du TGZ doit scanner récursivement noms et binaires,
pas seulement vérifier l'absence de `DATA00.PAC`. Les familles `.xex`,
`ppc_recomp.*`, `ppc_func_mapping.*`, `PPCFuncMappings` et les marqueurs de
runtime oracle doivent toutes être refusées.

## CPU, ABI et horloge

La [configuration XenonRecomp](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecompLib/config/config.toml)
conserve `skip_lr=false`, ce qui est favorable à l'observation des callers,
mais elle ajoute 129 frontières manuelles et référence un `switch.toml` absent
du dépôt. Le README consigne deux jump tables non résolues et un build local du
recompilateur avec des instructions ajoutées sans tests. Les 63 255 entrées de
la table générée ne valent donc pas 63 255 fonctions qualifiées.

Le code généré illustre bien la traduction SIMD possible : le contexte utilise
les intrinsics x86, le build impose `-march=sandybridge`, et les opérations
VMX/VMX128 sont émises en SSE, notamment via `_mm_dp_ps`. C'est une technique
de performance, pas une preuve sémantique. L'ordre des additions, les
estimations réciproques, NaN, exceptions, VSCR et arrondis doivent rester
qualifiés instruction par instruction avant tout port AC6.

Plusieurs divergences génériques sont visibles :

- le wrapper hôte lit tous les arguments entiers de registres comme `u32`, y
  compris lorsqu'un prototype C++ attend 64 bits ; voir
  [`function.h`, lignes 40–61](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/kernel/function.h#L40-L61) ;
- les arguments flottants et entiers utilisent des ordinaux distincts encore
  non testés, et les arguments de pile flottants retournent zéro ; voir
  [lignes 63–128](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/kernel/function.h#L63-L128)
  et [196–227](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/kernel/function.h#L196-L227) ;
- chaque `mftb` généré lit directement `__rdtsc`, tandis que
  `KeQueryPerformanceFrequency` annonce 49 875 000 Hz sans conversion du TSC
  hôte ; la seconde moitié est visible dans
  [`imports.cpp`, lignes 839–856](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/kernel/imports.cpp#L839-L856) ;
- `Translate` et `MapVirtual` ne bornent les adresses que par `assert`, donc les
  contrôles disparaissent en Release ; voir
  [`memory.h`, lignes 26–61](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/kernel/memory.h#L26-L61).

Ce runtime ne peut pas être un oracle de cadence. Pour AC6, le compteur invité,
le marqueur de frame et la cadence doivent être observés et scellés ; ni le TSC
hôte ni une constante déclarée par le producteur ne suffisent.

## Chargeur et identité XEX

Le runtime charge `./game/default.xex`, mais ne vérifie ni SHA-256, ni media ID,
ni version, ni taille de l'image contre le corpus C++ généré. Le
[`LdrLoadModule`, lignes 150–197](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/main.cpp#L150-L197)
interprète les headers sans bornes de fichier, ne couvre que les compressions
`NONE` et `BASIC`, copie directement dans la mémoire invitée et laisse une
compression inconnue continuer après un `assert` en Release.

Le [démarrage, lignes 249–274](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/main.cpp#L249-L274)
lance ensuite l'entry point de ce fichier arbitraire avec la table de fonctions
précompilée. Un XEX de mauvaise révision peut donc être couplé silencieusement
au mauvais codegen.

Garde AC6 correspondante : identité avant exécution `{sha256, taille, media_id,
title_id, version, base_version, image base/size, config codegen}` et rejet de
toute différence avant création du premier thread invité.

## Renderer et Xenos

L'architecture ne possède pas de command processor PM4. Comme Unleashed, elle
intercepte des méthodes D3D/XDK liées statiquement au jeu et les transforme en
commandes hôte. Le bas du fichier active seulement 29 hooks Army of Two
(`CreateDevice`, ressources, états, draws et shaders) ; le hook `Present` reste
commenté et l'import `VdSwap` est un stub. Voir
[`video.cpp`, lignes 7677–7728](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/gpu/video.cpp#L7677-L7728)
et [`imports.cpp`, lignes 864–878](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/kernel/imports.cpp#L864-L878).

La chaîne shader est structurellement ouverte :

- les tableaux DXIL/SPIR-V et la table de shaders ont une taille nulle ;
  [`video.cpp`, lignes 35–52](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/gpu/video.cpp#L35-L52) ;
- `GetOrLinkShader` journalise un stub et retourne `nullptr` ;
  [lignes 3752–3769](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/gpu/video.cpp#L3752-L3769) ;
- l'affectation des vertex/pixel shaders dans le pipeline est commentée ;
  [lignes 3959–3969](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/gpu/video.cpp#L3959-L3969) ;
- `ConvertFormat` ne reconnaît qu'un petit sous-ensemble et retourne
  `R16G16B16A16_FLOAT` après un `assert(false)` pour tout format inconnu ;
  [lignes 2979–3007](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/gpu/video.cpp#L2979-L3007) ;
- plusieurs chemins de cache d'origine Sonic déréférencent le résultat de
  `FindShaderCacheEntry` sans garde, alors que le cache est vide. Leur
  atteignabilité Army of Two n'est pas démontrée, mais ils interdisent toute
  promotion fail-closed.

Le dépôt ne suit ni source XenosRecomp ni shaders compilés. Le petit
`shader_cache.h` décrit seulement le format d'une entrée ; son `.cpp` ne
contient qu'un include et n'est pas compilé par la bibliothèque. Ce n'est donc
pas un exemple de traduction Xenos fonctionnelle.

Pour M01, le seul enseignement réutilisable est architectural et déjà confirmé
par ReOdyssey/Unleashed : une frontière D3D haute peut alimenter des
`DrawPacket` natifs. Army of Two ajoute la garde négative : une liste de hooks
de draw sans shaders, formats, profondeur, présentation et tests image ne
constitue pas une lane JV.

## XAM, contrôleurs et progression artificielle

`XamInputGetState` retourne immédiatement `1`. Tout le chemin HID/clavier,
pourtant présent dans le fichier, est commenté ; voir
[`xam.cpp`, lignes 409–497](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/kernel/xam.cpp#L409-L497).
Il n'existe ni capture, ni replay, ni seam poll-exact.

La présence de code SDL ne prouve donc pas une entrée fonctionnelle. C'est un
cas de test utile pour l'audit de couverture : chaque import doit être suivi
jusqu'au code effectivement compilé et jusqu'à un effet observé, pas seulement
retrouvé par nom de symbole.

La boîte de dialogue XAM est également divergente : en Release elle complète
automatiquement l'overlapped et envoie un événement, après avoir choisi la
valeur déjà placée dans `pResult`. Voir
[`xam.cpp`, lignes 205–258](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/kernel/xam.cpp#L205-L258).
Un boot qui franchit un écran grâce à cet automatisme ne prouve ni input humain
ni logique retail.

## Audio et synchronisation

Le démarrage affiche « AUDIO NEEDS TO BE INCLUDED » et laisse
`XAudioInitializeSystem` commenté ; voir
[`main.cpp`, lignes 93–147](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/main.cpp#L93-L147).
`XMACreateContext`, `XMAReleaseContext` et les callbacks render XAudio sont des
stubs dans
[`imports.cpp`, lignes 1631–1675](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/kernel/imports.cpp#L1631-L1675).

Le limiteur de présentation utilise `steady_clock`, `sleep_for` et une attente
active à 60 Hz ; voir
[`video.cpp`, lignes 2761–2783](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/gpu/video.cpp#L2761-L2783).
Cette cadence hôte n'est pas une horloge audio/guest et ne qualifie aucune
synchronisation A/V.

## VFS, contenu et sauvegardes

Le registre XContent indexe uniquement par hash du nom, sans conserver le nom
comme garde de collision et sans verrou visible ; voir
[`xam.cpp`, lignes 86–138](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/kernel/xam.cpp#L86-L138).

`ResolvePath` remplace les séparateurs mais ne canonicalise pas et ne vérifie
pas que le résultat reste sous la racine montée. Les segments `..` et les
symlinks ne sont donc pas fermés par ce code ; voir
[`file_system.cpp`, lignes 367–409](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/kernel/io/file_system.cpp#L367-L409).

Deux erreurs supplémentaires sont directement testables :

- `WIN32_FIND_DATAA` reçoit la moitié haute dans `nFileSizeLow` et la moitié
  basse dans `nFileSizeHigh` ;
  [`file_system.cpp`, lignes 69–82](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/kernel/io/file_system.cpp#L69-L82) ;
- `XWriteFile` renvoie `gcount()` après une écriture, alors que `gcount()`
  décrit la dernière lecture non formatée ;
  [lignes 353–365](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/kernel/io/file_system.cpp#L353-L365).

Le répertoire utilisateur porte encore le nom `UnleashedRecomp`. Plus grave,
`GetSaveFilePath(true)` et `GetSaveFilePath(false)` retournent le même chemin,
donc le fallback de démarrage tente de copier `SYS-DATA` sur lui-même lorsqu'il
n'existe pas. Voir
[`paths.h`, lignes 5–37](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/user/paths.h#L5-L37)
et le [site de copie](https://github.com/Jellybaby34/ArmyofTwoRecomp/blob/75432a71565cc4a33b12a10a092b67ede3f1aaa4/ArmyofTwoRecomp/main.cpp#L104-L124).

Ces cas confortent les contrats AC6 existants : cache importé par hashes,
résolution sous racine canonique, écritures temporaires puis rename, compte
d'octets explicite, corruption testée et sauvegarde indépendante du runtime
oracle.

## Matrice de confiance

| Domaine | État | Motif |
|---|---|---|
| Statut « migration ReXGlue » | `documented-unmatched` | aucun dépôt/source public retrouvé |
| Codegen PPC | `divergent` | instructions locales non testées, switch absent, code généré suivi |
| ABI hooks | `divergent` | arguments 64 bits tronqués, pile FP incomplète |
| Horloge | `divergent` | `mftb=__rdtsc`, fréquence invitée non reliée |
| XEX/VFS | `divergent` | identité et bornes absentes, chemins non confinés |
| XAM/input | `divergent` | import actif stub, code SDL mort |
| Renderer | `divergent` | caches shaders vides, Present absent, formats fail-open |
| XenosRecomp | `documented-unmatched` | aucun outil ni shader généré suivi |
| XMA/audio | `divergent` | initialisation et contextes stubs |
| Save | `divergent` | fallback source=destination, pas d'atomicité |
| Toute sémantique AC6 | non qualifiée | autre titre, autre XEX, aucune exécution croisée |

## Actions AC6 retenues

1. Étendre l'audit du paquet aux noms `ppc_recomp.*` et
   `ppc_func_mapping.*`, ainsi qu'au symbole binaire `PPCFuncMappings`.
2. Conserver le gate d'identité avant toute exécution d'un oracle recompilé.
3. Ajouter à terme un audit de couverture des seams critiques :
   `XamInputGetState`, présentation, XMA, VFS et sauvegarde doivent avoir un
   effet positif testé, pas seulement une fonction ou un hook enregistré.
4. Refuser les formats/commandes inconnus par erreur structurée en Release ;
   aucun `assert(false); return fallback` dans le produit.
5. Ne reprendre aucun code Army of Two : GPL-3.0, code généré dérivé et contenu
   retail rendent toute copie incompatible avec la frontière du produit.

## Validations

- HEAD, arbre, historique, branche, licence et 14 gitlinks recoupés localement ;
- recherche publique GitHub dépôt/code pour `ArmyofTwoRecomp`, `Army of Two`
  et `rexglue` : aucun second dépôt technique retrouvé ;
- inventaire suivi : 456 fichiers, 249 TU PPC, 63 255 mappings ;
- SHA-256, taille, type, media ID et blob Git du XEX enregistrés sans en copier
  les octets ;
- configuration Linux tentée sur checkout propre : échec borné aux sous-modules
  non initialisés, aucune prétention de build complet ;
- absence de tests/CI vérifiée par recherche CMake et arborescence ;
- 29 permaliens de source du rapport répondent HTTP 200 ;
- `git diff --check` requis avant publication.

## Risques résiduels

Une migration ReXGlue Army of Two peut exister dans un dépôt privé, supprimé
ou sous un autre nom ; elle n'est simplement pas vérifiable publiquement ici.
Les chemins copiés d'Unleashed peuvent contenir d'autres défauts non recensés,
mais leur exploration supplémentaire n'apporterait pas de gain M01 mesurable.
Le dépôt n'a pas été construit avec tous ses sous-modules et aucun XEX n'a été
lancé : l'audit conclut sur la source publique, pas sur une release externe.
