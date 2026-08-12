# Cycle 1549 — audit source de DownpourRecomp et de son fork ReXGlue

## Verdict

Downpour apporte surtout des **cas négatifs utiles** pour AC6. Le code public
qualifie quelques mécanismes du fork `v1.0`, mais pas les fonctionnalités
annoncées pour `v1.1` à `v1.1.6`. Les sept tags correspondants ne changent que
`README.md`; le dépôt SDK public s'arrête au tag `v1.0`.

Les seuls apports retenus pour AC6 sont des patrons à réimplémenter et tester :

- instrumentation par phase et clés de cache versionnées ;
- remise à zéro complète de l'état d'entrée lors d'une perte de focus ;
- lecture bornée suivie d'une vérification SHA-256 du payload attendu.

Le cache VFS négatif, le ring memexport, le traitement souris et les
contournements EDRAM restent `provisional-rexglue` ou `divergent`. Aucun ne
constitue une preuve retail AC6.

## Provenance

| Élément | Révision qualifiée | Arbre | Licence |
|---|---|---|---|
| `LittleBitUA/DownpourRecomp` | `66c075d9fe9cbf712ac1694a7b108ae630a0e06a` (`v1.1.6`) | `dfb815ecb2f9a99bead585a7cbc0e7a4e6730b82` | BSD-3-Clause |
| `LittleBitUA/rexglue-sdk-dpour` | `03b3282fd1263c5642f5925ba625b3ba0f6940c9` (`v1.0`) | `415f53ca5242cb712e09e7bfc48879cda8d49065` | BSD-3-Clause, dérivé de Xenia |

Les deux HEAD ont été recoupés avec `git ls-remote` le 12 août 2026. Le projet
contient 26 fichiers suivis et exclut `assets/` et `generated/`; le SDK contient
1 329 fichiers suivis et 22 sous-modules épinglés. Aucun XEX, actif retail ou
binaire de release n'a été lu.

Fichiers de preuve, SHA-256 :

- manifeste : `f638efad0d2f97b1b197d3830a11f91cf3edbcd1e8b490b16a36238e8fdb1c70` ;
- VFS : `e6a86bdbe4cc29a6c769c9ae529c63383f38c9831b21c69491bf7d5b86f7ae03` ;
- command processor D3D12 : `ddab260791df160c8fb6bffe10775bcde89e38fbc3046c0342d765ac7ed45cae` ;
- entrée MnK : `9bbbbcf8f467fd2dba1331c41ad0578af5a5e9850c072db12f14de4c3f9e0db1`.

## Frontière source/release

Les commits pointés par les tags suivants modifient exclusivement
`README.md` :

| Tag | Commit |
|---|---|
| `v1.1` | `d54b09c8126c6cee5e0558d3460b98a379a5c7b0` |
| `v1.1.1` | `79de8a6bd6ab09f38a024195d5706b8d7d17a4d8` |
| `v1.1.2` | `855b3aaea7b1967f443c637c561879bf1b26cea6` |
| `v1.1.3` | `944e81abd8ed07d8509c33847a748b833f2a962e` |
| `v1.1.4` | `6eccd923f85e013e5dfb12034051f58c108f1191` |
| `v1.1.5` | `b94a5c7d1f95298aa46720a0c9b0bb805b5a8dc1` |
| `v1.1.6` | `66c075d9fe9cbf712ac1694a7b108ae630a0e06a` |

Le SDK public n'implémente donc pas les symboles annoncés après `v1.0`, dont
`pending_case_a_reads_`, `mnk_raw_input_scale`, `mnk_stick_scale` et la route
`WM_INPUT`. Ces fonctions sont `documented-unmatched`, pas
`provisional-rexglue`.

Le manifeste public demande en outre `sdk_version = "0.8.2.19"`, tandis que le
fork public est étiqueté `v1.0` sur un socle CMake `0.8.0`. Le commentaire du
manifeste cite un générateur `v0.8.1.7-dev.g14275e8`. Sans manifeste de build
de la release liant ces identités, le binaire distribué n'est pas
reproductible depuis les seules sources publiées.

## VFS et cache négatif

Le fork ajoute un ensemble de 16 384 chemins introuvables sous verrou global.
Il vide ce cache lors des changements de périphériques et de liens, et tente
une invalidation ciblée après `CreatePath`. Le principe est intéressant pour
un VFS émulé très sollicité, mais deux écarts interdisent sa reprise directe :

1. le lookup teste la clé canonique **avant** résolution du lien symbolique,
   puis `remember_miss` insère la clé **après** résolution. Un miss via symlink
   n'accélère donc pas nécessairement la requête entrante suivante ;
2. sur un périphérique hôte writable, un répertoire dont l'arbre mémoire est
   vide court-circuite tout nouveau `ListFiles`. Une création hôte extérieure
   au VFS peut rester invisible indéfiniment.

Il n'existe aucun test VFS du cache, des symlinks, des mutations concurrentes
ou de l'éviction. AC6 possède déjà un cache retail v2 immuable après import :
ce mécanisme n'apporte rien à M01 tant qu'un profil montre de vrais misses
répétés. Si un cache négatif devient nécessaire, il devra être indexé sur la
forme résolue et invalidé par génération de montage, avec tests de mutation.

## Memexport et synchronisation GPU

Le code public `v1.0` possède un ring D3D12 de trois buffers. Pour une clé
chaude, il copie le memexport courant dans le slot d'écriture puis remet en
mémoire invitée le slot écrit deux tours auparavant. Si le slot de lecture
n'existe pas, il appelle encore immédiatement
`IssueDraw_MemexportReadbackFullPath`, qui ferme/attend la file.

