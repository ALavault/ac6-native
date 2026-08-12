# Cycle 1543 — Skate3Recomp comme référence de migration hybride

## Résultat

Skate3Recomp apporte la meilleure méthode observée pour remplacer
progressivement un renderer Xbox 360 : garder temporairement le command
processor ReXGlue, capter des états sémantiques dans le moteur du jeu, rendre
nativement, et pouvoir comparer ou céder ponctuellement au chemin émulé.

Il ne montre pas comment se passer de ReXGlue. Son renderer natif dépend encore
de sa mémoire invitée, de son dispatcher, du kernel/XAM, du command processor
PM4, des fences, queries, memexport, EDRAM, resolves et de son RHI. C'est une
architecture hybride, pas un runtime autonome.

Pour AC6, les éléments décisifs sont l'instrumentation après appel, les
snapshots par draw, le contrat backend-neutre et la bascule A/B. Le code Skate,
ses adresses, layouts, shaders et heuristiques ne sont pas réutilisables.

## Provenance et licence

- dépôt : <https://github.com/mchughalex/skate3recomp> ;
- commit : `f6e0ae87fdfecbadb5c1e36c55d66a744187a3cd`, 2026-07-24 ;
- aucun `LICENSE`, `COPYING` ou `NOTICE` à la racine : aucune copie de code
  Skate sans autorisation explicite ;
- fork ReXGlue : <https://github.com/mchughalex/rexglue-skate3>, commit
  `7eb0faf7787f5e01333c228b8e3f03c32f7295ea`, BSD-3-Clause ;
- le build peut utiliser le SDK source ou le paquet exact `0.8.1.19`.

La revue est statique. Le build complet requiert `default.xex`,
`EAWebkit.xex` et généralement le TU3 Skate privés. Le dépôt ignore ces actifs
et le code généré. Aucun de ces éléments n'est présent dans le produit AC6.

Skate3Recomp n'intègre directement ni XenonRecomp, ni XenonAnalyse, ni
XenosRecomp. Il utilise son fork ReXGlue pour codegen, runtime, HLE et GPU ; les
outils Hedge-dev ne figurent que dans les attributions du SDK.

## Chaîne d'exécution

```text
XEX + EAWebkit + TU3
  -> ReXGlue codegen
  -> PPCContext / mémoire invitée / kernel / XAM / XMA
  -> command processor Xenos D3D12 ou Vulkan
  -> hooks moteur Skate capturant scène et draw state
  -> renderer natif sémantique Skate
  -> callback NativeGuestRenderer
       true  : sortie native et suppression bornée de draws/resolves
       false : sortie ReXGlue émulée
```

La liaison finale passe uniquement par `rex::runtime`. Les configurations
contiennent environ 1 750 overrides de fonctions retail, 1 727 TU3 et 139
EAWebkit : ce volume mesure le travail spécifique au titre. Le build applique
en plus des remplacements textuels au C++ généré pour plusieurs patches. Ce
dernier procédé reste interdit chez AC6 : les sorties d'oracle sont immuables,
et toute adaptation doit vivre dans un hook manuscrit qualifié.

## CPU, mémoire et HLE ReXGlue

Le fork fournit une ABI uniforme `void(PPCContext&, uint8_t* base)`, des GPR
64 bits, pointeurs invités 32 bits, FPR, LR/CTR/XER/CR/FPSCR et VMX/VMX128. Les
loads/stores sont big-endian et `volatile`. Les appels indirects passent par
une table locale puis globale ; les fonctions sont remplaçables par alias
faibles et hooks mi-fonction.

Le runtime réserve 4 Gio virtuels et 512 Mio physiques, fournit les aliases
physiques, MMIO et writeback GPU. Un thread invité devient un thread hôte avec
stack, TLS, PCR et contexte propres. L'ordonnancement suit donc l'OS hôte et ne
constitue pas un oracle déterministe.

La surface HLE est large mais peu qualifiée : environ 2 407 déclarations de
stubs pour 460 implémentations dans le SDK complet. Pour AC6, la seule mesure
utile sera la liste des imports effectivement atteints par le replay M01 ; tout
stub atteint doit arrêter le run avec fonction, ordinal, callsite et tick.

