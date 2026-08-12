# Cycle 1546 — triage de l'inventaire public des recompilations Xbox 360

## Décision

L'inventaire fourni le 12 août 2026 devient un catalogue de solutions à
échantillonner, pas une preuve d'exactitude Xbox 360 et pas un élargissement du
scope produit au-delà de Mission 01.

La domination publique de ReXGlue confirme son utilité comme référence de
bring-up `provisional-rexglue`. Elle ne change pas les gates : un fork, un jeu
jouable ou une release ne qualifie aucune sémantique AC6 PAL par transitivité.
Les deltas intéressants doivent être isolés, testés et requalifiés dans le cône
M01 atteint.

Quinze projets ont été vérifiés par lecture de leur HEAD public. Skate 3 et
Sonic Unleashed, déjà revus aux cycles 1542–1544, ne sont pas recomptés.

## Matrice vérifiée et priorisée

| Priorité | Projet / HEAD au 12 août 2026 | Architecture publique observée | Apport potentiel AC6 | Frontière |
|---|---|---|---|---|
| A1 | Gears 1 `5e19554128026200fa201d2dbbfa737a2ae8ec1f` | XenonRecomp forké, runtime bas niveau, PM4/SPIR-V, XMA | XMA context/ring/replay, timeline input, contrats XAM, PM4/EDRAM/Vulkan, 20 CTest | README dit MIT mais aucun LICENSE racine ; aucune CI ; retail AC6 requis |
| A2 | Futari `b5281bcaa9c9fdc0cdaabdf14a189814c615297b` | fork ReXGlue vendored, shell + huit XEX en DLL séparées | meilleur modèle multi-XEX public, registre de modules, DLC/STFS/content | CI de gardes seulement, pas de build/test jeu |
| A3 | Downpour `66c075d9fe9cbf712ac1694a7b108ae630a0e06a` + SDK `03b3282fd1263c5642f5925ba625b3ba0f6940c9` | ReXGlue soft-fork | VFS negative cache, PSO async, memexport/readback, saves, input souris | contradiction README/source sur le batching Case A ; commit de release à retrouver |
| A4 | MarathonRecomp `bd9c0bbd8a99bcc2c0fabdf9521462e75e0ae7d8` | XenonRecomp/XenosRecomp, runtime Marathon autonome | XMA dérivé Xenia, kernel/XAM/HID/VFS, packaging Linux/macOS/Windows | GPLv3 ; workflows dépendant de ressources privées ; aucun test unitaire |
| A5 | Banjo-Tooie XBLA `87eb2e0fd046a8c1e21765ddd6c6755bac2e0d9b` + SDK `035aa253bdf8ad5bbf419b5b150a7be35189bf4f` | ReXGlue forké | SDL audio dummy drainé au temps réel, GetKeystroke, XContent/licence, Linux Vulkan | projet sans licence ; tests SDK désactivés et absents des workflows |
| B1 | ReOdyssey `803294cb9d74e9509b3576e3c4c08de9bbe6a627` | hybride ReXGlue + Plume + XenosRecomp | remplacement progressif par hooks D3D haut niveau et cache DXIL/SPIR-V | fences et synchronisation encore no-op ; aucun test/CI |
| B2 | Crazy Taxi XBLA `23cce0a46cbc42bde0ecf6df80e568f83772f5ba` | XenonRecomp PPC + runtime ReXGlue | cas de migration et cas négatif XMA/ADX/content | aucune licence/CI ; audio gameplay incomplet |
| B3 | Forza Horizon `80a25bed26ef231ea086a87235cd46aedae38120` | scaffold ReXGlue, multi-XEX WIP | cas négatif : `LoadUserModule` seul ne constitue pas un registre de modules recompilés | aucune licence/test ; checkout contenant beaucoup d'artefacts générés |
| B4 | Fable II `1e25911172f8e30458099eda96a1ad7b8992ed60` | scaffold `rexglue migrate` | forme moderne de migration et heap natif | aucune exécution démontrée ; provenance mixte à réconcilier |
| B5 | GoldenEye `fdee4d1f750aff4c3b5c6ba3d60f20281c21447d` | ReXGlue direct | hooks input/souris/XAM UI | réseau revendiqué mais aucun transport public auditable |
| C1 | Army of Two `75432a71565cc4a33b12a10a092b67ede3f1aaa4` | ancien Unleashed/XenonRecomp/XenosRecomp | historique seulement | audio/input/save non implémentés, texture bloquante ; inventaire « vidéos » non confirmé |
| C2 | Skate 2 `63ef2e191a493348063c55001838b9c7d86100fe` | XenonRecomp + runtime D3D9/FFmpeg | hooks historiques XMA/D3D | projet abandonné, aucune licence/test/CI |
| C3 | TDURecomp `c042612f9d2f73b68e150cbd87c586f6873607bd` | copie inachevée Unleashed/reblue | aucun apport fiable | cible/configs incohérentes et provenance non assainie |
| C4 | Re-Cherry `4f8f82028c02e25a32402b4de96f9c23e2f3b7c5` | petit projet ReXGlue migré | quelques hooks FPS/Xbox Live | aucune licence/test/CI, binaire 91 MiB suivi, complétude non documentée |
| C5 | PGR4 `57a97dc735f8ca73435e8372a06740219c8fe4e2` | documentation seulement dans le dépôt | aucun code réutilisable vérifiable | six fichiers Markdown, aucune source/build/config |