Le commentaire source dit explicitement que le batching des premiers misses
est un futur refactor. Aucun `pending_case_a_reads_` n'existe et `IssueSwap`
ne draine aucune liste de ce type. Cela contredit l'annonce `v1.1.1` du README.

Conséquences pour AC6 :

- le ring est une optimisation à visibilité retardée, pas une sémantique
  retail démontrée ;
- il ne doit pas servir à qualifier des compteurs, indirect draws ou matrices
  lus par le CPU dans la même frame ;
- le renderer C++ natif M01 n'émule pas PM4/memexport et ne doit pas importer
  ce mécanisme ;
- ReXGlue peut l'utiliser pour le bring-up, mais toute observation dépendante
  est marquée `provisional-rexglue`.

## Entrées

La route publique `v1.0` apporte une table explicite clavier/souris vers le
masque XInput, une queue d'arêtes `GetKeystroke` et un nettoyage central lors
de la perte de focus. Ce dernier invariant est réutilisable : après perte de
focus, boutons, deltas, sticks virtuels, molette et événements en attente
doivent être remis à zéro avant le prochain poll.

En revanche :

- l'EMA et la décroissance sont appliquées par appel à `GetState`, sans `dt` ;
- les répétitions utilisent `steady_clock`, donc dépendent de l'hôte ;
- `packet_number_` est incrémenté à chaque poll, même sans changement d'état ;
- le raw input annoncé à partir de `v1.1.2` n'est pas dans le dépôt SDK.

Cette route est impropre à une preuve déterministe. Le replay AC6 reste capturé
au seam XAM poll-exact, puis projeté une seule fois en entrées normalisées. Les
QoL souris éventuelles seront placées en amont de cet état normalisé et exclues
du profil `retail`.

## Textures, EDRAM et rendu

Deux documents sont particulièrement instructifs comme frontières :

- le correctif chromatique ignore volontairement certaines transitions
  depth vers `k_2_10_10_10_FLOAT`. C'est un contournement spécifique Downpour,
  désactivé par défaut, et non une sémantique Xenos ;
- le document BC3/DXT5 ne contient qu'une hypothèse RenderDoc. Il affirme que
  les indices alpha sont probablement mal décodés, mais aucun correctif ni
  vecteur de test n'est présent.

Le premier est `divergent` pour AC6. Le second confirme la gate déjà choisie :
BC3 tiled/endian ne passe qu'avec bytes retail bornés, vecteur synthétique de
bloc et contrôle image positif. Une capture visuellement plausible d'un autre
jeu ne suffit pas.

## PSO et instrumentation

Le cache D3D12 sépare utilement :

- descriptions portables `.xpso` et microcode `.xsh`, clés incluant title ID,
  échelle, voie ROV/RTV et versions traducteur/description ;
- `ID3D12PipelineLibrary` local, explicitement GPU/driver-spécifique ;
- compteurs atomiques par phase pour distinguer compilation, attente fence,
  texture, resolve et barrières.

Le patron de versionnement et les compteurs sont réutilisables pour le backend
Vulkan AC6. Le format lui-même ne l'est pas : la copie d'un seed n'est pas
authentifiée, une divergence du sidecar ne fait qu'émettre un warning, et les
écritures retirent la destination avant rename. Pour AC6, le cache pipeline
reste dérivable/reconstructible, lié au SHA des shaders et de la configuration,
et ne doit jamais être confondu avec le cache retail atomique.

## Import de contenu et STFS

L'installateur TU borne le fichier d'entrée à 256 Mio, accepte un payload brut
ou STFS, puis exige taille et SHA-256 exacts du `default.xexp` avant staging.
C'est une bonne frontière d'identité, mais le lecteur n'est pas un modèle
fail-closed général :

- `Entry::length` peut annoncer jusqu'à 4 Gio et est passé à `reserve` avant
  recoupement avec la taille du conteneur ;
- les chaînes de blocs ne possèdent ni ensemble `visited` ni plafond propre au
  payload ; un cycle peut répéter des blocs jusqu'à la longueur déclarée ;
- le STFS n'est pas authentifié ; seul le payload final connu est protégé ;
- le fichier final est ouvert avec `trunc` au lieu d'un temporaire + fsync +
  rename, donc une interruption détruit l'ancienne copie valide.

AC6 conserve son import v2 atomique et ses bornes strictes. La seule idée à
reprendre est la whitelist `taille + SHA-256` avant publication dans le cache.

## Tests et décision AC6

Le fork SDK contient des tests unitaires et PPC génériques, désactivés par
défaut. Les workflows publics construisent les trois plateformes mais
n'activent pas `REXGLUE_BUILD_TESTS`. Aucun test ne couvre les ajouts VFS,
memexport, PSO, MnK ou l'installateur STFS. Le projet Downpour n'a pas de CI.

Matrice finale :

| Élément | Classe AC6 | Action |
|---|---|---|
| métriques par phase, clé de cache versionnée | `provisional-rexglue` | réimplémenter seulement avec tests Vulkan |
| nettoyage d'entrée sur perte de focus | `provisional-rexglue` | ajouter lors de la vague QoL, hors profil retail |
| taille + SHA du payload avant staging | `provisional-rexglue` | invariant déjà satisfait par import v2 |
| cache VFS négatif actuel | `divergent` | ne pas porter sans profil et correctifs |
| ring memexport deux tours | `divergent` pour validation | autorisé seulement dans l'oracle de bring-up |
| skip depth vers 7e3 | `divergent` | exclure |
| BC3 annoncé | `documented-unmatched` | conserver la gate image positive AC6 |
| raw input/batching/save backup v1.1+ | `documented-unmatched` | attendre publication du code exact |

Cet audit ne ferme aucune lane M01. Il réduit surtout le risque de prendre une
release jouable pour une preuve de fidélité du runtime sous-jacent.