Le fork contient aussi des adaptations Skate dans le runtime, dont un title ID
codé en dur et des mounts `big:`/`dlcbig:`. Il ne doit pas être promu comme un
SDK Xbox neutre.

## GPU émulé et renderer natif

Le README dit que le jeu ne dépend plus de l'émulation GPU, mais les sources
montrent une frontière plus précise :

- ReXGlue lit toujours le ring et les packets PM4 ;
- il maintient les registres, fences, queries, memexport, EDRAM et resolves ;
- D3D12 et Vulkan traduisent le microcode Xenos à l'exécution en DXBC ou
  SPIR-V ;
- le cache texture gère formats, mips, packed tails, sparse memory,
  invalidation, tiling et endian via compute ;
- le callback natif peut supprimer seulement les opérations déclarées sûres ;
- les chemins non pris en charge restent rendus par le backend émulé.

Le renderer natif, lui, est sémantique et spécifique à Skate. Il observe les
listes de scène, `MeshContext`, palettes, constantes, shaders, streams,
indices, render states, viewport/scissor et appels draw. Il reconstruit monde,
personnages, eau, ombres, post-traitement, 2D et vidéos avec des shaders
manuscrits.

L'enseignement pour AC6 est le découplage : le command processor peut rester
un oracle provisoire tandis que le produit apprend à produire ses propres
paquets depuis les ressources et états retail. Il ne faut pas reproduire les
heuristiques de Skate ni embarquer son fallback émulé dans la preview AC6.

## Hooks et moment de capture

Le meilleur exemple se trouve dans `src/skate3_native_render.cpp` : le hook de
`RenderMesh` appelle d'abord la fonction invitée puis capture les constantes,
car elles ne sont finalisées qu'au cours du draw. Une capture à l'entrée
réutiliserait les constantes du draw précédent. Les meshes différés sont
détectés par un compteur de draws et corrigés après la soumission réelle.

Autres patrons utiles :

- hook de Swap comme frontière de frame ;
- capture des listes triées avant que le dispatcher ne les consomme ;
- snapshot post-appel des palettes et matrices ;
- hooks explicites pour shader, constantes, streams, indices, viewport,
  scissor et draw terminé ;
- lecture invitée bornée et fault-guarded ;
- enregistrement de la provenance du mesh, de l'instance et de la vue ;
- publication atomique d'un snapshot complet au renderer.

Pour AC6, chaque hook devra donc déclarer `before` ou `after`, les registres
live, les bytes attendus, la fonction englobante Ghidra et l'effet attendu. Le
moment de capture fait partie du contrat, pas d'un détail d'instrumentation.

## Bascule native/émulée : utile, mais seulement hors produit

Skate propose une bascule F5, des yields pour menus/chargements/FMVs/éditeurs,
et un fallback lorsque le natif échoue. C'est un excellent outil de bisect :
le même process conserve l'état invité et permet de localiser la classe de
draw manquante.

Ce mécanisme ne satisfait pas nos gates :

- certaines captures F11 sont séparées de 60 frames ;
- l'état et le frame ne sont donc pas identiques ;
- les yields masquent une absence de couverture native ;
- un fallback ReXGlue dans le produit violerait la preview sans dépendance
  Xbox/Xenia/ReXGlue.

Nous devons reprendre le principe dans un harness externe : même replay
scellé, process oracle et process natif séparés, ticks/frames alignés, puis
comparaison de captures et de reçus. Le binaire publié ne contient qu'une lane
Vulkan native et refuse un draw non couvert.

## Diagnostics à reprendre

Skate produit des snapshots de draws, buffers, scènes, fetches, constantes et
render states. Son format `.xtr` inclut PM4 primaire/indirect, mémoire,
registres, gamma, swap et EDRAM. Ces formats ne sont pas portables tels quels,
mais inspirent trois artefacts AC6 :

