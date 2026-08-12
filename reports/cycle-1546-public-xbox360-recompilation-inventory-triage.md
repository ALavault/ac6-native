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

Quinze projets ont constitué le triage initial par lecture de leur HEAD public.
Onze revues profondes supplémentaires couvrent désormais Forza Horizon,
Fable II, Skate 2, reNut, PGR4, Midnight Club: Los Angeles, Hydro Thunder,
GTA IV, Black Ops II, PGR3 et Diablo III.
Skate 3 et Sonic Unleashed, déjà revus aux cycles 1542–1544, ne sont pas
recomptés.

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
| B3 | Forza Horizon `80a25bed26ef231ea086a87235cd46aedae38120` | ReXGlue v0.2.2, seul `default.xex` recompilé ; deux images secondaires seulement chargées | cas négatif : transaction identité/plages/imports/TLS/attach avant publication d'un module | heap global réinitialisé au chargement ; aucun code hôte secondaire, licence ou test |
| B4 | Fable II `1e25911172f8e30458099eda96a1ad7b8992ed60` | reset de l'ancien arbre XenonRecomp vers un shell `rexglue migrate` | ordre lifecycle provisoire et gardes de provenance/codegen | SDK externe non épinglé, aucun reçu XEX ni exécution démontrée |
| B5 | GoldenEye `fdee4d1f750aff4c3b5c6ba3d60f20281c21447d` | ReXGlue direct | hooks input/souris/XAM UI | réseau revendiqué mais aucun transport public auditable |
| B6 | reNut `ef74a036676db6a71c3e2e93bd770e64cf539e5a` | ReXGlue, branche Linux et releases non reliées par reçu | capture oracle : shader sync/warm-up, watchdog et `PRESENT` télémétrique | code PPC de secours suivi ; build Linux issu de fichiers ignorés et SDK ambigu |
| B7 | MCLA `cdfea396e487a3f4b03053827ffa1eda0e3b1e39` + deux implémentations publiques | ReXGlue, ville/rendu confiés au runtime commun | garde dialecte/opcode VMX128 ; seam VFS `t:` ; census secteurs/textures à requalifier | aucune identité XEX/replay/test ; preuves réparties entre trois dépôts |
| B8 | Hydro Thunder `0216dae319eb5b61a7f1553d74529ca9e4ad55c5` + SDK `34b11ee6aed9d4ef914e49e6d8a8a092b02ced36` | pack ReXGlue minimal dans un monorepo | test Xenos `exp_adjust` signé dans dword 3, indépendant de `lod_bias` | scaffold Daytona/OutRun, aucune preuve eau/reflet/input ou XEX qualifié |
| B9 | GTA IV `3e98ca9d392a4b5618243e275469ac9cd7918e99` (`beta`) | ReXGlue vendored 0.7.5 ; migration 0.8.0 plus récente sans généré | timebase Xenon 50 MHz, montage VFS ordonné et instrumentation du streaming | XEX/SDK exacts non scellés ; renderer/imports divergents ; claims gameplay non appariés |
| B10 | Black Ops II `c096a06b1fee4925482e37870c2e54c82e3fd9b6` (renderer WIP) | ReXGlue + XenosRecomp `990d03b28a27b50277ee5d8d942e1c5f873869d1` | trace ordonnée command processor/VdSwap avec compteurs de pertes/fallback | dépend d'un SDK renderer privé introuvable ; input/cadence/renderer divergents |
| B11 | PGR3 `0216dae319eb5b61a7f1553d74529ca9e4ad55c5` + SDK `34b11ee6aed9d4ef914e49e6d8a8a092b02ced36` | scaffold ReXGlue, plugin Xenos Vulkan | fixtures négatives EDRAM 2xMSAA, depth/coverage, resolve cubemap et placeholder shader | aucun renderer PGR3 natif ni XEX/capture qualifié ; pack Linux non reproductible |
| C1 | Army of Two `75432a71565cc4a33b12a10a092b67ede3f1aaa4` | ancien Unleashed/XenonRecomp/XenosRecomp | historique seulement | audio/input/save non implémentés, texture bloquante ; inventaire « vidéos » non confirmé |
| C2 | Skate 2 `63ef2e191a493348063c55001838b9c7d86100fe` | squelette XenonRecomp autonome D3D9/FFmpeg | seam ABI guest/host et structures big-endian comme contre-exemples | générateur/corpus PPC/shaders absents ; aucune licence, preuve XEX, test ou CI |
| C3 | TDURecomp `c042612f9d2f73b68e150cbd87c586f6873607bd` | copie inachevée Unleashed/reblue | aucun apport fiable | cible/configs incohérentes et provenance non assainie |
| C4 | Re-Cherry `4f8f82028c02e25a32402b4de96f9c23e2f3b7c5` | petit projet ReXGlue migré | quelques hooks FPS/Xbox Live | aucune licence/test/CI, binaire 91 MiB suivi, complétude non documentée |
| C5 | PGR4 `57a97dc735f8ca73435e8372a06740219c8fe4e2` | dépôt documentaire + binaires de release ReXGlue 0.8.0 | A/B `execute_unclipped_draw_vs_on_cpu` pour extent/aliasing EDRAM | aucun source/build/test ; diagnostic CPU-VS seulement, pas renderer natif |
| C6 | Diablo III `11650aec28bc1d86c221da4992bff3b4b5778ccb` + SDK `f22cd9dc360dda5700358f7452230af24c2c2e69` | patch ReXGlue et releases installables non attestées | formes de tests save/XAM/pads, cadence et alias couleur/profondeur | closure source cassée (`slot_arbiter.h` absent), XEX/généré/logs privés |

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
- MCLA : SDK principal annoncé `cd778a8b0645753d130a59f4283d46352f955789`,
  correctif de dialecte VMX128 `fb2773781ad4ec562c4a1c5d36a00195ccb199b1` ;
