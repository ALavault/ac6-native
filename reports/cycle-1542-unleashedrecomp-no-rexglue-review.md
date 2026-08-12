# Cycle 1542 — ce qu'UnleashedRecomp sans ReXGlue apporte à AC6

## Résultat

UnleashedRecomp confirme qu'un jeu Xbox 360 peut fonctionner sans ReXGlue,
mais pas grâce à un runtime Xbox générique de remplacement. Le projet combine
du C++ produit par XenonRecomp avec un micro-runtime et un renderer écrits pour
Sonic Unleashed. Surtout, il intercepte les fonctions D3D/XDK liées dans le jeu
avant la construction du ring PM4. Les imports graphiques bas niveau restent
des stubs.

L'idée directement utile à AC6 est donc :

```text
fonction D3D/XDK qualifiée
  -> capture immuable de l'état invité au draw
  -> commande native typée
  -> cache pipeline/ressource
  -> Vulkan
```

Ce n'est pas une raison de remplacer notre produit C++ manuscrit par un runtime
de recompilation. C'est un plan crédible pour remplacer notre raster CPU par
des `DrawPacket` monde/HUD natifs, à condition de prouver que le cône M01 passe
bien par des frontières D3D équivalentes.

## Provenance et portée de la revue

- dépôt : <https://github.com/hedge-dev/UnleashedRecomp> ;
- commit : `cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c`, 2026-06-29 ;
- licence du projet : GPLv3 (`COPYING`) ;
- XenonRecomp épinglé :
  `c5bfd90d87f2ed0db8cff5c19ea3aff0e161e527`, MIT ;
- XenosRecomp épinglé :
  `990d03b28a27b50277ee5d8d942e1c5f873869d1`, MIT ;
- abstraction D3D12/Vulkan Plume épinglée :
  `11926860e878e68626ea99ec88562ce2b8badc4f`.

La revue est statique sur un checkout détaché propre. Aucun ISO ou Title Update
Sonic n'est disponible et aucun comportement dynamique n'est donc promu. Le
catalogue d'architecture local a été consulté et vérifié ; ses sources restent
des références génériques, jamais une preuve AC6 PAL.

La GPLv3 interdit de traiter le code du runtime/renderer comme une bibliothèque
de fragments librement copiables dans un produit sous une licence incompatible.
Les architectures et invariants peuvent être réimplémentés ; toute copie exige
une décision de licence explicite.

## Architecture réelle

```text
XEX/XEXP privés
  -> XenonAnalyse + configuration très spécifique
  -> XenonRecomp (261 unités C++ dans le build)
  -> PPCContext + mémoire invitée 4 Gio
  -> hooks manuels et services kernel/XAM ciblés
  -> fonctions D3D/XDK liées statiquement
  -> RenderCommand / PipelineState
  -> Plume D3D12 ou Vulkan
```

XenonRecomp ne fournit volontairement aucun runtime. Unleashed en écrit donc un
pour son titre : mémoire, threads invités, objets kernel, VFS, XAM, input, audio
et hooks. Le census source donne 276 hooks, 29 stubs de fonctions invitées,
196 imports enregistrés et 131 marqueurs `!!! STUB !!!`. Ces nombres mesurent
la surface du projet, pas sa fidélité Xbox.

Le runtime est profondément spécifique au jeu. Sa configuration active
`skip_lr=true`, contient 42 fonctions manuelles, 191 hooks mi-fonction et un
catalogue séparé de centaines de switch tables. Son ABI HLE et plusieurs
services sont bornés aux callsites rencontrés par Sonic. Le transposer à AC6
recréerait un second projet de recompilation, sans accélérer le jeu natif.

## Pourquoi il n'y a pas de command processor Xenos générique

Le point décisif se trouve dans `UnleashedRecomp/gpu/video.cpp` : le projet
hooke directement `CreateDevice`, créations/verrouillages de ressources,
`SetRenderTarget`, `SetTexture`, constantes, shaders, états, `DrawPrimitive`,
`DrawIndexedPrimitive`, `StretchRect` et `Present` autour de
`gpu/video.cpp:7798-7861`.