1. `OracleDrawTraceV1` : événements D3D/PM4, états et ressources par frame ;
2. `NativeDrawReceiptV1` : paquet produit, ressources résolues, pipeline et
   résultat Vulkan ;
3. `FrameCompareV1` : correspondance oracle/natif, premier écart structuré et
   métriques image/HUD.

Chaque artefact doit être borné, versionné, metadata-only lorsqu'il est
commité, scellé par SHA-256, XEX/cache/config/replay et indépendant des pointeurs
hôte. Les writes mémoire nécessaires au replay doivent être enregistrés : le
replayer Skate les omet et suppose que le command processor les reproduira.

## Limites de validation du projet

Le dépôt Skate ne contient ni suite de tests suivie ni workflow CI. Le SDK
ReXGlue possède des tests unitaires et des fixtures PPC, mais
`REXGLUE_BUILD_TESTS` vaut `OFF` par défaut et ses workflows construisent sans
`ctest`. Le hot-toggle et les essais humains démontrent une couverture
fonctionnelle, pas le déterminisme ou la fidélité bit à bit.

Le README reconnaît que le renderer natif est jeune, pas validé sur tout le
jeu et susceptible de présenter du pop-in/flicker. Ses affirmations de
performance et de couverture ne remplacent donc pas un replay synchronisé.

## Blueprint concret AC6 M01

### S1 — instrumentation provisoire

Instrumenter l'oracle ReXGlue PAL au seam `XamInputGetState_entry`, au marqueur
frame qualifié et aux frontières D3D/PM4. Enregistrer depuis avant le boot,
sans xdotool au replay. Toute cadence reste `unqualified` jusqu'à census ; les
polls, pas le temps hôte, ordonnent les inputs.

### S2 — hooks déclaratifs

Créer un manifeste externe au produit :

```text
module + XEX SHA + fonction Ghidra + adresse + bytes
+ phase before/after + registres + effet + état de confiance
```

Les hooks produisent uniquement une trace normalisée. Ils ne modifient pas le
C++ généré et n'entrent pas dans le binaire natif.

### S3 — double reçu, pas double renderer

Pour une fenêtre stable M01 :

- rejouer les mêmes inputs dans l'oracle et le natif ;
- produire un reçu de draw par lane ;
- joindre par frame, ordinal, ressource et identité shader ;
- comparer ordre, états, transforms, constantes, targets et captures ;
- arrêter au premier paquet non attribué.

Le fallback émulé sert seulement à diagnostiquer l'oracle. La preview native
échoue proprement si le monde/HUD exige un paquet non porté.

### S4 — migration par classes

Ordre recommandé pour M01 : target/depth et caméra monde, terrain/ville,
textures/matériaux, ciel/eau/végétation, joueur/unités, puis HUD/2D. Chaque
classe passe par `provisional-rexglue` pour le bring-up, puis devient
`retail-qualified` seulement avant JV/M01-F/publication.

## Décisions

- Utiliser Skate comme patron de capture, de reçus et de migration A/B.
- Ne pas présenter son renderer comme indépendant de ReXGlue.
- Ne copier aucun code Skate tant que sa licence racine n'est pas clarifiée.
- Ne jamais modifier le code généré par remplacement textuel.
- Ne pas embarquer de fallback émulé dans le produit AC6.
- Exploiter le backend ReXGlue comme référence `provisional-rexglue`, puis
  qualifier tardivement le seul cône M01 atteint.

## Validation et risque résiduel

La revue a vérifié commit, submodule, intégration CMake, manifests, hooks,
contrat native guest renderer, command processors D3D12/Vulkan, caches
textures, traces, tests et workflows. Aucun actif privé ni code généré n'a été
lu ou modifié.

Risque principal : l'architecture Skate cache encore beaucoup de travail dans
ReXGlue. Une estimation fondée uniquement sur la taille du renderer natif
sous-estimerait PM4, EDRAM, queries, memexport, XMA, VFS et scheduling. Notre
route M01 évite ce piège en conservant ReXGlue comme oracle provisoire, jamais
comme dépendance produit.
