# AC6 retail NTSC-U/J — r491 — le nuanceur pixel réellement utilisé par les tirages d'interface rejetés (`e41b4b062083e5bf`) ne référence AUCUNE texture : `fetch_const[1]` (r490) est un cul-de-sac pour ces tirages précis, pas une seconde cause de rejet

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé, aucun nouveau lancement (sondage natif ou route
oracle) ce cycle — analyse statique sur des données déjà produites par
un cycle antérieur et retrouvées sur disque, plus lecture directe du
registre à signatures épinglées déjà committé.

## Contexte

Nommé par r490 (« Named for r491 »), option 2 : lire directement le
nuanceur pixel des tirages rejetés pour déterminer s'il référence
réellement l'unité de texture t0 — méthode statique, ne nécessitant
pas d'attendre la retombée de la contention hôte (toujours mesurée à
40-45 juste avant ce cycle, cohérent avec r489).

## Établi — la corrélation per-tirage manquante existait déjà, sur disque, non committée

r490 affirmait cette corrélation « non établie » faute du journal
source de r475. Une recherche directe sur `/fastdata/lavaulta/tmp/` a
retrouvé `/fastdata/lavaulta/tmp/ac6-r478-probe.log` — le journal brut
du cycle r478 (pas r475), jamais nettoyé, portant le même format de
trace `AC6_NATIVE_VD_TRACE` mais avec le champ `pixel_digest` que le
rapport r478 n'avait pas cité dans son texte. Ce fichier scratch,
antérieur à ce cycle, n'a pas été produit par ce cycle — seulement
lu :

```
r478 probe: primitive_type=0x01 rect_strip_expand=0 vertex_digest=09dd1c7cddae1141 ... pixel_digest=e41b4b062083e5bf pixel_ok=1
r478 probe: primitive_type=0x08 rect_strip_expand=1 vertex_digest=57b8e5f14b93cff4 ... pixel_digest=e41b4b062083e5bf pixel_ok=1
r478 probe: primitive_type=0x08 rect_strip_expand=1 vertex_digest=4dd456c4ea0923c1 ... pixel_digest=e41b4b062083e5bf pixel_ok=1
```
(9 lignes au total dans le fichier, les 3 digests vertex connus de
r477/r478/r481 s'y répètent — **toutes partagent le même
`pixel_digest=e41b4b062083e5bf`**, toujours accepté (`pixel_ok=1`).
Ceci confirme directement que le nuanceur pixel en question EST le
bon candidat à examiner pour statuer sur `fetch_const[1]`.

## Établi — ce nuanceur pixel ne sample aucune texture, sous aucune de ses 4 variantes de modification pinnées

`tools/materialize_pinned_shader_capsule.py:parse_capsule()` (lecture
seule) sur `native/fixtures/pinned-shader-registry.v1.bin` (état
actuel, 321 entrées) : 4 entrées `shader_type=1` (pixel) portant le
digest `e41b4b062083e5bf`, différant uniquement par `modification`
(`0x400000010001`, `0x400000000001`, `0x10001`, `0x1`) — même
microcode source (`[0, 268551168, 570425344, 3356459008, 0, 3254779904,
0, 0, 0]`) pour les 4.

Désassemblage SPIR-V direct (`spirv-dis`, les 4 variantes) :
- **Aucune ne déclare `OpTypeImage`/`OpTypeSampledImage`/`OpImageSample*`/`OpImageFetch`.**
- Les seules ressources liées sont `xe_uniform_system_constants`,
  `xe_uniform_bool_loop_constants`, `xe_uniform_fetch_constants`
  (buffer uniforme générique contenant TOUTES les constantes de fetch
  brutes — présent dans le squelette standard du traducteur, PAS une
  preuve d'échantillonnage de texture), `xe_shared_memory`,
  l'interpolateur d'entrée et la sortie fragment.

**Ce nuanceur pixel n'échantillonne donc RIEN — ni t0, ni aucune autre
unité de texture.** Il s'agit très probablement d'un nuanceur de
composition/couleur unie (lecture de constantes + interpolateur
uniquement), cohérent avec un élément d'interface simple (logo/texte
non texturé, ou un remplissage de couleur).

## Conséquence directe — `fetch_const[1]` (r490) est un cul-de-sac pour CES tirages précis

Puisque le nuanceur pixel réellement utilisé ne lit aucune constante
de texture fetch en pratique (même si le registre matériel
`fetch_const[1]` contient une valeur résiduelle, comme le supposait
déjà r490 avec prudence), le format qu'elle décode (`k_24_8`/
`k_16_16_16_16`, ni l'un ni l'autre `k_8_8_8_8`) n'a **aucun effet** sur
le résultat de ces tirages : `decode_pixel_texture()` n'est appelée
que pour les liaisons de texture réellement présentes dans le SPIR-V
épinglé (`parse_pixel_texture_bindings()`,
`native_vulkan_backend.cpp:2746`, en aval du point de rejet actuel) —
un nuanceur sans liaison de texture n'atteint jamais ce chemin.
**L'hypothèse prudente de r490 (« un second motif de rejet potentiel,
indépendant ») est réfutée pour ces tirages précis : il n'y a qu'UN
seul motif de rejet réel, déjà identifié depuis r476/r477/r478 —
l'incompatibilité de modification côté VERTEX.**