Pins transitifs utiles :

- Gears 1 : XenonRecomp `e481deca4b0e6fe1e9ebf8058e1837f9e6848eb5`,
  Xenia `a54abbc530f2530f8607fc2b9eabaccf27f49505`, FFmpeg-XMA
  `d980192e175e6ff95bcd287af77e16fcb6597974` ;
- Futari : fork SDK `040d5bca3de2f2970e9e29b4bf789739a48f5301`,
  puis modifications vendored ;
- Downpour : fork SDK `03b3282fd1263c5642f5925ba625b3ba0f6940c9` ;
- Marathon : XenonRecomp `c3714b8d7d35d202df293c4965b52bd74ae9df02`,
  XenosRecomp `fb32631ee398e46f2a113d8f9103201dbaa000b4` ;
- ReOdyssey : XenosRecomp `c1891538e9ec69819bb70fb3cc123cf65c5f6da2`,
  Plume `561428b7d0499eaf96b17d04bd6aa594d3b1260f`.

## Résultats par besoin M01

### XMA et audio

Gears 1 est le candidat prioritaire. Le dépôt expose un protocole de contexte
matériel 16 mots big-endian, des rings PCM, paquets/frames, doubles buffers,
boucles, un fork FFmpeg `AV_CODEC_ID_XMAFRAMES` et un exécutable `xma_replay`
construit contre le chemin runtime. Le mainteneur documente une comparaison
indépendante corrélée à `1.000000` sur 142 secondes ; ce résultat reste à
reproduire avant réutilisation.

Marathon possède un décodeur issu de Xenia sans preuve publique équivalente.
Le fork Banjo apporte une correction directement testable dans notre harness :
si aucun périphérique audio n'est disponible, SDL `dummy` continue de drainer
les frames avec cadence réelle. Crazy Taxi confirme qu'un boot/rendu ne suffit
pas à conclure que XMA gameplay est couvert.

### XAM, input et replay

- Gears 1 possède une timeline d'entrée indexée sur les polls invités, un mode
  headless reproductible et des tests XAM user/apps/content/config. Il devient
  le premier comparatif de notre replay poll-exact `XamInputGetState`.
- Downpour traite raw input, DPI, décroissance de stick et fallback souris.
- Banjo corrige la queue `GetKeystroke`, XAM content/licence et l'auto-dismiss.
- GoldenEye expose des hooks input/UI, mais aucune preuve réseau réutilisable.

### Xenos et renderer

Gears 1 est la seule alternative basse couche réellement substantielle de cet
échantillon : ring PM4 invité, traduction shader issue de Xenia vers SPIR-V,
ressources EDRAM et Vulkan maison. Downpour reste dans ReXGlue et optimise
memexport, PSO et texture cache. ReOdyssey illustre plutôt une frontière haute
par hooks `D3DDevice_*` vers Plume. Marathon est mature et multiplateforme,
mais dépend davantage de seams spécifiques au jeu.

Le README Downpour affirme que les commandes « Case A » sont batchées jusqu'à
`IssueSwap`; le HEAD public du SDK dit au contraire que ce refactor reste à
faire et emprunte encore le full path. Cette fonctionnalité est classée
`documented-unmatched`, jamais `provisional-rexglue`, jusqu'à identification du
source exact de la release.

### VFS, saves et contenu

- Downpour implémente un cache de résolutions négatives avec invalidation
  ciblée lors de la création, au lieu de purger toutes les entrées.
- Futari sépare data/save, installe DLC/XContent/STFS et écrit sa configuration
  via temporaire puis rename.
- Marathon fournit VFS ISO/XContent/directory et mode portable.
- L'extracteur STFS Banjo ne vérifie ni signatures ni hashes et ne gère pas les
  répertoires ; il reste un cas négatif pour notre import fail-closed.

### Multi-XEX

Futari est la seule preuve complète trouvée : huit modules à base invitée
`0x88000000`, codegen isolé par XEX pour empêcher l'analyse croisée, une DLL
hôte par module, puis `RegisterRecompiledModule(name, guest_path, dll)`. Forza
ne fait que précharger des XEX non recompilés ; ce n'est pas un modèle complet.

## Ordre des audits profonds

