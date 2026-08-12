# Cycle 1539 — replay d'inputs oracle synchronisé

Date : 2026-08-12.

## Résultat

Un replay déterministe de `XamInputGetState` est faisable sans savestate. La
clé primaire retenue est l'ordinal global des appels XAM (`poll_index`), et non
une horloge ou un compteur de présentation. Elle existe dès le boot et préserve
nativement zéro, un ou plusieurs polls par tick. Chaque poll est gardé par
l'ordre dans le marqueur, le LR invité, l'utilisateur, les flags et la nullité
du pointeur. L'identifiant de thread et l'adresse complète du pointeur ne sont
stricts qu'entre deux exécutions du même producteur ; ils ne sont pas portables
entre Xenia, AC6_recomp et le natif.

Le prototype commun est dans `tools/ac6_controller_input_replay.py`, avec ses
tests dans `tools/tests/test_ac6_controller_input_replay.py`. Il scelle une
trace JSONL poll-exacte, refuse les identités ou fichiers invalides, compare un
record et son replay, extrait une fenêtre de marqueurs liée par SHA-256 à son
parent, puis la projette directement en `AC6RTPLY` v3. La capture complète reste
obligatoirement `cadence.status=unqualified`; seule la fenêtre resealée porte la
cadence mesurée. La projection refuse un marqueur contenant zéro ou plusieurs
états user 0 réussis. Une cadence mesurée 30→60 applique le zero-order hold x2
exactement une fois, sans TSV intermédiaire.

Aucun patch Xenia n'est promu par ce cycle : l'ABI et les sites sont établis,
mais la cadence du meilleur marqueur sémantique n'a pas encore été recensée en
runtime. Produire un patch avant cette mesure aurait figé une hypothèse. Aucun
Xenia n'a été exécuté, aucun service n'a été arrêté, et l'installation, le
profil Wine, les fichiers retail, Vulkan et le produit C++ sont restés intacts.

## Qualification des sources

- Source autoritaire Xenia Canary : dépôt officiel, checkout détaché propre au
  commit complet `16e1eb8e28a2935b75c36707b585a4f5e174ad43`.
- Exécutable Windows épinglé, lu seulement pour son hash : SHA-256
  `c52d27f9a115c036257efbedd91006e74964e0c12aebb09b0c1dd93a31280f9a`.
- Cible : Xbox 360 PAL, title ID `4E4D07D1`, `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`,
  media ID `0379EFB3`, version XEX et base `v0.0.0.11`.
- AC6_recomp de comparaison : checkout de référence
  `dcd41b7457fcac8242f8ef40de83d1719390d5af`; sa modification locale de config
  préexistante a été préservée.

## Frontières Xenia exactes

### ABI d'entrée

À ce commit, `src/xenia/hid/input.h:77-92` définit un
`X_INPUT_GAMEPAD` big-endian de 12 octets et un `X_INPUT_STATE` de 16 octets :
packet `be32`, boutons `be16`, deux triggers `u8`, quatre sticks `be16`.

`src/xenia/kernel/xam/xam_input.cc:101-141` est la seule frontière qui voit
tous les appels invités : pointeur nul, utilisateur invalide, UI XAM active,
succès, déconnexion et packet final. `src/xenia/hid/input_system.cc:115-138`
ne voit que le chemin pilote et perd donc les retours courts XAM. Il applique
en outre le deadzone et met à jour le slot utilisé. Le hook record/replay doit
donc vivre dans `XamInputGetState_entry`, sans corriger au passage son
comportement actuel (notamment le passage de `user_index`, pas
`actual_user_index`, à `InputSystem::GetState`).