En parallèle, `kernel/imports.cpp:794-927` laisse `VdSwap`, le ring buffer, le
system command buffer, les interruptions et l'initialisation des moteurs sous
forme de stubs. Il ne décode donc pas le PM4 produit par le jeu : il remplace la
frontière avant que ce PM4 devienne nécessaire.

Cette stratégie est excellente si trois conditions sont prouvées :

1. toutes les soumissions visibles passent par les hooks ;
2. les états nécessaires sont encore lisibles à cette frontière ;
3. aucun chemin direct ring, memexport, resolve ou readback ne porte un effet
   requis par le titre.

Le README historique de rexdex rappelle la difficulté opposée : les fonctions
D3D peuvent être inlinées dans un jeu Xbox 360. La présence d'une frontière
propre dans Sonic ne prouve donc pas sa présence dans AC6.

## IR et renderer à reprendre conceptuellement

Le renderer définit un `PipelineState` compact et canonique couvrant shaders,
déclaration de sommets, profondeur, blend, culling, topologie, strides,
formats, MSAA et spécialisations (`gpu/video.cpp:121-150`). Il définit ensuite
un `RenderCommandType` et une union de commandes pour ressources, targets,
viewport, textures, constantes et draws (`gpu/video.cpp:811-910`).

Les qualités transférables sont :

- snapshot de l'état au point de soumission, plutôt que relecture tardive de
  mémoire invitée mutable ;
- commandes typées et bornées entre thread invité et thread de rendu ;
- état pipeline assaini, hashé par XXH3 et dédupliqué
  (`gpu/video.cpp:4135-4144`) ;
- préchauffage asynchrone des pipelines pendant les chargements ;
- ensemble explicite de copies et resolves différés avant consommation ;
- deux contextes de frame et fences par contexte
  (`gpu/video.cpp:302-325`) ;
- descripteurs bindless séparés par type de texture dans les constantes
  partagées ;
- queue locale qui publie états et constantes avant chaque draw, puis queue de
  rendu globale.

Cela valide l'orientation actuelle `DrawPacket`, mais montre aussi ce qui doit
encore y entrer : identité shader, déclaration de sommet, tous les états fixes,
bindings texture/sampler, plages de constantes, targets, depth, scissor et
transitions de ressources. Un paquet qui ne porte qu'un mesh préprojeté et une
texture n'est pas une frontière de rendu M01 suffisante.

## Shaders, formats et textures

XenosRecomp traduit hors ligne le microcode vers HLSL puis DXIL/SPIR-V et
indexe le cache par XXH3-64. Cette partie est utile comme bootstrap et comme
deuxième lecture du microcode. Elle n'est pas un runtime Xenos : elle ne traite
ni PM4, ni EDRAM, ni resolves, ni tiling.

Ses propres limites sont documentées : conteneur reverse-engineeré pour Sonic,
indexation dynamique incomplète, constantes entières absentes, seulement une
partie des fetch texture, mini-fetch et memexport absents, vertex locations et
instancing spécifiques au titre, formats 16 bits corrigés par une convention
Sonic et textures 1D non prises en charge. Pour AC6 :

- conserver le hash d'ucode brut comme identité stable ;
- utiliser les sorties comme oracle de traduction, jamais comme source produit ;
- recenser les opcodes/fetch/formats réellement atteints par M01 ;
- écrire des golden tests AC6 pour chaque élément avant promotion ;
- conserver nos lecteurs NDXR/NTXR et nos contrôles endian/tiled comme autorité
  de contenu.

## Kernel, XAM, input et audio

Le micro-runtime démontre qu'une surface HLE étroite peut suffire à un titre,
mais pas que ses services ressemblent au retail :