1. Gears 1 : XMA, timeline input, tests XAM, PM4/EDRAM.
2. Futari : génération et cycle de vie multi-XEX, imports, TLS et collisions de
   bases.
3. Downpour : réconcilier release/source, puis VFS, memexport et PSO.
4. Marathon : distinguer runtime générique et seams Sonic, puis XMA/VFS/HID.
5. Banjo-Tooie : diff minimal du fork SDK contre ReXGlue, audio dummy,
   GetKeystroke et XContent.

ReOdyssey suit comme audit spécialisé du remplacement progressif du renderer.

Audit A1 terminé :
[`cycle-1551-gears1-low-level-runtime-review.md`](cycle-1551-gears1-low-level-runtime-review.md).
Verdict : excellent laboratoire de fixtures et d'instrumentation XMA/PM4/XAM,
mais backend non transplantable sans réécriture ; licence racine absente,
preuves privées non reproductibles et divergences certaines dans le command
processor, le lock XMA, le renderer et la VFS.

Audit A2 terminé : `cycle-1548-futari-multixex-rexglue-review.md`. Il retient
les registres exact-path et le retrait avant réemploi d'une base invitée, mais
diffère tout chargeur multi-XEX AC6 jusqu'à un census M01 de `XexLoadImage`.

Audit A3 terminé : `cycle-1549-downpour-rexglue-source-audit.md`. Il établit
notamment que les tags publics Downpour `v1.1` à `v1.1.6` sont des changements
README uniquement et que le code SDK public s'arrête à `v1.0`.

Audit A4 terminé : `cycle-1550-marathon-autonomous-runtime-review.md`. Il
confirme la frontière D3D/XDK haute sans PM4, mais classe XMA, XAM/HID et les
lecteurs ISO/XContent comme non réutilisables sans durcissement et preuve AC6.

Audit A5 terminé : `cycle-1552-banjo-tooie-rexglue-fork-review.md`. Il retient
le fallback SDL `dummy` pour les seuls runs headless et rejette comme généraux
la queue `GetKeystroke`, les automatismes XAM et l'extracteur STFS.

Audit B2 terminé : `cycle-1553-crazy-taxi-hybrid-recomp-review.md`. Il montre
que le HEAD exécute la sortie ReXGlue, pas l'ancien corpus XenonRecomp, et
classe comme divergents les défauts de `switch`, appels indirects et fautes de
pages neutralisés pour continuer l'exécution. Il confirme `skip_lr=false` et
le seam XAM invité comme choix provisoires, sans qualifier timing, XMA ou GPU.

Audit B1 terminé : `cycle-1555-reodyssey-renderer-review.md`. Il retient la
frontière haute D3D, les ABI invitées typées et la décomposition
fetch/detile/endian/upload comme patrons d'architecture. Il rejette comme
preuves retail le cache SMOL-V incohérent, les formats en fallback, l'absence
de mips/cubemaps et les synchronisations neutralisées ; aucune lane M01 n'est
fermée par ce dépôt.

Audit B5 terminé : `cycle-1559-goldeneye-rexglue-instrumentation-review.md`.
Il retient le tuple de télémétrie ring/swap/fence/thread, mais sépare
strictement observation et récupération : le watchdog public écrit horloge,
sémaphore, bit skip et pointeurs guest, donc invalide toute trace oracle. Il
confirme aussi la nécessité d'auditer récursivement le paquet final et les gros
tableaux d'octets, pas seulement les fichiers suivis.

Audit C4 terminé : `cycle-1554-lollipop-rexglue-runtime-review.md`. Le dépôt
public Re-Cherry n'embarque ni fork/pin ReXGlue, ni code généré : XAM, XMA,
VFS et Xenos sont `documented-unmatched`. Seule la forme adresse/phase/registres
d'un hook MIDASM reste un patron provisoire.

## Entrées non vérifiables dans cette passe

KinectSportsRecomp `0fe6ed940c43f1ea277ce65eaa50713b3037bb9b`
ne contient aucun fichier suivi. Saints Row 2006
`124e939922a30a46d83e9420893ffbb486153d68` ne contient qu'un README
prospectif et une licence. Aucun URL public exact n'a été retrouvé pour GTA V
Marathon/XenonRecomp, Diablo III, Turok 2008, Spider-Man Web of Shadows, Black
Ops II ou UFC 3 ; ces lignes restent `unverified-url`, pas « inexistantes ».

## Validation et provenance

Les HEAD ont été relus puis revérifiés par `git ls-remote`. Dix-sept clones
temporaires propres ont été utilisés. Le catalogue local d'architecture Xbox
360 a été consulté et ses pins XenonRecomp/XenosRecomp/Xenia recoupés. Aucun
build, XEX ou actif retail n'a été ouvert. Les statuts jouables demeurent des
affirmations des mainteneurs sauf lorsqu'un test de code public est cité.