L'ajout d'un dernier argument hôte `const ppc_context_t& ctx` ne change pas
l'ABI invité : le mécanisme shim fournit déjà ce pseudo-paramètre à de nombreux
exports (`src/xenia/kernel/util/shim_utils.h:178-201,352-377`). Il donne
`ctx->lr` et `ctx->thread_id` (`src/xenia/cpu/ppc/ppc_context.h:378-424`). Le
replay doit recopier exactement les 16 octets sémantiques et le `X_RESULT`, puis
conserver l'appel `XmpVolumePatch::OnInputPoll` sur succès non nul
(`xam_input.cc:133-136`).

Le LR qualifié n'est pas `0x823911CC` : `0x823911C0` tail-branche vers l'import
et conserve le LR de son appelant. Les deux retours observés sont
`0x8234D418` (poll régulier) et `0x8234D4DC` (reconnexion ponctuelle), établis
par le cycle 408 et recoupés par les callsites `0x8234D414/0x8234D4D8`. La
fixture utilise donc `0x8234D418`.

### Horloge et présentation

`Clock::QueryGuestTickCount` appelle `UpdateGuestClock`, qui échantillonne
l'horloge hôte et met à jour un cache sous mutex
(`src/xenia/base/clock.cc:78-104,166-171`). Son résultat varie donc avec
l'ordonnancement et l'appeler depuis la sonde perturbe lui-même la mesure. Il
faut ajouter un `PeekGuestTickCount` verrouillé, sans nouvel échantillon hôte,
et garder cette valeur comme télémétrie seulement.

`ExecutePacketType3_XE_SWAP` appelle `IssueSwap` puis incrémente `counter_`
(`src/xenia/gpu/pm4_command_processor_implement.h:644-667`). `counter_` est un
simple `uint32_t` (`src/xenia/gpu/command_processor.h:128-129,512`) : le lire
depuis le fil XAM serait une data race C++. Une instrumentation doit ajouter un
compteur oracle atomique dédié au paquet invité `XE_SWAP`. Il mesure les swaps
invités, pas les présentations hôte, qui peuvent être retardées, remplacées ou
supprimées. Même atomique, son ordre relatif à un poll sur un autre fil reste
une télémétrie, jamais une clé de consommation.

### Marqueur invité

Le breakpoint standard n'est pas adapté à 60 Hz : un hit suspend tous les fils
et place l'exécution en pause (`src/xenia/cpu/processor.cc:616-677`). Sur x64,
le JIT connaît en revanche `current_guest_function_`
(`src/xenia/cpu/backend/x64/x64_emitter.cc:111-137`) et possède un
`CallNative` sûr après le prologue (`:202-274,836-859`). Un observateur borné
peut donc être émis au début d'une fonction qualifiée sans modifier un octet
invité. Cette voie couvre Windows x64 sous Wine et Linux x64 avec le même code
source.

Trois rôles restent distincts :

| frontière | preuve actuelle | usage permis |
|---|---|---|
| `0x821CA908` | Ghidra canonique : un seul appelant dans `0x821D7A90`, stage d'entrée « every frame » ; cadence runtime non recensée | meilleur garde sémantique, `cadence.status=unqualified`, export interdit |
| `0x821EFBA0` | observation AC6_recomp antérieure : 4 250 appels pour 4 250 vblank source 0, environ 60 Hz | candidat de cadence ; à reproduire sous le Xenia épinglé et à relier aux polls |
| `0x8226D1C8` | frontière manager de la capture AC6_recomp actuelle, échantillonnée à 30 Hz | trace v2 30 Hz, conversion native explicite ZOH x2 ; jamais présentée comme marqueur 60 Hz |

Le format encode donc `marker_role`, `cadence.status`, `source_hz`,
`native_hz`, `resampling` et la politique de projection. Il n'impose aucune
cadence universelle.

## Format commun et fermeture des erreurs

Le header scelle :

- producteur : lane, commit complet, SHA-256 du binaire, plateforme ;
- cible : title ID, media ID, version XEX, base version, module, SHA-256 XEX,
  XXH3 du code chargé, adresse et SHA-256 du code du marqueur ;
- session : manifestes de contenu et profil/save, configuration runtime,
  configuration comportementale portable, route et origine `clean_boot` ou
  `sealed_retail_save` ;
