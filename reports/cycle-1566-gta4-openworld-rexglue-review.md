# Cycle 1566 — GTA IV : audit open-world/ReXGlue

Audit public arrêté au 12 août 2026. Aucun ISO, XEX, XEXP, STFS, RPF,
save ou autre contenu retail GTA IV n'a été téléchargé, ouvert ou exécuté. Les
exécutables hôtes n'ont pas été lancés. La seule release publique a été
inventoriée statiquement après contrôle de son SHA-256.

## Décision

Aucun dépôt GTA IV public audité n'est `retail-qualified` et aucun résultat ne
ferme une lane AC6 PAL Mission 01.

Trois lignées publiques distinctes ont été retrouvées :

- [`OZORDI/LibertyRecomp`](https://github.com/OZORDI/LibertyRecomp/tree/e952c8b78618e532a86d62f977e6400312ca2170)
  est un fork GitHub de `sonicnext-dev/MarathonRecomp`. Sa branche par défaut
  est incomplète et plus ancienne que ses branches de travail. La branche
  `beta` contient le travail GTA IV le plus avancé et 86 unités C++ générées,
  mais les propres documents du projet décrivent encore un crash pendant
  l'initialisation GPU.
  La branche la plus récente migre vers un SDK appelé « Graine », retire les
  sorties générées et n'a ni build public ni pin SDK propre ;
- [`luisxl15/GTA-IV-RECOMP-XBOX-360`](https://github.com/luisxl15/GTA-IV-RECOMP-XBOX-360/tree/6c9ca0b10299b36cb75f587cdaba3751914ec693)
  est le rapport de bring-up le plus cohérent. Le mainteneur annonce boot,
  cinématique et gameplay interactif, mais publie aussi l'absence globale de
  collision statique, les saves rejetées et des erreurs XMA. Il ne fournit ni
  capture, ni log brut, ni généré, ni binaire, ni CI et dépend d'une branche
  ReXGlue externe non épinglée ;
- [`Jenish094/GTA4Recomp`](https://github.com/Jenish094/GTA4Recomp/tree/23313a574cfa847922c13c4a1794369b2a145016)
  est une adaptation directe Marathon/Unleashed avec XenonRecomp et
  XenosRecomp. Son README dit explicitement que le build ne fonctionne pas.

La qualification utile est donc étroite :

- `provisional-rexglue` pour l'invariant de timebase Xenon à 50 MHz, le montage
  ordonné de partitions `cache:`/`cache1:` et la méthode d'instrumentation
  d'une machine d'état de streaming ;
- `documented-unmatched` pour les affirmations de boot/gameplay, renderer
  ReXGlue, XMA, cinématique, save et Linux sans artefact contrôlé ;
- `divergent` pour les appels invités inconnus ignorés, les hooks GPU de
  remplacement complet, les imports fail-open/destructifs et le VFS par
  recherche de sous-chaîne ;
- aucune donnée `retail-qualified`.

## Identités publiques et branches

| Projet | Révision auditée | Arbre | État |
|---|---|---|---|
| `OZORDI/LibertyRecomp`, `main` | [`e952c8b78618e532a86d62f977e6400312ca2170`](https://github.com/OZORDI/LibertyRecomp/commit/e952c8b78618e532a86d62f977e6400312ca2170), 9 mars 2026 | `a5ce65b6fa2c67ced87547b170e605a47ce64bcb` | branche par défaut, 17 700 fichiers suivis ; build statiquement incomplet |
| même dépôt, `beta` | [`3e98ca9d392a4b5618243e275469ac9cd7918e99`](https://github.com/OZORDI/LibertyRecomp/commit/3e98ca9d392a4b5618243e275469ac9cd7918e99), 23 juin 2026 | `1feae906c8729af5a8809f566933d2ef05f8bbd7` | branche technique principale, ReXGlue vendored 0.7.5, généré GTA IV suivi |
| même dépôt, `codex/graine-rexglue-migration` | [`1e0fb02f2687affb91e11c9850009807ff2b4c46`](https://github.com/OZORDI/LibertyRecomp/commit/1e0fb02f2687affb91e11c9850009807ff2b4c46), 18 juillet 2026 | `ddb5ef63919a370f4812213d0d25ae60464441c6` | deux commits après `beta`, SDK vendored 0.8.0, généré absent |
| `luisxl15/GTA-IV-RECOMP-XBOX-360`, `main` | [`6c9ca0b10299b36cb75f587cdaba3751914ec693`](https://github.com/luisxl15/GTA-IV-RECOMP-XBOX-360/commit/6c9ca0b10299b36cb75f587cdaba3751914ec693), 8 août 2026 | `5498e708550efa704bb5d01b9944e8d1b353de17` | 7 commits, 22 fichiers, sans sous-module |
| `Jenish094/GTA4Recomp`, `main` | [`23313a574cfa847922c13c4a1794369b2a145016`](https://github.com/Jenish094/GTA4Recomp/commit/23313a574cfa847922c13c4a1794369b2a145016), 29 janvier 2026 | `849a7606599b747898b974b96439406d25a27bd6` | 28 commits, build déclaré cassé |

Les 14 forks GitHub de LibertyRecomp ont aussi été inventoriés. Les branches
par défaut sont identiques au `main` ci-dessus. Le fork actif
`J-KING838/LibertyRecomp_arm64` ajoute neuf commits de presets/doc Linux ARM64
et deux corrections de configuration, jusqu'à
[`229016105a36f3ce8c41d30c8548cbfd81c3935f`](https://github.com/J-KING838/LibertyRecomp_arm64/commit/229016105a36f3ce8c41d30c8548cbfd81c3935f),
mais aucun workflow, test, artefact ou résultat gameplay. Il ne constitue pas
une quatrième preuve indépendante.

## XEX, région et frontière retail

Les trois projets ciblent GTA IV USA/TU8, pas AC6 PAL :

- LibertyRecomp configure `assets/default.xex` et
  `assets/default_v8.xex`, sans SHA-256
  ([configuration, lignes 1–17](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/glue/gta4-recomp/gta4_config.toml#L1-L17)).
  L'installateur sait lire le Title ID `0x545407F2` et exige NTSC-U, mais son
  contrôle de contenu est désactivé et ses quatre fichiers attendus ont zéro
  hash
  ([constante, lignes 27–49](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/LibertyRecomp/install/installer.h#L27-L49),
  [hashes, lignes 10–25](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/LibertyRecomp/install/hashes/game.cpp#L10-L25)) ;
- la migration Graine ne conserve que `sdk_version = "0.8.0"` et le chemin
  `assets/default_v8.xex`, toujours sans digest
  ([manifest, lignes 1–8](https://github.com/OZORDI/LibertyRecomp/blob/1e0fb02f2687affb91e11c9850009807ff2b4c46/glue/rexglue-sdk-main/gta4-recomp/gta4_manifest.toml#L1-L8)) ;
- le projet Luis publie le libellé humain TU8 et Media ID `6AC07221`, puis les
  chemins base/patch, mais aucun SHA-256, Title ID lu ou reçu de patch
  ([manifest, lignes 7–37](https://github.com/luisxl15/GTA-IV-RECOMP-XBOX-360/blob/6c9ca0b10299b36cb75f587cdaba3751914ec693/gta4_manifest.toml#L7-L37)) ;
- Jenish ne donne que « GTA4 (USA) » et exige trois RPF pré-extraits
  ([README, lignes 1–23](https://github.com/Jenish094/GTA4Recomp/blob/23313a574cfa847922c13c4a1794369b2a145016/README.md#L1-L23)).

La branche `beta` suit 86 fichiers `gta4_recomp.N.cpp` et leur table de
sources. Ils ont seulement été comptés et leur métadonnée de génération lue ;
aucune sémantique, adresse ou séquence issue de ce généré retail tiers n'a été
utilisée. La migration Graine ne les publie plus et son CMake échoue
explicitement s'ils n'ont pas été régénérés localement
([CMake, lignes 7–13](https://github.com/OZORDI/LibertyRecomp/blob/1e0fb02f2687affb91e11c9850009807ff2b4c46/glue/rexglue-sdk-main/gta4-recomp/CMakeLists.txt#L7-L13)).

Les adresses GTA IV, les noms RAGE, le Media ID et les fonctions générées sont
donc hors corpus AC6. Ils ne peuvent être croisés ni avec le projet Ghidra
canonique AC6, ni avec le SHA-256 PAL qualifié.

## ReXGlue, forks et outils de génération

### LibertyRecomp

Le `main` vend un arbre ReXGlue ordinaire, pas un sous-module : arbre
`f9f4d2b30429659cd7b2ec1c921a7e41081b7dcd`, version CMake `0.2.3`. Il épingle
directement :

- XenonRecomp
  [`c3714b8d7d35d202df293c4965b52bd74ae9df02`](https://github.com/sonicnext-dev/XenonRecomp/commit/c3714b8d7d35d202df293c4965b52bd74ae9df02),
  arbre `5115314b5dbaf0329b02110508891071dc585d44` ;
- XenosRecomp
  [`811240b0137dc9806ae1480d96314cf43941c4b9`](https://github.com/sonicnext-dev/XenosRecomp/commit/811240b0137dc9806ae1480d96314cf43941c4b9),
  arbre `a785a5475feb80e582b6c1836a995754f828a568`.

Ces deux commits sont publics, mais le checkout racine ne l'est pas : le
gitlink `tools/extract-xiso` n'a aucune entrée dans
[`.gitmodules`](https://github.com/OZORDI/LibertyRecomp/blob/e952c8b78618e532a86d62f977e6400312ca2170/.gitmodules),
et `git submodule status` termine à 128. Le CMake exige en outre
`generated/sources.cmake`, `shader_cache.cpp` et `crt_stubs.cpp`, tous absents
du même arbre
([CMake, lignes 13–25](https://github.com/OZORDI/LibertyRecomp/blob/e952c8b78618e532a86d62f977e6400312ca2170/LibertyRecompLib/CMakeLists.txt#L13-L25)).
Le symlink `LibertyRecompResources -> MarathonRecompResources` est aussi
pendant. Le `main` n'est donc pas un checkout construisible.

La branche `beta` remplace XenonRecomp par le codegen ReXGlue et vendorise un
arbre déclaré `0.7.5`. Ce n'est toujours pas un pin de commit : son arbre
`76871b5b39db7589177072ad4132049458e17480` diffère de l'arbre du tag ReXGlue
public
[`v0.7.5`](https://github.com/rexglue/rexglue-sdk/commit/89b8e57abd629d5fa721140c13e4a2a8755e82ac),
et inclut des modifications/titres locaux. Le lien README vers
`sonicnext-dev/rexglue-sdk` répondait 404 lors de l'audit. Les tests du SDK
sont forcés à `OFF`
([glue CMake, lignes 1–25](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/glue/CMakeLists.txt#L1-L25)).

Le pin Xenos de `beta` et Graine est
`134dc9aa6d5261ec3c6690e10d7ee1cad78334ad`. L'API du dépôt configuré
`sonicnext-dev/XenosRecomp` répond « No commit found », contrairement aux deux
pins ci-dessus. Les branches possèdent 52 gitlinks ; le premier gitlink
vendored `glue/rexglue-sdk-main/thirdparty/FFmpeg` n'a pas de mapping dans le
`.gitmodules` racine, donc leur `git submodule status` termine aussi à 128.

La migration Graine remplace cet arbre par un vendor CMake `0.8.0`, arbre
`98818c8e747ad5869169fd9b268f700bfad7ebc8`, sans le rattacher au tag ReXGlue
public
[`v0.8.0`](https://github.com/rexglue/rexglue-sdk/commit/2bdb97f95f154f32d281aaa08446ae007b8ca117).
Un numéro de version et un nom de migration ne suffisent pas à identifier les
patches CPU, kernel, Xenos ou XMA réellement utilisés.

### Projet Luis

Le dépôt n'embarque aucun SDK. Son README demande la branche mobile
`skate3-sdk-clean` de `mchughalex/rexglue-skate3` et l'application d'un patch
local
([README, lignes 47–70](https://github.com/luisxl15/GTA-IV-RECOMP-XBOX-360/blob/6c9ca0b10299b36cb75f587cdaba3751914ec693/README.md#L47-L70)).
Il n'épingle ni commit, ni tag, ni digest. Au moment de l'audit, cette branche
résolvait vers
[`7eb0faf7787f5e01333c228b8e3f03c32f7295ea`](https://github.com/mchughalex/rexglue-skate3/commit/7eb0faf7787f5e01333c228b8e3f03c32f7295ea),
arbre `70080ed29377a18d38474aa035c6bc0f76245fd9`, 94 commits après son seul tag
`v0.8.1.19`, alors que son CMake annonce encore `0.8.0`. Le patch GTA IV
s'applique proprement à ce HEAD, mais cette résolution d'audit n'est pas un
verrou du projet.

Ce fork contient des spécialisations Skate 3 jusque dans son VFS et son
logging. Il est sous BSD-3-Clause, tandis que le dépôt Luis n'a aucune licence.
Ce n'est donc ni un SDK neutre, ni une base dont le code titre peut être repris
dans AC6 sans réécriture et qualification.

### Jenish et rexdex

Jenish n'utilise pas ReXGlue. Ses gitlinks directs sont le XenonRecomp
`c3714b8...` déjà qualifié ci-dessus et XenosRecomp
[`3938ef6464e08542f31b3941ee4543c3df6d20d1`](https://github.com/sonicnext-dev/XenosRecomp/commit/3938ef6464e08542f31b3941ee4543c3df6d20d1),
arbre `8823a070888511cce6ecec9d89f7b5c2f3578200`
([gitmodules, lignes 43–48](https://github.com/Jenish094/GTA4Recomp/blob/23313a574cfa847922c13c4a1794369b2a145016/.gitmodules#L43-L48)).
Ces pins sont publics, mais le titre ne construit pas et aucun codegen exécuté
n'est publié.

Aucun des trois projets n'a de dépendance ou pin `rexdex`. Le fork ReXGlue
Skate 3 cite seulement XenonRecomp et rexdex comme inspirations dans son
README ; cela ne constitue pas une dépendance outillée ni une validation.

## Claims gameplay, releases, CI et tests

### LibertyRecomp

Le README `beta` se dit « early development », mais coche simultanément
ReXGlue, audio XMA et « full save system »
([lignes 9–15 et 27–48](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/README.md#L9-L48)).
Les trois images de performance du même README sont explicitement légendées
Crossover/Wine, Xenia et RPCS3, pas LibertyRecomp
([lignes 136–144](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/README.md#L136-L144)).

Les artefacts techniques contredisent l'idée d'un renderer terminé :

- l'étude renderer dit que toute l'infrastructure n'a « jamais été connectée »
  car les hooks étaient aux mauvaises adresses, que la couche PM4 est no-op et
  que quatre chargeurs FXC sont remplacés par des stubs
  ([synthèse, lignes 1–50](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/docs/rendering_research.md#L1-L50),
  [blocker, lignes 272–303](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/docs/rendering_research.md#L272-L303)) ;
- sa liste de travaux demande encore des fonctions de shader manquantes et la
  vérification d'une adresse de hook possiblement erronée
  ([lignes 360–371](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/docs/rendering_research.md#L360-L371)) ;
- une chronologie publique décrit un crash 174 ms après le chargement XEX,
  pendant la création des premiers render targets, avec un fallback de format
  inconnu vers RGBA8
  ([lignes 1–25 et 53–78](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/docs/post-fix-crash-research/14-frame-by-frame-chronology.md#L1-L78)).
  Les logs `/tmp/liberty_BEFORE.log` et `AFTER.log` cités ne sont pas suivis.

LibertyRecomp possède cinq tags et trois releases. `v1.0.0` et `v1.0.1` n'ont
aucun asset. La pré-release
[`v0.1.0-alpha`](https://github.com/OZORDI/LibertyRecomp/releases/tag/v0.1.0-alpha)
contient un ZIP macOS ARM64 de 34 210 996 octets, SHA-256 GitHub/local
`9833d72a337029d3dbbeb35fb94aba4fc31743f30f02f346edd0ccf2f3e27cd5`.
L'inventaire statique montre 11 entrées, un Mach-O arm64 et MoltenVK ; le
`Info.plist` annonce `v1.0.0`, pas `v0.1.0-alpha`, et aucune chaîne de commit ou
build ID ne rattache l'archive au tag. La release promet seulement
XenonRecomp/XenosRecomp et l'installateur, pas un boot gameplay.

Les 128 runs Actions publics se répartissent en 115 échecs, 10 annulations et
3 skips : zéro succès. Le run multi-plateforme
[`22826007207`](https://github.com/OZORDI/LibertyRecomp/actions/runs/22826007207)
échoue au checkout pour Linux, Flatpak, macOS et Windows avant configuration.
Les runs de release des commits `v0.1.0-alpha` et `v1.0.1` échouent aussi
([`20187929403`](https://github.com/OZORDI/LibertyRecomp/actions/runs/20187929403),
[`20188708505`](https://github.com/OZORDI/LibertyRecomp/actions/runs/20188708505)).
Le `main` courant ne contient plus de workflow. Aucun test titre n'est branché ;
les tests du SDK vendored sont désactivés.

### Projet Luis et Jenish

Luis revendique 35 888 fonctions recompilées, zéro appel non résolu, D3D12,
une cinématique complète, HUD/radar/sous-titres/pause, clavier-souris et monde
streamé
([README, lignes 19–27](https://github.com/luisxl15/GTA-IV-RECOMP-XBOX-360/blob/6c9ca0b10299b36cb75f587cdaba3751914ec693/README.md#L19-L27)).
Il déclare dans le paragraphe suivant collision statique absente, saves
énumérées mais rejetées et avertissements XMA
([lignes 29–41](https://github.com/luisxl15/GTA-IV-RECOMP-XBOX-360/blob/6c9ca0b10299b36cb75f587cdaba3751914ec693/README.md#L29-L41)).

Le dépôt ne suit aucune image, vidéo, trace ou log ; il n'a ni tag, ni release,
ni workflow, ni test, ni issue/PR publique. Son seul code titre est une classe
`ReXApp` sans hook actif
([`gta4_app.h`, lignes 9–29](https://github.com/luisxl15/GTA-IV-RECOMP-XBOX-360/blob/6c9ca0b10299b36cb75f587cdaba3751914ec693/src/gta4_app.h#L9-L29)).
Les claims sont précis et plausibles, mais restent `documented-unmatched` faute
de reçu liant XEX, SDK, binaire, entrées, image et chronologie.

Jenish n'a également ni tag, release, CI ou test. Sa documentation reconnaît
qu'il s'agit principalement de renommage/adaptation de Marathon et de sources
communautaires, puis que plusieurs éléments ne fonctionnent pas encore
([changements, lignes 1–7 et 41–69](https://github.com/Jenish094/GTA4Recomp/blob/23313a574cfa847922c13c4a1794369b2a145016/docs/CHANGES.md#L1-L69)).
Sa liste de sources est incomplète et sans liens
([`SOURCES.md`](https://github.com/Jenish094/GTA4Recomp/blob/23313a574cfa847922c13c4a1794369b2a145016/docs/SOURCES.md)).

## Streaming world, VFS et collision

Le pattern public le plus utile vient du patch Luis. Il ajoute deux
`HostPathDevice` écrits sous un répertoire utilisateur pour :

- `\Device\Harddisk0\CachePartition0` -> `cache:` -> `cache0` ;
- `\Device\Harddisk0\CachePartition1` -> `cache1:` -> `cache1`.

Ils sont enregistrés avant le `NullDevice`, car le VFS ReXGlue choisit dans
l'ordre d'enregistrement
([patch, lignes 61–104](https://github.com/luisxl15/GTA-IV-RECOMP-XBOX-360/blob/6c9ca0b10299b36cb75f587cdaba3751914ec693/docs/rexglue-sdk-gta4.patch#L61-L104)).
Le mainteneur rapporte que ce montage a rendu la géométrie mondiale visible,
mais n'a pas restauré la collision ; remplir manuellement les 125 `.img` n'a
rien changé
([collision, lignes 96–104](https://github.com/luisxl15/GTA-IV-RECOMP-XBOX-360/blob/6c9ca0b10299b36cb75f587cdaba3751914ec693/docs/COLLISION_BUG.md#L96-L104)).

La transposition AC6 est un seam, pas du code RAGE : lorsqu'une trace AC6
montre un volume cache invité, monter le device le plus spécifique avant un
fallback, séparer données immuables et écritures, puis tester les aliases et
l'ordre. Ce résultat ne justifie ni `cache:` par défaut, ni la lecture de PAC
retail pendant `play`.

Le document collision décrit aussi une méthode valable de diagnostic : isoler
une machine de 500 slots, observer les flags avant/après, distinguer « data
loaded » de « inserted into physics », puis placer le hook au call-site plutôt
que sur une fonction partagée
([état observé, lignes 27–75](https://github.com/luisxl15/GTA-IV-RECOMP-XBOX-360/blob/6c9ca0b10299b36cb75f587cdaba3751914ec693/docs/COLLISION_BUG.md#L27-L75),
 [instrumentation, lignes 106–136](https://github.com/luisxl15/GTA-IV-RECOMP-XBOX-360/blob/6c9ca0b10299b36cb75f587cdaba3751914ec693/docs/COLLISION_BUG.md#L106-L136)).
Les techniques `weak symbol` et `midasm_hook` sont documentées, mais aucun
harness ni log brut correspondant n'est publié. Pour AC6, seule la démarche
« producteur -> transition -> consommateur » est retenue, avec adresses PAL et
Ghidra canonique propres ; aucun flag, offset ou nom RAGE ne l'est.

Le VFS Jenish est un contre-exemple. Il met en minuscules, retire le premier
préfixe `:`, accepte un mapping si le préfixe apparaît n'importe où dans le
chemin, concatène le reste à l'hôte et essaie une liste d'extensions, sans
canonicalisation ni contrôle de confinement
([lignes 62–100 et 105–171](https://github.com/Jenish094/GTA4Recomp/blob/23313a574cfa847922c13c4a1794369b2a145016/GTA4Recomp/kernel/vfs.cpp#L62-L171)).
Il est `divergent`, indépendamment du build déjà cassé.

## Import et intégrité : contre-exemples à conserver

### LibertyRecomp

L'installateur `beta` ne doit pas être repris :

- `SkipHashValidation = true` et tous les comptes de hashes jeu valent zéro ;
- les fichiers sont chargés puis écrits directement dans le chemin final,
  sans staging/rename atomique
  ([copie, lignes 214–294](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/LibertyRecomp/install/installer.cpp#L214-L294)) ;
- le chemin interne RPF est seulement débarrassé de ses slashs initiaux puis
  concaténé à `outputDir`, sans rejet de `..` ou contrôle `weakly_canonical`
  ([lignes 524–548](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/LibertyRecomp/install/rpf_extractor.cpp#L524-L548)) ;
- si la décompression échoue, les bytes compressés sont écrits comme s'ils
  étaient valides ; les erreurs d'écriture ne font pas échouer l'opération et
  `success` est posé inconditionnellement
  ([lignes 550–601](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/LibertyRecomp/install/rpf_extractor.cpp#L550-L601)) ;
- une archive RPF est supprimée après ce « succès » ; elle est même supprimée
  si le répertoire cible existe seulement et n'est pas vide
  ([lignes 851–884](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/LibertyRecomp/install/installer.cpp#L851-L884)).

Cette combinaison fail-open + suppression est `divergent` et potentiellement
destructive. Elle renforce les invariants AC6 existants : identité de source,
bornes et chemins vérifiés, staging complet, digest des sorties, publication
atomique, reprise idempotente et conservation de la source jusqu'au commit.

### Projet Luis

Les extracteurs Python sont compacts mais pas qualifiés pour un import :

- XDVDFS vérifie seulement le magic puis joint le nom issu du disque au
  répertoire cible, sans confinement
  ([`extract_xiso.py`, lignes 22–32 et 103–120](https://github.com/luisxl15/GTA-IV-RECOMP-XBOX-360/blob/6c9ca0b10299b36cb75f587cdaba3751914ec693/tools/extract_xiso.py#L22-L120)) ;
- STFS vérifie magic/layout et qu'une chaîne de blocs se lit jusqu'au bout,
  mais pas les hashes STFS, signatures, Title ID ou Media ID ; il reconstruit
  et écrit aussi les noms sans confinement
  ([lecture, lignes 38–52 et 83–147](https://github.com/luisxl15/GTA-IV-RECOMP-XBOX-360/blob/6c9ca0b10299b36cb75f587cdaba3751914ec693/tools/extract_stfs.py#L38-L147),
  [écriture, lignes 150–174](https://github.com/luisxl15/GTA-IV-RECOMP-XBOX-360/blob/6c9ca0b10299b36cb75f587cdaba3751914ec693/tools/extract_stfs.py#L150-L174)) ;
- `setup.py` copie/merge directement `game/`, `tu8/` et `update/`; `--force`
  ne purge pas les anciens arbres, et le seul contrôle TU est la présence de
  `default.xexp`
  ([lignes 43–105](https://github.com/luisxl15/GTA-IV-RECOMP-XBOX-360/blob/6c9ca0b10299b36cb75f587cdaba3751914ec693/setup.py#L43-L105)).

Le texte « Media ID must match » n'est pas appliqué par le code. Ces outils
sont utiles pour comprendre le layout attendu, pas pour importer AC6.

## Xenos, shaders et présentation

Le projet Luis n'a aucun renderer titre : son exécutable instancie `ReXApp` et
délègue command processor, D3D12, shaders, textures et présentation au fork
ReXGlue externe. Le claim « initializes and presents » ne qualifie aucun
packet PM4, fetch constant, shader, EDRAM resolve, format, mip ou image.

LibertyRecomp contient au contraire un renderer manuel très étendu, mais ses
preuves publiques l'invalident comme référence : hooks aux mauvaises
adresses, chargeurs FXC stubés, PM4 no-op, format inconnu rabattu sur RGBA8 et
crash avant le premier parcours de jeu. Le CMake dit lui-même que la
recompilation shader est désactivée et qu'un `shader_cache.cpp` stub est utilisé
jusqu'à extraction GTA IV
([lignes 53–86](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/LibertyRecompLib/CMakeLists.txt#L53-L86)).

La leçon AC6 est négative mais forte : ne jamais remplacer un wrapper GPU
invité complet par une allocation hôte partielle, ne jamais no-op un packet ou
fallbacker un format pour progresser, et ne considérer `PRESENT` qu'après un
receipt `resource -> shader -> draw -> resolve -> image`. Les implémentations
GTA IV restent `divergent`; le runtime ReXGlue reste seulement
`provisional-rexglue`.

## Input, replay et cadence

Aucun dépôt ne publie d'enregistreur/rejoueur d'inputs, de poll ordinal, de
format de trace ou de comparaison déterministe.

Luis active seulement `mnk_mode` dans un exemple de configuration et laisse
tous les hooks `ReXApp` vides
([config, lignes 16–37](https://github.com/luisxl15/GTA-IV-RECOMP-XBOX-360/blob/6c9ca0b10299b36cb75f587cdaba3751914ec693/docs/gta4.toml.example#L16-L37)).
LibertyRecomp instancie le backend input ReXGlue, tandis que son app titre
contient encore des adresses placeholders et définit un callback sans appelant
dans l'arbre, qui accumule un `deltaTime` fourni et attend la swapchain
([`app.cpp`, lignes 43–103](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/LibertyRecomp/app.cpp#L43-L103)).
Son patch GTA IV input est explicitement « to be connected »
([lignes 138–174](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/LibertyRecomp/patches/gta4_input_patches.cpp#L138-L174)).

Deux petits invariants hôte sont raisonnables mais insuffisants : remise à
zéro vibration/caméra lors de la perte de focus et numéro de paquet monotone
à chaque `GetState`
([SDL HID, lignes 768–783 et 842–858](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/LibertyRecomp/hid/driver/sdl_hid.cpp#L768-L858)).
Ils ne disent rien du nombre ou de l'ordre des polls invités.

Le seul invariant cadence concret est le timebase Xbox 360 à exactement
50 000 000 Hz. Liberty le règle explicitement après `Runtime::Setup`
([`main.cpp`, lignes 641–683](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/LibertyRecomp/main.cpp#L641-L683)).
Le SDK vendored le réglait déjà dans `Runtime::Setup`
([lignes 48–58](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/glue/rexglue-sdk-main/src/system/runtime.cpp#L48-L58)),
donc le commentaire présentant l'appel titre comme indispensable n'est pas une
preuve causale. La valeur reste un bon assert de runtime AC6 ; elle ne remplace
ni cadence 60 Hz,
ni compteur invité, ni replay poll-exact. `WaitOnSwapChain`/`PRESENT` ne doit
jamais devenir un tick de mission.

Le patch Luis ajoute aussi `invalid_call_nonfatal`: il journalise LR/r3-r5 puis
ignore une fonction invitée non enregistrée. Son propre commentaire dit que
l'état invité est corrompu et que le mode ne sert qu'au bring-up
([lignes 9–49](https://github.com/luisxl15/GTA-IV-RECOMP-XBOX-360/blob/6c9ca0b10299b36cb75f587cdaba3751914ec693/docs/rexglue-sdk-gta4.patch#L9-L49)).
C'est une sonde `provisional-rexglue` si elle provoque ensuite un arrêt ; la
laisser active pour une claim gameplay est `divergent`.

## XMA, vidéo et sauvegardes

Le claim Luis « full opening cinematic » décrit une séquence rendue, pas une
preuve de décodage vidéo. Le dépôt ne contient ni pipeline Bink/XMV, ni média,
ni trace A/V. Les avertissements XMA sont reconnus sans flux, codec, cue,
langue, timestamp, underrun ou mesure audio publiés. XMA et vidéo restent
`documented-unmatched`.

Le README Liberty coche un décodeur XMA, mais `beta` retire toutes les sources
APU titre et délègue l'audio au SDK
([CMake, lignes 307–310](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/LibertyRecomp/CMakeLists.txt#L307-L310),
 [setup, lignes 656–667](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/LibertyRecomp/main.cpp#L656-L667)).
Le `main` cassé conserve un hook audio « address TBD » qui renvoie simplement
succès
([lignes 94–151](https://github.com/OZORDI/LibertyRecomp/blob/e952c8b78618e532a86d62f977e6400312ca2170/LibertyRecomp/patches/audio_patches.cpp#L94-L151)).
La présence de code FFmpeg/XMA générique n'est donc pas un reçu titre.

Pour les saves, Luis dit explicitement « enumerated ... but rejected ».
Liberty `beta` a retiré son système save titre et s'appuie sur le
`ContentManager` générique ReXGlue. Son `main` cassé contient encore
`GTA4SavePatches::Init` et `GetSavePath` en TODO
([lignes 191–209](https://github.com/OZORDI/LibertyRecomp/blob/e952c8b78618e532a86d62f977e6400312ca2170/LibertyRecomp/patches/gta4_patches.cpp#L191-L209)),
et son writer écrit directement avec `ofstream`, sans temporaire, fsync,
rename ou validation de taille
([lignes 113–169](https://github.com/OZORDI/LibertyRecomp/blob/e952c8b78618e532a86d62f977e6400312ca2170/LibertyRecomp/user/persistent_storage_manager.cpp#L113-L169)).
Aucun projet ne publie un test create/save/restart/load/corrupt. La sauvegarde
reste `documented-unmatched`; l'écriture directe est un contre-exemple.

## Linux

Luis fournit des presets Linux AMD64/ARM64 valides syntaxiquement
([CMakePresets, lignes 21–33 et 66–123](https://github.com/luisxl15/GTA-IV-RECOMP-XBOX-360/blob/6c9ca0b10299b36cb75f587cdaba3751914ec693/CMakePresets.json#L21-L123)),
mais ses prérequis documentent uniquement Windows/MSVC et disent que les
autres plateformes n'ont pas été essayées. Il n'a ni SDK pin, généré, CI ou
artefact Linux.

Liberty revendique plusieurs plateformes dans le README, mais n'a aucun run
vert. Sa branche `beta` inclut `install/rpf_extractor.cpp` dans les sources
desktop
([CMake, lignes 378–391](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/LibertyRecomp/CMakeLists.txt#L378-L391)),
alors que le `#else` Linux inclut `CommonCrypto/CommonCrypto.h`, bibliothèque
Apple
([préprocesseur, lignes 12–23](https://github.com/OZORDI/LibertyRecomp/blob/3e98ca9d392a4b5618243e275469ac9cd7918e99/LibertyRecomp/install/rpf_extractor.cpp#L12-L23)).
Cette erreur statique suffit à invalider l'installateur Linux public, en plus
du checkout récursif cassé. Le fork ARM64 n'ajoute que presets/doc et aucun
build.

Jenish annonce seulement un build déjà cassé. Linux/Vulkan GTA IV est donc
`documented-unmatched` et ne réduit aucune validation Linux/Xenos AC6.

## Licences et réutilisation

LibertyRecomp porte GPL-3.0 à la racine ; ses arbres ReXGlue vendored ont une
licence BSD-3-Clause et de nombreuses dépendances tierces. Le généré GTA IV et
les vendored modifiés ne sont pas identifiés par un commit source unique. Le
projet Luis et Jenish n'ont aucune licence de dépôt. Indépendamment de la
licence, la politique AC6 interdit de copier le généré ou les implémentations
titre : seuls les invariants et formes d'instrumentation décrits ici sont
retenus.

## Classement par besoin AC6 M01

| Besoin | Classement | Résultat utilisable / limite |
|---|---|---|
| Streaming monde / VFS | `provisional-rexglue` | montage cache spécifique avant fallback ; doit être déclenché par une trace AC6 et viser les artefacts importés |
| Import | `divergent` | hashes désactivés, writes directs, traversal non borné, succès fail-open et suppression de source sont des tests négatifs à conserver |
| Collision / physique | `documented-unmatched` | bonne méthode de trace de machine d'état ; aucun flag/adresse/sens RAGE transposable |
| Xenos / renderer | `divergent` pour Liberty, `provisional-rexglue` pour le runtime | aucun packet/draw/image contrôlé ; hooks incomplets et crash GPU |
| Input | `provisional-rexglue` plomberie seulement | focus/rumble/packet utiles ; aucun poll ordinal ni binding titre validé |
| Replay / cadence | absent, sauf timebase | assert 50 MHz ; aucun replay ou tick gameplay, cadence host/present rejetée |
| XMA | `documented-unmatched` | runtime générique et warnings sans flux/cue/mesure |
| Vidéo/cinématique | `documented-unmatched` | claim textuel de cinématique rendue, aucun pipeline ou reçu A/V |
| Saves | `documented-unmatched` / writer `divergent` | saves rejetées ou TODO ; aucune transaction/reprise/corruption testée |
| Linux | `documented-unmatched` | presets sans build ; CI zéro verte et include Apple sur Linux |
| Fidélité retail | aucune preuve | aucun SHA-256 XEX lié à un build, aucune trace/replay/capture scellée |

## Actions AC6 retenues

1. Conserver un assert explicite `guest_tick_frequency == 50_000_000`, mais
   dériver le tick mission d'un compteur invité qualifié et du replay, jamais
   du host delta ou de `PRESENT`.
2. Si les imports AC6 révèlent un device/cache Xbox, enregistrer le device le
   plus spécifique avant tout `NullDevice`, séparer racines read-only et
   writable, puis tester aliases, casse, slashs, `..`, ordre et erreurs.
3. Appliquer au streaming Mission 01 la démarche mesurée
   `producer -> slot/state transition -> consumer`, avec hooks aux call-sites,
   bytes PAL et projet Ghidra canonique. Ne reprendre aucun nom/adresse RAGE.
4. Ajouter ou maintenir les négatifs import : digest absent, Media/Title ID
   insuffisant, STFS hash faux, path traversal, décompression fausse, write
   court, répertoire partiel, reprise après interruption et source jamais
   supprimée avant publication atomique.
5. Exiger un pin commit/arbre pour codegen, runtime et Xenos. Un
   `sdk_version`, un vendor modifié ou une branche mouvante ne suffit pas.
6. Interdire en mode fidélité les invalid calls ignorés, PM4 no-op, formats
   inconnus rabattus, wrappers GPU full-replace incomplets et saves directes.
7. Ne prendre aucun claim GTA IV audio, vidéo, save, Linux ou gameplay comme
   oracle AC6. Les gates AC6 restent fondées sur receipts locaux PAL.

## Validations de l'audit

- recherche GitHub publique par GTA IV, LibertyRecomp, ReXGlue,
  XenonRecomp/XenosRecomp et forks ; trois lignées distinctes et 14 forks
  Liberty inventoriés ;
- clones propres des cinq révisions/branches, HEAD, arbres, nombre de commits,
  fichiers, gitlinks, branches, tags, releases, licences, workflows et jobs
  recomptés ;
- `git submodule status` reproduit les erreurs de mapping Liberty `main` et
  `beta` ; les trois pins Xenon/Xenos publics vérifiés par commit/arbre et le
  pin `134dc9...` confirmé absent du dépôt public configuré ;
- 128 runs Actions Liberty regroupés par conclusion ; runs multi-plateforme
  et release inspectés jusqu'au niveau job/step ;
- ZIP `v0.1.0-alpha` téléchargé, SHA-256 GitHub/local identique, liste, tailles,
  plist, type Mach-O et chaînes inventoriés statiquement ; aucun binaire lancé ;
- scripts Luis validés avec `python -m py_compile`; six TOML parsés en UTF-8
  (deux ont un BOM), JSON CMake validé, `cmake --list-presets=all` réussi ;
- patch Luis validé par `git apply --check` contre le HEAD d'audit du fork SDK ;
- aucun contenu retail ou généré tiers copié dans AC6, aucune sémantique du C++
  généré inspectée ;
- permaliens ciblés sur des commits immuables ;
  `git diff --no-index --check` exécuté sur ce rapport non suivi.

## Risques résiduels

- des branches privées/supprimées, Discord, builds locaux ou logs non publiés
  peuvent dépasser cet état ; ils ne sont pas auditables ;
- les claims Luis peuvent être exacts, mais sans capture/log/binaire et
  identité XEX+SDK ils restent impossibles à rattacher à une révision ;
- le commit Xenos `134dc9...` peut subsister dans un fork privé ou objet Git
  non annoncé ; il n'est pas résolvable depuis l'upstream déclaré ;
- le patch Luis s'applique au SDK du 12 août 2026, mais la branche mouvante peut
  changer dès le prochain push ;
- les documents Liberty dérivent de logs `/tmp` non publiés et contiennent des
  hypothèses successivement réfutées ; seuls les constats statiques recoupés
  ont été retenus ;
- aucun constat GTA IV n'est une preuve binaire AC6 PAL. Les validations
  streaming, renderer, input/replay, XMA/vidéo, saves et Linux d'AC6 restent
  ouvertes.