- les threads invités sont des threads hôte ;
- de nombreux imports journalisent un stub ou renvoient un défaut choisi ;
- l'ABI HLE n'est pas une implémentation générale de l'ABI Xbox ;
- les services XAM sont adaptés aux menus et saves du titre ;
- `XamInputGetState` est une frontière nette, utile au replay par poll ;
- XMA create/release restent des stubs
  (`kernel/imports.cpp:1584-1592`) ;
- le son utile provient de l'interception de PCM déjà mixé, pas d'un décodeur
  XMA générique.

Unleashed ne débloque donc pas notre lane XMA/ASF. ReXGlue et son worker XMA
FFmpeg restent une meilleure référence provisoire, avec qualification tardive
du cône M01 effectivement utilisé.

## Ce que cette revue change pour Mission 01

### Lot U1 — census de frontière D3D PAL

Ajouter à l'oracle ReXGlue PAL provisoire une trace des appels D3D/XDK à partir
du boot jusqu'à une sortie contrôlée M01 : adresse qualifiée, phase avant/après,
thread, ordinal, ressources, états modifiés et draw/resolve/present. La trace
est diagnostique et scellée par l'identité du XEX et de ReXGlue.

En parallèle, recouper chaque hook dans `ghidra-projects/ace-combat-6` :
fonction englobante `.pdata`, bytes, ABI, callsites et absence de chemin direct
concurrent. ReXGlue donne les candidats et la cadence ; Ghidra garde les
frontières.

### Lot U2 — IR AC6 indépendante

Définir un flux manuscrit versionné :

```text
ResourceUpload | TargetBind | PipelineBind | DrawPacket |
Resolve | Readback | Present
```

Chaque `DrawPacket` capture monde, caméra, vertex/index, matériau, textures,
samplers, constantes, viewport/scissor, depth/blend/cull et provenance. Les
handles sont des identités produit, jamais des pointeurs ou structs oracle.

### Lot U3 — preuve de couverture

Sur le replay M01 synchronisé, exiger :

- tous les draws de la frame oracle attribués à une commande typée ;
- tous les targets/resolves/readbacks consommés attribués ;
- aucun appel ring/memexport hors contrat ;
- ordre et hashes de snapshots reproductibles sur deux runs ;
- premier point de divergence structuré.

Si la couverture est complète, le backend Vulkan reçoit directement l'IR. Si
elle ne l'est pas, seul le sous-ensemble PM4 manquant est étudié ; aucun
processeur Xenos complet n'est construit par anticipation.

### Lot U4 — qualification différée

Pendant le bring-up, les états observés peuvent être
`provisional-rexglue`. Avant JV/M01-F/publication, le cône atteint doit devenir
`retail-qualified` par bytes PAL, ABI/dataflow statique, contrôle exécuté quand
nécessaire et test natif. Toute contradiction devient `divergent` et reste
fail-closed.

## Décisions

- Oui à la stratégie de hooks D3D haut niveau comme première hypothèse GPU.
- Oui à une IR typée, snapshotée au draw, caches canoniques et préchauffage.
- Non à un port du micro-runtime Sonic ou à une dépendance GPL implicite.
- Non à l'idée qu'« absence de ReXGlue » signifie « émulation Xbox complète
  remplacée ».
- Non à toute promotion automatique des hypothèses shader/format Sonic.
- Le produit final reste le C++ manuscrit dans `reconstruction/ace-combat-6` ;
  XenonRecomp, XenosRecomp, ReXGlue et Xenia restent hors produit.

## Validation et risques résiduels

Revue effectuée par inspection ciblée du commit, des submodules, licences,
configurations, hooks, services, renderer, shader toolchain et workflows. Aucun
build n'a été lancé faute d'actifs Sonic ; ce manque interdit une conclusion
dynamique mais ne change pas la séparation architecturale visible dans les
sources.

Risque principal : AC6 peut avoir inliné ou contourné une partie de la
frontière D3D que Sonic expose. Le census U1 est donc le prochain test
falsifiable ; sans couverture totale, on ne doit pas forcer le modèle
Unleashed sur M01.