- segment : `full_recording` sans parent, ou `marker_window` lié au SHA-256 du
  fichier parent, à son `payload_sha256` et à la fenêtre source
  `(start_marker, marker_count)` ;
- synchronisation : clé, gardes portables, diagnostics locaux, télémétries,
  rôle, phase (`before_input`/`after_input`) et cadence. Le schéma lie les
  couples connus : `ac6_frame_input_stage/821CA908/before_input` et
  `mission_manager_tick/8226D1C8/after_input`; toute autre combinaison est
  refusée. `0x821EFBA0` reste une télémétrie de cadence inter-thread, pas un
  marqueur poll-associable dans cette version.

Chaque poll conserve l'ordinal, le marqueur et son sous-ordinal, LR, thread,
adresse pointeur, user, flags, résultat et l'état complet. Un query
`SUCCESS + pointeur NULL` est conservé avec `state=null`; tout résultat d'erreur
a `state=null`. La projection `InputFrame` refuse ces queries. Le footer scelle
header, événements et ses propres compteurs par SHA-256 canonique.

Le parseur refuse notamment : fichier vide, sans poll ou sans marqueur, ou
supérieur à 128 Mio, plus d'un
million d'événements/polls, plus de 500 000 marqueurs, JSON non canonique,
troncature, corruption du footer, trous d'ordinal, temps/presents décroissants,
champs inconnus, plages XInput invalides, chaînes ou module non bornés, cible,
session ou configuration divergente. Aucun octet retail, profil ou save n'est
placé dans la trace, seulement des digests et des états manette.
La projection native est bornée séparément à un million de frames, soit
9 000 121 octets maximum pour `AC6RTPLY` v3. Le produit
`marker_count × hold` est vérifié avant toute allocation ; l'index de cache est
borné à 128 Mio et le reçu à 64 Kio. Les sorties replay/reçu sont préparées dans
leur répertoire, synchronisées puis publiées sans écrasement ; une validation,
un hash ou une publication échouée ne laisse aucune paire partielle.

La comparaison est stricte au sein d'une même lane et d'un producteur exact :
elle inclut thread et adresse pointeur. Entre lanes, elle compare seulement
l'ordre, LR, user, flags, nullité du pointeur, résultat et état ; les valeurs
locales restent diagnostiques. Entre lanes, le digest de configuration
comportementale reste strict, tandis que les configurations runtime spécifiques
à Xenia/ReXGlue peuvent différer. `guest_tick` et `present_index` produisent des
deltas, sans décider l'égalité. La commande `verify` sans header attendu ne
déclare que `qualification=structural_only`; une gate PAL qualifiée doit fournir
le header attendu et obtenir `qualification=identity_checked`.

## AC6_recomp : plus simple techniquement, pas encore une route M01 fiable

Dans ReXGlue, `thirdparty/rexglue-sdk/src/kernel/xam/xam_input.cpp:90-112`
offre directement la même frontière XAM. Le contexte PPC courant est disponible
dans le runtime, donc LR et thread peuvent être capturés sans JIT. Le seam
`InputSystem::GetState` (`thirdparty/rexglue-sdk/src/input/input_system.cpp:77-127`)
est simple mais fusionne les pilotes et, comme chez Xenia, ne voit pas tous les
retours XAM. L'injection poll-exacte doit donc être remontée à XAM.

La pile capture existante contient déjà un replay TSV dans la glue : elle
remplace l'état par tick manager et teste zéro, un ou plusieurs `GetState` par
tick. C'est une bonne preuve de faisabilité, mais elle rejoue un état 30 Hz,
pas la séquence exacte des polls. Elle doit consommer le même JSONL commun au
niveau XAM. Les marqueurs se posent via les wrappers forts hand-written de
capture (`rex_sub_ADDRESS` appelant `__imp__rex_sub_ADDRESS`) ou le harness.
Seuls `src/ac6_oracle_probe.cpp`, les wrappers manuels, ReXGlue et le harness
sont autorisés ; `ppc_recomp.*` et tout autre C++ généré restent intacts.

