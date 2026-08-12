# Cycle 1564 — Midnight Club: Los Angeles : audit open-world/ReXGlue

Audit public réalisé le 12 août 2026. Aucun ISO, XEX, RPF ou autre contenu
retail MCLA n'a été téléchargé, copié dans AC6 ou exécuté. Les trois dépôts
publics retrouvés ont été lus statiquement. La seule release inspectée est un
ZIP hôte de 16,3 Mo ; ses exécutables Windows n'ont pas été lancés.

## Décision

L'inventaire ne doit pas être rattaché à un unique dépôt sans nuance :

- [`mzzvxm/larecomp`](https://github.com/mzzvxm/larecomp/tree/cdfea396e487a3f4b03053827ffa1eda0e3b1e39) est le projet public
  principal et le plus ancien ; son auteur se présente dans le programme comme
  celui de « LA Recompiled ». Son HEAD public par défaut ne suit cependant ni
  README, ni sortie générée, ni test, ni release ; les changements les plus
  récents sont sur une branche `development` non fusionnée ;
- [`zarif98/midnightclub`](https://github.com/zarif98/midnightclub/tree/08b6e5e443e3812b0e55106eda7eecf0e13a8828) est une
  réalisation indépendante. C'est elle qui publie la preuve la plus concrète
  correspondant à la ligne d'inventaire : README « ville, trafic, sauvegarde,
  free roam/race selection » et GIF de 44 secondes montrant boot, ville, HUD et
  conduite ;
- [`3bdull4h2008/mcla-recompilation`](https://github.com/3bdull4h2008/mcla-recompilation/tree/7f849a1583383efdd9ad5f8f1898908976bcdf8f)
  est un agrégat ultérieur d'un seul commit. Il annonce davantage qu'il ne
  permet de reconstruire : deux gitlinks sans `.gitmodules`, aucun code généré,
  deux workflows rouges et une release ajoutée manuellement après l'échec du
  workflow de release.

La qualification correcte est donc `provisional-rexglue`. Il n'existe aucune
preuve `retail-qualified` : pas de SHA-256 XEX lié à l'exécutable hôte, pas de
trace d'entrées, pas de compteurs/ticks scellés, pas de capture oracle, pas de
comparaison audiovisuelle et pas de test de campagne. « Menus, ville, free
roam et sélection de courses » est crédible comme état de bring-up ; seul
boot/ville/HUD/conduite dispose d'une preuve visuelle publique, elle-même non
contrôlée. Cette revue ne ferme aucune lane AC6 Mission 01.

Sa valeur AC6 est néanmoins réelle sur trois points :

1. un conflit de dialectes PowerPC a décodé un `vsldoi128` comme `mullhwu.` et
   provoqué un défaut de collision ; c'est une garde VMX128 directement
   pertinente ;
2. la ville n'a nécessité dans le projet hôte qu'un montage VFS `t:` correct,
   le reste du renderer restant dans ReXGlue et le code invité ; c'est un seam
   d'intégration, pas une implémentation open-world réutilisable ;
3. le sous-projet CodeX expose des hypothèses utiles sur secteurs/AABB, jointure
   matériau-texture et tiling BC1/2/3, mais aussi des défauts objectifs qui
   interdisent toute reprise : formats inconnus rabattus sur BC1, mips/cubemaps
   non chargés, taille A8R8G8B8 erronée, absence de tests et absence de licence.

## Identités publiques

| Projet | Révision qualifiée | État public vérifiable |
|---|---|---|
| `mzzvxm/larecomp`, `main` | HEAD `cdfea396e487a3f4b03053827ffa1eda0e3b1e39`, arbre `be86ec5e3c0fa94d34479ce5f377ed0556561b60`, 14 commits | branche par défaut, 8 fichiers, aucun tag/release/CI/licence/README |
| même dépôt, `development` | HEAD `fb883f087345fb490da8d5895af48d26d153f0e7`, arbre `24791716d66c02f1d149536618d4a2557384edc5` | installer ISO, hooks QoL et Discord ; non fusionné dans `main` |
| `zarif98/midnightclub`, `master` | HEAD `08b6e5e443e3812b0e55106eda7eecf0e13a8828`, arbre `40bc1b2ec530020da46bb3847d4ee350172ac5f5`, 7 commits | codegen suivi, GIF de gameplay ; aucun tag/release/CI/test/licence |
| `3bdull4h2008/mcla-recompilation`, `main` | HEAD/arbre `7f849a1583383efdd9ad5f8f1898908976bcdf8f` / `d4dc1f9cfd299667b36bce915c15bf838ae7a70c` | un commit ; tag léger et pré-release GitHub `v0.1.0` sur le HEAD ; deux workflows en échec |
| `Foxxyyy/CodeX.Games.MCLA`, gitlink annoncé | commit `3e0826f4b13fc328978b7a2e42bc5c8a57e76676`, arbre `e352c23c885977b167d55f9e6104035d2f642c1b` | viewer/éditeur C# de formats RPF3/RSC5, pas le runtime natif |

Le commit principal MCLA affirme un build testé avec
`ReXGlue v0.8.0.31-rc.gcd778a8`. Le préfixe résout vers le commit SDK
[`cd778a8b0645753d130a59f4283d46352f955789`](https://github.com/rexglue/rexglue-sdk/commit/cd778a8b0645753d130a59f4283d46352f955789),
mais ce pin n'est ni un sous-module ni un verrou CMake : le
[`manifest`, lignes 1–10](https://github.com/mzzvxm/larecomp/blob/cdfea396e487a3f4b03053827ffa1eda0e3b1e39/larecomp_manifest.toml#L1-L10)
ne conserve que `sdk_version = "0.8.0.31"`, et `generated/` est ignoré.

Le dépôt Zarif demande un package numérique `0.7.8.2` dans son
[`rexglue.cmake`, lignes 5–25](https://github.com/zarif98/midnightclub/blob/08b6e5e443e3812b0e55106eda7eecf0e13a8828/generated/rexglue.cmake#L5-L25),
sans source SDK ni sous-module. Le commit mentionne « 0.7.8 », mais il est
impossible de rattacher sans ambiguïté le suffixe `.2-dev` à un commit public.
Le correctif de dialecte utile est, lui, identifiable au commit SDK
[`fb2773781ad4ec562c4a1c5d36a00195ccb199b1`](https://github.com/rexglue/rexglue-sdk/commit/fb2773781ad4ec562c4a1c5d36a00195ccb199b1).

Le troisième dépôt publie dans son
[`manifest`, lignes 1–10](https://github.com/3bdull4h2008/mcla-recompilation/blob/7f849a1583383efdd9ad5f8f1898908976bcdf8f/mcla_rexglue/mcla_manifest.toml#L1-L10)
`ReXGlue v0.8.1.68-dev.g8dadea6`, soit le commit public
[`8dadea63d5ccbd92e4c340c76af587d0cc251250`](https://github.com/rexglue/rexglue-sdk/commit/8dadea63d5ccbd92e4c340c76af587d0cc251250).
Il ne suit toutefois pas `rexglue-sdk-src`. La release se présente dans ses
chaînes comme `[rexglue-v0.8.0.0-dev.unknown-Release]` : aucune attestation ne
lie donc le binaire publié à ce commit, au manifest ou au XEX annoncé.

Aucun des trois projets MCLA n'utilise directement XenonRecomp ou
XenosRecomp à sa révision auditée. CPU, services et Xenos sont fournis par
ReXGlue. Les outils Xenon/Xenos ne peuvent donc pas être crédités d'un pin ou
d'une validation indépendante dans cet état public.

## XEX, région, provenance et frontière retail

Les dépôts principal et Zarif désignent « Complete Edition » et le second
emploie le chemin humain `Midnight Club ... Complete Edition (USA, Europe)`
dans sa
[`configuration`, lignes 6–8](https://github.com/zarif98/midnightclub/blob/08b6e5e443e3812b0e55106eda7eecf0e13a8828/midnightclub_config.toml#L6-L8).
Ce libellé n'est ni un bit de région lu du XEX ni un reçu d'identité.

La branche `development` du projet principal ajoute un bon contrôle minimal :
elle reconnaît XDVDFS, borne les chemins, exige `XEX2` et vérifie le Title ID
`0x545407F8` dans l'Execution Info
([lecture et contrôle, lignes 206–264](https://github.com/mzzvxm/larecomp/blob/fb883f087345fb490da8d5895af48d26d153f0e7/src/isoinstaller/larecomp_iso_installer.cpp#L206-L264)).
Le Title ID ne distingue cependant ni révision, ni région, ni mise à jour. Il
n'existe aucun SHA-256, Media ID ou manifeste de fichiers attendu.

Le troisième dépôt est le seul à publier un fingerprint XEX : taille
9 252 864 octets et SHA-1
`38084797f60cf920069452bc36f7bb38ee8b8494`, dans son
[`rapport`, lignes 6–32](https://github.com/3bdull4h2008/mcla-recompilation/blob/7f849a1583383efdd9ad5f8f1898908976bcdf8f/MCLA_REVERSE_ENGINEERING_REPORT.md#L6-L32).
Ce fingerprint est une déclaration non accompagnée du XEX — ce qui est sain —
mais aucun reçu ne lie ce SHA-1 au codegen, au binaire de release ou à une
région. Le même rapport signale des valeurs de stack/TLS « probablement
tronquées » ; il n'est pas un parseur de référence.

Constats de frontière :

- aucun `.xex`, `.rpf`, `.iso`, `.bik`, XMA ou conteneur retail n'est suivi
  dans les trois arbres ;
- le dépôt principal ignore explicitement `assets/` et `generated/` ;
- le dépôt Zarif suit en revanche 60 unités de fonctions générées, leurs
  tables et macros : 64 fichiers, 126 504 587 octets, environ 4,09 millions de
  lignes et 29 784 définitions de fonctions. Il suit aussi un GIF de capture
  de 100 469 614 octets. Ce sont des artefacts dérivés/visuels sans licence,
  impropres à toute reprise dans le produit C++ manuscrit AC6 ;
- le sous-projet CodeX suit quatre captures de contenu MCLA et un corpus de
  chaînes de 3,49 Mo, également sans licence racine ;
- aucun des quatre dépôts n'a de licence racine. Même les sources manuscrites
  ne sont donc pas réutilisables juridiquement par défaut.

La frontière AC6 reste inchangée : ne copier ni code généré, ni tables de
fonctions, ni capture retail, ni parseur CodeX. Seuls les invariants génériques
réécrits et requalifiés sur les octets PAL AC6 sont recevables.

## Release tierce : contenu et reproductibilité

La [release `v0.1.0`](https://github.com/3bdull4h2008/mcla-recompilation/releases/tag/v0.1.0)
de `3bdull4h2008/mcla-recompilation` a été inventoriée
avant extraction statique :

| Artefact | Valeur |
|---|---|
| ZIP | `mcla-v0.1.0-win64.zip`, 16 295 761 octets |
| SHA-256 GitHub et local | `e30473204d7c67346836659515170175e1a422800fac6a740b5b029ebfe57a4a` |
| Membres | 6 entrées, toutes sous le préfixe Windows `mcla-release-v0.1.0\` |
| Hôte | `mcla.exe` 39 987 200 o ; `rexruntime.dll` 12 422 144 o ; `TracyClient.dll` 241 152 o |
| Autres | `gamecontrollerdb.txt` 599 249 o ; `mcla.toml` 478 o ; `README.txt` 849 o |
| Retail | aucun XEX/RPF/ISO/vidéo/audio ; aucun marqueur ASCII `XEX2` dans l'EXE ou le runtime |

Le binaire charge `rexruntime.dll` et contient du code PPC recompilé ; il est
donc exactement le type d'artefact généré que le paquet AC6 doit refuser, même
en l'absence d'un XEX littéral. La configuration livrée force notamment
`target_fps=60`, `vsync=false`, échelle de rendu 2, invalid fetch constants et
alpha fuzzy : c'est `divergent`, pas un profil retail.

La chaîne de publication n'est pas reproductible : les gitlinks `360tools` et
`CodeX.Games.MCLA` existent sans `.gitmodules`, donc le checkout récursif
échoue. Le
[`workflow build`, lignes 18–76](https://github.com/3bdull4h2008/mcla-recompilation/blob/7f849a1583383efdd9ad5f8f1898908976bcdf8f/.github/workflows/build-test.yml#L18-L76)
s'est arrêté au checkout dans le
[run `29353009305`](https://github.com/3bdull4h2008/mcla-recompilation/actions/runs/29353009305) ;
le job tests a été sauté. Le
[`workflow release`, lignes 15–84](https://github.com/3bdull4h2008/mcla-recompilation/blob/7f849a1583383efdd9ad5f8f1898908976bcdf8f/.github/workflows/release.yml#L15-L84)
a également échoué au checkout dans le
[run `29355297353`](https://github.com/3bdull4h2008/mcla-recompilation/actions/runs/29355297353),
avant toute compilation ou archive. Le ZIP a
été téléversé ensuite par le propriétaire. Il ne contient ni notices de
licence ni SBOM. Sa présence n'est pas une preuve de build, encore moins de
fidélité.

## CPU, VMX128 et SIMD : résultat à conserver

Le résultat le plus fort de l'audit est le commit Zarif
[`08b6e5e443e3812b0e55106eda7eecf0e13a8828`](https://github.com/zarif98/midnightclub/commit/08b6e5e443e3812b0e55106eda7eecf0e13a8828).
Avant régénération, l'instruction à `0x825E18C4` était décrite comme
`mullhwu.` et avait reçu une implémentation scalaire manuelle. Après correction
du désassembleur, les mêmes octets sont émis comme `vsldoi128` et traduits par
un intrinsic SIMDe d'alignement de vecteurs. Le mainteneur relie le défaut
antérieur à la voiture traversant la route.

Le correctif ReXGlue correspondant
[`fb277...`, `disasm.cpp`](https://github.com/rexglue/rexglue-sdk/blob/fb2773781ad4ec562c4a1c5d36a00195ccb199b1/src/codegen/ppc/disasm.cpp#L13-L36)
remplace le dialecte binutils textuel par un masque Xenon explicite : PPC,
64-bit, POWER4, CELL, AltiVec, VMX128 et CLASSIC. Il n'ajoute toutefois aucun
test au même commit.

Conséquences AC6 :

- une fonction qui « ressemble » à de la physique n'autorise jamais une
  sémantique fondée sur le mnemonic d'un seul désassembleur ;
- la garde générique doit tester sur opcode synthétique que le dialecte Xenon
  choisit VMX128 avant PPC405, puis sceller la révision de l'outil ;
- pour AC6, chaque instruction ambiguë reste liée aux bytes PAL, au projet
  Ghidra canonique et à un contrôle positif. Le croisement ReXGlue corrigé +
  XenonAnalyse peut réduire les micro-exécutions, pas les abolir ;
- le C++ généré emploie bien des intrinsics SIMD portables que Clang peut
  abaisser vers SSE/AVX. Cette observation prouve une possibilité de codegen,
  pas l'équivalence d'arrondi, VSCR, NaN ou exceptions.

Ce cas justifie une garde durable AC6 « aucun opcode VMX128 ne peut être
reclassé PPC405 sans divergence explicite ». Il ne justifie pas de copier la
fonction MCLA ou ses bytes.

## Renderer, ville et streaming

Le dépôt Zarif ne contient aucun renderer propre au titre. Il délègue Xenos,
EDRAM, shaders, textures, mips et présentation à `rex::graphics`; son CMake ne
compile que `src/main.cpp` plus le codegen
([CMake, lignes 13–26](https://github.com/zarif98/midnightclub/blob/08b6e5e443e3812b0e55106eda7eecf0e13a8828/CMakeLists.txt#L13-L26)).
La capture du README montre qu'un chemin ReXGlue peut présenter la ville, mais
elle ne qualifie aucun fetch, format, mip, resolve EDRAM, shader, culling ou
frame précis.

Le seam hôte déterminant est plus simple : le jeu cherche les fichiers de
ville/art/collision sur `t:`. L'app enregistre `t:` vers
`\Device\Harddisk0\Partition1`
([`midnightclub_app.h`, lignes 78–83](https://github.com/zarif98/midnightclub/blob/08b6e5e443e3812b0e55106eda7eecf0e13a8828/src/midnightclub_app.h#L78-L83)).
Le projet principal fait de même par `game_data_root`, et la branche
`development` ajoute plusieurs variantes slash/backslash. C'est une hypothèse
`provisional-rexglue` utile : l'identité de volume et la normalisation des
chemins peuvent débloquer du streaming sans réécrire le moteur invité.

Ce seam ne s'applique pas directement au produit AC6 après import : AC6 ne doit
plus relire les PAC. La transposition correcte est un test de normalisation et
de résolution des aliases vers le cache v2, jamais un montage durable du
retail brut pendant `play`.

Le CodeX MCLA épinglé expose un modèle de viewer, distinct du jeu :

- `MCLAMap` construit un nœud par secteur, un BVH de boîtes de streaming et
  charge/décharge suivant la position
  ([initialisation, lignes 42–97](https://github.com/Foxxyyy/CodeX.Games.MCLA/blob/3e0826f4b13fc328978b7a2e42bc5c8a57e76676/MCLAMap.cs#L42-L97),
  [update, lignes 100–165](https://github.com/Foxxyyy/CodeX.Games.MCLA/blob/3e0826f4b13fc328978b7a2e42bc5c8a57e76676/MCLAMap.cs#L100-L165)) ;
- les secteurs publient nom et AABB, mais de nombreux champs restent inconnus
  et l'écriture n'est pas implémentée
  ([`Rsc5City.cs`, lignes 37–75](https://github.com/Foxxyyy/CodeX.Games.MCLA/blob/3e0826f4b13fc328978b7a2e42bc5c8a57e76676/RSC5/Rsc5City.cs#L37-L75)) ;
- les textures externes sont jointes aux paramètres shader par nom normalisé,
  hash Jenkins et cache de pack, puis propagées aux slots de géométrie
  ([`Rpf3FileManager.cs`, lignes 440–557](https://github.com/Foxxyyy/CodeX.Games.MCLA/blob/3e0826f4b13fc328978b7a2e42bc5c8a57e76676/RPF3/Rpf3FileManager.cs#L440-L557)).

La forme « secteur -> drawable -> shader param -> texture -> slot » peut aider
à structurer les receipts AC6 MDLP/MATE/NDXR. Les offsets, hashes et formats
RAGE ne sont pas transposables à AC6.

## Textures, endian, tiling et mips : hypothèse, pas code à porter

Le lecteur CodeX ouvre les dictionnaires XTD en big-endian
([`XtdFile.cs`, lignes 11–37](https://github.com/Foxxyyy/CodeX.Games.MCLA/blob/3e0826f4b13fc328978b7a2e42bc5c8a57e76676/Files/XtdFile.cs#L11-L37)).
Il lit un descripteur D3D, distingue texture normale/cube/volume, BC1/BC2/BC3,
A8R8G8B8 et L8, applique un swap par mot de 16 bits puis les fonctions
`XGAddress2DTiledX/Y`
([texture, lignes 143–185](https://github.com/Foxxyyy/CodeX.Games.MCLA/blob/3e0826f4b13fc328978b7a2e42bc5c8a57e76676/RSC5/Rsc5Texture.cs#L143-L185),
[unswizzle, lignes 699–816](https://github.com/Foxxyyy/CodeX.Games.MCLA/blob/3e0826f4b13fc328978b7a2e42bc5c8a57e76676/RPF3/Rpf3Crypto.cs#L699-L816)).

L'algorithme ne peut pas servir d'implémentation AC6 :

- tout format inconnu est silencieusement traité comme BC1 ;
- `MipLevels`, cube et volume sont lus mais seule une image de base est
  chargée ;
- la taille A8R8G8B8 est calculée comme `width*height`, sans facteur 4 ;
- le découpage BC utilise des divisions entières sans arrondi de blocs pour
  les dimensions non multiples de quatre ;
- le padding virtuel n'est traité que pour les dimensions inférieures à 128 ;
- aucun test, corpus positif, contrôle d'image ou rejet négatif n'est publié ;
- le projet est sans licence et dépend de `CodeX.Core`/`CodeX.Forms` absents
  ([projet, lignes 1–18](https://github.com/Foxxyyy/CodeX.Games.MCLA/blob/3e0826f4b13fc328978b7a2e42bc5c8a57e76676/CodeX.Games.MCLA.csproj#L1-L18)).

Pour AC6, seules les idées génériques « swap 16-bit avant untile » et
`XGAddress2DTiledX/Y` méritent une comparaison avec le catalogue Xenos local.
Elles restent `documented-unmatched` tant qu'un payload NTXR PAL borné, un mip
attendu et une image positive ne les confirment pas. Le fallback inconnu->BC1
doit au contraire devenir un test de rejet obligatoire.

## Caméra, culling, IA et missions

Aucun dépôt hôte ne réimplémente caméra, culling, trafic, IA ou logique de
course. Ces systèmes s'exécutent dans le code invité généré et restent sans
noms fiables. Le dépôt principal expose quelques labels `RaceGrid`,
`ReplayMgr` et `RaceDescription` dans son TOML, puis des hooks Discord dont les
commentaires disent explicitement qu'ils sont des « candidates » à confirmer
([`discord_hooks.cpp`, lignes 57–99](https://github.com/mzzvxm/larecomp/blob/fb883f087345fb490da8d5895af48d26d153f0e7/src/discord_rpc/discord_hooks.cpp#L57-L99)).

Les options permettant de court-circuiter collision et rubber-banding sont des
divergences de mod, non des implémentations : les hooks peuvent retourner
avant la fonction à `0x825CC930` et deux managers IA
([configuration, lignes 24958–24979](https://github.com/mzzvxm/larecomp/blob/fb883f087345fb490da8d5895af48d26d153f0e7/larecomp_config.toml#L24958-L24979)).
Ils ne renseignent ni producteurs, ni conditions, ni transitions.

Le troisième dépôt va plus loin dans la divergence : il remplace par succès
trois fonctions présentées comme compteurs/vertex GPU et affecte des
sémantiques Xbox Live, refcount et handle sans preuve
([`mcla_stubs.cpp`, lignes 17–107](https://github.com/3bdull4h2008/mcla-recompilation/blob/7f849a1583383efdd9ad5f8f1898908976bcdf8f/mcla_rexglue/src/mcla_stubs.cpp#L17-L107)).
Les accès objets ignorent en outre le pointeur `base` invité et l'endian. Cela
est `divergent` et ne doit pas informer les 339 compteurs AC6.

Conclusion gameplay : aucune information réutilisable pour activation de
cible, succès/échec ou tutoriel AC6. Les noms MCLA sont au mieux des points de
sonde à requalifier dans leur propre XEX.

## Input, replay et cadence

Les projets n'ont aucun enregistreur d'inputs, aucun poll ordinal, aucun format
de replay hôte et aucune comparaison de trace. `XamInputGetState` apparaît
seulement parmi les imports gérés par ReXGlue. Les noms `ReplayMgr` du TOML ne
sont accompagnés d'aucune implémentation ni exécution contrôlée.

La cadence publiée ne doit surtout pas être reprise :

- la branche MCLA `development` remplace `Sleep` sous Windows par
  `steady_clock`, `sleep_for` puis busy-wait
  ([`threading.cpp`, lignes 39–78](https://github.com/mzzvxm/larecomp/blob/fb883f087345fb490da8d5895af48d26d153f0e7/src/mc_engine/threading.cpp#L39-L78)) ;
- le troisième dépôt accroche `VdSwap` à l'horloge hôte et écrit delta/scale/
  compteur dans une prétendue zone scratch
  ([`mcla_app.h`, lignes 23–118](https://github.com/3bdull4h2008/mcla-recompilation/blob/7f849a1583383efdd9ad5f8f1898908976bcdf8f/mcla_rexglue/src/mcla_app.h#L23-L118)) ;
- `UpdateDeltaTime` n'est appelé nulle part dans les sources suivies ;
- les adresses scratch `0x8235427C–0x82354298` se trouvent à l'intérieur de la
  fonction générée `0x82354230–0x82354A38` dans le corpus indépendant Zarif,
  donc la qualification « scratch » contredit la carte de code publique ;
- le profil release force pourtant `target_fps=60`.

Ces constats invalident statiquement ce mécanisme, sans micro-exécution. Ils
renforcent la stratégie AC6 déjà retenue : replay poll-exact, entrées
normalisées, compteur invité observé, horloge de référence scellée et cadence
native 60 Hz séparée. Une horloge hôte ou un `PRESENT` n'est jamais un tick de
gameplay.

## XMA, vidéo, VFS, sauvegarde et import

Les affirmations audio ne sont que `provisional-rexglue` : le principal projet
mentionne dans son historique musique au boot et radio encore cassée ; le
troisième README annonce XMA/FFmpeg ; son runtime contient effectivement les
symboles XMA/FFmpeg. Aucun flux, langue, cue, sous-titre, vidéo, mesure dB ou
horodatage A/V n'est publié. Le viewer CodeX laisse même
`LoadAudioPack` en `NotImplementedException`
([lignes 559–562](https://github.com/Foxxyyy/CodeX.Games.MCLA/blob/3e0826f4b13fc328978b7a2e42bc5c8a57e76676/RPF3/Rpf3FileManager.cs#L559-L562)).

La sauvegarde est déclarée fonctionnelle par Zarif, mais l'app fixe à la main
les chemins Windows de données, logs et profils
([`midnightclub_app.h`, lignes 33–43 et 78–93](https://github.com/zarif98/midnightclub/blob/08b6e5e443e3812b0e55106eda7eecf0e13a8828/src/midnightclub_app.h#L33-L93)).
Il n'existe aucun test de création, reprise ou corruption.

L'installateur MCLA `development` possède deux qualités à conserver comme
invariants génériques : refus des composants `.`/`..`/absolus et bornes de
lecture du conteneur. Mais il extrait chaque fichier directement avec
`std::ios::trunc`, sans staging, digest global ni renommage atomique
([lignes 282–333](https://github.com/mzzvxm/larecomp/blob/fb883f087345fb490da8d5895af48d26d153f0e7/src/isoinstaller/larecomp_iso_installer.cpp#L282-L333)).
Il est donc un contre-exemple pour AC6 : le cache v2 doit rester transactionnel
et aucune PAC ne doit être relue après import.

## Linux, tests et CI

Le dépôt Zarif cible Windows x86-64 dans son README et inclut `windows.h` sans
garde dans l'app
([lignes 7–21](https://github.com/zarif98/midnightclub/blob/08b6e5e443e3812b0e55106eda7eecf0e13a8828/src/midnightclub_app.h#L7-L21)).
Il n'a aucun test ni workflow.

Le dépôt principal contient des presets Linux, mais son CMake lie `dxgi` sans
condition sur toutes les plateformes
([`CMakeLists.txt`, lignes 20–30](https://github.com/mzzvxm/larecomp/blob/cdfea396e487a3f4b03053827ffa1eda0e3b1e39/CMakeLists.txt#L20-L30)).
La branche `development` conserve ce lien, ajoute GTK et des fonctions
Windows-only, sans workflow. Un preset n'est pas une preuve de build Linux.

Le troisième projet cible explicitement D3D12/Windows. Ses deux runs GitHub
publics sont rouges : build/lint `29353009305`, release `29355297353`. Le
premier s'arrête au checkout récursif ; le second également. Le job tests est
sur un runner séparé, utilise `dotnet test --no-build` et ne récupère aucun
artefact du job build, autre défaut statique. Aucun test runtime, image, audio,
input, replay, Vulkan, X11 ou Wayland n'existe.

Linux MCLA reste donc `documented-unmatched`, et ce corpus ne réduit pas la
validation Linux/Vulkan d'AC6.

## Classement par besoin AC6 M01

| Besoin | Classement | Ce qui reste |
|---|---|---|
| VMX/VMX128 | `provisional-rexglue` utile | garde dialecte VMX128>PPC405 ; bytes PAL et contrôle positif AC6 toujours requis |
| Streaming/VFS | `provisional-rexglue` | alias de volume/path à tester ; aucun lecteur PAC/cache AC6 fourni |
| MDLP/MATE, jointure texture | `provisional-rexglue` comme forme de graphe | formats/offsets RAGE non transposables, source sans licence |
| NTXR/Xenos | `documented-unmatched` | lecteur CodeX incomplet et fail-open ; aucun contrôle image/mip/cubemap |
| EDRAM/shaders/present | `provisional-rexglue` runtime seulement | aucun code titre ni parity capture |
| Scene/TCAM/culling | `documented-unmatched` | viewer AABB/BVH seulement, aucune caméra retail |
| IA/événements/conditions | `divergent` ou absent | hooks de mods et labels candidats, aucun producteur qualifié |
| Input/replay/cadence | `divergent` ou absent | aucun poll replay ; horloge hôte MCLA rejetée |
| XMA/ASF/vidéo | `documented-unmatched` | symboles runtime sans flux/cues/mesures |
| Import/save | `provisional-rexglue` | vérification Title ID/path utile ; atomicité et identité insuffisantes |
| Linux/Vulkan | `documented-unmatched` | aucun build ou run public qualifié |
| Fidélité retail | aucune preuve | zéro `retail-qualified` |

## Actions AC6 retenues

1. Ajouter au corpus générique une garde synthétique de sélection du dialecte
   Xenon couvrant explicitement VMX128 contre PPC405, puis enregistrer le
   commit exact de chaque désassembleur/codegen.
2. Lorsqu'un défaut de physique/IA apparaît, vérifier d'abord la classe
   d'opcode et les bytes avant toute sémantique manuelle. Le cas MCLA montre
   qu'une micro-exécution ciblée coûte moins cher qu'un faux port scalaire.
3. Conserver `t:`/aliases comme piste de diagnostic VFS seulement ; pour AC6,
   les résoudre vers les artefacts importés et tester slash, backslash, casse,
   traversal et absence de PAC après import.
4. Utiliser la forme de jointure CodeX comme checklist de receipt
   `drawable -> matériau/shader -> paramètre -> texture -> slot`, sans copier
   code, hash ni offsets.
5. Ajouter/maintenir les négatifs NTXR : format inconnu rejeté, taille RGBA
   exacte, dimensions BC arrondies en blocs, tous les mips et toutes les faces
   consommés ou rejetés explicitement, contrôle image positif après
   endian+untile.
6. Garder le paquet AC6 hostile à `PPCFuncMappings`, sorties `*_recomp.*`,
   runtimes ReXGlue/Xenia et code généré. L'absence de XEX littéral ne suffit
   pas.
7. Ne prendre aucune cadence, IA, audio ou gameplay MCLA comme oracle. La
   stratégie replay synchronisé AC6 reste inchangée.

## Validations de l'audit

- recherche GitHub publique par titre, Title ID `545407F8`, `larecomp`,
  ReXGlue et XenonRecomp ; trois dépôts MCLA distingués ;
- clones complets, HEAD/arbre/branches/tags/historique recomptés ; aucune
  modification des clones ;
- métadonnées GitHub de releases, workflows, jobs et gitlinks vérifiées ;
- release tierce inventoriée avant extraction, SHA-256 GitHub/local identique,
  liste et tailles contrôlées, PE/imports/chaînes inspectés statiquement ;
- aucun exécutable hôte ou invité lancé ; aucun contenu retail téléchargé ;
- sous-projet CodeX lu à son commit gitlink exact ; dépendances, absence de
  licence/tests et limites du lecteur vérifiées ;
- permaliens de ce rapport ciblés sur des commits immuables ;
- `git diff --check` exécuté sur le rapport AC6.

## Risques résiduels

- des développements privés, Discord ou branches supprimées peuvent dépasser
  l'état GitHub public ; ils ne sont pas vérifiables et ne changent pas cette
  qualification ;
- le SHA-1 XEX tiers n'a pas été recalculé localement, conformément à
  l'interdiction de télécharger du retail ;
- le GIF prouve une séquence visuelle mais pas sa révision XEX, ses entrées, sa
  cadence, son audio, ni l'absence de stubs silencieux ;
- le binaire de release peut provenir du commit ReXGlue annoncé malgré sa
  chaîne `dev.unknown`, mais aucun reçu public ne permet de le démontrer ;
- aucun constat MCLA n'est une preuve binaire AC6 PAL. Les lanes checkpoint 2
  et les gates JF/JV/JP/JG restent ouvertes jusqu'à qualification AC6 propre.