- Hydro Thunder : fork SDK `34b11ee6aed9d4ef914e49e6d8a8a092b02ced36` ;
- reNut Linux : tag SDK résolu vers
  `f5c85215174c9dcd67b4c77227a979c4fc33197a`, sans reçu le liant au binaire.
- GTA IV `beta` : XenonRecomp `c3714b8d7d35d202df293c4965b52bd74ae9df02`
  et XenosRecomp `811240b0137dc9806ae1480d96314cf43941c4b9`,
  mais le vendor ReXGlue n'est identifié que par arbre ;
- Diablo III : patch applicable uniquement à ReXGlue
  `f22cd9dc360dda5700358f7452230af24c2c2e69`, pas à v0.9.0 ni au HEAD.

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

Black Ops II confirme l'intérêt d'ordonner les événements à la frontière
invitée et de compter explicitement pertes, files pleines et fallback. Son
chemin public remplace toutefois l'état invité par une file hôte et ne fournit
ni poll-exact, ni cadence qualifiée : c'est un contre-exemple, pas le producteur
de notre replay. Diablo III ne publie aucun artefact reliant ses correctifs de
slots pads à un XEX ou à une séquence reproductible.

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

PGR4 ajoute un test diagnostic ciblé : les draws HUD/fullscreen non clippés
doivent borner explicitement leur extent et leurs plages EDRAM ; l'interpréteur
VS CPU reste hors produit. MCLA impose une garde de dialecte avant toute
sémantique VMX128, après qu'un `vsldoi128` public a été pris pour `mullhwu.`.
Hydro corrobore que `exp_adjust` est un signé six bits du dword 3, distinct du
`lod_bias` du dword 4. Ces trois constats restent provisoires jusqu'à un paquet
et un contrôle image M01 PAL positifs.

PGR3 ajoute des cas négatifs plus bas niveau : surface couleur/depth aliasée,
2xMSAA, couverture par sample et resolve vers une face de cubemap. Ses deux
chemins FBO/FSI se contredisent visuellement et ne permettent donc de copier
aucun résultat ; seule la forme de la fixture est retenue. Le renderer Black
Ops II ne se reconstruit pas avec les SDK publics et son backend D3D12 partiel
reste hors cible Vulkan Linux.