## Non établi

- **Ce que `fetch_const[1]` représente réellement dans l'état
  matériel** (valeur résiduelle d'un tirage antérieur non lié, comme
  l'envisageait déjà r490) — sans objet pour la question de rejet
  puisque ce nuanceur ne le lit jamais, mais reste une curiosité non
  résolue si un futur cycle en a besoin pour une autre raison.
- **Si un AUTRE nuanceur pixel, pas encore rencontré par le sondage
  natif, échantillonnerait bien t0 avec ce format** — hors périmètre
  (aucune preuve qu'un tel nuanceur existe pour ces tirages précis).

## Décisions prises

- Réutiliser directement le journal scratch retrouvé
  (`/fastdata/lavaulta/tmp/ac6-r478-probe.log`) plutôt que de relancer
  un sondage — c'est une preuve déjà produite par un cycle antérieur
  de CETTE campagne, pas une donnée externe non vérifiée ; son
  intégrité est confirmée par la cohérence totale avec les digests
  vertex déjà cités par r477/r478/r481 (aucune divergence).
- Ne PAS relancer de sondage natif ni de capture oracle ce cycle — la
  question posée (le nuanceur pixel échantillonne-t-il une texture ?)
  se répond entièrement par lecture statique du SPIR-V déjà pinné.
- Ne PAS modifier `native/` — aucun correctif n'est nécessaire ni
  justifié : la conclusion de ce cycle est que `decode_pixel_texture()`
  n'est PAS en cause pour ces tirages, pas qu'il faille l'étendre.
- Corriger la prudence de r490 : la corrélation qu'il jugeait
  nécessaire pour statuer existait déjà sur disque (scratch non
  nettoyé d'un cycle antérieur) — r490 n'avait pas cherché sous
  `/fastdata/lavaulta/tmp/` avant de conclure à une lacune de preuve.

## Gate

Aucune source de production modifiée ce cycle (analyse uniquement).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
`ctest` racine et natif inchangés (aucune source de production
modifiée ce cycle).

## Named for r492

**La piste `fetch_const[1]`/format de texture est close pour ces
tirages.** Le seul motif de rejet réel et déjà caractérisé depuis
r476/r477/r478 reste l'incompatibilité de modification côté vertex —
inchangé par ce cycle. Reste ouvert :
1. Piste 2 de r488 (chercher un autre mécanisme d'attente bloquant via
   lecture Ghidra directe sur un point de blocage concrètement
   observé) — toujours non tentée, méthode statique disponible
   immédiatement.
2. Une fois la contention hôte retombée : capture oracle ciblée avec
   minutage ajusté (nommée par r477) pour capturer les 3 nuanceurs
   vertex sous la bonne modification — la cible précise reste
   inchangée depuis r477/r478 (2-3 nuanceurs vertex, pas une nouvelle
   exploration large).
3. Décision de committage groupé de l'arriéré natif restant (si
   applicable, une fois `native/` confirmé stablement libre de la
   chaîne concurrente).

## Files

Aucun artefact gitignoré nouveau conservé (`/fastdata/lavaulta/tmp/r491-pixel-*.spv`,
désassemblages temporaires pour lecture, supprimés après usage).