Son horloge invitée est également dérivée de l'hôte
(`thirdparty/rexglue-sdk/src/native/core/clock.cpp:16-73,129-137`) et ne peut
pas être une clé. Surtout, le boot M01 n'est pas aujourd'hui reproductible :
les routes récentes s'arrêtent de façon intermittente avant la transition,
sur `LOADING`, ou au menu armes, sans atteindre régulièrement
`0x822A6710/0x8226D1C8`. L'injection y est donc plus facile à coder que dans
Xenia, mais elle ne résout pas à elle seule cette divergence de route.

## Architecture de trace Xenia / recomp / natif

1. `ac6.controller-input-replay.v1` est le contrat brut poll-exact partagé par
   Xenia et AC6_recomp. `native` n'est pas une lane productrice valide : lui
   attribuer LR, thread, pointeur ou horloge invités fabriquerait de faux faits.
2. `slice-reseal` extrait une fenêtre contiguë de marqueurs de la capture
   complète, renumérote marqueurs, polls et événements, mesure la cadence sur ce
   seul segment et scelle les SHA-256 ainsi que les coordonnées du parent. Une
   capture complète ne peut donc pas hériter d'une cadence mesurée ailleurs.
3. `project-ac6rtply-v3` consomme directement cette fenêtre brute et l'index de
   cache courant. Elle produit les cinq champs `controller_input` : `pitch=LY`,
   `roll=LX`, `yaw=LB ? -32768 : RB ? 32767 : RX`, `throttle=RT`,
   `buttons=raw`. Packet, LT et RY restent dans le brut. Le hold entier mesuré
   est appliqué ici exactement une fois ; aucun TSV ni second ZOH n'intervient.
4. `ac6.execution-trace.v2` reste le contrat de parité à cinq domaines. La
   projection alimente seulement `controller_input`; les quatre autres domaines
   doivent être observés séparément et ne sont jamais fabriqués depuis la trace
   manette.

Le fichier natif suit exactement `RetailSessionReplay::write_file` : magic
`AC6RTPLY\0`, version 3 et métadonnées M01/Normal/loadout `1/1`/capability true
en little-endian, digest de l'index de cache, seed stable non nul
`0xAC60000000000001`, zéro checkpoint, `final_tick=frame_count`, SHA-256 de la
concaténation des neuf octets de chaque frame, puis le nombre et les frames.

Le reçu canonique `ac6.native-controller-projection-receipt.v1` est strictement
métadonnées : SHA-256 du replay brut et de son payload, hashes et coordonnées du
parent, identité PAL scellée, cadence/hold, mapping, index de cache, compteurs,
digest des inputs et SHA-256 de la sortie. Il ne contient ni frame, ni octet
retail, ni champ guest inventé. L'export TSV conservé par l'outil sert seulement
aux diagnostics historiques ; il est exclu de cette route native.

## Protocole record → replay → compare

1. Le harness clone le profil/save local dans une racine privée de run, calcule
   les manifestes sans copier leurs octets dans l'artefact, qualifie binaire,
   XEX, contenu, config, route, code marqueur et module chargé.
2. Le record démarre avant la reprise du thread principal et couvre le boot,
   les menus et le tutoriel. Un save retail peut réduire les prérequis, mais le
   replay repart toujours d'un processus frais ; ce n'est pas un point de
   reprise arbitraire.
3. XAM réserve un ordinal à chaque entrée, enregistre tous les chemins de
   retour et scelle le flux seulement à l'arrêt propre. Le contrôleur physique
   est ignoré en replay.
4. Avant chaque retour replay, le moteur vérifie l'ordinal et les gardes. Le
   premier mismatch, EOF prématuré, événement restant, hash ou identité
   divergente arrête le titre et marque la capture invalide.