### VFS, saves et contenu

- Downpour implémente un cache de résolutions négatives avec invalidation
  ciblée lors de la création, au lieu de purger toutes les entrées.
- Futari sépare data/save, installe DLC/XContent/STFS et écrit sa configuration
  via temporaire puis rename.
- Marathon fournit VFS ISO/XContent/directory et mode portable.
- L'extracteur STFS Banjo ne vérifie ni signatures ni hashes et ne gère pas les
  répertoires ; il reste un cas négatif pour notre import fail-closed.

GTA IV suggère de monter les caches VFS dans un ordre explicite et d'attacher à
chaque requête de streaming l'identité logique, l'offset, la taille, le résultat
et le tick invité. Cette instrumentation est réutilisable ; ses imports
fabriqués et son renderer ne le sont pas. Les scénarios save/restart décrits par
Diablo III deviennent une checklist, sans promotion tant que sa release n'est
pas reproductible depuis la source publiée.

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

Audit C1 terminé : `cycle-1561-army-of-two-legacy-runtime-review.md`. Le seul
dépôt public retrouvé reste l'ancien bring-up XenonRecomp, pas une migration
ReXGlue vérifiable. Il ne fournit aucun composant M01 ; il ajoute surtout des
gardes négatives de paquet, identité XEX, couverture des seams et refus des
fallbacks Release. Le dépôt suit directement un XEX retail et environ 270 Mo
de C++ PPC généré.

Audits B3/B4/C2 terminés : `cycle-1557-forza-horizon-multixex-review.md`,
`cycle-1558-fable2-migration-review.md` et
`cycle-1560-skate2-legacy-runtime-review.md`. Ils ferment trois raccourcis :
charger une image secondaire n'enregistre pas son code recompilé, une commande
`rexglue migrate` ne prouve pas une migration sémantique, et un ancien runtime
XenonRecomp incomplet ne qualifie ni ABI, ni XMA, ni renderer par son seul
codegen.

Audit reNut terminé : `cycle-1562-renut-rexglue-release-review.md`. Il retient
`async_shader_compilation=false` ou un warm-up mesuré pour toute capture oracle,
et maintient `PRESENT` comme télémétrie. Les releases Linux ne sont pas reliées
à leur source/SDK et le corpus PPC de secours reste interdit au produit.

Audit C5 terminé : `cycle-1563-pgr4-rexglue-renderer-review.md`. Le correctif
UI public active l'interprétation CPU du VS pour estimer l'extent EDRAM ; il
fournit une forme de test aliasing/ownership, pas un renderer à reprendre.

Audits B7/B8 terminés : `cycle-1564-mcla-openworld-rexglue-review.md` et
`cycle-1565-hydro-thunder-water-renderer-review.md`. MCLA apporte la garde
VMX128 et un seam VFS, Hydro un test `exp_adjust`; aucun des deux ne ferme une
lane Scene, Xenos, input, audio ou gameplay AC6.

## Entrées non vérifiables dans cette passe

KinectSportsRecomp `0fe6ed940c43f1ea277ce65eaa50713b3037bb9b`
ne contient aucun fichier suivi. Saints Row 2006
`124e939922a30a46d83e9420893ffbb486153d68` ne contient qu'un README
prospectif et une licence. Aucun URL public exact n'a été retrouvé pour GTA V
Marathon/XenonRecomp, Diablo III, Turok 2008, Spider-Man Web of Shadows, Black
Ops II ou UFC 3 ; ces lignes restent `unverified-url`, pas « inexistantes ».

## Validation et provenance

Les HEAD ont été relus puis revérifiés par `git ls-remote`. Les clones
temporaires, pins transitifs, releases et contrôles de liens sont consignés par
rapport. Le catalogue local d'architecture Xbox 360 a été consulté et ses pins
XenonRecomp/XenosRecomp/Xenia recoupés. Aucun XEX ni actif retail n'a été
ouvert. Les statuts jouables demeurent des affirmations des mainteneurs sauf
lorsqu'un test de code public est cité.
