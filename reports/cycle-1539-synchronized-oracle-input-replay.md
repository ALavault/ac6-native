# Cycle 1539 — replay d'inputs oracle synchronisé

Date : 2026-08-12.

## Résultat et niveau de preuve

Un replay déterministe de `XamInputGetState` reste faisable sans savestate. La
clé primaire est l'ordinal global des appels XAM (`poll_index`), pas une horloge
ni un compteur de présentation. Elle conserve zéro, un ou plusieurs polls par
marqueur. Chaque poll est gardé par son ordre dans le marqueur, le LR invité,
l'utilisateur, les flags et la nullité du pointeur. Thread et adresse complète
du pointeur restent des diagnostics locaux à une lane.

Le prototype commun est `tools/ac6_controller_input_replay.py`. La migration
incompatible produit maintenant :

- `ac6.controller-input-replay.v3`, replay brut poll-exact ;
- `ac6.controller-cadence-census.v1`, census canonique séparé ;
- `ac6.native-controller-projection-receipt.v3`, reçu de projection ;
- `AC6RTPLY` v3, replay natif M01/Normal.

Le niveau disponible est uniquement
`integrity_only_runtime_census`. Il prouve la canonicalité, les bornes, les
digests et la cohérence interne des identités déclarées. Il ne prouve ni que le
producteur a réellement observé le runtime, ni l'authenticité du binaire, de la
configuration ou du marqueur. Il autorise provisoirement `slice-reseal`,
`export-controller-tsv` et `project-ac6rtply-v3`, mais jamais une gate de
parité. Le lecteur C++ retourne donc toujours
`source_lineage_verified=false`.

`tools/build_ac6_execution_trace_v2.py` refuse ce niveau avant de produire une
trace native. Une simple relabellisation canonique du reçu en
`runner_attested` est aussi refusée : aucune primitive d'attestation externe
fiable n'est intégrée. Le label n'est jamais traité comme une preuve.

Aucun census runtime AC6 n'a été capturé ni qualifié pendant ce cycle. Les
fixtures testent le contrat d'intégrité, pas l'authenticité d'un runner. Le
producteur runtime devra faire l'objet d'un audit séparé avant toute promotion.

## Contrat de census v1

Le sidecar est un objet JSON canonique d'une ligne, borné à 16 Mio et à
500 000 records. Il contient exactement :

- l'identité du producteur : lane, commit, SHA-256 du binaire, SHA-256 du build
  et plateforme ;
- les SHA-256 des configurations runtime et comportementale ;
- les SHA-256 fichier/payload du parent et la fenêtre
  `start_marker + marker_count` ;
- l'identité PAL complète et le contrat marqueur ;
- le code marqueur par `offset`, `length` et SHA-256 ;
- une horloge de référence rationnelle, 64 bits, lue avant le marqueur ;
- pour chaque marqueur observé : `sequence`, `parent_marker_index`,
  `event_sequence` et `reference_tick` ;
- un `payload_sha256`, calculé sans le champ digest.

Le census exige `N >= 2` records et dérive toujours `N - 1` intervalles. Pour
une horloge de référence `F = Fn/Fd`, la cadence source est calculée par
arithmétique entière exacte :

`source_hz = Fn * (N - 1) / (Fd * (last_tick - first_tick))`.

Les intervalles adjacents doivent être uniformes ; dénominateur non réduit,
flottant, tick non croissant, cadence non entière, record hors fenêtre,
off-by-one ou ratio natif non entier sont refusés. Le census ne contient aucun
`source_hz`, compte de ticks natifs ou durée déclarée à croire. La cadence
native 60 Hz provient uniquement du contrat épinglé
`ac6.native-simulation-clock.v1` (`ac6_native_fixed_step`, 60/1,
`one_simulation_step`) ; aucune option CLI ne peut la remplacer.

Avant reseal ou projection, le consommateur recalcule le SHA-256 fichier et le
payload du census, vérifie toutes les identités, la fenêtre parent, chaque
`event_sequence` contre le marqueur réel du parent, puis recalcule la cadence.
La fenêtre v3 ne garde ensuite que les résultats dérivés, les digests, les
comptes `N`/`N-1`, le contrat marqueur et le contrat d'horloge native.

## Qualification statique des frontières

- Xenia Canary : checkout détaché au commit
  `16e1eb8e28a2935b75c36707b585a4f5e174ad43` ; exécutable Windows épinglé
  SHA-256 `c52d27f9a115c036257efbedd91006e74964e0c12aebb09b0c1dd93a31280f9a`.