5. Le replay émet sa propre trace observée. `compare` vérifie événements,
   résultats et états, puis rapporte séparément les dérives d'horloge et de
   swap. La projection controller n'est faite qu'après égalité poll-exacte et
   qualification de cadence.
6. `slice-reseal` extrait la fenêtre commune validée, attache la cadence mesurée
   au segment et la lie cryptographiquement à la capture complète.
7. `project-ac6rtply-v3` vérifie le cache, exige exactement un poll user 0
   réussi par marqueur, applique le hold une seule fois et publie atomiquement
   le replay v3 avec son reçu canonique. Le natif ne consomme jamais le JSONL
   brut Xenia/ReXGlue.

## Plan de patch exact

Pour Xenia au commit épinglé :

- ajouter un service borné `controller_input_replay.{h,cc}` et ses tests de
  parsing/identités dans `src/xenia/base/` ;
- initialiser et qualifier le service dans `Emulator::CompleteLaunch`, après
  `FinishLoadingUserModule` et le chargement de config mais avant
  `main_thread_->Resume` (`src/xenia/emulator.cc:1485-1545,1705-1726`) ; finaliser
  dans `TerminateTitle` ;
- instrumenter tous les retours de `XamInputGetState_entry` et ajouter le
  pseudo-paramètre contexte ; préserver les effets XMP et documenter/émuler les
  effets HID `UpdateUsedSlot` lorsque le pilote physique est court-circuité ;
- ajouter `PeekGuestTickCount` sous mutex ;
- ajouter un compteur oracle `std::atomic<uint64_t>` au paquet `XE_SWAP` ;
- émettre l'observateur de fonction qualifiée dans le body x64 après le
  prologue, jamais via un breakpoint ;
- ajouter les cvars record/replay/manifest/marker, toutes désactivées par
  défaut, et refuser un build commit ou une identité runtime divergents.

Avant promotion, le patch devra être produit depuis un worktree propre du
commit complet, puis passer `git apply --check` sur ce même commit, build/tests
Windows et Linux, tests négatifs du format, et deux records/replays complets.
Ce cycle n'a produit aucun patch source Xenia ; il n'y a donc pas de patch
partiel à appliquer ou qualifier.

## Validation et risques résiduels

Validations du prototype : `pytest` ciblé **30/30**, `ruff check`,
`ruff format --check`, `py_compile` et `git diff --check` réussis. Les tests
couvrent notamment corruption, troncature, identité/session, ordinals,
replay vide, query NULL, état d'erreur, bornes, cadence non qualifiée, ZOH
30→60, lien parent/fenêtre, layout binaire v3 exact, reçu sans frames, cache
corrompu, refus d'écrasement/publication partielle, projection ambiguë, marqueur
absent, footer et comparaison cross-lane.

Le contrôle d'intégration store-backed a projeté la fixture 30→60 directement
vers le cache PAL v2 qualifié
`cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85`.
Le lecteur C++ produit a accepté le fichier v3 : 4 frames, 4 ticks, 4
échantillons de trace, 20 événements, digest du reçu identique, replay
déterministe et scénario resté fail-closed. Aucun TSV ni constructeur legacy
30→60 n'a été appelé.

Risques restant à fermer dynamiquement : interleaving de polls simultanés,
stabilité du thread et du pointeur entre deux boots identiques, effet HID d'un
bypass XAM, cadence et phase exactes du marqueur, relation marker↔poll↔vblank,
capabilities/keystrokes non rejoués, dérive des timers et route jusqu'au
tutoriel/M01. Un census neutre de deux boots doit précéder tout replay utile.

Estimation : **1 journée** pour l'intégration Xenia/ReXGlue et les builds
offline ; **1 à 2 journées** pour qualifier deux boots, le tutoriel puis M01,
selon la stabilité de la route. Le premier checkpoint recevable est plus court :
record/replay boot→menu avec égalité poll-exacte, cadence encore non projetée.
