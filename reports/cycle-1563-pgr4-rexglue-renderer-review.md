# Cycle 1563 — audit public PGR4/ReXGlue, renderer et services

Date de qualification : **2026-08-12**. Cible de réutilisation : AC6 PAL,
Mission 01 uniquement. Project Gotham Racing 4 n'a pas été téléchargé ni
exécuté ; aucun fichier retail n'a été utilisé.

## Décision

Le seul dépôt public substantiel identifié pour la recompilation de Project
Gotham Racing 4 est
[`beatrixzy/PGR4-Recomp`](https://github.com/beatrixzy/PGR4-Recomp). Il ne
contient cependant **aucun source, manifeste, script de build, test ou CI** :
le HEAD suivi ne contient que six fichiers Markdown. Les seules pièces
techniques publiques sont les binaires des releases V1/Beta et les journaux
embarqués dans Beta.

L'audit apporte un résultat utile mais étroit : le « correctif UI » de PGR4
n'est pas un renderer natif. V1 réutilise exactement le même exécutable que
Beta et active le CVar ReXGlue
`execute_unclipped_draw_vs_on_cpu`. Dans ReXGlue 0.8.0, ce CVar interprète le
vertex shader de certains draws non clippés sur CPU afin d'estimer la hauteur
EDRAM utilisée. Le cas PGR4 désigne donc prioritairement une frontière
**extent/aliasing/ownership EDRAM**, et non une preuve de correction générale
des shaders ou textures.

Pour AC6, ce mécanisme est un diagnostic `provisional-rexglue` intéressant :
il justifie un test natif explicite sur les draws HUD/fullscreen non clippés et
leurs plages EDRAM. Il ne peut être copié dans le produit interactif, ne ferme
aucune lane M01 et ne devient pas `retail-qualified`.

## Identité et bornage du dépôt

| Élément | Valeur qualifiée |
|---|---|
| Dépôt | `https://github.com/beatrixzy/PGR4-Recomp.git` |
| HEAD `main` | [`57a97dc735f8ca73435e8372a06740219c8fe4e2`](https://github.com/beatrixzy/PGR4-Recomp/commit/57a97dc735f8ca73435e8372a06740219c8fe4e2) |
| Arbre HEAD | `4b985437e531e2f5d50a5b82974f6aa0cf88018b` |
| Branche `testing` | `8409a0ab5267f5b8245ca162eacfa78d84102847` |
| Tag léger `V1` | `4667a0e17cee8440157c5031906b58216db0f220` |
| Tag léger `Beta` | `d27db200d94a49aefb6092ffb7664128df796fb8` |
| Historique | 65 commits publics au moment de l'audit |
| Contenu HEAD | `README.md`, `about.md`, `Setup.md`, `Status.md`, `Future-plans.md`, `Linux-Support.md` |
| Licence | aucune licence détectée par GitHub et aucun `LICENSE`/`COPYING` dans l'historique |
| Automation | zéro workflow GitHub Actions, zéro test public |

La recherche GitHub par nom, description et README ne retourne qu'un dépôt
PGR4 de recompilation. Le README le présente comme le projet courant, crédite
ReXGlue et décrit ses limites matérielles ; cela suffit pour l'identifier
comme dépôt de référence public, pas pour qualifier ses affirmations de
fidélité ([README épinglé](https://github.com/beatrixzy/PGR4-Recomp/blob/57a97dc735f8ca73435e8372a06740219c8fe4e2/README.md#L1-L21)).

Le dépôt ne déclare ni région, ni Media ID, ni SHA-256 de `default.xex`, ni
adresses d'image qualifiées. Les logs indiquent seulement le Title ID
`4D5307F9` et une image invitée `0x82000000–0x82D70000`, avec code recompilé
`0x82270000–0x829D063C`. Aucune correspondance retail ne peut être établie.

## Releases et reproductibilité

### Artefacts vérifiés

| Artefact public | Taille | SHA-256 |
|---|---:|---|
| `PGR4-Recomp-V1.zip` | 18 765 514 | `580765947acac65a86baec28ca90646abba7a34a78a7536dfbb276e79bce86be` |
| `pgr4.exe` V1 autonome | 59 542 528 | `61f2f35cbf92ed98dfb32e9f3bf5ccf9ac7c158f9cdde220ad494a166efe1a2e` |
| `PGR4.Recomp.Test.zip` Beta | 22 903 022 | `9b6438b912fe11c066cfbebc7b791ca85c8eac03f9866c97f9ae4eaf2230d75d` |
| `rexruntime.dll` actif | 12 487 680 | `4878f7c7bbb688e7bc771b8cb8a3aa34bcb6cfdbac16f53e3260bf475a4f1e57` |
| `TracyClient.dll` | 246 784 | `d9f2086935e382d9d0949ee17c30e052d0a4ee12faf847ca2025548b081293d2` |

Les digests locaux correspondent aux digests publiés par GitHub pour
[V1](https://github.com/beatrixzy/PGR4-Recomp/releases/tag/V1) et
[Beta](https://github.com/beatrixzy/PGR4-Recomp/releases/tag/Beta).

Constats déterminants :

- `pgr4.exe` est **bit-identique** dans Beta, V1 et l'asset autonome V1 ;
- `rexruntime.dll` et `TracyClient.dll` sont également identiques entre Beta
  et V1 ;
- le passage Beta → V1 n'est donc pas une correction de code. V1 ajoute le
  CVar GPU dans `pgr4.toml` et `run.bat` ;
- l'exécutable contient la marque `[rexglue-v0.8.0-Release]` et un chemin de
  build vers `rexglue-sdk-0.8.0-win-amd64` ;
- le DLL runtime actif et Tracy sont bit-identiques aux deux fichiers de la
  release officielle
  [ReXGlue v0.8.0](https://github.com/rexglue/rexglue-sdk/releases/tag/v0.8.0),
  dont le ZIP Windows a pour SHA-256
  `2031374a8c8ee45e3d61c883a2dd62bf945c703f806acc677953aeb382f9b6f7` ;
- le tag SDK exact est
  [`2bdb97f95f154f32d281aaa08446ae007b8ca117`](https://github.com/rexglue/rexglue-sdk/commit/2bdb97f95f154f32d281aaa08446ae007b8ca117),
  arbre `ff1c1b67f4dfae8f35269977bcd0570d1d174701` ;
- Beta contient aussi un `rexruntime.dll.bak` non actif, SHA-256
  `e994af4475e29b72563c04943e8657fa0279c1d8f8de41945edb42d5e5417792`,
  marqué `v0.8.1.4-dev.ge8ce24f`. Le commit complet
  `e8ce24fa73cd7c1ede80262c06f34893b7963dbe` a toutefois exactement le même
  arbre que le tag v0.8.0 : ce backup n'apporte pas de patch renderer distinct.

Cette identification binaire permet de relire le SDK exact. Elle ne rend pas
PGR4 reproductible : le projet ne publie ni C++ généré, ni sources hôte, ni
manifest ReXGlue, ni configuration de codegen, ni XEX qualifié, ni commande de
build. Les tags de documentation ne scellent pas les assets de release, et le
binaire précède les tags.

L'exécutable contient en outre trois sentinelles fatales de calls non résolus :

- `0x82746BBC → 0x82746C84` ;
- `0x8275614C → 0x82756214` ;
- `0x82704D68 → 0x82704E30`.

Rien ne prouve qu'elles soient atteintes dans le parcours revendiqué, mais
aucun test ne prouve non plus qu'elles soient inaccessibles.

## Renderer, Xenos et EDRAM

### Ce que V1 corrige réellement

La branche de test documente le symptôme : écran/UI noir, contourné en activant
`execute_unclipped_draw_vs_on_cpu`
([README `testing`, lignes 5–16](https://github.com/beatrixzy/PGR4-Recomp/blob/8409a0ab5267f5b8245ca162eacfa78d84102847/README.md#L5-L16)).
Le ZIP V1 automatise uniquement ce réglage ; son EXE est celui de Beta.

Dans le ReXGlue exact :

- le CVar est désactivé par défaut et décrit comme « Execute unclipped draw
  vertex shader on CPU »
  ([définition](https://github.com/rexglue/rexglue-sdk/blob/2bdb97f95f154f32d281aaa08446ae007b8ca117/src/graphics/util/draw_extent_estimator.cpp#L27-L58)) ;
- pour un draw `clip_disable`, il n'interprète le VS que lorsque le scissor
  ressemble au cas D3D9 `8192×8192`, puis borne `max_y` avec les positions
  exportées par le VS
  ([algorithme](https://github.com/rexglue/rexglue-sdk/blob/2bdb97f95f154f32d281aaa08446ae007b8ca117/src/graphics/util/draw_extent_estimator.cpp#L263-L309)) ;
- cette hauteur alimente directement le calcul des render targets et des
  transferts de propriété de plages EDRAM
  ([jointure RT/EDRAM](https://github.com/rexglue/rexglue-sdk/blob/2bdb97f95f154f32d281aaa08446ae007b8ca117/src/graphics/pipeline/render_target/cache.cpp#L567-L588)).

Le commentaire amont explique le risque : sans extent précis, le runtime peut
considérer toute l'EDRAM utilisée, provoquer des transferts de propriété
parasites entre render targets et corrompre un target adjacent, notamment
quand un format invité doit être représenté de façon lossy sur l'hôte.

**Inférence bornée :** puisque le même binaire PGR4 passe d'une UI noire à une
UI rendue par ce seul CVar, le défaut est compatible avec une surestimation de
plage/aliasing EDRAM lors de draws UI non clippés. Cela ne prouve ni le render
target précis en faute, ni une correspondance pixel au retail.

Ce mécanisme n'est pas un rasterizer CPU : le draw reste soumis au backend
D3D12. Il reste néanmoins une interprétation interactive CPU du VS et une
heuristique de runtime. Pour AC6, il doit rester un diagnostic hors produit ;
le backend Vulkan natif doit recevoir une extent déterministe déjà qualifiée.

### Ce que le projet ne fournit pas

- aucun source de renderer ou hook PGR4 ;
- aucune intégration XenonRecomp ou XenosRecomp épinglée ;
- aucun `DrawPacket`, aucun backend Vulkan natif, aucune traduction de shader
  spécifique au jeu ;
- aucune table de formats, mips, cubemaps, tiled/endian ou resolves propre à
  PGR4 ;
- aucune capture, référence retail, SSIM, IoU de silhouette ou tolérance HUD ;
- aucun shader blob public, hash de shader ou corpus reproductible.

Les logs Beta prouvent seulement que le runtime D3D12 démarre sur Intel UHD
630 et NVIDIA GTX 1050 Ti, initialise la présentation et, selon le cache,
traduit notamment 131 shaders et recrée jusqu'à 182 pipelines. Les nombres
varient entre les runs et ne sont reliés à aucun frame/capture qualifié. Ils ne
constituent pas un corpus shader.

Le propre statut du projet garde « Graphics Work » ouvert et repousse un
renderer natif à une future v5
([statut](https://github.com/beatrixzy/PGR4-Recomp/blob/57a97dc735f8ca73435e8372a06740219c8fe4e2/Status.md#L1-L18),
[projets renderer](https://github.com/beatrixzy/PGR4-Recomp/blob/57a97dc735f8ca73435e8372a06740219c8fe4e2/Future-plans.md#L1-L8)).
La description « Xenos renderer » du projet désigne donc le renderer
Xenia/ReXGlue D3D12, pas un renderer PGR4 natif réutilisable.

## Input, cadence et replay

Les logs publics confirment SDL 3.5.0, le driver clavier/souris, l'ajout et le
retrait d'une manette, ainsi que la résolution des imports XAM/input. Ils ne
contiennent aucun flux d'inputs, aucun scénario, aucun tick et aucun résultat
de course structuré.

Le SDK exact précise la frontière utile à notre replay synchronisé :

- `XamInputGetState` rabat « any user » sur l'utilisateur 0, puis délègue à
  l'`InputSystem`
  ([XAM](https://github.com/rexglue/rexglue-sdk/blob/2bdb97f95f154f32d281aaa08446ae007b8ca117/src/kernel/xam/xam_input.cpp#L94-L116)) ;
- le driver SDL actualise son état au moment du poll et n'incrémente le packet
  number qu'en cas de changement observé
  ([SDL](https://github.com/rexglue/rexglue-sdk/blob/2bdb97f95f154f32d281aaa08446ae007b8ca117/src/input/sdl/sdl_input_driver.cpp#L168-L201)) ;
- plusieurs drivers physiques sont fusionnés (OR des boutons, max des
  gâchettes, axe de plus grande magnitude)
  ([fusion](https://github.com/rexglue/rexglue-sdk/blob/2bdb97f95f154f32d281aaa08446ae007b8ca117/src/input/input_system.cpp#L77-L109)).

Conséquence AC6 : un replay déterministe ne doit pas injecter des événements
SDL horodatés. Il doit remplacer les périphériques physiques à la frontière
`XamInputGetState` et sceller, pour chaque poll, l'index d'appel, le tick invité,
le packet number et le `X_INPUT_STATE` normalisé. PGR4 renforce donc notre
stratégie poll-exacte ; il ne fournit pas son implémentation.

La cadence ReXGlue 0.8.0 n'est pas une preuve de 60 Hz déterministe. Le clock
invité est dérivé du compteur hôte ; le worker vblank calcule un intervalle à
partir du mode vidéo, rattrape les intervalles dépassés et dort 1 ms entre
échantillons
([clock hôte](https://github.com/rexglue/rexglue-sdk/blob/2bdb97f95f154f32d281aaa08446ae007b8ca117/src/core/clock.cpp#L20-L83),
[worker vblank](https://github.com/rexglue/rexglue-sdk/blob/2bdb97f95f154f32d281aaa08446ae007b8ca117/src/graphics/graphics_system.cpp#L153-L180)).
Le thread `GPU VSync` vu dans les logs ne qualifie donc ni 3 600 ticks ni une
séquence de polls reproductible.

Le jeu tente de créer `GAME:\Game\Replay`, mais les logs retournent
`0xc0000022`; un run tente aussi `SAVE:\buffer.save` sans succès. Ces lignes
ne prouvent ni replay retail exploitable ni sauvegarde fonctionnelle. Aucun
savestate, input recording ou comparateur de premier point de divergence
n'est publié.

## XAM, XMA, VFS et sauvegarde

| Frontière | Preuve publique | Qualification |
|---|---|---|
| XAM/input | imports input/force feedback résolus, SDL initialisé | `provisional-rexglue` seulement |
| XMA/audio | threads `XMA Decoder` et `Audio Worker`, système audio initialisé | aucun test de voix, cue, niveau ou synchro |
| VFS | chemin hôte monté en `\Device\Harddisk0\Partition1`, puis `game:\default.xex` chargé | runtime lit les fichiers extraits à chaque lancement |
| Save | un accès `SAVE:\buffer.save` échoue ; aucune reprise publiée | `documented-unmatched` |
| Kernel | `IoDismountVolumeByFileHandle` journalisé explicitement `stub` | service incomplet |

Le mode PGR4 exige un répertoire extrait contenant `default.xex`
([instructions](https://github.com/beatrixzy/PGR4-Recomp/blob/57a97dc735f8ca73435e8372a06740219c8fe4e2/Setup.md#L1-L13)).
Il n'offre ni import atomique, ni cache versionné, ni séparation retail/hôte.
Cette architecture est divergente de l'invariant AC6 : après `import`, aucune
PAC retail ne doit être relue par `play` ou `replay`.

## SIMD et baseline CPU

La panne pré-AVX2 documentée n'est pas mystérieuse dans l'artefact V1 : le SDK
ReXGlue 0.8.0 officiel est compilé avec `-march=x86-64-v3` sur Windows et Linux
([preset exact](https://github.com/rexglue/rexglue-sdk/blob/2bdb97f95f154f32d281aaa08446ae007b8ca117/CMakePresets.json#L10-L47)),
et les binaires contiennent effectivement des instructions utilisant YMM.

Un commit amont postérieur à V1 a abaissé le preset par défaut à x86-64-v2
([`8078ea7e`](https://github.com/rexglue/rexglue-sdk/commit/8078ea7e88d58f8d11997d5a6a533200067da8e5)),
mais PGR4 n'a publié aucun nouvel exécutable l'utilisant à la date de l'audit.

Leçon AC6 : compiler le chemin portable avec une baseline x86-64 explicite,
puis isoler AVX2 derrière multiversioning/dispatch testé. Le compilateur peut
vectoriser le C++, mais une optimisation AVX2 ne doit jamais devenir une
exigence accidentelle de la preview.

## Linux, paquet et provenance

La release V1 contient uniquement des PE Windows : `pgr4.exe`,
`rexruntime.dll`, `TracyClient.dll`, une configuration et un batch. Le runtime
importe DXGI et les runtimes MSVC. Aucun ELF, Vulkan loader, AppImage/TGZ,
install relogeable, X11/Wayland ou diagnostic de dépendance n'est fourni. Le
projet décrit Linux comme partiel/futur
([Linux](https://github.com/beatrixzy/PGR4-Recomp/blob/57a97dc735f8ca73435e8372a06740219c8fe4e2/Linux-Support.md#L1)).

La configuration V1 conserve un chemin absolu de la machine de l'auteur ; le
batch demande un remplacement manuel. Cela explique une partie des problèmes
de lecteurs Windows documentés et n'est pas un modèle de packaging.

Inventaire V1 :

- aucun membre `.xex`, `.iso`, GOD/STFS, texture, audio ou asset de jeu ;
- aucune signature `XEX2` dans `pgr4.exe` ;
- les tokens `XDBF`, `LIVE` et `PIRS` repérés dans `rexruntime.dll` appartiennent
  au runtime générique et ne démontrent pas un conteneur retail embarqué ;
- `pgr4.exe` contient en revanche la traduction AOT du code PPC invité. Même
  sans octets XEX bruts identifiés, il s'agit de code généré depuis le retail,
  expressément interdit dans le paquet AC6 ;
- aucune notice de licence n'est livrée pour ReXGlue, Tracy ou les autres
  composants, et le projet lui-même n'a pas de licence ;
- l'absence de source empêche de refaire un audit de provenance ou de
  reproduire le binaire.

Conclusion paquet : **aucun fichier retail brut évident dans V1**, mais pas de
preuve exhaustive d'absence de toute séquence dérivée/embarquée. Le paquet ne
peut servir ni de modèle juridique ni de modèle technique pour AC6.

## Taxonomie AC6

| Classe | Éléments PGR4 |
|---|---|
| `retail-qualified` | **aucun** |
| `provisional-rexglue` | boot D3D12, cache shader/pipeline, input SDL/XAM, threads XMA, diagnostic extent EDRAM |
| `divergent` | VS interprété sur CPU pour l'extent, runtime D3D12 Windows, horloge hôte, baseline AVX2, lecture directe du retail, code AOT généré distribué |
| `documented-unmatched` | races, IA, « fully stable », save/reprise, Linux partiel, fidélité graphique globale |

Les affirmations « UI renders », « races work », « AI works » et « fully
stable » sont des déclarations du mainteneur, pas des gates exécutées
([Status](https://github.com/beatrixzy/PGR4-Recomp/blob/57a97dc735f8ca73435e8372a06740219c8fe4e2/Status.md#L1-L8)).

## Actions retenues pour AC6 M01

1. Ajouter au contrat des `DrawPacket` les champs invités nécessaires à
   l'extent réelle : `clip_disable`, scissors window/screen, offsets,
   EDRAM base, pitch, format, MSAA, extent et destination de resolve.
2. Construire une garde M01 ciblée sur les draws HUD/fullscreen non clippés :
   scissor sentinelle `8192×8192`, render targets adjacents/aliasés, et preuve
   qu'aucune transition n'écrase la plage voisine.
3. Utiliser l'A/B `execute_unclipped_draw_vs_on_cpu` uniquement sur un oracle
   ReXGlue AC6 qualifié pour localiser une divergence. La sortie reste
   `provisional-rexglue`; aucune sémantique AC6 n'est admise sans bytes PAL et
   contrôle positif.
4. Garder l'interpréteur VS/estimation CPU dans l'outillage diagnostic. Le
   renderer produit doit soumettre directement les paquets typés à Vulkan.
5. Poursuivre le replay poll-exact à `XamInputGetState`, avec périphériques
   physiques neutralisés et ordre de polls scellé ; ne pas prendre le vblank
   ou le temps hôte ReXGlue comme cadence de référence.
6. Maintenir une baseline CPU portable et des variantes SIMD dispatchées ;
   tester explicitement un hôte sans AVX2.
7. Conserver les audits de paquet qui rejettent code généré, `default.xex`,
   runtime Xbox/Xenia/ReXGlue et dépendances D3D12.

**Impact de checkpoint :** aucun JF/JV/JP/JG fermé, aucune des six lanes du
checkpoint 2 fermée. La découverte réduit le risque de la lane
NDXR/NTXR/Xenos en ajoutant un invariant et un test précis ; elle ne fournit
pas l'implémentation AC6.

## Validations de l'audit

- recherche GitHub et API GitHub sur dépôt, branches, tags, releases, assets,
  licence et workflows ;
- clone détaché du HEAD PGR4 et du tag ReXGlue exact ; HEAD/tree/remotes
  vérifiés ;
- digests GitHub des trois assets PGR4 reproduits localement ;
- inventaire ZIP, contrôle des chemins membres, `file`, `objdump`, `strings` et
  recherche bornée de signatures retail ;
- comparaison binaire du runtime/Tracy PGR4 avec la release ReXGlue 0.8.0 ;
- inspection source du CVar, du cache RT/EDRAM, du clock/vblank et du chemin
  XAM/SDL au commit épinglé ;
- aucun exécutable PGR4 lancé, aucun asset retail téléchargé ;
- liens GitHub permanents contrôlés ;
- `git diff --no-index --check /dev/null
  reports/cycle-1563-pgr4-rexglue-renderer-review.md` (rapport encore non
  indexé dans le worktree partagé).

## Risques résiduels

- le dépôt public peut ne pas refléter des sources privées ou des builds
  distribués via Goopie/Discord ; ils sont hors périmètre car non vérifiables ;
- aucune image ni session retail PGR4 ne permet de confirmer la causalité
  visuelle au-delà de l'A/B documenté ;
- l'absence de SHA-256 XEX empêche de qualifier région, révision et limites de
  fonctions ;
- le scan d'artefact exclut les conteneurs/signatures évidents, pas toute
  similarité binaire avec le retail ;
- la correction native AC6 de l'extent EDRAM reste à implémenter et à valider
  sur M01 PAL.