- Cible : title ID `4E4D07D1`, media ID `0379EFB3`, `default.xex` PAL
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`,
  versions XEX/base `v0.0.0.11`.
- AC6_recomp de comparaison : commit
  `dcd41b7457fcac8242f8ef40de83d1719390d5af` ; sa configuration locale
  préexistante est restée intacte.

À ce commit Xenia, `src/xenia/hid/input.h:77-92` définit un
`X_INPUT_GAMEPAD` big-endian de 12 octets et un `X_INPUT_STATE` de 16 octets.
`src/xenia/kernel/xam/xam_input.cc:101-141` est la frontière qui voit tous les
retours invités, y compris pointeur nul, utilisateur invalide, UI XAM, succès
et déconnexion. `InputSystem::GetState` perd les retours courts et ne convient
donc pas à un record/replay poll-exact.

Le shim peut fournir le contexte PPC hôte sans modifier l'ABI invité. Il donne
LR et thread. Le replay doit préserver les 16 octets sémantiques, le résultat
et l'effet `XmpVolumePatch::OnInputPoll` sur succès. Les LR qualifiés restent
`0x8234D418` pour le poll régulier et `0x8234D4DC` pour la reconnexion ;
`0x823911C0` tail-branche et ne fournit pas le LR attendu.

`Clock::QueryGuestTickCount` rééchantillonne l'horloge hôte et perturberait la
mesure. Une sonde Xenia devra lire un snapshot verrouillé sans update. Le
compteur `XE_SWAP` existant n'est pas atomique ; un compteur oracle dédié doit
l'être, et reste de la télémétrie inter-thread. Breakpoints et présentations
hôte ne sont pas des clés de consommation.

Les candidats marqueurs restent distincts :

| frontière | état actuel | usage permis |
|---|---|---|
| `0x821CA908` | stage d'entrée qualifié statiquement, cadence runtime non auditée | contrat sémantique provisoire |
| `0x821EFBA0` | observation AC6_recomp antérieure proche de 60 Hz | candidat à recenser sous runner épinglé |
| `0x8226D1C8` | manager observé historiquement proche de 30 Hz | candidat de fenêtre logique, phase à requalifier |

Un breakpoint 60 Hz suspendrait tous les threads. Sur x64, Xenia connaît la
fonction invitée courante et dispose d'un `CallNative` sûr après prologue : un
observateur borné peut être injecté sans modifier les octets invités. Dans
ReXGlue, la frontière XAM et le contexte PPC sont directement accessibles ;
le seam est plus simple, mais la route M01 doit encore devenir reproductible.
Aucun C++ généré ne doit être modifié.

## Projection et lecture C++

La projection exige exactement un poll réussi user 0 par marqueur. Elle mappe
`pitch=LY`, `roll=LX`, `yaw=LB ? -32768 : RB ? 32767 : RX`, `throttle=RT` et
`buttons=raw`. Pour une cadence dérivée 30→60, elle applique un zero-order hold
x2 une seule fois ; 60→60 reste une identité. Aucun TSV intermédiaire
n'intervient dans la route native.

Le reçu v3 scelle les digests brut/parent, la fenêtre, la cible PAL et le code
marqueur, la référence census `N`/`N-1`, le contrat natif épinglé, le mapping,
l'index de cache, les compteurs de frames et le SHA-256 de la sortie. Il ne
contient ni frames, ni octets retail, ni faits invités synthétiques.

Le lecteur C++ `retail_projection_receipt` accepte ce reçu uniquement comme
sidecar provisoire. Il vérifie sur les mêmes snapshots bornés le JSON canonique,
le reçu, le replay `AC6RTPLY` v3 et l'index de cache. Il borne séparément reçu et
replay, leur taille cumulée, le code marqueur et la fenêtre
`start_marker + marker_count`. Son succès signifie
`native_output_verified=true` et `source_lineage_verified=false`.
Ses descripteurs sont ouverts non bloquants et doivent désigner des fichiers
réguliers ; FIFO, socket et périphérique sont refusés avant lecture.

## Fermeture des erreurs

Les lecteurs Python prennent un snapshot borné, puis analysent et hachent ces
mêmes octets. Le builder borne chaque artefact à 512 Mio, le brut à 1 Gio et la
somme des cinq entrées à 1 Gio. Le comparateur borne chaque trace avant
`json.loads` et limite leur somme à 1 Gio. Les artefacts ne sont jamais
rehachés par une seconde ouverture pour remplir le résultat.

Les écritures CLI sont atomiques individuellement et refusent tout alias
entrée/sortie, y compris les hardlinks existants. Pour la projection, le replay
est synchronisé et publié avant le reçu ; le reçu publié et son répertoire
synchronisé constituent le marqueur de commit. Une interruption brutale peut
laisser un replay orphelin sans reçu, donc non consommable comme paire ; elle ne
peut pas laisser un reçu durable désignant un replay encore absent. Une erreur
rattrapée retire les sorties partielles.

Le comparateur de traces est lui aussi fermé comme gate : en l'absence de
vérification d'attestation runner, toute comparaison est refusée par défaut.
L'option explicite `--allow-legacy-diagnostic` conserve l'analyse structurelle
des anciens JSON, avec `proof_level=structural_diagnostic`, jamais une preuve
de parité. Profondeur, nœuds, conteneurs, chaînes, fenêtre totale et nombres
non finis sont bornés avant ou pendant le décodage.

Le parseur brut refuse notamment : JSON non canonique, troncature, footer ou
digest corrompu, trous d'ordinal, fichier vide, absence de poll ou de marqueur,
plages XInput invalides, champs inconnus, identités/session divergentes,
chaînes non bornées et dépassements d'événements, marqueurs ou octets. La
projection est bornée à un million de frames.

## Architecture record → replay → compare

1. Le runner qualifie binaire, build, XEX, contenu, configurations, route et
   code marqueur avant de reprendre le thread principal.
2. XAM réserve un ordinal à chaque entrée, enregistre tous les retours et
   scelle le replay brut v3 à l'arrêt propre.
3. Le replay vérifie ordinal et gardes avant chaque retour ; EOF, événement
   restant ou identité divergente invalide le run.
4. Le runner émet séparément un census brut pour la même capture et la même
   fenêtre. L'outil en vérifie l'intégrité et dérive la cadence sur `N-1`.
5. `slice-reseal` lie parent, fenêtre, records, census et horloge native.
6. `project-ac6rtply-v3` revalide le census, sélectionne le poll user 0,
   applique le hold dérivé et publie le replay puis son reçu-marqueur v3.
7. Cette sortie peut alimenter un replay produit provisoire, mais le builder
   `ac6.execution-trace.v2` reste fermé tant qu'une attestation externe n'est
   pas implémentée et vérifiée.

## Migration fail-closed v1/v2 → v3

Les replays bruts v1 et v2 sont rejetés. Les reçus v1 et v2 sont rejetés. Les
CLI exigent `--cadence-census`; les anciennes options Hz et
`--cadence-evidence` n'existent plus. Une migration ne doit jamais recopier des
Hz déclarés : il faut produire de nouveaux records census sur le parent
original.

Une fenêtre dérivée doit référencer le census v1 et le contrat natif épinglé.
Le vieux label `qualification=runtime_census` ne confère aucun niveau de
preuve. `runner_attested` est réservé mais non accepté tant que le builder ne
dispose pas d'une attestation externe vérifiable.

## Validation et risques résiduels

Validations ciblées après migration :

- C++ : build de `ac6-retail-projection-receipt-tests`, CTest ciblé 1/1,
  puis CTest complet 87/87, dont 5 skips explicites faute de ressources ;
- Python ciblé : 66/66 tests replay/census/trace/comparateur ; suite
  `tools/tests` : 254 tests et 37 sous-tests réussis ;
- Ruff format/check et `git diff --check` réussis.

Les tests couvrent notamment : v1/v2 refusés, `N=2 => N-1=1`, trois records
et deux intervalles, intervalles non uniformes, mutations record/producteur/
configuration/parent/fenêtre/code marqueur, bornes et off-by-one, ZOH 30→60,
identité 60→60, sélection discriminante des états et hashes aux ticks 2/4
contre 1/3, reçu provisoire refusé par la gate, relabel+reseal
`runner_attested` refusé, gate comparateur fermée sans attestation, lecture
comparateur bornée avant JSON, NaN/infini/profondeur refusés, somme des entrées,
SHA-256 sur snapshot, publication reçue comme marqueur de commit et alias.

Risques ouverts : aucun producteur Xenia/ReXGlue v3 n'a encore été audité ni
exécuté ; l'authenticité des records, l'interleaving de polls, la phase exacte
du marqueur, la relation marker/poll/vblank, les effets HID d'un bypass XAM et
la route reproductible vers tutoriel/M01 restent à qualifier. Ces risques
interdisent encore toute affirmation de parité ou fermeture de lane.
