# AC6 retail NTSC-U/J — r224 : DOCUMENTATION SEULE — les trampolines `XamShow*` sont la même table de repli dynamique que r212 (2026-09-02/03)

- Recherche binaire brute de l'adresse propre du trampoline
  `XamShowMarketplaceUI` (`0x821f4680`) dans le XEX qualifié : trouvée
  comme entrée d'une VRAIE table `{adresse_fonction, tag}` d'au moins
  14 entrées, toutes avec le même motif de tag `0x4000____` — EXACTEMENT
  la forme du mécanisme de compatibilité ascendante déjà analysé par
  r212 pour `XexGetModuleHandle`/`XexGetProcedureAddress` (résolution
  dynamique échoue toujours dans ce build → repli sur adresse statique
  fixe). Cette table EST la liste de ces adresses de repli.
- Conséquence : les trampolines `XamShow*` (bloc « non tracé » de r206/
  r212) sont RÉELLEMENT atteints via ce même mécanisme confirmé actif —
  pas du code orphelin comme la priorisation précédente le suggérait
  implicitement. Correction de priorisation, pas de correction d'un fix
  antérieur.
- Non implémenté : confirmer l'atteignabilité ne détermine pas la
  valeur sûre à retourner pour chacun — nécessite de tracer la fonction
  résolveur qui lit cette table (équivalent de `Function_821FCCE0` de
  r212 mais pour cette table à entrées multiples), effort pluri-cycle.
- Aucun changement de code. Tests 205/205, `ctest` 10/10 (inchangés).
  Voir
  `reports/ac6-retail-native-codegen-gate2-r224-doc-xamshow-trampolines-are-the-fallback-table-from-r212-20260902.md`.

# AC6 retail NTSC-U/J — r223 : VRAI CORRECTIF — `XamUserReadProfileSettings` retourne succès (2026-09-02)

- Tracé les deux cibles de branchement « ne correspond pas au code
  spécifique » de l'appelant (`0x821ce6dc`, `0x821ce8c8`) : AUCUNE
  n'est un chemin d'erreur — l'une est un simple `return 0` propre,
  l'autre pose un drapeau interne et retourne 1. N'importe quelle
  valeur de retour hors 0x7a/0x3e5 est donc déjà gérée sans risque.
  Site d'appel réel : `dwNumSettingIds=0`/`pdwSettingIds=NULL` (appel
  dégénéré, zéro réglage demandé).
- Corrigé : retourne `STATUS_SUCCESS`. Signature réelle au-delà des 4
  premiers paramètres pas assez confirmée pour écrire via
  `pcbResults`/`pResults` (8 registres + 1 argument pile observés, 2 de
  plus que la forme à 7 paramètres classique) — seul le statut de
  retour est corrigé.
- Tests 205/205 (204/204 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r223-real-fix-xamuserreadprofilesettings-returns-success-20260902.md`.

# AC6 retail NTSC-U/J — r222 : VRAI CORRECTIF — `XamShowMessageBoxUIEx` s'achève de manière synchrone (2026-09-02)

- Réexamen de la réserve de r221 : lire un argument pile via
  `ctx.r1.u32 + 0x54` n'est PAS une supposition ABI — les stubs natifs
  reçoivent `ctx` inchangé de l'appelant (aucune poussée de frame en
  traversant vers un stub natif), donc `ctx.r1.u32` EST le `r1` de
  l'appelant au moment du `bl`, par construction directe, pas par
  convention à part confirmer. Le désassemblage montre déjà ce que
  l'appelant y a écrit.
- Corrigé : écrit `pMessageBoxResult` (r10) et `pOverlapped+0x14`
  (lu via `ctx.r1.u32 + 0x54`) à 0 (« bouton 0 » par défaut) et
  retourne `STATUS_SUCCESS` — JAMAIS 997, donc l'appelant saute
  toujours le helper d'attente asynchrone et lit le résultat
  directement.
- Tests 204/204 (203/203 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r222-real-fix-xamshowmessageboxuiex-completes-synchronously-20260902.md`.

# AC6 retail NTSC-U/J — r221 : DOCUMENTATION SEULE — signature réelle de `XamShowMessageBoxUIEx` résolue, convention d'accès aux arguments pile toujours non confirmée (2026-09-02)

- Numéroté méthodiquement chaque adresse relative à la pile du wrapper
  (`Function_821F5BA8`) : signature réelle à 9 paramètres entièrement
  résolue contre le site d'appel réel, `pOverlapped` identifié comme le
  9e argument (passé sur la pile, valeur = buffer local à
  `r1+0x68`), zéro-initialisé avant l'appel. Le helper d'attente (r220)
  opère sur `pOverlapped+8` (convention XAM réelle de réutilisation des
  champs Offset/OffsetHigh). Le résultat final (bouton pressé) est lu à
  `pOverlapped+0x14` si l'appel initial ne retourne PAS 997.
- Non implémenté : écrire ce fix exigerait de lire le 9e argument
  (passé sur la pile) depuis le stub natif via `ctx.r1.u32 + 0x54`
  (raisonnement ABI PowerPC standard), mais AUCUN cas existant dans ce
  projet ne lit un argument pile au-delà de r10 pour confirmer cette
  convention spécifiquement pour l'accès stub natif. Recherché un
  exemple confirmé dans le code PPC généré — résultat négatif (faux
  positif, usage local non lié à un argument). Toujours non implémenté.
- Aucun changement de code. Tests 203/203, `ctest` 10/10 (inchangés).
  Voir
  `reports/ac6-retail-native-codegen-gate2-r221-doc-xamshowmessageboxuiex-signature-resolved-stack-arg-convention-still-unconfirmed-20260902.md`.

# AC6 retail NTSC-U/J — r220 : DOCUMENTATION SEULE — protocole d'achèvement overlapped partiellement tracé, non implémenté (2026-09-02)

- Suite à r219 : tracé le helper d'attente générique
  (`Function_821F50F8`) que `XamShowMessageBoxUIEx` invoque quand son
  retour initial est 997 (ERROR_IO_PENDING). Confirmé : lit
  `*pOverlapped` (+0, comparé à 0x3e5) puis +4 comme résultat réel —
  layout OVERLAPPED standard (Internal@0/InternalHigh@4). Une
  complétion synchrone (même précédent que `NtReadFile`, r124/r126)
  est architecturalement possible en principe.
- Non implémenté : reconstituer QUELLE adresse pile porte réellement
  `pOverlapped` a exigé de suivre plusieurs adresses relatives à la
  pile candidates (`&r1+0xcc`, `&r1+0x68`, `&r1+0x70`, `&r1+0x60`) sans
  confirmation suffisante du nombre/ordre exact des paramètres réels de
  cette version du SDK — écrire au mauvais offset corromprait un état
  local non lié plutôt que le bon. Arrêté avant d'implémenter plutôt
  que deviner un offset.
- Aucun changement de code. Tests 203/203, `ctest` 10/10 (inchangés).
  Voir
  `reports/ac6-retail-native-codegen-gate2-r220-doc-overlapped-completion-protocol-partially-traced-not-implemented-20260902.md`.

# AC6 retail NTSC-U/J — r219 : DOCUMENTATION SEULE — corrige r209/r211 : la porte `XamGetExecutionId` est TOUJOURS contournée (2026-09-02)

- Tracé les 5 appelants réels du wrapper `0x821f7668` jusqu'à LEURS
  propres appelants : les 5 passent une valeur de contrôle LITTÉRALE
  `0` (`li r3,0x0`). La logique du wrapper (`beq` sur contrôle==0 →
  raccourci succès SANS jamais appeler le vrai `XamGetExecutionId`)
  signifie que cette porte est TOUJOURS contournée dans ce XEX — pas
  un cas partiel, un chemin jamais exercé.
- Conséquence : `XamUserCreateStatsEnumerator`/
  `XamUserCreateAchievementEnumerator` sont en réalité déjà adéquats
  (tout appelant traite un retour non nul comme un skip propre — même
  case que `XamUserAreUsersFriends`). `XamUserReadProfileSettings` reste
  différé, mais pour la VRAIE raison : son propre contrat d'achèvement
  asynchrone (codes 0x7a/0x3e5, même famille que
  `XamShowMessageBoxUIEx` de r206), pas la porte `XamGetExecutionId`.
- Aucun changement de code. Tests 203/203, `ctest` 10/10 (inchangés).
  Voir
  `reports/ac6-retail-native-codegen-gate2-r219-doc-corrects-r209-r211-xamgetexecutionid-gate-is-always-bypassed-20260902.md`.

# AC6 retail NTSC-U/J — r218 : DOCUMENTATION SEULE — bilan du balayage r169-r217 (2026-09-02)

- Aucun changement de code. Bilan complet du balayage des imports
  offline (125→87 restants) : catégorise ce qui est corrigé (bugs réels,
  fixes cosmétiques à retour ignoré, corrections de cycles antérieurs,
  déjà-adéquats confirmés) et ce qui reste, par gros chantier (réseau
  NetDll_*, SEH, écriture de sauvegarde, famille XMsg, moteur printf,
  grappe identité/profil XamGetExecutionId, trampolines UI non tracés,
  clé console hors de portée permanente, imports réellement
  inatteignables 0 site, `XamTaskCloseHandle` inerte sans
  `XamTaskSchedule`, `NtDuplicateObject` re-examiné — signature réduite
  à 3 registres sur 7, risque réel d'écriture sauvage si corrigé
  naïvement).
- Voir
  `reports/ac6-retail-native-codegen-gate2-r218-doc-sweep-status-checkpoint-r169-through-r217-20260902.md`
  pour le détail complet par catégorie.

# AC6 retail NTSC-U/J — r217 : VRAI CORRECTIF — `VdSetDisplayMode` réussit toujours (2026-09-02)

- Site réel unique `0x821f075c` (même fonction que
  `VdGetSystemCommandBuffer` de r198) : retour totalement ignoré.
- Corrigé : `STATUS_SUCCESS` sans condition — même précédent que
  `VdRetrainEDRAM`.
- Vérifié aussi, non corrigé : `VdPersistDisplay` (site réel
  `0x821f09d8` — VÉRIFIE le retour et alimente une soumission de
  frame réelle — territoire renderer natif fail-closed, hors scope).
- Tests 203/203 (202/202 → +1). `ctest` 10/10. Le reste de la liste
  générique (~87 imports) se concentre désormais dans une poignée de
  gros chantiers déjà documentés (réseau NetDll_*, SEH, écriture de
  sauvegarde, famille XMsg, moteur printf, grappe identité/profil
  XamGetExecutionId, trampolines UI Xam non tracés) — les petites
  victoires isolées se raréfient. Voir
  `reports/ac6-retail-native-codegen-gate2-r217-real-fix-vdsetdisplaymode-always-succeeds-20260902.md`.

# AC6 retail NTSC-U/J — r216 : VRAI CORRECTIF — la famille `NtSetTimerEx`/`NtCancelTimer`/`NtCreateTimer` se déclenche réellement (2026-09-02)

- Signature réelle à 8 arguments de `NtSetTimerEx` entièrement résolue
  via le wrapper (`0x82204c54`/`0x82390ab0`) : `TimerApcRoutine=NULL`,
  `TimerType=1` (Synchronization), `DueTime≈-101984` (100ns, ~10,2ms
  relatif), `Period=0` (unique). Attendu via `NtWaitForSingleObjectEx`
  sur le même handle (déjà modélisé par `g_events`), pas via l'APC.
  `NtCreateTimer` partageait le stub générique avec `NtCreateMutant` —
  n'enregistrait JAMAIS le handle dans `g_events`, donc même un
  minuteur réel se serait toujours soldé en TIMEOUT (même classe que
  r145 pour `NtCreateSemaphore`).
- Corrigé : `NtCreateTimer` enregistre maintenant via `create_event`
  (auto-reset). `NtSetTimerEx` lit le vrai `DueTime` 64 bits et lance
  un `std::thread` détaché (`set_timer()`) qui dort puis appelle
  `set_event()`, se répétant sur `Period` si non nul. `NtCancelTimer`
  pose un drapeau d'annulation par minuteur (`cancel_timer()`) vérifié
  avant chaque déclenchement.
- Tests 202/202 (201/201 → +1, 1 reciblé). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r216-real-fix-nt-timer-family-actually-fires-20260902.md`.

# AC6 retail NTSC-U/J — r215 : VRAI CORRECTIF — `XMsgCancelIORequest` réussit toujours (2026-09-02)

- `XMsgCancelIORequest` (famille `XMsg` de r203) : 3 sites d'appel
  réels, tous vérifiés individuellement — retour totalement ignoré
  partout.
- Corrigé : `STATUS_SUCCESS` sans condition — même classe que
  `KeLockL2`/`KeUnlockL2` (r197). Le transport `XMsg` reste différé
  (r203), donc jamais de vraie requête en vol à annuler — succès
  honnête quel que soit l'état du transport.
- Tests 201/201 (200/200 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r215-real-fix-xmsgcanceliorequest-always-succeeds-20260902.md`.

# AC6 retail NTSC-U/J — r214 : VRAI CORRECTIF — `HalReturnToFirmware` termine proprement (2026-09-02)

- `VOID HalReturnToFirmware(...)` : ne retourne jamais (redémarre/arrête
  la console). Site réel unique `0x821f7e6c` — preuve plus faible que
  r193/r209/r213 (un épilogue normal EST émis après l'appel), mais un
  compilateur ignorant qu'un callee ne retourne jamais en émet un quand
  même par prudence; le contrat documenté reste gouvernant.
- Corrigé : `std::exit(0)` — même classe que `XamLoaderTerminateTitle`
  (r209), puisque c'est un arrêt niveau CONSOLE ENTIÈRE, pas par thread
  (le déroulement `GuestThreadTerminated` de r213 ne s'applique pas ici).
- Tests 200/200 (199/199 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r214-real-fix-halreturntofirmware-exits-cleanly-20260902.md`.

# AC6 retail NTSC-U/J — r213 : VRAI CORRECTIF — `ExTerminateThread` déroule proprement son propre thread (2026-09-02)

- `VOID ExTerminateThread(DWORD)` : ne retourne jamais (même preuve que
  r193/r209 — aucun épilogue après son 2e site d'appel réel
  `0x82390b38`). Contrairement à `KeBugCheck`/`XamLoaderTerminateTitle`,
  ne termine QUE le thread appelant, pas tout le processus/titre — les
  threads invités tournent en vrais `std::thread` détachés
  (`ExCreateThread`, r111-r115).
- Corrigé : nouveau type `ac6::native::GuestThreadTerminated` dans le
  VRAI header `native/include/ac6/native_runtime.h`. `ExTerminateThread`
  le lance; le lambda de `ExCreateThread` ET l'appel de sonde
  d'entrée principale (`native/src/ac6recomp_main.cpp`) le rattrapent
  désormais pour laisser leur thread se terminer proprement au lieu de
  faire s'échapper l'exception (ce qui appellerait `std::terminate()`
  sur tout le processus). `tools/prepare.py` relancé avant build
  (édition de vrais fichiers `native/`).
- Corrigé aussi : `ExRegisterTitleTerminateNotification` (9 sites
  réels, 2 tracés — retour totalement ignoré partout, même classe que
  `KeLockL2`/`KeUnlockL2`).
- Tests 199/199 (196/196 → +3). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r213-real-fix-exterminatethread-unwinds-its-own-thread-cleanly-20260902.md`.

# AC6 retail NTSC-U/J — r212 : DOCUMENTATION SEULE — l'échec de `XexGetModuleHandle`/`XexGetProcedureAddress` EST le bon chemin de repli (2026-09-02)

- Les deux sites d'appel réels suivent le motif standard Xbox 360 de
  compatibilité ascendante : sonder un export XAM plus récent, sinon
  retomber sur une implémentation statique déjà liée. `kOfflineStatus`
  fait échouer systématiquement, ce qui route TOUJOURS vers le repli
  statique — le même chemin qu'un vrai dashboard plus ancien prendrait
  aussi. Ce n'est PAS un bug négligé : le corriger nécessiterait de
  synthétiser un vrai pointeur de fonction appelable invité pour
  `bctrl`, plus risqué (crash) que le repli actuel, pour aucun gain.
  Confirmé adéquat, aucun fix prévu.
- Vérifié aussi, pas de nouveau fix : `XamContentCreateEx` (site réel
  unique, plusieurs validations de paramètres avant l'import; quand
  atteint, appartient au territoire save/reload déjà différé par r202).
- Aucun changement de code ce cycle. Tests 196/196, `ctest` 10/10
  (inchangés depuis r210). Voir
  `reports/ac6-retail-native-codegen-gate2-r212-doc-xexgetmodulehandle-failure-is-the-safe-fallback-path-20260902.md`.

# AC6 retail NTSC-U/J — r211 : DOCUMENTATION SEULE — portée réelle de `XamGetExecutionId`, signature pointeur-vers-pointeur (2026-09-02)

- Escalade : le wrapper `0x821f7668` déjà tracé par r206/r209 garde
  AUSSI `XamUserCreateStatsEnumerator` (2 sites) et
  `XamUserCreateAchievementEnumerator` (1 site) — portée totale
  confirmée : au moins 7 sites d'appel réels à travers 3 API distinctes.
- Correction : la vraie signature est `DWORD
  XamGetExecutionId(PXAM_EXECUTION_INFO *ppInfo)` — un pointeur-VERS-un-
  pointeur (double indirection confirmée dans le wrapper), pas un
  remplissage de struct dans un buffer appelant comme r206 l'avait
  décrit. Le champ à +0xC de la structure pointée n'est pas confirmé
  indépendamment — pas de valeur écrite pour éviter une supposition.
- Vérifié aussi, aucun fix nécessaire : `XamUserAreUsersFriends` (site
  réel `0x82205518` — l'appelant ne vérifie JAMAIS le retour, ne lit
  que son propre buffer pré-rempli — comportement déjà correct).
- Vérifié aussi, non corrigé : `XamUserGetXUID`/`XamUserGetSigninInfo`
  (motif de wrapper différent, masque des bits de la VALEUR DE RETOUR
  elle-même contre `0x70000` — signification non confirmée).
- Aucun changement de code ce cycle. Tests 196/196, `ctest` 10/10
  (inchangés depuis r210). Voir
  `reports/ac6-retail-native-codegen-gate2-r211-doc-xamgetexecutionid-gates-more-than-scoped-real-signature-is-pointer-to-pointer-20260902.md`.

# AC6 retail NTSC-U/J — r210 : VRAI CORRECTIF — `XMACreateContext`/`XMAReleaseContext` (2026-09-02)

- `XMACreateContext` (site réel `0x823aec8c`) : `r3` pointeur de sortie
  handle (confirmé — relu immédiatement après succès). Vérifié en
  signé — `kOfflineStatus` bloquait systématiquement toute
  initialisation de contexte audio XMA. `XMAReleaseContext` (site réel
  `0x823ae37c`) : retour totalement ignoré.
- Corrigé : Create alloue un handle via `g_next_handle` et l'écrit en
  sortie; Release réussit sans condition.
- Vérifié aussi, non corrigé : `XamVoiceSubmitPacket` (dépend d'un
  handle que seul `XamVoiceCreate` produit, laissé en échec honnête par
  r207 — fixer Submit seul serait inerte).
- Tests 196/196 (195/195 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r210-real-fix-xmacreatecontext-xmareleasecontext-20260902.md`.

# AC6 retail NTSC-U/J — r209 : VRAI CORRECTIF — `XamLoaderTerminateTitle` termine proprement (2026-09-02)

- `VOID XamLoaderTerminateTitle(VOID)` : ne retourne jamais sur vrai
  matériel. Site réel `0x821f608c` concluant : l'instruction suivante
  (`0x821f6090`) est le PROLOGUE d'une AUTRE fonction — aucun épilogue
  émis après cet appel. Même classe que `KeBugCheck` (r193), mais
  sortie normale, pas un fault.
- Corrigé : `std::exit(0)` (pas `std::abort()`, puisque c'est une
  sortie de titre normale demandée, pas un crash).
- Tests 195/195 (194/194 → +1). `ctest` 10/10.
- **Addenda** : `XamGetExecutionId` (différé en r206) s'est révélé
  garder AU MOINS 4 sites d'appel réels de `XamUserReadProfileSettings`
  via le même wrapper (`0x821f7668`) — portée plus large que ce que
  r206 avait cadré. Toujours non corrigé (champ de struct non confirmé,
  valeur de vérification par site non tracée) — nommé pour un futur
  cycle.
  Voir
  `reports/ac6-retail-native-codegen-gate2-r209-real-fix-xamloaderterminatetitle-exits-cleanly-20260902.md`.

# AC6 retail NTSC-U/J — r208 : VRAI CORRECTIF — `XamVoiceClose` réussit toujours (2026-09-02)

- `VOID XamVoiceClose(HANDLE)` : 3 sites d'appel réels, tous vérifiés
  individuellement ce cycle — chacun ignore totalement la valeur de
  retour.
- Corrigé : `STATUS_SUCCESS` sans condition — même classe que
  `KeLockL2`/`KeUnlockL2`/`KiApcNormalRoutineNop` (r197) et la famille
  `IoDismountVolume` (r204).
- Tests 194/194 (193/193 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r208-real-fix-xamvoiceclose-always-succeeds-20260902.md`.

# AC6 retail NTSC-U/J — r207 : VRAI CORRECTIF — `XamVoiceHeadsetPresent` signale l'absence (2026-09-02)

- `BOOL XamVoiceHeadsetPresent(HANDLE)` : booléen simple, pas un
  NTSTATUS. Site réel `0x82206900` compare directement à zéro (pas de
  vérification de statut signé). `kOfflineStatus` (non nul) était lu
  comme VRAI (« casque présent ») à chaque appel — aucun périphérique
  micro/casque réel dans ce projet.
- Corrigé : retourne `0` (FAUX) sans condition — stable dans la logique
  de détection de changement de l'appelant.
- Vérifié aussi, non corrigé : `XamVoiceCreate` (site réel `0x82207620`,
  vérifié en signé — l'échec actuel semble déjà être le résultat
  honnête vu l'absence de vrai microphone; pas de fix forcé sans
  confirmation que « réussir avec un faux handle » serait meilleur).
- Tests 193/193 (192/192 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r207-real-fix-xamvoiceheadsetpresent-reports-absent-20260902.md`.

# AC6 retail NTSC-U/J — r206 : VRAI CORRECTIF — famille `XAudioRegisterRenderDriverClient`/`Unregister`/`SubmitRenderDriverFrame` (2026-09-02)

- `XAudioUnregisterRenderDriverClient` (site réel `0x823a664c`) : retour
  vérifié en SIGNÉ — `kOfflineStatus` (négatif) faisait échouer TOUJOURS
  le chemin de réinit avant même d'atteindre l'appel Register suivant —
  vrai blocage d'ordre d'initialisation.
  `XAudioRegisterRenderDriverClient` (site réel `0x823a667c`) : `r4` est
  un pointeur de sortie handle (confirmé — relit la même case que
  Unregister). `XAudioSubmitRenderDriverFrame` (site réel `0x823a68c0`) :
  retour totalement ignoré. Aucun vrai pipeline audio à fabriquer —
  pure gestion de handle + remise de trame sans conséquence.
- Corrigé : Register alloue un handle via `g_next_handle` et l'écrit en
  sortie; Unregister/Submit réussissent sans condition.
- Vérifié aussi, non corrigé : `XamGetExecutionId` (champ de struct non
  confirmé indépendamment); `XamShowMessageBoxUIEx` (contrat UI
  asynchrone ERROR_IO_PENDING, sous-système d'attente overlapped non
  implémenté); `XamShowSigninUI` et 6 dialogues `XamShow*` frères
  (trampolines à une instruction, appelants non tracés individuellement).
- Tests 192/192 (191/191 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r206-real-fix-xaudio-render-driver-client-family-20260902.md`.

# AC6 retail NTSC-U/J — r205 : VRAI CORRECTIF — `XamNotifyCreateListener` retourne un vrai handle (2026-09-02)

- `HANDLE XamNotifyCreateListener(...)` retourne un HANDLE, pas un
  NTSTATUS. Site réel `0x82204f08` : `cmplwi r3,0x0; beq <retry>` —
  n'importe quelle valeur non nulle est lue comme « handle acquis ».
  `kOfflineStatus` (non nul) faisait donc passer un code de statut pour
  un handle valide — pas un bug observable actuellement (r200 a rendu
  tout consommateur du handle inoffensif), mais malhonnête.
- Corrigé : alloue un vrai handle via `g_next_handle`, même compteur
  que `NtCreateTimer`/`NtCreateMutant`.
- Tests 191/191 (190/190 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r205-real-fix-xamnotifycreatelistener-returns-a-real-handle-20260902.md`.

# AC6 retail NTSC-U/J — r204 : VRAI CORRECTIF — `IoDismountVolume`/`IoDismountVolumeByFileHandle` réussissent toujours (2026-09-02)

- `IoDismountVolume` (2 sites réels, dans la même fonction que
  `XamTaskShouldExit` de r198)/`IoDismountVolumeByFileHandle` (1 site
  réel, queue de nettoyage inconditionnelle de l'écriveur de
  sauvegarde tracé par r202 — atteinte sur succès ET échec) : retour
  totalement ignoré par tous les appelants réels.
- Corrigé : `STATUS_SUCCESS` sans condition pour les deux — même classe
  que `KeLockL2`/`KeUnlockL2`/`KiApcNormalRoutineNop` (r197).
- Tests 190/190 (189/189 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r204-real-fix-iodismountvolume-family-always-succeeds-20260902.md`.

# AC6 retail NTSC-U/J — r203 : VRAI CORRECTIF — `XAudioGetVoiceCategoryVolume`/`VolumeChangeMask`; corrige r197 (2026-09-02)

- **Correction de r197** : `0x823cfe4c` N'EST PAS une fonction interne
  non importée comme r197 l'affirmait — c'est le vrai import
  `XMsgStartIORequest` (17 sites d'appel réels, confirmé). La constante
  `0xfb` que r197 appelait un « id d'événement télémétrie » est en
  réalité le paramètre `MessageType` de cet appel. La famille de
  wrappers `XamSession*` de r197 fait donc du VRAI trafic IPC via
  `XMsgStartIORequest`, pas de la télémétrie — une découverte plus
  large que ce que r197 pensait, corrigée ici par nom et numéro de
  cycle comme l'exige la discipline de preuve de ce projet.
- `XMsgStartIORequest` (17 sites)/`XMsgInProcessCall` (6 sites) : vrai
  transport de dispatch de messages inter-sous-système utilisé par de
  nombreuses API XAM de haut niveau. Nécessiterait d'énumérer et
  comprendre les types de messages distincts sur 17+ sites — effort
  bien plus large qu'un fix borné, non tenté.
- Corrigé : `XAudioGetVoiceCategoryVolumeChangeMask`/
  `XAudioGetVoiceCategoryVolume` (sites réels `0x823ad1fc`/`0x823ad22c`,
  confondus par une recherche par sous-chaîne naïve — même piège que
  r193). Aucun mixeur de volume réel : « rien n'a changé » (masque 0)
  et volume plein (1.0f) sont les défauts honnêtes.
- Tests 189/189 (188/188 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r203-real-fix-xaudio-voice-category-volume-corrects-r197-20260902.md`.

# AC6 retail NTSC-U/J — r202 : DOCUMENTATION SEULE — chemin réel d'écriture de sauvegarde tracé; SEH et clé console vérifiés, aucun fix sûr (2026-09-02)

- `NtWriteFile` (8 sites réels)/`NtDeviceIoControlFile` (3 sites réels,
  même voisinage) forment un vrai ÉCRIVEUR DE FICHIER DE SAUVEGARDE :
  `NtOpenFile` → IOCTL de géométrie de volume → boucle `NtWriteFile` à
  décalage croissant dimensionnée sur cette géométrie (motif FATX
  classique, pas de la télémétrie). C'est la forme binaire concrète de
  la frontière « save/reload » déjà nommée bloquée par Gate 2 dans
  `NEXT.md`. Non implémenté : nécessiterait un vrai support d'écriture
  dans `NativeGuestMediaService` (lecture seule aujourd'hui) plus le
  décodage du code IOCTL réel — deux efforts propres, pas un fix borné.
  Adresses nommées pour un futur cycle save/reload.
- Vérifié aussi, non corrigé : `RtlRaiseException`/`RtlUnwind`/
  `RtlCaptureContext` (moteur SEH complet nécessaire, aucune primitive
  seule n'est utile isolément); `XeKeysConsolePrivateKeySign`/
  `XeKeysConsoleSignatureVerification` (clé privée matérielle
  spécifique à la console — hors de portée DE FAÇON PERMANENTE, pas
  juste ce cycle).
- Aucun changement de code ce cycle. Tests 188/188, `ctest` 10/10
  (inchangés depuis r201). Voir
  `reports/ac6-retail-native-codegen-gate2-r202-doc-real-save-write-path-found-seh-and-console-key-imports-checked-no-safe-fix-20260902.md`.

# AC6 retail NTSC-U/J — r201 : VRAI CORRECTIF — `NtOpenFile` réutilise le chemin média de `NtCreateFile` (2026-09-02)

- `NtOpenFile` : 9 sites d'appel réels, le plus haut compte trouvé dans
  ce balayage. Site `0x821f7308` confirme la signature réelle à 6
  arguments : `FileHandle`/`ObjectAttributes`/`IoStatusBlock` exactement
  aux mêmes positions r3/r5/r6 que `NtCreateFile` (r122/r123/r129).
- Corrigé : `NtOpenFile` rejoint le `render_body` de `NtCreateFile`
  (même chemin `ObjectAttributes`→`ANSI_STRING`→
  `native_guest_media_service().open_file()`). Nettoyage incident : le
  message de trace était littéralement `"[NtCreateFile]"` — devient une
  f-string interpolant le vrai nom pour ne pas mal étiqueter les appels
  `NtOpenFile`.
- Tests 188/188 (187/187 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r201-real-fix-ntopenfile-reuses-ntcreatefiles-media-service-path-20260902.md`.

# AC6 retail NTSC-U/J — r200 : VRAI CORRECTIF — `XNotifyGetNext` signale l'absence de notification (2026-09-02)

- `BOOL XNotifyGetNext(...)` : 4 sites d'appel réels. Site `0x82165868`
  confirme le contrat (`cmpwi r3,0x0; beq` → 0=aucune notification,
  sinon lit `*pdwId`). `kOfflineStatus` non nul faisait croire à une
  notification à CHAQUE appel, lisant un id de notification depuis une
  case pile jamais écrite par ce projet — un vrai branchement sur
  mémoire non initialisée, à chaque appel.
- Corrigé : retourne `0` (FAUX) sans condition — même classe que
  `XamTaskShouldExit` (r198). `XNotifyPositionUI` (cosmétique, retour
  ignoré par son site d'appel réel) corrigé au même titre.
- Tests 187/187 (186/186 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r200-real-fix-xnotifygetnext-reports-no-notification-pending-20260902.md`.

# AC6 retail NTSC-U/J — r199 : VRAI CORRECTIF — `NtFlushBuffersFile` réussit toujours (2026-09-02)

- `NTSTATUS NtFlushBuffersFile(HANDLE, PIO_STATUS_BLOCK)` : 2 sites
  d'appel réels (`0x82392848`, `0x8239130c`), aucun ne relit
  `IoStatusBlock` après l'appel — l'un vérifie seulement succès/échec,
  l'autre ignore totalement le retour. Le média invité de ce projet est
  en lecture seule (r189/r190/r197) : jamais d'écriture en attente à
  purger.
- Corrigé : retourne `STATUS_SUCCESS` sans condition. `IoStatusBlock`
  laissé non écrit puisqu'aucun site d'appel réel ne le relit.
- Tests 186/186 (185/185 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r199-real-fix-ntflushbuffersfile-always-succeeds-20260902.md`.

# AC6 retail NTSC-U/J — r198 : VRAI CORRECTIF — `XamTaskShouldExit` reste au travail par défaut (2026-09-02)

- `BOOLEAN XamTaskShouldExit(VOID)` : sans paramètre (site réel
  `0x82391950`). Le no-op offline (`kOfflineStatus` non nul) était lu
  comme « doit sortir = vrai » — la boucle de travail de ce site
  abandonnait immédiatement à chaque passage.
- Corrigé : retourne `0` (FAUX, continue le travail) — défaut honnête en
  l'absence de tout mécanisme de signal de sortie réel.
- Vérifié aussi, non corrigé : `XamTaskSchedule`/`XamTaskCloseHandle`
  (site réel `0x82391df0`/`0x82391e00` — appelle en vrai un pointeur de
  fonction invité en tâche de fond, nécessiterait un sous-système
  d'exécution de callback invité entier, hors scope d'un cycle borné);
  `VdGetSystemCommandBuffer` (site réel `0x821f061c` — plomberie
  graphique, hors politique du renderer natif fail-closed).
- Tests 185/185 (184/184 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r198-real-fix-xamtaskshouldexit-defaults-to-keep-working-20260902.md`.

# AC6 retail NTSC-U/J — r197 : VRAI CORRECTIF — `KeLockL2`/`KeUnlockL2`/`KiApcNormalRoutineNop` réussissent toujours (2026-09-02)

- 3 imports à site d'appel réel unique dont l'appelant IGNORE totalement
  la valeur de retour (aucun `cmpwi`/branche après l'appel).
  `KeLockL2`/`KeUnlockL2` (`0x821eded0`, `0x821eea94`) : verrouillage de
  voies de cache L2 matériel réel, sans équivalent côté hôte à émuler.
  `KiApcNormalRoutineNop` (`0x821e6908`) : no-op documenté par son propre
  nom — routine APC par défaut sans routine utilisateur réelle.
- Corrigé : `STATUS_SUCCESS` sans condition pour les trois — même
  précédent que `VdRetrainEDRAM`.
- Vérifié aussi, non corrigé : `XamSessionCreateHandle`/
  `XamSessionRefObjByHandle` (11+1 sites réels, famille de wrappers
  télémétrie autour de `0x821fd3e8`-`0x821fd9xx`, objet résolu semble
  seulement journalisé, pas déréférencé — pas assez tracé pour un vrai
  fix); `NtDuplicateObject` (1 site réel, seulement 3 registres passés,
  aucun handle de sortie capturé — signature ambiguë).
- Tests 184/184 (183/183 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r197-real-fix-kelockl2-keunlockl2-kiapcnormalroutinenop-always-succeed-20260902.md`.

# AC6 retail NTSC-U/J — r196 : VRAI CORRECTIF — `ObCreateSymbolicLink`/`ObDeleteSymbolicLink` réussissent toujours (2026-09-02)

- Site réel `0x821ea034` : boucle réelle de réessai de montage de
  périphérique (lettre de lecteur). Le no-op offline (`kOfflineStatus`
  négatif) prenait TOUJOURS le chemin d'échec de cette boucle — un vrai
  blocage de séquence de boot, dans le territoire même de « atteindre le
  gameplay Mission01 » que Gate 2 vise encore.
- Corrigé : retourne `STATUS_SUCCESS` sans condition pour les deux
  imports. La résolution de chemin de ce projet
  (`guest_path_to_relative`) ne consulte jamais de table de liens
  symboliques enregistrée — même précédent que `VdRetrainEDRAM` (« le
  côté natif possède déjà le cycle de vie de ce sous-système »).
- Tests 183/183 (182/182 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r196-real-fix-obcreatesymboliclink-obdeletesymboliclink-always-succeed-20260902.md`.

# AC6 retail NTSC-U/J — r195 : VRAI CORRECTIF — `XamAlloc`/`XamFree` utilisent l'allocateur bump invité (2026-09-02)

- `DWORD XamAlloc(DWORD Type, SIZE_T Size, PVOID* pAddress)`/`DWORD
  XamFree(PVOID pAddress)` : statut Win32 (0=succès), pas un NTSTATUS.
  Site réel `0x821fd440` confirme `r3=Type,r4=Size,r5=&pAddress`;
  l'appelant traite le retour comme SIGNÉ et ne prend son chemin d'erreur
  que si négatif — `kOfflineStatus` (0xC00000BB) est négatif en 32 bits
  signé, donc les 3 sites d'appel réels `XamAlloc` de ce XEX échouaient
  systématiquement.
- Corrigé : réutilise `allocate_guest` (même mécanisme
  qu'`ExAllocatePool`/`MmAllocatePhysicalMemoryEx`), écrit l'adresse
  réelle via `pAddress`, retourne 0 ou `ERROR_OUTOFMEMORY` (0xE).
  `XamFree` : no-op retournant 0, même précédent qu'`ExFreePool`.
- Tests 182/182 (181/181 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r195-real-fix-xamalloc-xamfree-use-the-guest-bump-allocator-20260902.md`.

# AC6 retail NTSC-U/J — r194 : VRAI CORRECTIF — `KeDelayExecutionThread` attend réellement (2026-09-02)

- Site d'appel réel unique `0x821f74e8`, dans un wrapper qui convertit
  des millisecondes en un vrai `LARGE_INTEGER` relatif négatif (100 ns)
  avant l'appel. Ce wrapper normalise déjà tout statut en 0 ou 0xC0, et
  `kOfflineStatus` tombait par accident dans la branche 0 — le statut
  n'était donc jamais le vrai bug. Le vrai bug : le no-op offline
  retournait instantanément, transformant un vrai délai calculé en zéro
  temps écoulé.
- Corrigé : lit le vrai `Interval` 64 bits, si négatif (relatif — seule
  forme produite par ce site d'appel réel) convertit en durée
  `std::chrono` et `std::this_thread::sleep_for` réel, puis
  `STATUS_SUCCESS`. Un `Interval` positif (absolu) reste non géré plutôt
  que deviné.
- Un test préexistant utilisait `KeDelayExecutionThread` comme exemple du
  no-op générique; reciblé sur `NtCancelTimer` (toujours générique) plutôt
  que supprimé.
- Tests 181/181 (180/180 → +1, 1 reciblé). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r194-real-fix-kedelayexecutionthread-actually-sleeps-20260902.md`.

# AC6 retail NTSC-U/J — r193 : VRAI CORRECTIF — `KeBugCheck`/`KeBugCheckEx` s'arrêtent au lieu de continuer silencieusement (2026-09-02)

- `VOID KeBugCheck(ULONG)`/`VOID KeBugCheckEx(ULONG, ULONG_PTR×4)` sont des
  API NT documentées qui NE RETOURNENT JAMAIS (arrêt matériel réel). Le
  no-op offline générique retournait normalement avec `kOfflineStatus` —
  un vrai risque d'exécution après un point que le code compilé de ce XEX
  n'a jamais prévu de reprendre.
- Sites d'appel réels distingués précisément (une recherche par
  sous-chaîne naïve confond `KeBugCheck`/`KeBugCheckEx`) :
  `KeBugCheck` (`823d054c`) 7 références (6 appels + 1 saut de queue
  réel); `KeBugCheckEx` (`823d03ec`) 4 appels réels.
- Corrigé : trace un diagnostic (code/paramètres réels lus depuis
  r3-r7, rien inventé) puis `std::abort()` — respecte le contrat « ne
  retourne jamais » réel au lieu de fabriquer une continuation.
- Tests 180/180 (179/179 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r193-real-fix-kebugcheck-family-aborts-instead-of-silently-continuing-20260902.md`.

# AC6 retail NTSC-U/J — r192 : VRAI CORRECTIF — sémaphores/try-spinlock obtiennent une vraie exclusion mutuelle (2026-09-02)

- `KeTryToAcquireSpinLockAtRaisedIrql` (site réel `0x823a8bf4`, masqué en
  BOOLEAN via `rlwinm`) : variante non bloquante de `KfAcquireSpinLock`
  (r191), réutilise `spin_lock_for(key).try_lock()`.
- `KeInitializeSemaphore`/`KeReleaseSemaphore` (sites réels `0x823add7c`,
  `0x823ad268`, `0x823ad8d4`) : un vrai `KSEMAPHORE` (en-tête dispatcher
  auto-référencé) était initialisé mais jamais relâché — tout
  `KeWaitForSingleObject` sur ce sémaphore expirait toujours en TIMEOUT.
  Réutilise le modèle événement auto-reset de r145
  (`NtCreateSemaphore`/`NtReleaseSemaphore`), clé = adresse invitée de
  l'objet directement (motif `Ke*` déjà établi par
  `KeSetEvent`/`KeResetEvent`). Compte précédent non modélisé, retourné 0
  — même précédent que r145, mais via la valeur de retour (pas un
  pointeur de sortie) puisque c'est le vrai contrat `KeReleaseSemaphore`.
- Vérifié aussi, non corrigé : `NtQueryInformationFile` (site réel unique
  `0x823908a4`) fait partie d'une séquence de finalisation de fichier
  (lit la position d'écriture, puis `NtSetInformationFile` positionne
  EndOfFile/AllocationSize à cette position) — bloqué par le média invité
  en lecture seule, même famille que r178. `sprintf`/`_vsnprintf` (7+2
  sites réels) nécessiteraient un moteur printf varargs complet — hors
  scope d'un cycle borné, non tenté.
- Tests 179/179 (178/178 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r192-real-fix-semaphore-and-try-spinlock-primitives-get-real-mutual-exclusion-20260902.md`.

# AC6 retail NTSC-U/J — r191 : VRAI CORRECTIF — primitives spinlock/IRQL obtiennent une vraie exclusion mutuelle (2026-09-02)

- `KfAcquireSpinLock`/`KfReleaseSpinLock`, `KeAcquireSpinLockAtRaisedIrql`/
  `KeReleaseSpinLockFromRaisedIrql`, `KeRaiseIrqlToDpcLevel`/`KfLowerIrql`
  étaient tous encore le no-op offline générique. Signatures NT/Xbox 360
  documentées standard (connaissance externe protocolaire, même classe que
  `XINPUT_STATE`/`TIME_FIELDS` déjà acceptée par ce projet).
- Preuve réelle : site d'appel `0x821e5fd0` de ce XEX — `KfAcquireSpinLock`
  (`0x821e600c`) et `KfReleaseSpinLock` (`0x821e6060`) encadrent un vrai
  ajout de file (`0x821e6020`-`0x6054`). Sites d'appel réels : dizaines à
  88-110 selon la fonction — famille répandue sur la majorité de la plage
  d'adresses du moteur, jamais touchée avant. Même risque de concurrence
  réelle que r116 (r111-r115 : vrais threads hôtes concurrents via
  `ExCreateThread`), pour une primitive bien plus utilisée.
- `spin_lock_for(key)` : nouvel aide, calqué sur `critical_section_for` de
  r116, `std::mutex` NON récursif (un vrai spinlock n'est pas réentrant non
  plus — l'auto-réacquisition bloque aussi sur le vrai matériel).
  `g_dpc_level_mutex` (pour Raise/LowerIrql, sans objet de verrou associé)
  EST `std::recursive_mutex` : l'IRQL réel est un état par thread, pas une
  identité d'objet — un même thread relève légitimement l'IRQL en
  imbriqué; un mutex simple provoquerait un auto-blocage sur le premier
  Raise imbriqué.
- « Ancien IRQL » retourné par `KfAcquireSpinLock`/`KeRaiseIrqlToDpcLevel` :
  `PASSIVE_LEVEL` (0), non tracé jusqu'à un consommateur ce cycle — nommé
  honnêtement.
- Tests 178/178 (177/177 → +1). `ctest` 10/10 (aucun fichier `native/`
  touché, seul l'outil de génération de stubs — pas de re-run
  `prepare.py` nécessaire). Voir
  `reports/ac6-retail-native-codegen-gate2-r191-real-fix-spinlock-and-irql-primitives-get-real-mutual-exclusion-20260902.md`.

# AC6 retail NTSC-U/J — r190 : VRAI CORRECTIF — `NtQueryVolumeInformationFile` remplit un vrai FS_SIZE_INFORMATION (2026-09-02)

- Contrat réel : struct-fill via r5, pas un statut ignoré. Les 3 sites
  d'appel réels demandent tous `FileFsSizeInformation` (classe 3,
  0x18 octets); un site calcule les octets libres/totaux réels
  (SectorsPerAllocationUnit × BytesPerSector × AllocationUnits) et les
  reporte à son appelant — vrai contrôle d'espace disque.
- Seul `FileFsSizeInformation` est implémenté; toute autre classe demandée
  renvoie `STATUS_INVALID_INFO_CLASS` plutôt qu'un layout deviné sans site
  d'appel tracé.
- Valeurs : unité d'allocation 0x4000 (16 Kio, taille de cluster FATX
  Xbox 360 documentée par défaut pour grandes partitions — pas inventée);
  espace total/disponible = 8 Gio, défaut généreux ordinaire (le média
  invité est en lecture seule ici, aucune écriture réelle n'est câblée,
  donc « assez d'espace libre » évite un faux blocage sans affirmer une
  taille de partition console précise).
- Un 2e site compare l'unité d'allocation calculée à une valeur attendue
  fournie par l'appelant, non retracée ce cycle — nommé honnêtement, pas
  affirmé comme résolu.
- Tests 177/177 (176/176 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r190-real-fix-ntqueryvolumeinformationfile-fills-real-fs-size-info-20260902.md`.

# AC6 retail NTSC-U/J — r189 : VRAI CORRECTIF — `NtQueryFullAttributesFile` remplit le vrai struct (2026-09-02)

- Contrat réel : `NTSTATUS NtQueryFullAttributesFile(POBJECT_ATTRIBUTES,
  PFILE_NETWORK_OPEN_INFORMATION)` — réutilise la forme
  ObjectAttributes/ANSI_STRING déjà confirmée par r122/r123 pour
  `NtCreateFile`. Les 2 sites d'appel réels confirment le layout standard
  `FILE_NETWORK_OPEN_INFORMATION` (52 octets) en lisant `FileAttributes` à
  struct+0x30 — offset réel Microsoft exact.
- Corrigé selon la même discipline que `NtCreateFile` : un fichier réel
  absent du média lié est un échec normal et attendu
  (`STATUS_OBJECT_NAME_NOT_FOUND`), pas une erreur fabriquée. Ajoute
  `NativeGuestMediaService::file_size()` — un petit accesseur sur le
  service média EXISTANT, pas un nouveau sous-système (aucun import
  n'avait besoin de la taille réelle sans cycle complet open/read/close
  jusqu'ici). Timestamps laissés à 0 (aucun site ne les lit).
- Tests 176/176 (175/175 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r189-real-fix-ntqueryfullattributesfile-fills-the-real-struct-20260902.md`.

# AC6 retail NTSC-U/J — r188 : VRAIS CORRECTIFS — `RtlUnicodeStringToAnsiString`/`RtlFreeAnsiString` (2026-09-02)

- Compagnons allocation/libération réels, découverts ensemble à des
  adresses adjacentes (`0x823927f0`/`0x8239281c`) dans la même fonction.
- `RtlUnicodeStringToAnsiString` (contrat réel : `NTSTATUS
  RtlUnicodeStringToAnsiString(PANSI_STRING, PCUNICODE_STRING, BOOLEAN)`) :
  le seul site d'appel réel confirme `AllocateDestinationString=1` et le
  contrat NTSTATUS réel (même convention que r187); `RtlFreeAnsiString`
  est appelé en cas de succès, confirmant qu'une vraie allocation est
  attendue. Layout `ANSI_STRING` {Length@0,MaxLength@2,Buffer@4} déjà
  confirmé par r122/r123; `UNICODE_STRING` = même forme fixe Microsoft
  avec buffer WCHAR — connaissance de protocole externe, pas un offset
  deviné.
- Corrigé : alloue via `allocate_guest` (même aide que `ExAllocatePool`),
  convertit avec le même mapping que r187, termine par NUL, remplit
  Length/MaxLength/Buffer. `RtlFreeAnsiString` vide les champs plutôt que
  d'inventer une libération par allocation — même précédent que
  `ExFreePool` (pages jamais réclamées individuellement).
- Tests 175/175 (173/173 → +2). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r188-real-fixes-rtlunicodestringtoansistring-rtlfreeansistring-20260902.md`.

# AC6 retail NTSC-U/J — r187 : VRAI CORRECTIF — `RtlUnicodeToMultiByteN` convertit et réussit (2026-09-02)

- Contrat réel : `NTSTATUS RtlUnicodeToMultiByteN(PCHAR, ULONG, PULONG,
  PCWCH, ULONG)`. Le seul site d'appel réel (`0x821f4758`) confirme la
  forme des arguments ET le contrat NTSTATUS (`bge` = tout retour non
  négatif est un succès) — `kOfflineStatus` étant un NTSTATUS
  authentiquement négatif, ce site prenait TOUJOURS la branche d'échec
  vers `RtlNtStatusToDosError` (déjà implémenté, r126) au lieu de
  convertir quoi que ce soit.
- Corrigé : convertit chaque unité UTF-16 vers son octet bas pour les
  points de code ≤ 0xFF (mapping Latin-1 standard) et le caractère de
  remplacement conventionnel `?` (0x3F) au-delà — comportement NT
  standard, pas une valeur choisie pour forcer un résultat. Renvoie
  `STATUS_SUCCESS`.
- Tests 173/173 (172/172 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r187-real-fix-rtlunicodetomultibyten-converts-and-succeeds-20260902.md`.

# AC6 retail NTSC-U/J — r186 : VRAIS CORRECTIFS — `RtlFillMemoryUlong`/`RtlCompareMemoryUlong` (2026-09-02)

- Primitives RTL standard à algorithme fixe et connu (même catégorie que
  r184/r185 — aucune ambiguïté à résoudre). `RtlFillMemoryUlong` : le seul
  site d'appel réel (`0x821f3354`) remplit un buffer local de 0x320
  octets avec le motif `0x80000000` — le fallback générique n'écrivait
  rien, laissant la mémoire de pile garbage au lieu du motif attendu.
  `RtlCompareMemoryUlong` : compagnon réel, compare contre un motif
  répété et renvoie la longueur du préfixe correspondant.
- Corrigés selon l'algorithme fixe standard (remplir/comparer par blocs
  de 4 octets).
- Tests 172/172 (170/170 → +2). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r186-real-fixes-rtlfillmemoryulong-rtlcomparememoryulong-20260902.md`.

# AC6 retail NTSC-U/J — r185 : VRAIS CORRECTIFS — `RtlTimeToTimeFields`/`RtlTimeFieldsToTime` complètent r179 (2026-09-02)

- 2 de 4 sites d'appel réels de `KeQuerySystemTime` (r179) alimentent
  directement `RtlTimeToTimeFields` pour peupler un vrai struct
  calendaire — le fix r179 restait INCOMPLET seul : le FILETIME calculé
  était transmis à un no-op qui n'écrivait jamais `TimeFields`.
- Layout `TIME_FIELDS` standard Win32 confirmé octet pour octet aux 2
  sites d'appel réels : +0x0/0x2/0x4/0x6/0x8/0xa/0xc/0xe = Année/Mois/
  Jour/Heure/Minute/Seconde/Milliseconde/JourSemaine.
  `RtlTimeFieldsToTime` (inverse réelle) : même layout confirmé à son site
  d'appel réel, plus le contrat de retour BOOLEAN (octet bas de r3).
- Corrigés via les facilités calendaires C++20 `<chrono>`
  (`year_month_day`, `weekday`, `hh_mm_ss`) — algorithme grégorien réel et
  standard, pas une réimplémentation manuelle; vérifié par compilation
  autonome avant intégration.
- Tests 170/170 (168/168 → +2). `ctest` 10/10 (confirme la compilation du
  nouveau code `<chrono>`). Voir
  `reports/ac6-retail-native-codegen-gate2-r185-real-fix-rtltimetotimefields-and-inverse-complete-r179-20260902.md`.

# AC6 retail NTSC-U/J — r184 : VRAI CORRECTIF — `XeCryptSha` calcule un vrai condensé SHA-1 (2026-09-02)

- Contrat réel : `VOID XeCryptSha(pbInput1, cbInput1, pbInput2, cbInput2,
  pbInput3, cbInput3, pbDigest, cbDigestSize)` — confirmé par le site
  d'appel réel `0x82390f04` : les 8 registres d'arguments entiers sont
  tous peuplés, `r10` (cbDigestSize) = `0x14` (longueur SHA-1 exacte).
- Le condensé calculé alimente immédiatement une comparaison
  (`0x823d0abc`) qui contrôle un vrai branchement — un condensé
  absent/garbage échouerait toute comparaison réelle en aval.
- Corrigé : calcule le VRAI SHA-1 (interface EVP d'OpenSSL, même patron
  que `native_xex.cpp` pour l'AES-CBC, `OpenSSL::Crypto` déjà lié) sur les
  octets invités réels des tampons fournis — le seul cas de ce balayage
  où « la vraie valeur » n'a aucune ambiguïté à résoudre : l'algorithme la
  calcule exactement.
- Tests 168/168 (167/167 → +1). `ctest` 10/10 (confirme la compilation/
  liaison du nouveau code EVP). Voir
  `reports/ac6-retail-native-codegen-gate2-r184-real-fix-xecryptsha-computes-a-real-digest-20260902.md`.

# AC6 retail NTSC-U/J — r183 : VRAI CORRECTIF — `RtlImageXexHeaderField` signale « absent » (2026-09-02)

- Contrat réel : `PVOID RtlImageXexHeaderField(PVOID, DWORD)` — CONTRAIREMENT
  à presque tous les autres imports corrigés jusqu'ici, la valeur de retour
  ELLE-MÊME est le pointeur de champ (0 = absent), pas un code de statut.
- Les 2 sites d'appel réels DÉRÉFÉRENCENT la valeur de retour quand elle
  est non nulle : `0x821f7d88` fait `lwz r30,0x0(r3)` directement;
  `0x82390e40` stocke la valeur brute comme donnée de champ pour son
  propre appelant. `kOfflineStatus` étant non nul, les deux sites
  traitaient un champ non implémenté/absent comme « trouvé » — risque réel
  de crash (déréférencement de `0xC00000BB` comme pointeur invité), pas un
  trou cosmétique.
- Corrigé : `0u` (absent) sans condition — aucune preuve locale que ces
  champs d'en-tête optionnels (`0x20401`, `0x40006`) sont réellement
  présents dans l'en-tête de ce XEX (aucun analyseur de table de champs
  d'en-tête n'existe encore sous `native/`); les deux sites ont un chemin
  de traitement par défaut bien défini pour le cas « absent ».
- Tests 167/167 (166/166 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r183-real-fix-rtlimagexexheaderfield-reports-not-present-20260902.md`.

# AC6 retail NTSC-U/J — r182 : VRAI CORRECTIF — `XamUserCheckPrivilege` accorde et réussit (2026-09-02)

- `0x823cfe8c`, que le rapport r176 avait seulement nommé « une fonction
  différente (chemin de repli) » sans l'identifier, s'avère être le thunk
  de `XamUserCheckPrivilege` (confirmé par correspondance d'adresse).
- Contrat réel : `DWORD XamUserCheckPrivilege(DWORD, DWORD, LPBOOL)` —
  remplissage de struct (le bool) PLUS un vrai statut de retour, pas un
  statut ignoré. `0x82206bcc`/`0x82206bf0` testent le retour contre 0
  (`ERROR_SUCCESS`) ET relisent le bool de sortie (`==1`) — échec de
  l'appel et privilège ACCORDÉ mènent à la MÊME branche; seul « appel
  réussi ET privilège refusé » diverge. `0x821f44ac` (le helper de
  résolution de connexion de r176) renvoie ce résultat brut comme SA
  PROPRE valeur de retour sans autre vérification.
- Corrigé : `ERROR_SUCCESS` (0), bool de sortie = `TRUE` (accordé) —
  cohérent avec l'hypothèse de profil hors-ligne unique et non restreint
  déjà établie (r176), pas une valeur choisie pour forcer le seul site où
  le résultat diverge réellement.
- Tests 166/166 (165/165 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r182-real-fix-xamusercheckprivilege-grants-and-succeeds-20260902.md`.

# AC6 retail NTSC-U/J — r181 : VRAI CORRECTIF — `XamInputGetKeystrokeEx` renvoie `ERROR_EMPTY` (2026-09-02)

- Dernier import restant de la famille `XamInput*` (r180 avait couvert
  `GetState`/`SetState`/`GetCapabilities`). Contrat réel : `DWORD
  XamInputGetKeystrokeEx(DWORD, DWORD, PXINPUT_KEYSTROKE)`, forme
  différente — la réponse normale en régime établi est `ERROR_EMPTY`
  (`0x4306`), pas un succès avec sortie peuplée.
- Le seul site d'appel réel (`0x82390de0`) n'inspecte pas la valeur de
  retour lui-même; `kOfflineStatus` reste un statut NT absurde pour cette
  API en forme d'erreur Win32 quoi qu'il arrive.
- Corrigé : `0x4306` (`ERROR_EMPTY`) sans condition — réponse réelle et
  valide ne nécessitant aucune écriture de sortie. Aucune file d'événements
  keystroke (appui/relâchement) n'existe encore dans ce projet
  (`NativeGuestInputService`, r180, n'expose que l'état courant sondé, pas
  une file discrète) — nommé comme trou plutôt que fabriquer des
  événements.
- Tests 165/165 (164/164 → +1). `ctest` 10/10. Voir
  `reports/ac6-retail-native-codegen-gate2-r181-real-fix-xaminputgetkeystrokeex-returns-error-empty-20260902.md`.

# AC6 retail NTSC-U/J — r180 : VRAI CORRECTIF — backend d'entrée manette natif SDL2 (2026-09-02)

- Utilisateur a explicitement autorisé SDL2 pour ce backend. `XamInputGetState`/
  `XamInputSetState`/`XamInputGetCapabilities` étaient entièrement non
  implémentés — candidat le plus prometteur pour « contrôles nuls ».
- Nouveau service `NativeGuestInputService`
  (`native/include/ac6/native_guest_input.h`/`native/src/native_guest_input.cpp`),
  même patron singleton que `native_guest_vd_service()`/`native_guest_media_service()`,
  enveloppant l'API `SDL_GameController`.
- Preuve réelle du contrat de transfert : le site d'appel réel
  `0x8234cedc` confirme la forme `(dwUserIndex, &XINPUT_STATE)` et le code
  d'erreur réel `0x48F` (`ERROR_DEVICE_NOT_CONNECTED`); `0x82390d48`
  confirme struct+0x1 (SubType)/+0x2 (Flags) octet pour octet contre le
  layout `XINPUT_CAPABILITIES` publié par Microsoft (ABI fixe multi-
  plateforme, même catégorie que `XC_LANGUAGE_ENGLISH` r174, pas un offset
  deviné propre à ce XEX).
- **Découverte environnementale** : modifier `native/` exige de relancer
  `tools/prepare.py --profile native` (qui resynchronise le snapshot
  `native-source/`) — `build.py` seul ne le fait pas. Toutes les corrections
  r148-r179 n'avaient touché que `tools/materialize_native_import_stubs.py`
  (régénéré à chaque build), jamais `native/` lui-même, d'où la découverte
  tardive.
- Bug réel d'ordre de liaison statique découvert et corrigé
  (`target_link_libraries(ac6_native_guest PRIVATE ac6_native_xenos)`) —
  rien dans `ac6_native_xenos` ne référençait le nouveau service (contrairement
  à Vd/média, atteints via `NativeRuntime`), donc `ld` n'extrayait jamais
  l'objet avant que `ac6_native_guest` (qui l'appelle) ne soit traité.
  `build.py`/`validate.py` avaient aussi des comptages 9/8 binaires codés
  en dur — mis à jour à 10/9.
- Tests 164/164 (163/163 → +3). `ctest` 10/10 (9/9 → +1, nouveau
  `ac6_native_guest_input_tests`). `tools/validate.py --runtime native`
  passe de bout en bout. Voir
  `reports/ac6-retail-native-codegen-gate2-r180-real-fix-native-sdl2-controller-input-backend-20260902.md`.
- Aucune manette physique dans ce bac à sable — le symptôme « contrôles
  nuls » lui-même reste à confirmer par une observation runtime future
  avec un vrai périphérique, non entreprise ce cycle.

# AC6 retail NTSC-U/J — r179 : VRAI CORRECTIF — `KeQuerySystemTime` remplit un FILETIME réel et changeant (2026-09-02)

- `KeQuerySystemTime` (contrat réel : `VOID KeQuerySystemTime(PLARGE_INTEGER)`
  — remplissage de struct via pointeur, même catégorie que `VdQueryVideoMode`
  r168/r169, pas un statut). Le fallback générique n'écrivait rien à
  travers le pointeur. Les 4 sites d'appel réels de ce XEX exigent tous une
  valeur RÉELLE et CHANGEANTE : 2 alimentent `RtlTimeToTimeFields` (date
  calendaire réelle, une constante fixe donnerait une date absurde); 1
  calcule un delta de temps écoulé entre deux horodatages (une constante
  figerait ce delta à zéro pour toujours — un minuteur/animation en attente
  ne progresserait jamais); 1 prend les 32 bits bas comme graine de session
  (une constante la rendrait identique à chaque run).
- Corrigé : utilise l'horloge murale réelle de l'hôte
  (`std::chrono::system_clock::now()`), convertie vers l'époque FILETIME
  Windows (offset standard `116444736000000000`, non inventé), écrite via
  `PPC_STORE_U64`. Compile proprement contre le vrai toolchain
  (`<ratio>` ajouté aux includes).
- Tests 161/161 (160/160 → +1). `ctest` 9/9. Voir
  `reports/ac6-retail-native-codegen-gate2-r179-real-fix-kequerysystemtime-fills-a-real-changing-filetime-20260902.md`.

# AC6 retail NTSC-U/J — r178 : `XexCheckExecutablePrivilege` vérifié sans fix sûr; backend d'entrée natif nécessite une décision de cadrage (2026-09-02)

- `XexCheckExecutablePrivilege` (3 sites d'appel réels, IDs de privilège
  DIFFÉRENTS 0xa et 0x17) : `kOfflineStatus` étant non-nul, tous les sites
  évaluent actuellement « privilège accordé » — comportement coïncidemment
  non cassé, même catégorie que r164 pour `RtlTryEnterCriticalSection`.
  Aucune preuve locale ne fixe la sémantique réelle de ces IDs; deviner
  « refusé » risquerait d'INTRODUIRE un nouveau chemin de repli qui
  n'existe pas actuellement. Nommé, non corrigé, suivant le précédent r164.
- Backend d'entrée manette natif (`XamInputGetState`/`SetState`/
  `GetCapabilities`) : candidat le plus prometteur pour « contrôles nuls »,
  mais `native/CMakeLists.txt` ne lie AUCUNE bibliothèque d'entrée hôte —
  implémenter une vraie E/S manette nécessite un nouveau choix de
  dépendance, du travail Ghidra supplémentaire (layout de struct réel) et
  de nouveaux fichiers source — pas un simple fix de stub généré. Décision
  de cadrage explicite nécessaire avant toute implémentation; non
  entrepris ce cycle.
- Aucun code natif modifié, aucun build touché ce cycle. Voir
  `reports/ac6-retail-native-codegen-gate2-r178-xexcheckexecutableprivilege-checked-no-safe-fix-identified-input-backend-needs-scoping-20260902.md`.

# AC6 retail NTSC-U/J — r177 : VRAI CORRECTIF — `XamGetSystemVersion` reste sous tous les seuils observés (2026-09-02)

- `XamGetSystemVersion` (contrat réel : `DWORD XamGetSystemVersion(VOID)`,
  numéro de build, pas un statut). 5 sites d'appel réels le comparent à un
  seuil. 4 d'entre eux dégradent proprement quelle que soit la valeur
  (sondage de fonctionnalité optionnelle déjà en échec via
  `XexGetModuleHandle`/`XexGetProcedureAddress`, ou choix cache/no-cache
  vers la MÊME fonction). Le 5e (`0x821f4440`, le helper de résolution de
  connexion que r176 vient de corriger) SAUTE ENTIÈREMENT la boucle de
  connexion si `>= 0x20096b00` — une valeur trop haute annulerait
  silencieusement le correctif r176.
- Corrigé : `0x20000000`, sous tous les seuils observés (le plus bas est
  `0x20096b00`) — nécessaire pour que r176 s'applique réellement à ce site,
  et confirmé sans risque aux 4 autres sites.
- Tests 160/160 (159/159 → +1). `ctest` 9/9. Voir
  `reports/ac6-retail-native-codegen-gate2-r177-real-fix-xamgetsystemversion-stays-below-every-observed-threshold-20260902.md`.

# AC6 retail NTSC-U/J — r176 : VRAI CORRECTIF — `XamUserGetSigninState` : index 0 signalé connecté localement (2026-09-02)

- Balayage frais des 151 imports restants sur le fallback générique
  (méthode r90/r93/r164), motivé par le symptôme historique « XPSO-164
  atteint gameplay, contrôles nuls » (STATE.md).
- `XamUserGetSigninState` (contrat réel : `DWORD
  XamUserGetSigninState(DWORD dwUserIndex)`, énumération réelle 0/1/2, pas
  un statut). `0x821f4428` (résolution du joueur connecté) boucle les
  index 0..3 et teste l'égalité EXACTE à 1 pour trouver l'utilisateur actif
  — sinon tombe dans un chemin totalement différent (invite de connexion).
  `kOfflineStatus` n'égale jamais 1 : le jeu tombait TOUJOURS dans ce
  chemin de repli — bug réel, pas cosmétique, contribuant plausiblement à
  un flux qui n'atteint jamais le gameplay faute d'utilisateur « connecté ».
  `0x82206954` teste le résultat contre 0 pour sauter une mise à jour par
  joueur — cohérent avec le même énumérateur.
- Corrigé : index 0 → 1 (connecté localement, cohérent avec la convention
  hors-ligne établie par ce projet); tout autre index → 0.
- Tests 159/159 (158/158 → +1). `ctest` 9/9. Voir
  `reports/ac6-retail-native-codegen-gate2-r176-real-fix-xamusergetsigninstate-reports-index-zero-signed-in-locally-20260902.md`.
- **Prochains candidats identifiés, non implémentés** : `XamGetSystemVersion`
  (contrôle aussi ce même flux `0x821f4428` via un seuil de version, encore
  sur le fallback générique); `XamInputGetState`/`XamInputSetState`/
  `XamInputGetCapabilities` (E/S manette réelle — nécessite un backend
  d'entrée natif qui n'existe pas encore sous `native/`, tâche plus large
  qu'un simple fix de forme de contrat).

# AC6 retail NTSC-U/J — r175 : VRAI CORRECTIF — `VdGetCurrentDisplayInformation` struct+0x05 tracé jusqu'à un choix d'algorithme de scaling réel (2026-09-02)

- r170 avait confirmé struct+0x05 comme champ booléen réel mais différé son
  implémentation (le comparateur immédiat ne fixait pas de valeur). Ce
  cycle trace plus loin, dans les DEUX fonctions que la comparaison
  sélectionne réellement.
- `0x821ea4d8` : `valeur==1` sélectionne `0x821eb778`, un vrai scaler par
  INTERPOLATION LINÉAIRE (combine deux lookups voisins par calcul de
  différence avant mise à l'échelle); `valeur!=1` sélectionne `0x821eb6e0`,
  un scaler PLUS PROCHE VOISIN (lookup direct, sans combinaison). `0x821ea2a4` :
  `valeur!=1` positionne un bit dans un octet de flags persisté que
  `valeur==1` laisse à zéro — même direction aux deux sites (1 = cas
  par défaut/non marqué).
- Corrigé : struct+0x05 = 1 (u8). Le scaler interpolé de meilleure qualité
  est le choix physiquement sensé pour la cible HD/widescreen déjà établie
  par ce projet (1280x720, r169); aucune preuve aux deux sites ne pointe
  dans l'autre sens.
- Ferme tout trou actuellement nommé dans la famille de configuration
  plateforme Vd/X ouverte par r168.
- Tests 158/158 (assertion ajoutée, pas de nouveau test). `ctest` 9/9. Voir
  `reports/ac6-retail-native-codegen-gate2-r175-real-fix-vdgetcurrentdisplayinformation-struct-plus-0x05-traced-to-a-scaler-choice-20260902.md`.

# AC6 retail NTSC-U/J — r174 : VRAI CORRECTIF — `XGetLanguage` renvoie l'anglais (2026-09-02)

- `XGetLanguage` (contrat réel : `DWORD XGetLanguage(VOID)`). Le seul site
  d'appel réel (`0x821f5d9c`) conserve la valeur (`r31`), la borne-vérifie
  contre `10`, et l'utilise pour indexer une table de correspondance par
  langue — contrairement à `XGetAVPack`, la valeur exacte compte ici.
  Corrigé : `1`, la constante XDK Xbox 360 standardisée
  `XC_LANGUAGE_ENGLISH` — une valeur de protocole fixe (pas spécifique à
  la compilation de ce XEX, contrairement à un offset de struct), sûre
  vis-à-vis du contrôle de borne et cohérente avec la cible NTSC-U/J.
- Ferme la famille des imports de configuration plateforme ouverte par
  r171 (`XGetVideoMode`/`XGetGameRegion`/`XGetAVPack`/`XGetLanguage`) —
  aucun autre import de cette famille n'est actuellement identifié comme
  non vérifié.
- Tests 158/158 (157/157 → +1). `ctest` 9/9. Voir
  `reports/ac6-retail-native-codegen-gate2-r174-real-fix-xgetlanguage-returns-english-20260902.md`.

# AC6 retail NTSC-U/J — r173 : VRAI CORRECTIF — `XGetAVPack` évite les 4 valeurs qui sautent la configuration (2026-09-02)

- `XGetAVPack` (contrat réel : `DWORD XGetAVPack(VOID)`). Le seul site
  d'appel réel (`0x821f5d14`) compare le retour à exactement 4 valeurs
  (`0x3`/`0x6`/`0x8`/`0x4`) qui mènent TOUTES à la même cible (« sauter la
  configuration »); la valeur n'est ni stockée ni relue ensuite. Toute
  valeur hors de cet ensemble est donc comportementalement identique ici —
  contrairement à `XGetGameRegion` (r172), aucune preuve ne distingue les
  valeurs restantes. Corrigé : `0u`, la plus simple hors de l'ensemble,
  non asserté comme correspondant à une sémantique AV-pack précise.
- Tests 157/157 (156/156 → +1). `ctest` 9/9. Voir
  `reports/ac6-retail-native-codegen-gate2-r173-real-fix-xgetavpack-avoids-the-four-skip-setup-values-20260902.md`.

# AC6 retail NTSC-U/J — r172 : VRAI CORRECTIF — `XGetGameRegion` renvoie le code de région privilégié à correspondance exacte (2026-09-02)

- `XGetGameRegion` (contrat réel : `DWORD XGetGameRegion(VOID)`) est lu par
  ses 3 vrais sites d'appel; contrairement au 1er (`0x821babdc`, où les 4
  constantes `0x1ff`/`0x101`/`0x102`/`0x1fc` mènent à la MÊME branche), les
  2 autres montrent que la valeur EXACTE compte :
  - `0x821f4a68` : extrait byte1 (bits 8-15). `0x101`/`0x102` partagent
    byte1=0x01 et prennent la même branche, mais `r3==0x101` EXACT donne
    le code caché 20, tout autre byte1=0x01 (dont `0x102`) donne 21.
  - `0x821f4b0c` : extrait byte2 (bits 16-23). Les 4 constantes partagent
    byte2=0x01 et prennent la même branche, mais `r3==0x101` EXACT donne
    catégorie 2, tout autre (dont `0x102`/`0x1ff`/`0x1fc`) donne
    catégorie 7 (repli/générique).
- `0x101` n'est donc pas un choix arbitraire entre codes également
  plausibles pour une cible « -us »/« ntsc-uj » (la convention de nommage
  seule serait une base faible) : c'est la SEULE valeur que le contrôle de
  flux propre de ce XEX traite comme cas privilégié à correspondance
  exacte dans DEUX fonctions consommatrices indépendantes — aucun site
  n'inverse ce traitement en faveur de `0x102`.
- Tests 156/156 (155/155 → +1). `ctest` 9/9. Voir
  `reports/ac6-retail-native-codegen-gate2-r172-real-fix-xgetgameregion-returns-the-privileged-exact-match-region-code-20260902.md`.

# AC6 retail NTSC-U/J — r171 : VRAI CORRECTIF — `XGetVideoMode` remplit le champ refresh-rate utilisé comme diviseur (2026-09-02)

- `XGetVideoMode` (contrat réel : `VOID XGetVideoMode(XVIDEO_MODE*)`) est le
  wrapper XAM réel de `VdQueryVideoMode` sur le matériel réel — même type de
  struct. Le seul site d'appel réel de ce XEX (`0x82339798`) lit
  struct+0x14 comme float et l'utilise comme DIVISEUR réel
  (`fdivs f1,f31,f0`) — offset identique à celui confirmé par r169,
  corroboration indépendante depuis un second site d'appel réel.
- Non implémenté auparavant, ce diviseur était la mémoire de pile non
  initialisée de ce XEX — risque réel de division par une valeur proche de
  zéro produisant Inf/NaN propagé dans le calcul flottant du guest. Corrigé :
  struct+0x14 = 60.0f (même défaut NTSC que r169). Seul cet offset est
  implémenté (aucune preuve de lecture pour +0x00/+0x04/+0x08 à ce site).
- Tests 155/155 (154/154 → +1). `ctest` 9/9. Voir
  `reports/ac6-retail-native-codegen-gate2-r171-real-fix-xgetvideomode-fills-the-refresh-rate-field-used-as-a-division-divisor-20260902.md`.
- **Prochain candidat identifié, non implémenté** : `XGetGameRegion` (3
  sites d'appel réels, ex. `0x821babdc`) — la valeur de retour est stockée
  puis relue et comparée à plusieurs constantes précises (`0x1ff`, `0x101`,
  `0x102`, `0x1fc`) qui contrôlent un vrai branchement — bug de région
  plausible, mais la valeur correcte nécessite d'analyser les 3 sites.

# AC6 retail NTSC-U/J — r170 : VRAIS CORRECTIFS — `VdQueryVideoFlags`, `VdGetCurrentDisplayGamma`, `VdGetCurrentDisplayInformation` (2026-09-02)

- **`VdQueryVideoFlags` n'est PAS un remplissage de struct** contrairement à
  l'hypothèse de r168/r169 : contrat réel = valeur de retour bitmask
  (`DWORD VdQueryVideoFlags(VOID)`). Le seul site d'appel réel
  (`0x821f31cc`) ne teste que le bit 0 du retour. Le fallback générique
  renvoyait `kOfflineStatus` (0xC00000BB, bit 0 = 1 par coïncidence — impair)
  ce qui forçait la même branche sans rapport avec le vrai comportement
  matériel. Corrigé : `0u` (aucun flag), valeur neutre non choisie pour
  forcer un résultat.
- **`VdGetCurrentDisplayGamma`** : contrat réel = 2 sorties par pointeur
  (`DWORD* type, FLOAT* value`), confirmé au site d'appel `0x821eb454`. Le
  cache en aval démarre non initialisé donc la reconstruction de table a
  lieu au premier appel quelle que soit la valeur fournie — aucun risque de
  crash. Valeurs = defaults de production ordinaires (`type=0`,
  `gamma=2.2`), non lues des octets de ce XEX, non choisies pour forcer un
  résultat.
- **`VdGetCurrentDisplayInformation`** : remplissage de struct confirmé aux
  TROIS sites d'appel réels. `0x821f0764` valide r169 de façon croisée :
  struct+0x48/+0x4a/+0x56 (u16) sont transmis EXACTEMENT vers les mêmes
  champs de sortie (0x5414/0x5418/0x541c) que `VdQueryVideoMode` remplit —
  et ici largeur (+0x48) et largeur-réelle (+0x56) viennent de DEUX offsets
  DIFFÉRENTS, confirmant que ce sont des champs réels distincts (pas
  toujours identiques comme r169 l'a vu par coïncidence). Les deux autres
  sites (`0x821ea4d8`, `0x821ea2a4`) confirment un champ booléen réel à
  struct+0x05, mais sa cible de comparaison (`!= 1`) alimente une logique
  aval non tracée cette fois — nommé, non implémenté (précédent r168).
  Implémenté : +0x48/+0x56=1280, +0x4a=720 (réutilise l'hypothèse de
  résolution déjà établie par r169).
- Tests 154/154 (151/151 → +3). `ctest` 9/9. Voir
  `reports/ac6-retail-native-codegen-gate2-r170-real-fixes-vdqueryvideoflags-vdgetcurrentdisplaygamma-vdgetcurrentdisplayinformation-20260902.md`.

# AC6 retail NTSC-U/J — r169 : VRAI CORRECTIF — `VdQueryVideoMode` : remplissage de struct, offsets dérivés de la désassemblation de CE XEX (2026-09-01)

- **4 offsets CONFIRMÉS par preuve directe** (pas assumés d'une source
  externe) : `+0x00`/`+0x04` (u32, motif de duplication largeur/
  "largeur réelle", 2 sites d'appel) ; `+0x08` (u32 booléen, confirmé
  via l'idiome PPC `cntlzw`/shift/`xori` de normalisation non-zéro→1,
  SITE D'APPEL DIFFÉRENT — croise l'évidence) ; `+0x14` (float, utilisé
  dans un vrai calcul arithmétique de taux de rafraîchissement).
  `+0x0C`/`+0x10` non lus par aucun site tracé — laissés non implémentés.
- **Valeurs choisies** : largeur/hauteur = 1280×720 (PAS inventé —
  hypothèse de résolution DÉJÀ établie par ce projet dans ses fixtures
  PM4/swap) ; interlaced=0 (progressif, correct pour 720p) ;
  refresh_rate=60.0f (NTSC standard, cible NTSC-U/J unique de ce
  projet) — les 2 seules valeurs non lues directement, defaults de
  production ordinaires, pas choisies pour forcer un résultat
  spécifique.
- Tests 151/151 (150/150 → +1). Import trace confirme non-géré → géré.
  `ctest` 9/9. Crash `sub_821F7C80` inchangé (appel en amont, sans
  rapport). Voir
  `reports/ac6-retail-native-codegen-gate2-r169-real-fix-vdqueryvideomode-struct-fill-derived-from-this-xexs-own-disassembly-20260901.md`.

# AC6 retail NTSC-U/J — r168 : `VdQueryVideoMode` est un VRAI trou (remplissage de struct), pas un fix de forme comme r162-r167 — différé (2026-09-01)

- **Différent des fixes précédents** : contrat réel = remplissage de
  struct via pointeur (`r3 = &struct`), pas juste une valeur de retour.
  Les 2 sites d'appel réels lisent PLUSIEURS champs de la struct après
  l'appel et font de VRAIS calculs dessus (pas une coïncidence comme
  RtlTryEnterCriticalSection).
- **Source publique consultée** (`has207/xenia-edge`,
  `xboxkrnl_video.cc`) confirme le vrai contrat XDK
  (`X_VIDEO_MODE` : display_width/height, refresh_rate, is_interlaced,
  etc.) mais son layout exact d'octets N'A PAS été localisé, et de
  toute façon une réimplémentation indépendante n'est PAS une source
  autorisée pour les offsets réels de CE binaire — répéterait
  exactement le pattern d'hypothèse non vérifiée que la discipline du
  projet refuse.
- **DÉCISION** : différé, pas bâclé. Nommé comme trou réel, plus
  important en portée que la famille r148-r167, pour un futur cycle
  dédié : dériver le layout réel depuis la désassemblation de CE XEX.
- **Aucun code modifié, aucun build ce cycle**. Voir
  `reports/ac6-retail-native-codegen-gate2-r168-vdqueryvideomode-is-a-real-gap-struct-fill-not-a-contract-shape-fix-deferred-20260901.md`.

# AC6 retail NTSC-U/J — r167 : VRAI CORRECTIF — `NetDll_XNetStartup`/`NetDll_WSAStartup` signalaient un échec sur une frontière hors-ligne (2026-09-01)

- **Wrappers transparents identifiés** (`sub_821FCCE0`/`sub_821FCED0`,
  atteints indirectement, pas via `bl` direct — même motif que r156)
  renvoient la valeur de l'import SANS MODIFICATION comme leur propre
  résultat (`bl ... / addi r1,r1,0x70 / blr` — r3 jamais touché).
- **Vrai contrat WinSock/XNet** : `INT`, 0 = succès. `kOfflineStatus`
  (non-nul) = échec sous la convention standard `== 0` — faux pour une
  frontière hors-ligne sans vraie condition d'échec, alors que le
  projet a déjà établi la convention "offline-only... succeeds past
  absent network" pour d'autres stubs similaires.
- **Corrigé** : renvoie `0u` (succès) au lieu de `kOfflineStatus`.
- Revalidation persistée le 2026-09-02 : 150 tests passés, 1 skip explicite,
  puis self-test, 9 binaires natifs et audits installation/symboles/capsule
  verts. Preuves sous
  `artifacts/retail-us-native-r167-evidence-refresh-20260902/`.
- Les anciennes affirmations de trace import live et de reproduction GDB de
  `sub_821F7C80` n'avaient aucun artefact persistant : elles sont déclassées
  en observations documentaires et ne participent plus au gate qualifié.
- Voir
  `reports/ac6-retail-native-codegen-gate2-r167-real-fix-netdll-xnetstartup-wsastartup-reported-failure-on-an-offline-boundary-20260901.md`.

# AC6 retail NTSC-U/J — r166 : l'allocateur de pile de threads confirme la dépendance à l'historique du MÊME thread — aucun autre fix borné n'existe (2026-09-01)

- **Mécanisme confirmé** : `ExCreateThread` alloue chaque nouveau
  thread invité depuis un compteur global `g_next_thread_stack`
  décroissant, tranches de 64 Kio JAMAIS réutilisées entre threads.
  Combiné à `mmap(MAP_ANONYMOUS)` (zéro garanti au premier touché),
  le garbage à `[r1+88]` vient forcément de l'historique d'appel du
  MÊME thread (pas de contamination inter-thread, pas un échec de
  zéro-initialisation).
- **Pourquoi aucun fix borné n'existe** : zéro-remplir explicitement
  les tranches fraîches ne changerait rien (déjà garanti zéro au
  premier touché) — la collision se produit APRÈS la création du
  thread, via son propre graphe d'appel. Rendre cette valeur sûre
  exigerait soit (a) un investissement général de fidélité HLE ouvert
  (pas cette portée), soit (b) coder en dur une valeur — le pattern
  synthétique refusé (précédent r53).
- **DÉCISION** : ferme le volet mécanisme de la question laissée
  ouverte par r161. L'arc d'investigation DATA.TBL est maintenant
  complet aux deux niveaux (valeur : r161 ; mécanisme : r166). Aucun
  travail supplémentaire mis en file sur ce fil précis.
- **Aucun code modifié, aucun build ce cycle**. Voir
  `reports/ac6-retail-native-codegen-gate2-r166-thread-stack-allocator-confirms-same-thread-history-dependence-no-further-bounded-fix-exists-20260901.md`.

# AC6 retail NTSC-U/J — r165 : correctifs cosmétiques de forme — `RtlTryEnterCriticalSection`/`KeEnterCriticalRegion`/`KeLeaveCriticalRegion` (2026-09-01)

- **Implémente les 3 candidats identifiés par r164** (même principe
  défensif que r162, même sans effet observé) : `KeEnterCriticalRegion`/
  `KeLeaveCriticalRegion` (VOID réel) renvoient `0u` ; `RtlTryEnterCriticalSection`
  (BOOLEAN réel) renvoie `1u` (canonique) au lieu de `kOfflineStatus`.
- **Aucun changement de flux de contrôle observé** (confirmé r164 par
  avance) — correctif de forme pur.
- Tests 148/148 (145/145 → +3). Import trace confirme les 3 imports
  ne sont plus "offline-import" non gérés. `ctest` 9/9. Crash
  `sub_821F7C80` inchangé (gdb).
- **DÉCISION** : ferme le balayage forme-de-contrat NTSTATUS-vs-réel
  lancé par r148 — chaque stub offline-import jamais surfacé par la
  trace live a maintenant été vérifié contre son vrai contrat noyau
  Xbox 360, et chaque écart réel corrigé (r148, r162, r163, r165).
  Voir
  `reports/ac6-retail-native-codegen-gate2-r165-cosmetic-contract-shape-fixes-rtltryentercriticalsection-keenter-leave-criticalregion-20260901.md`.

# AC6 retail NTSC-U/J — r164 : scan des stubs offline-import restants — AUCUN autre bug de forme trouvé (2026-09-01)

- **Scan complet** des imports offline restants (fréquence ≤2/run) :
  `RtlTryEnterCriticalSection` (5 sites réels) coche déjà juste par
  chance (`kOfflineStatus` non-nul évalue "verrou acquis" — même
  résultat qu'un vrai `1`) ; `KeEnterCriticalRegion`/
  `KeLeaveCriticalRegion` (VOID réel) ont leur retour genuinement jeté
  (écrasé par l'instruction suivante).
- **DÉCISION** : aucun autre fix à haute valeur à ce palier de
  fréquence. Contrairement à r163, aucun des deux ne change le flux de
  contrôle actuellement — corriger la forme serait cosmétique
  (principe r162), pas entrepris ce cycle sans raison spécifique.
  Ferme le fil de scan ouvert par r163 avec un résultat négatif
  documenté.
- **Aucun code modifié, aucun build ce cycle**. Voir
  `reports/ac6-retail-native-codegen-gate2-r164-remaining-low-frequency-offline-imports-checked-no-further-shape-bugs-found-20260901.md`.

# AC6 retail NTSC-U/J — r163 : VRAI CORRECTIF — `KeQueryBasePriorityThread` : valeur de retour RÉELLEMENT clampée et utilisée par un appelant (2026-09-01)

- **Différent de ses frères r162** : `ObDereferenceObject`/
  `KeSetBasePriorityThread` avaient leur retour toujours jeté ; SON
  unique site d'appel réel (`sub_821F3EA0`, seul xref vers
  `0x823D010C`) clampe RÉELLEMENT le retour à [-16,15] et le renvoie
  comme son propre résultat.
- **kOfflineStatus (0xC00000BB) = -1073741381 signé, TOUJOURS < -16** →
  chaque requête de priorité renvoyait systématiquement le plancher du
  clamp (-15), pas une valeur réelle — conséquence OBSERVABLE, pas
  seulement latente (contrairement aux 2 fixes r162).
- **Corrigé** : renvoie `0u` (priorité normale/baseline) — garde le
  clamp de l'appelant en no-op au lieu de toujours se déclencher.
- Tests 145/145 (144/144 → +1). Import trace confirme non-géré →
  géré. `ctest` 9/9. Crash `sub_821F7C80` inchangé (gdb).
- **Famille close** : `ObDereferenceObject`/`KeSetBasePriorityThread`/
  `KeQueryBasePriorityThread` sont maintenant TOUS corrigés. Voir
  `reports/ac6-retail-native-codegen-gate2-r163-real-fix-kequerybasepriority-thread-return-value-was-actually-clamped-and-used-20260901.md`.

# AC6 retail NTSC-U/J — r162 : VRAI CORRECTIF — `ObDereferenceObject`/`KeSetBasePriorityThread` renvoyaient un code de statut au lieu d'un compteur réel (2026-09-01)

- **Bug de forme de contrat, même catégorie que r148** :
  `ObDereferenceObject`/`KeSetBasePriorityThread` ont un vrai contrat
  `LONG` (compteur/increment précédent), pas NTSTATUS — le fallback
  générique renvoyait `kOfflineStatus` (0xC00000BB), une sentinelle
  NTSTATUS négative fausse.
- **Vérifié statiquement contre TOUS les sites d'appel réels** (18 pour
  ObDereferenceObject, 3 pour KeSetBasePriorityThread, via
  `Ac6Xrefs`/`Ac6XenonDisasm`) : chacun jette la valeur de retour
  immédiatement — confirme (ne contredit pas) la note de
  dépriorisation déjà présente dans le code.
- **Corrigé quand même** : même principe que r148 — une forme fausse
  reste fausse même si aucun appelant tracé actuellement n'en dépend ;
  un futur chemin de code pourrait en hériter. Les 2 stubs renvoient
  maintenant `0u`.
- **Aucun effet observable sur le run actuel** (contrairement à r148) —
  correctif de forme, pas de comportement. Crash `sub_821F7C80`
  inchangé (confirmé gdb).
- Tests 144/144 (142/142 → +2). Import trace confirme les 2 imports
  ne sont plus "offline-import" non gérés. `ctest` 9/9. Voir
  `reports/ac6-retail-native-codegen-gate2-r162-real-fix-obdereferenceobject-ketesetbasepriority-thread-returned-a-status-code-instead-of-a-real-count-20260901.md`.

# AC6 retail NTSC-U/J — r161 : MESURE LIVE DÉCISIVE — Xenia Edge laisse `0x39e8` (14824), PAS une taille garbage catastrophique — la chaîne causale DATA.TBL est maintenant PROUVÉE dépendante de l'environnement (2026-09-01)

- **Méthode principielle** : désassemblation du code JIT réel au point
  de trap (`x/40i $rip`) plutôt que deviner — révèle littéralement
  `mov 0x30(%rsi),%rbx` (rsi=PPCContext*, r1 invité à ctx+0x30, CONFIRMÉ
  contre le vrai source public `has207/xenia-edge`) puis
  `movbe 0x58(%rdi,%rax,1),%rbx` (rdi=base mémoire invité, chargement
  swappé big-endian) — l'instruction CIBLE elle-même.
- **VALEUR MESURÉE** : adresse hôte cible = `rdi + r1 + 0x58` =
  `0x17018f908`. Octets bruts (ordre d'adresse) : `00 00 00 00 00 00 39
  e8` = big-endian 64-bit = **`0x39e8` = 14824 décimal**.
- **VÉRIFICATION CROISÉE** : les octets `[r1+84..+87]` (juste avant)
  lisent `00 00 00 01` = 1 exactement — l'invariant connu et
  indépendamment dérivé de r159 (`sub_823382A8` écrit `1` à cet
  offset). Confirme tout le calcul d'adresse. 2 méthodes de lecture
  gdb indépendantes concordent.
- **ÉTABLIT DE FAÇON DÉCISIVE** : Xenia Edge laisse une petite valeur
  ORDINAIRE (14824) à cet emplacement — PAS une classe ~4 GiB
  catastrophique. Convertit le modèle plausible de r150 en FAIT PROUVÉ :
  chaque environnement a son propre contenu de pile résiduel
  génuinement non initialisé — notre recompilation (`0xfeffffee`) est
  l'exception catastrophique, pas la norme.
- **N'ÉTABLIT PAS** : la vraie valeur matériel réel (Xenia Edge a sa
  propre histoire d'allocation) ; une recette de fix (reproduire la
  valeur de Xenia Edge serait la valeur synthétique refusée, précédent
  r53) ; pourquoi le pattern de réutilisation de pile de NOTRE
  recompilation diffère assez pour être catastrophique.
- **DÉCISION** : ferme le fil de comparaison live DATA.TBL (r150-r161).
  Tout travail futur serait une investigation de fix native
  séparément scopée, pas une continuation de la comparaison oracle.
- **Aucun code modifié, aucun build ce cycle**. Voir
  `reports/ac6-retail-native-codegen-gate2-r161-decisive-live-value-xenia-edge-leaves-14824-not-a-catastrophic-garbage-allocation-size-20260901.md`.

# AC6 retail NTSC-U/J — r160 : le breakpoint live de Xenia Edge ATTEINT l'instruction exacte `[r1+88]` mais décoder ses registres JIT est un effort séparé, non investi (2026-09-01)

- **Breakpoint Xenia Edge (`break_on_instruction=0x823385d0`) FONCTIONNE**,
  sous gdb : SIGTRAP capturé exactement à l'instruction cible (`rcx`
  contient l'adresse `0x823385d0` telle quelle) — confirme que ce
  chemin de code est RÉELLEMENT atteint en boot normal, pas mort/évité.
- **Décodage des registres JIT NON abouti** : `rax`/`rbx`/`r10`
  (`0x7018f8xx`, dans la plage des adresses de pile invité déjà vues
  dans le log) inaccessibles directement via `x` gdb ; `r12`/`r13`
  (`0x8291...`) accessibles mais lus à zéro, relation avec `r1` invité
  non établie.
- **DÉCISION** : mécanisme viable et vérifié, mais décoder la
  convention de registres JIT de Xenia Edge (binaire release, sans
  symboles) est un investissement séparé, plus important, au
  bénéfice incertain — deviendrait exactement le pattern "règle
  plausible sans contrôle" que le projet refuse (précédent r111/r113).
  Arrêté ici, coût/bénéfice (pattern r144/r154).
- **Aucun code modifié, aucun build ce cycle**. Voir
  `reports/ac6-retail-native-codegen-gate2-r160-xenia-edge-live-breakpoint-works-but-jit-register-decoding-is-a-separate-uninvested-effort-20260901.md`.

# AC6 retail NTSC-U/J — r159 : la vraie désassemblation Ghidra montre que la lecture non initialisée est FIDÈLE octet-par-octet — CORRIGE r157/r158 (2026-09-01)

- **Désassemblation RÉELLE** (`Ac6XenonDisasm`, pas le C++ généré) de
  `sub_82338568` ET `sub_823382A8` : `ld r31,0x58(r1)` (= offset 88)
  EXISTE littéralement dans le binaire retail réel, inconditionnel ;
  `sub_823382A8` n'écrit QUE les offsets +0 et +4, jamais +8 — vérifié
  contre les instructions réelles, pas seulement le C++ généré.
- **CORRIGE r157/r158** : leur formulation "confirmé spécifique à notre
  recompilation" est INEXACTE. Le C++ généré que r130-r153 lisent
  depuis le début est une traduction FIDÈLE du vrai binaire — AUCUN
  bug de traduction XenonRecomp ici. Ce que r157/r158 ont bien établi
  (le vrai jeu boote sans planter) reste valable ; leur interprétation
  causale (mauvaise traduction) ne l'est pas.
- **Explication corrigée** : chaque environnement (notre recompilation,
  Xenia Edge, le vrai matériel) a son propre historique d'exécution
  antérieur, donc son propre contenu de pile résiduel à cette adresse
  — TOUS lisent une mémoire génuinement non initialisée, avec des
  octets différents. C'est exactement la formulation de r150
  ("jamais fixée à 0 par quoi que ce soit — ce que la pile contenait
  par hasard") qui était correcte depuis le début.
- **Implication pour un fix** : plus étroit que r157/r158 ne le
  suggéraient — pas "corriger une mauvaise traduction" (il n'y en a
  pas), mais potentiellement "reproduire le motif de réutilisation de
  pile du vrai matériel" — frôle la valeur synthétique/codée en dur que
  la discipline du projet refuse (précédent r53). Décision non prise.
- **Aucun code modifié, pas de build ce cycle** — désassemblation
  statique pure. Voir
  `reports/ac6-retail-native-codegen-gate2-r159-real-disassembly-shows-the-uninitialized-read-is-byte-for-byte-faithful-corrects-r157-r158-20260901.md`.

# AC6 retail NTSC-U/J — r158 : release Xenia Edge PINNÉE (60ff861) reproduit r157 et va plus loin (écran ESRB) — résultat maintenant QUALIFIÉ (2026-09-01)

- **Release pinnée obtenue** : `has207/xenia-edge` tag `60ff861`,
  téléchargée via l'API GitHub Releases, SHA-256 vérifié
  `c2cac2a0...` = EXACTEMENT le hash pinné par
  `scripts/run_xenia_edge_native.sh` et `cycle-1734`. Installée à
  `.tools/xenia-edge-60ff861/`.
- **REPRODUIT r157 à l'identique** : même titre `4E4D07D1`, même
  séquence shaders/pipelines, et l'écran légal/marques déposées est
  PIXEL-IDENTIQUE (même hash SHA-256 exact que la capture r157) malgré
  2 binaires AppImage différents — forte preuve de reproductibilité.
- **VA PLUS LOIN que r157** : atteint un 2e écran de crédits (imagerie
  satellite : Japan Space Imaging, GeoEye, INTA Spaceturk,
  DigitalGlobe/HitachiSoft, Bink Video) PUIS un vrai écran d'avis ESRB
  ("Game experience may change during online play") — un point encore
  plus avancé de la séquence de boot réelle.
- **DÉCISION** : le résultat de r157 passe de PROVISOIRE à QUALIFIÉ.
  La conclusion (blocage DATA.TBL spécifique à notre recompilation, pas
  au vrai jeu) tient maintenant sur une capture oracle pleinement
  qualifiée et reproductible.
- **Aucun code source modifié** — preuves dans
  `reports/ac6-retail-native-xenia-edge-oracle-r158-pinned-20260901/`.
  Voir
  `reports/ac6-retail-native-codegen-gate2-r158-pinned-xenia-edge-release-qualifies-r157-fully-progresses-past-esrb-notice-20260901.md`.

# AC6 retail NTSC-U/J — r157 : ORACLE Xenia Edge BOOTE au-delà de TOUT ce que notre recompilation atteint — le blocage DATA.TBL/pile non initialisée est CONFIRMÉ spécifique à notre recompilation (2026-09-01)

- **RÉSULTAT MAJEUR** : Xenia Edge (build non pinné, provenance non
  établie — CAPTURE PROVISOIRE, pas qualifiée) contre l'ISO NTSC-U/J
  CORRECT (hash vérifié) boote le titre réel : écran légal/marques
  déposées rendu correctement, PUIS écran-titre avec fond cinématique
  réel (pont, avion de chasse) — shaders Xenos traduits, pipelines
  Vulkan réels, >2000 frames. Va BIEN au-delà de tout ce que cette
  campagne a jamais atteint côté recompilation native.
- **ÉTABLIT** : le blocage `sub_821F7C80`/pile non initialisée
  `[r1+88]` (tracé r130-r153, fermé "aucun levier" par r144) est
  maintenant montré CONFIRMÉ SPÉCIFIQUE À NOTRE RECOMPILATION — le
  vrai jeu (émulé fidèlement) ne bloque PAS ici. Renverse la clôture
  coût/bénéfice de r144 pour ce sous-fil : un vrai fix natif est
  maintenant défendable, pas une supposition.
- **N'ÉTABLIT PAS** : accès `DATA.TBL` par nom dans le log (pas de
  log par nom de fichier à ce niveau de verbosité) ; la valeur réelle
  attendue à `[r1+88]` (Xenia Edge a sa propre implémentation de pile,
  pas une preuve directe du matériel réel) ; ce n'est PAS une capture
  oracle QUALIFIÉE (le binaire ne correspond pas à la release pinnée
  `c2cac2a0...` du script existant — provenance à établir).
- **DÉCISION** : le résultat le plus significatif de cette session sur
  le sous-fil DATA.TBL. Nommé comme investigation prioritaire :
  obtenir la release Xenia Edge pinnée pour qualifier pleinement cette
  capture, puis comparer ciblé au point exact de lecture `[r1+88]`.
- **Aucun code source modifié** — capture oracle hors dépôt versionné
  sauf le rapport et son répertoire de preuves
  (`reports/ac6-retail-native-xenia-edge-oracle-r157-20260901/`). Voir
  `reports/ac6-retail-native-codegen-gate2-r157-xenia-edge-oracle-boots-past-data-tbl-real-title-screen-confirms-datatbl-stall-is-recompilation-specific-20260901.md`.

# AC6 retail NTSC-U/J — r156 : INVESTISSEMENT AUTORISÉ (oracle + analyse complète) — `sub_821F5630` résolu (dispatch indirect), Xenia natif confirmé bloqué sur NTSC-U/J aussi (2026-09-01)

- **Utilisateur a autorisé "Oracle and invest"** — première vraie
  utilisation d'un oracle depuis le début de cette campagne.
- **Analyse Ghidra COMPLÈTE (`-analysis`, pas `-noanalysis`)** sur une
  COPIE scratch du projet (le projet canonique read-only n'est PAS
  modifié) : 8 xrefs vers le thunk `NtQueryInformationFile` au lieu de
  1 — 7 nouvelles, toutes `COMPUTED_CALL` (`bctrl`, invisibles à
  `-noanalysis` ET au grep littéral du C++ généré). `sub_821F5630`
  (la fonction que le backtrace de r154 avait nommée) EST un vrai
  appelant : `PPC_CALL_INDIRECT_FUNC(ctr.u32)` après double
  déréférencement vtable (`+1996` puis `+32`) — un dispatcher
  GÉNÉRIQUE de fournisseur, pas un appel direct nommé.
- **CORRIGE r154** : l'hypothèse "élision d'appel terminal -O3" était
  FAUSSE — la vraie raison est structurelle (appel indirect invisible
  au grep littéral), pas une optimisation qui efface des frames.
- **Affine r150** : `sub_82390880` (appel direct, capture vidéo debug,
  r147) et `sub_821F5630` (appel indirect, dispatch générique) sont
  2 sites SANS RAPPORT vers le même thunk. DATA.TBL passe par
  `sub_821F5630`, PAS par l'idiome capture vidéo de r147 — la question
  ouverte depuis r150 est maintenant FERMÉE avec une vraie réponse.
- **Session Xenia RÉELLE** contre l'ISO NTSC-U/J CORRECT (pas le PAL
  `game-files/default.xex` des scripts existants) : bloque au MÊME
  point que la limitation déjà documentée pour PAL
  (`XENIA_WINE_ORACLE_HANDOFF.md` : écran noir, `SDL_OpenAudioDevice()
  failed`) — confirmé pour NTSC-U/J aussi, 7min20s sans progression de
  log, capture d'écran à l'appui. Négatif réel, nouveau point de
  donnée, pas une simple relecture.
- **DÉCISION** : le fil `sub_82390880`/`sub_821F5630` est fermé avec
  une réponse VÉRIFIÉE. Xenia natif Linux confirmé non viable pour ce
  titre aussi ; la route Wine (documentée seulement pour PAL) serait un
  investissement séparé, plus important, pas tenté ce cycle.
- **Aucun code source modifié** — copie scratch Ghidra et session
  Xenia toutes deux hors du dépôt versionné. Voir
  `reports/ac6-retail-native-codegen-gate2-r156-oracle-and-full-analysis-investment-sub_821f5630-indirect-caller-resolved-xenia-stalls-confirmed-20260901.md`.

# AC6 retail NTSC-U/J — r155 : un xref Ghidra `-noanalysis` est CONTREDIT par l'instrumentation live — `sub_82390880` n'est jamais entré ce run (2026-09-01)

- **Scan Ghidra statique réel** (`Ac6Xrefs.java`, base de données de
  références, pas du texte) sur l'adresse réelle du thunk
  `NtQueryInformationFile` (`0x823D031C`, lue dans
  `ppc_func_mapping.cpp`) : 1 seule xref, `bl 0x823d031c` à
  `0x823908a4`, DANS `sub_82390880`. D'ACCORD avec r146/r147 et avec le
  grep littéral du C++ généré — 2 méthodes statiques concordantes.
- **CONTREDIT par l'instrumentation live** : `fprintf` INCONDITIONNEL
  (pas de garde `getenv`, pour éliminer toute cause environnementale)
  placé en première instruction de `__imp__sub_82390880`, PRÉSENCE
  vérifiée dans le binaire compilé via `objdump` — ne s'affiche JAMAIS
  sur un run complet où `NtQueryInformationFile` se déclenche pourtant
  bien (trace d'import). `sub_82390880` n'est RÉELLEMENT jamais entré
  ce run.
- **Réconciliation plausible (NON vérifiée)** : le scan Ghidra était
  `-noanalysis` — son gestionnaire de références ne résout pas les
  appels indirects (`bctrl` via adresse chargée d'une table) ; un appel
  indirect vers `0x823D031C` contournerait entièrement le corps compilé
  de `sub_82390880` sans laisser de xref visible sans analyse complète.
- **DÉCISION** : applique le principe CLAUDE.md "mesurer l'instrument"
  dans l'autre sens — 2 méthodes statiques CONCORDANTES étaient toutes
  deux fausses ; seule l'instrumentation live contrôlée l'a détecté.
  Le fil `sub_82390880` reste fermé (r154), maintenant avec la raison
  documentée. Résoudre pour de vrai demanderait une passe Ghidra
  `-analysis` complète — investissement plus important que ce que ce
  fil (sans levier depuis r147) justifie.
- **Aucun code source modifié ce cycle** — 1 diagnostic temporaire
  (inconditionnel), annulé et vérifié (ctest 9/9 après reconstruction
  propre). Voir
  `reports/ac6-retail-native-codegen-gate2-r155-ghidra-noanalysis-xref-contradicted-by-live-instrumentation-sub_82390880-genuinely-never-entered-20260901.md`.

# AC6 retail NTSC-U/J — r154 : `backtrace()` échoue sous élision d'appel terminal ; le fil `sub_82390880` fermé coût/bénéfice — les 2 frontières nommées sont à nouveau bloquées (2026-09-01)

- **`backtrace()` PEU FIABLE ici** : instrumenter `sub_82390880` (site
  d'appel littéral unique selon r146/r147) ne déclenche JAMAIS, alors
  que `NtQueryInformationFile`/`NtSetInformationFile` se déclenchent
  bien juste après l'ouverture de DATA.TBL. Instrumenter le stub lui-
  même montre un backtrace résolvant vers `sub_821F5630` — mais lire
  cette fonction en entier (68 lignes) montre qu'elle n'appelle PAS
  littéralement `NtQueryInformationFile` (son seul appel va vers
  `sub_821F75B8`, qui lui-même n'appelle qu'un helper RTL). Conclusion :
  optimisation d'appel terminal (`-O3`) élidant des frames réelles —
  `backtrace()` seul n'est PAS une preuve fiable de l'appelant ici (à
  la différence de r152, où chaque frame avait été vérifiée contre un
  appel littéral du source).
- **Fil `sub_82390880`/DATA.TBL FERMÉ coût/bénéfice** : re-dériver le
  vrai chemin demanderait un scan Ghidra statique du XEX réel — travail
  réel mais dont r147 avait déjà établi la non-pertinence (capture
  vidéo debug, sans rapport avec l'échec DATA.TBL), et r150-r153 ont
  depuis fermé la chaîne causale DATA.TBL par une route ENTIÈREMENT
  séparée (`sub_821CC288→sub_82222D80`) qui n'en dépend pas.
- **Les 2 frontières nommées de Gate 2 sont À NOUVEAU bloquées** :
  chaîne DATA.TBL entièrement tracée (r144, re-confirmé r153) ;
  `IM_LOAD_IMMEDIATE`→SPIR-V bloqué par politique (pas d'oracle, r144).
  3 vrais correctifs natifs trouvés cette session (r145/r148/r149)
  conservés sur leurs propres mérites.
- **Aucun code source modifié ce cycle** — 2 diagnostics temporaires,
  annulés et vérifiés (ctest 9/9 après reconstruction propre). Voir
  `reports/ac6-retail-native-codegen-gate2-r154-backtrace-caller-id-fails-under-tail-call-elision-sub_82390880-thread-closed-cost-benefit-20260901.md`.

# AC6 retail NTSC-U/J — r153 : le mécanisme de r141/r142 est RE-CONFIRMÉ octet-par-octet contre le binaire actuel — chaîne DATA.TBL close (2026-09-01)

- **Trace complète, purement statique** (pas de diagnostic ce cycle) de
  `sub_822834C0` → `sub_82338568` → `sub_82339D10`, jusqu'à la lecture
  du slot de pile. Confirme le MÊME mécanisme que r141/r142 : le slot
  `[r1+88]` de la frame de `sub_82338568` n'est écrit par AUCUNE
  fonction de la chaîne (`sub_823382A8` n'écrit que +0/+4 ;
  `sub_82339D10` n'écrit RIEN via son pointeur `r28`) — vérifié en
  lisant les 2 fonctions en entier. `sub_82339D10` prend la branche
  succès (via `sub_82343F20`, confirmé 5/5 par r138 ; `sub_82344058`,
  confirmé renvoyer 1..5 par r140/r141), ce qui fait prendre à
  `sub_82338568` la branche qui DISCARD le résultat réel et lit
  `[r1+88]` — mémoire jamais écrite — comme valeur de retour.
- **Seul ce qui a changé** : la valeur garbage exacte occupant ce slot
  (`0` avant les 3 correctifs, `1`/`0xfeffffee` maintenant) — exactement
  la conséquence prédite par r150/r151/r152, maintenant PROUVÉE plutôt
  que supposée.
- **DÉCISION** : ferme la question "le mécanisme a-t-il changé ?" —
  NON. Aucun nouveau levier natif actionnable (cohérent avec la
  conclusion coût/bénéfice de r144). La chaîne DATA.TBL
  `sub_821CC288→sub_82222D80` est maintenant close contre le binaire
  actuel ; pas besoin de re-tracer davantage.
- **Aucun code modifié, aucun diagnostic ce cycle** — lecture statique
  pure du code déjà généré. Voir
  `reports/ac6-retail-native-codegen-gate2-r153-r141-r142s-mechanism-fully-reconfirmed-byte-for-byte-against-the-current-binary-20260901.md`.

# AC6 retail NTSC-U/J — r152 : r150 et r151 sont la MÊME chaîne — `sub_822834C0` renvoie directement la taille garbage (2026-09-01)

- **Lien établi entre r150 et r151** : `backtrace()` capturé au moment où
  `sub_82222D80` reçoit `r4=0xfeffffee` (gdb avec condition sur `ctx`
  échoue — pas de DWARF locals dans ce build ; `backtrace()`+`addr2line`
  fonctionne). Résolu : `sub_82222D80` <- `sub_821CC288` <-
  `sub_821D5F48` <- `sub_821D7DE0` <- `_xstart`.
- **`sub_821CC288` CONFIRME l'attribution de r135** (auto-corrigée en
  cycle contre `sub_821CC508` plus tôt cette session) : son seul appel
  avant `sub_82222D80` est `sub_822834C0` — EXACTEMENT la fonction que
  r150 avait déjà instrumentée (son appel à `sub_82338388` renvoyant
  `1`). `rotlwi r30,r3,0` est une copie de registre pure : le retour de
  `sub_822834C0` DEVIENT directement `r4` (la taille) sans rien entre
  les deux.
  **Donc le retour de `sub_822834C0` EST la taille garbage** — pas une
  valeur qui y contribue seulement.
- **N'établit PAS encore** : l'arithmétique EXACTE à l'intérieur de
  `sub_822834C0` reliant "`sub_82338388` renvoie `1`" à "`sub_822834C0`
  renvoie `0xfeffffee`" — c'est maintenant une trace À L'INTÉRIEUR
  D'UNE SEULE FONCTION, plus une chaîne multi-fonctions.
- **Aucun code source modifié ce cycle** — 1 diagnostic temporaire,
  annulé et vérifié (ctest 9/9 après reconstruction propre).

# AC6 retail NTSC-U/J — r151 : la taille garbage d'allocation (r135/r137) est TOUJOURS atteinte après les 3 correctifs — valeur exacte différente, même classe (2026-09-01)

- **Retrace du point 1 de la liste "Next" de r150** : `sub_82222D80`
  (l'allocateur identifié par r135) reçoit toujours un 4e appel avec une
  taille garbage classe ~4 GiB (`0xfeffffee`, préfixe `0xFEFF....`) —
  PAS la valeur exacte de r137 (`0xfefffff8`-classe), mais le MÊME motif.
  Confirme une 2e fois (après le slot `sub_82338388` de r150) que le
  mécanisme "valeur périmée sensible à l'historique d'exécution" est
  généralisé sur cette chaîne, pas isolé à un seul slot.
- **N'établit PAS encore** : si `0xfeffffee` traverse le même chemin
  `sub_82339AA8`/`sub_82338388`/`[r1+88]` que r139-r142 avaient tracé, ou
  un chemin différent — reste à vérifier. Voir
  `reports/ac6-retail-native-codegen-gate2-r151-*.md`.
- **Aucun code source modifié ce cycle** — 1 diagnostic temporaire sur
  `sub_82222D80`, annulé et vérifié (ctest 9/9 après reconstruction
  propre). `git status` ne montre que l'état sale préexistant, sans
  rapport (conversion submodule demo, arbre `reconstruction/`).

# AC6 retail NTSC-U/J — r150 : la valeur de pile périmée (r139/r142) a CHANGÉ de `0` à `1` après les 3 correctifs de cette session — DATA.TBL traverse maintenant l'idiome de troncature — site de crash INCHANGÉ (2026-09-01)

- **Pourquoi cette vérification valait la peine** : 3 correctifs réels
  cette session (r145, r148, r149) ont chacun changé le vrai
  ordonnancement/flux d'exécution du jeu. r142 avait établi que le
  "retour" de `sub_82338388` est une lecture de mémoire de PILE JAMAIS
  ÉCRITE — une valeur ENTIÈREMENT fonction de ce que le code
  précédent, sans rapport, a laissé sur ce slot exact. Vérifier à
  nouveau plutôt que de supposer que la caractérisation de r139-r142
  (un `0` effectivement fixe) tient toujours.
- **LA VALEUR A CHANGÉ** : mesuré en direct sur le MÊME site que
  r139/r141/r142 — **`sub_82338388` renvoie maintenant `1`, PAS `0`.**
  Ceci CONFIRME (ne contredit PAS) la caractérisation de r142 : la
  valeur n'a jamais été fixée à `0` par quoi que ce soit dans le code
  tracé — c'était ce que la pile contenait par hasard, et les
  correctifs de cette session ont changé ce que la pile contient.
- **Changement encore plus surprenant** : la même session montre
  `NtQueryInformationFile`/`NtSetInformationFile` (idiome de
  troncature identifié par r146/r147 comme "capture vidéo de debug")
  s'exécuter sur le handle qui vient d'ouvrir **DATA.TBL** ! Soit
  `sub_821E9F50`/`sub_821EA2F8` sont des utilitaires plus GÉNÉRAUX que
  la lecture étroite de r147 (un seul des appelants possibles avait été
  tracé jusqu'à la chaîne "capture vidéo"), soit c'est un chemin de
  code réellement NOUVEAU exposé par les correctifs de cette session.
  La conclusion de r147 n'est PAS contredite par une preuve directe
  pour CE site précis, mais elle n'est plus toute l'histoire.
- **Site de crash INCHANGÉ** : `sub_821F7C80` se reproduit toujours à
  l'identique (backtrace gdb confirmé), malgré ces deux changements.
- **DÉCISION** : enregistré comme un changement significatif et
  CONFIRMÉ des propres mesures antérieures de cette investigation — pas
  une contradiction, mais un rappel que les valeurs intermédiaires de
  la chaîne causale DATA.TBL (établies sur r130-r142) ont été mesurées
  contre un binaire qui ne correspond plus à l'état committé actuel.
  Retracer ENTIÈREMENT cette chaîne depuis `sub_821CC288` contre le
  binaire ACTUEL est une entreprise substantielle, comparable en
  ampleur à tout l'arc r130-r142 — explicitement PAS tentée ce cycle
  (le faire à la hâte risquerait de re-dériver des conclusions
  périmées contre une cible mouvante).
- **Aucun code source modifié ce cycle** — 1 diagnostic temporaire,
  annulé et vérifié (ctest 9/9, 142/142 Python après reconstruction
  propre).
  **Prochain cycle** : re-tracer en UNE passe consolidée les mesures
  clés de r130-r142 (taille d'allocation de r135, valeur garbage de
  r137, requête catégorie/réglage de r139, trace de contenu de pile de
  r141/r142) contre le binaire ACTUEL — traiter comme un NOUVEAU fil
  d'investigation multi-cycles, pas un correctif d'un seul cycle. Voir
  `reports/ac6-retail-native-codegen-gate2-r150-the-stale-stack-value-changed-from-0-to-1-after-three-fixes-crash-site-unchanged-full-retrace-needed-20260901.md`.

# AC6 retail NTSC-U/J — r149 : VRAI CORRECTIF — `KeSetAffinityThread` renvoyait un code de statut négatif là où le vrai contrat attend un masque d'affinité positif (2026-09-01)

- **Suivant le "prochain" de r148** : lecture du seul site d'appel de
  `KeSetAffinityThread` (atteint seulement depuis le correctif
  `ObReferenceObjectByHandle` de r148) — signature réelle
  `(Handle, DWORD Affinity, DWORD* PreviousAffinity)`.
- **DEUX problèmes trouvés** : (1) le vrai contrat NT renvoie le MASQUE
  D'AFFINITÉ PRÉCÉDENT lui-même dans `r3`, PAS un code de statut —
  confirmé par l'appelant lui-même qui calcule `31 -
  compte_zéros_de_tête(masque)` pour trouver l'INDEX du cœur précédent.
  Un masque réel sur une topologie Xenon à 6 threads matériels est
  toujours petit et non-négatif — le garde `blt` de l'appelant est du
  code MORT sur le vrai matériel ; l'ancien `kOfflineStatus` générique
  (négatif comme masque signé) le déclenchait à CHAQUE appel. (2) la
  sortie `*PreviousAffinity` n'était JAMAIS écrite — même passé le
  garde, le bit-scan tournerait sur de la pile non initialisée (même
  classe de bug que le `KeResumeThread` de r148).
- **CORRECTIF** : renvoie `1u` dans `r3` (un vrai masque, pas un
  statut) et écrit `1u` via `*PreviousAffinity` — "cœur 0", toujours
  dans la plage valide. Ce projet ne modélise aucune affinité de cœur
  réelle (ordonnancement hôte unique) — une valeur petite, cohérente,
  garde le bit-scan de l'appelant significatif.
- **Tests** : nouveau
  `test_ke_set_affinity_thread_returns_a_real_mask_not_a_status`. Suite
  complète : 142/142 (était 141/141), 29/29 dans ce fichier.
- **VÉRIFIÉ EN DIRECT** : `KeSetAffinityThread` n'apparaît plus comme
  import non géré. Le compte de statuts `RtlNtStatusToDosError` non
  mappés CHUTE de 19 à 2 dans la même session — cohérent avec cette
  chaîne d'appel réussissant maintenant au lieu d'échouer en boucle. Le
  crash `sub_821F7C80` persiste au même site (chaîne causale séparée).
- **DÉCISION** : conservé et committé sur ses propres mérites, même
  précédent que r145/r148.
- **Gates** : mission01 (même échec pré-existant), ctest 9/9, Python
  142/142, `git status` propre, démo 185 inchangé.
  **Prochain cycle** : `ObDereferenceObject` (35 appels) et
  `KeSetBasePriorityThread` (17 appels) restent les 2 imports non gérés
  les plus fréquents — aucun site échantillonné ne vérifie leur retour
  (trouvaille de r148), priorité plus basse, mais à re-vérifier
  rapidement maintenant que plus de code s'exécute après eux. Voir
  `reports/ac6-retail-native-codegen-gate2-r149-real-fix-kesetaffinitythread-returned-a-status-code-instead-of-a-real-affinity-mask-20260901.md`.

# AC6 retail NTSC-U/J — r148 : VRAI CORRECTIF — `ObReferenceObjectByHandle` n'écrivait JAMAIS sa sortie, échouant un `KeResumeThread` sur de la mémoire de pile non initialisée (2026-09-01)

- **Suivant le "prochain" de r147** : la trace complète contre le binaire
  r145/r147 montre `ObReferenceObjectByHandle` comme l'import non géré
  le PLUS FRÉQUENT de cette session (35 appels) — tombant à chaque fois
  sur le stub générique offline (retourne `kOfflineStatus`, n'écrit
  rien).
- **4 sites d'appel indépendants lus** : signature réelle cohérente
  `(Handle, ObjectType, PVOID* Object)`. TOUS traitent l'"objet" renvoyé
  comme un jeton OPAQUE immédiatement repassé à une AUTRE API kernel de
  thread (`KeSetBasePriorityThread`/`KeQueryBasePriorityThread`/
  `ObDereferenceObject`/`KeResumeThread`) — JAMAIS déréférencé
  directement. Nos propres handles (déjà des entiers opaques)
  fonctionnent directement comme ce jeton.
- **VRAI BUG SÉPARÉ trouvé** : un site d'appel (après un
  `ExCreateThread`) appelle `KeResumeThread` sur la sortie
  d'`ObReferenceObjectByHandle` SANS MÊME vérifier son statut — comme
  l'ancien stub générique n'écrivait JAMAIS `*Object`, `KeResumeThread`
  recevait de la MÉMOIRE DE PILE NON INITIALISÉE au lieu du VRAI handle
  de thread — le thread parqué sous `CREATE_SUSPENDED` ne pouvait JAMAIS
  réellement reprendre.
- **CORRECTIF** : `ObReferenceObjectByHandle` écrit maintenant le vrai
  handle directement comme "objet" et renvoie SUCCESS.
  `KeSetBasePriorityThread`/`KeQueryBasePriorityThread`/
  `ObDereferenceObject`/`KeSetAffinityThread` (nouveau, jamais vu avant)
  restent des stubs génériques inchangés — aucun site échantillonné ne
  vérifie leur retour.
- **Tests** : nouveau
  `test_ob_reference_object_by_handle_writes_the_real_handle_through`.
  Suite complète : 141/141 (était 140/140), 28/28 dans ce fichier.
- **VÉRIFIÉ EN DIRECT** : `ObReferenceObjectByHandle` n'apparaît PLUS
  comme import non géré — le correctif est réellement exercé. De
  NOUVEAUX imports jamais vus dans TOUTE cette investigation
  apparaissent en aval (`KeSetAffinityThread`). Le crash
  `sub_821F7C80` persiste au MÊME site exact — attendu, chaîne causale
  totalement séparée (r130-r142).
- **DÉCISION** : conservé et committé sur ses propres mérites, même
  précédent que r145 — 2 vrais bugs corrigés, changement de
  comportement observable confirmé, indépendant du crash actuellement
  investigué.
- **Gates** : mission01 (même échec pré-existant), ctest 9/9, Python
  141/141, `git status` propre, démo 185 inchangé.
  **Prochain cycle** : lire les sites d'appel de `KeSetAffinityThread`
  (jamais vu avant) avant de décider s'il nécessite une vraie gestion.
  Continuer à vérifier systématiquement l'effet observable de chaque
  correctif natif réel. Voir
  `reports/ac6-retail-native-codegen-gate2-r148-real-fix-obreferenceobjectbyhandle-never-wrote-its-output-stranding-a-resumed-thread-20260901.md`.

# AC6 retail NTSC-U/J — r147 : `sub_82390880` est une fonctionnalité de CAPTURE VIDÉO de debug — CONFIRMÉ sans rapport avec DATA.TBL, PAS implémenté (2026-09-01)

- **Suivant le "prochain" de r146** : `sub_82390880` a exactement 2 sites
  d'appel, tous deux dans un helper d'ouverture de fichier en boucle,
  dispatché par mode (`sub_821F4C10`), qui construit un nom de fichier
  via `sprintf` avec le format `"%d%s"` (segments numérotés) avant
  d'ouvrir et tronquer chacun.
- **PREUVE DÉCISIVE** : lecture des octets juste après ce format
  `"%d%s"` dans l'image XEX statique (`DumpBytes.java` à `0x82068170`) —
  à `0x82068178` : **`"D3D: Unable to create movie capture file segment
  %s.\n"`**. TOUTE cette boucle d'ouverture/troncature de fichiers est
  une **fonctionnalité de CAPTURE VIDÉO de debug/développeur** — des
  fichiers de segments vidéo numérotés — preuve DIRECTE et NON AMBIGUË
  tirée des propres données du binaire retail.
- **DÉCISION** : `NtQueryInformationFile`/`NtSetInformationFile` NE sont
  PAS implémentés ce cycle — cette piste spécifique est FERMÉE. Le site
  d'appel signalé par r146 est confirmé sans rapport avec DATA.TBL, la
  boucle de relance (`sub_821D5F48`/`sub_821CC508`) ou la chaîne de
  crash `sub_821F7C80`. Construire l'infrastructure de suivi de
  position pour servir une fonctionnalité de capture de debug serait
  exactement le genre de scope creep que ce projet interdit.
- **Aucun code source modifié ce cycle.**
  **Prochain cycle** : la détermination de r144 (aucun travail Gate 2
  actionnable au-delà du correctif réel de r145) est reconfirmée pour
  ce fil précis. Chercher D'AUTRES effets observables du correctif du
  sémaphore de r145 (même méthodologie que r146 : vérifier le lien avec
  la chaîne de crash active AVANT d'investir) plutôt que de continuer
  sur cette piste de capture vidéo maintenant fermée. Voir
  `reports/ac6-retail-native-codegen-gate2-r147-sub_82390880-is-a-movie-capture-debug-feature-unrelated-to-data-tbl-not-implementing-20260901.md`.

# AC6 retail NTSC-U/J — r146 : le correctif de r145 atteint un terrain RÉELLEMENT NOUVEAU — `NtQueryInformationFile`/`NtSetInformationFile` atteints pour la première fois (2026-09-01)

- **Suivant le "prochain" de r145** : `AC6_NATIVE_IMPORT_TRACE=1` complet
  contre l'ISO qualifiée atteint `NtQueryInformationFile`,
  `NtSetInformationFile`, `NetDll_XNetStartup`, `NetDll_WSAStartup`,
  `XamShowMessageBoxUIEx`, `ExRegisterTitleTerminateNotification` —
  AUCUN n'apparaît dans les traces précédentes de cette investigation
  (r130-r133 listaient `XamLoaderLaunchTitle`, `XamShowDirtyDiscErrorUI`,
  `VdGetSystemCommandBuffer` — un ensemble DIFFÉRENT). **Preuve directe
  que le correctif du sémaphore a changé le vrai ordonnancement/
  progression du jeu**, pas juste de la comptabilité interne — un
  chemin qui bloquait avant sur une attente de sémaphore toujours
  ratée progresse maintenant mesurablement plus loin, même si le crash
  `sub_821F7C80` lui-même reste inchangé.
- **Ce que fait le nouveau site d'appel** : `NtQueryInformationFile`
  (site unique, `sub_82390880`) confirme la vraie signature NT standard
  (r3=Handle, r4=&IoStatusBlock, r5=&FileInformation, r6=Length,
  r7=FileInformationClass) directement par désassemblage. Séquence de 3
  appels : query position (classe 14) → set EndOfFile (classe 20) à
  cette position → set Allocation (classe 19) à la même position — un
  idiome standard "tronquer ce handle à sa position actuelle", plus
  probablement lié à un fichier de save/log qu'à DATA.TBL.
- **DÉCISION** : trouvaille enregistrée, PAS agie ce cycle. Implémenter
  ces imports nécessiterait d'ajouter un suivi de position de fichier
  guest-visible à `NativeGuestMediaService` (notre `NtReadFile` utilise
  toujours un offset explicite, jamais de position implicite) — et le
  lien de ce site d'appel avec le crash Gate 2 actif n'est PAS établi.
  L'implémenter maintenant serait du scope creep non étayé par les
  preuves de ce cycle.
- **Aucun code source modifié ce cycle.**
  **Prochain cycle** : tracer les appelants de `sub_82390880` pour
  déterminer s'il appartient au chemin de sauvegarde déjà modélisé par
  `AtomicSaveStore`, ou à un mécanisme log/temp sans rapport, AVANT de
  décider si l'implémentation vaut l'infrastructure de suivi de
  position qu'elle nécessiterait. Continuer à vérifier systématiquement
  "ce correctif a-t-il changé un comportement observable ailleurs" après
  chaque vrai correctif natif. Voir
  `reports/ac6-retail-native-codegen-gate2-r146-r145s-fix-reaches-new-ground-ntqueryinformationfile-ntsetinformationfile-now-hit-20260901.md`.

# AC6 retail NTSC-U/J — r145 : VRAI CORRECTIF — `NtCreateSemaphore` n'enregistrait JAMAIS d'objet attendable ; `NtReleaseSemaphore` utilisait le MAUVAIS registre comme pointeur de sortie (2026-09-01)

- **Réouvre un terrain que r144 a déclaré bloqué** : la conclusion "aucun
  travail actionnable" de r144 est re-vérifiée avant d'être acceptée — le
  POURQUOI du timeout de r142 (structurel vs course réelle) valait la
  peine d'être creusé.
- **`wait_event()` renvoie `false` IMMÉDIATEMENT (aucune attente réelle)
  pour tout handle absent de `g_events`.** `NtCreateSemaphore` partageait
  un stub GÉNÉRIQUE avec `NtCreateTimer`/`NtCreateMutant` qui alloue
  SEULEMENT un numéro de handle — JAMAIS `create_event()`. Diagnostic en
  direct : les handles exacts que `sub_82338388` attend (`0x12e`,
  `0x131`, les MÊMES que r142) sont créés via `NtCreateSemaphore` —
  TOUT `NtWaitForSingleObjectEx` dessus est un `STATUS_TIMEOUT`
  GARANTI et DÉTERMINISTE, indépendamment de toute activité RÉELLE de
  `NtReleaseSemaphore` ailleurs dans le jeu. **Un bug STRUCTUREL de
  notre propre stub HLE, pas une course.**
- **Second bug trouvé en confirmant la convention d'appel réelle** (via
  désassemblage des vrais sites d'appel de cette XEX) : la vraie
  signature NT de `NtReleaseSemaphore` est `(HANDLE, LONG ReleaseCount,
  PLONG PreviousCount)` — `r4`=ReleaseCount (un ENTIER), `r5`=le VRAI
  pointeur de sortie. L'ancien stub partagé avec `NtReleaseMutant`
  écrivait via `r4` pour LES DEUX — pour Semaphore, ceci écrit
  `PPC_STORE_U32(ctx.r4.u32, 0u)` à l'adresse = ReleaseCount, PAS un
  vrai pointeur — une VRAIE corruption mémoire distincte.
- **CORRECTIF** : `NtCreateSemaphore` est maintenant son propre cas —
  enregistre le handle via `create_event(handle, manual_reset=false,
  signaled=InitialCount>0)` (r5, confirmé par désassemblage). Un
  auto-reset event correspond exactement au contrat sémaphore ("un
  permis consommé par attente réussie"). `NtCreateTimer`/`NtCreateMutant`
  INCHANGÉS (aucune preuve qu'ils participent au même modèle).
  `NtReleaseSemaphore` est maintenant son propre cas — appelle
  `set_event(r3)` et écrit via `r5` (le bon registre). `NtReleaseMutant`
  INCHANGÉ (son propre usage de `r4` était déjà correct).
- **Tests** : test existant scindé pour vérifier chaque import
  séparément ; nouveau `test_create_semaphore_registers_a_waitable_event`.
  Suite complète : 140/140 (était 139/139), 27/27 dans ce fichier
  (était 26/26).
- **VÉRIFIÉ EN DIRECT : le correctif est réel, mais INSUFFISANT pour
  changer le résultat du crash r131** — la sonde plante TOUJOURS au
  MÊME site (`sub_821F7C80`), MÊME chaîne d'appel (backtrace gdb
  confirmé). PAS un échec de ce correctif — cohérent avec la propre
  découverte SÉPARÉE de r142 : la valeur finale de `sub_82338388` vient
  de `[r1+88]`, un AUTRE slot de pile que RIEN n'écrit, QUE l'attente
  réussisse OU expire. Corriger l'attente ne remplit pas ce slot non
  lié.
- **DÉCISION** : correctif conservé et committé MALGRÉ ne pas changer le
  crash actuellement investigué — sur ses propres mérites : corrige 2
  vrais bugs étayés par preuves (timeout garanti structurel + corruption
  mémoire), correspond à la signature NT documentée confirmée par
  désassemblage réel, entièrement testé, pourrait affecter d'autres
  patterns d'attente/signal ailleurs dans le binaire retail non encore
  tracés. Même précédent que r130-r131.
- **Gates** : mission01 (même échec pré-existant), ctest 9/9, Python
  140/140, `git status` propre (2 fichiers source intentionnels +
  submodule pré-existant non lié), démo 185 inchangé.
  **Prochain cycle** : les déterminations de r144 tiennent pour les 2
  frontières qu'il a nommées — ce cycle n'a changé ni l'une ni l'autre.
  Vérifier si ce correctif a un effet observable ailleurs dans la sonde
  (une autre attente maintenant correctement signalée). Voir
  `reports/ac6-retail-native-codegen-gate2-r145-real-fix-ntcreatesemaphore-never-registered-a-waitable-object-ntreleasesemaphore-wrong-register-20260901.md`.

# AC6 retail NTSC-U/J — r144 : les deux frontières nommées sont CONFIRMÉES BLOQUÉES — audits de maintenance propres, AUCUN travail Gate 2 actionnable actuellement disponible (2026-09-01)

- **Le pivot de r143 vérifié AVANT d'agir dessus** : `IM_LOAD_IMMEDIATE`
  Xenos→SPIR-V est EXPLICITEMENT bloqué par POLITIQUE — le traducteur
  (`native_shader_translator.cpp`) refuse déjà, EN DUR, tout microcode
  Xenos, avec son propre commentaire : "reste un input fail-closed
  explicite jusqu'à ce que ses signatures de fetch AC6 soient qualifiées
  contre l'oracle offline scellé de référence." Ce projet n'a utilisé
  AUCUN oracle sur toute la campagne — un vrai BLOCAGE QUALIFIÉ selon la
  propre définition de ce projet.
- **Aussi actuellement INATTEIGNABLE** : le crash tracé r130-r143 se
  produit pendant le chargement de ressources PRÉCOCE, bien avant que le
  jeu n'atteigne la soumission de commandes GPU. Implémenter la
  traduction de shaders maintenant n'aurait AUCUNE sonde pour l'exercer.
- **Fermeture de r143 re-vérifiée** : pas de correctif défensif
  légitime disponible pour `sub_821CC288` (code guest retail, pas le
  runtime de ce projet — le raisonnement de r142/r143 tient toujours).
- **Audits de maintenance routiniers exécutés** (tous les outils
  bon-marché/toujours-sûrs de CLAUDE.md) : `audit_claude_md_numbers`,
  `audit_contract_derivations`, `audit_ac6_contract_addresses`,
  `audit_instrument_discipline_index` — TOUS PASSENT proprement. SEUL
  `audit_ac6_contract_artifacts` échoue, sur 3 chemins TOUS sous l'arbre
  N2 `reconstruction/ace-combat-6`, EXPLICITEMENT ABANDONNÉ selon la
  section "Frontières" de NEXT.md elle-même — pas nouveau, pas
  actionnable (même nature que le mismatch `retail_session.cpp` déjà
  connu depuis r107).
- **DÉCISION** : aucun travail Gate 2 actionnable, dans le périmètre, non
  bloqué n'est actuellement disponible. Nommé explicitement plutôt que
  masqué par une tâche de faible valeur.
- **Aucun code source modifié ce cycle.**
  **Prochain cycle** : re-vérifier cette détermination SI (1) une
  session oracle devient disponible pour cette campagne, OU (2) de
  nouvelles preuves montrent que la sonde peut progresser au-delà de son
  point de crash actuel par un moyen non encore considéré. En l'absence
  des deux, de nouveaux déclenchements de la boucle à la cadence
  actuelle ne feraient que re-dériver cette même conclusion — la boucle
  est ARRÊTÉE plutôt que de continuer à sonder sans nouvelle
  information. Voir
  `reports/ac6-retail-native-codegen-gate2-r144-both-named-frontiers-confirmed-blocked-maintenance-audits-clean-20260901.md`.

# AC6 retail NTSC-U/J — r143 : vérification coût-bénéfice — ferme le sous-fil DATA.TBL à sa profondeur actuelle, PIVOT vers la prochaine frontière Gate 2 (2026-09-01)

- **Vérification nommée par r142** : d'autres appelants de
  `sub_821F4128`/`sub_821F7538` (le wrapper d'attente kernel générique)
  lisent-ils AUSSI un slot de pile pareillement après l'appel (suggérant
  une vraie convention `IO_STATUS_BLOCK`) ? `sub_821F4128` a des
  appelants dans **DOUZE** fichiers générés différents — un wrapper
  d'attente GÉNÉRIQUE utilisé partout, pas quelque chose de spécifique à
  DATA.TBL. Un site d'appel échantillonné (`ppc_recomp.11.cpp:11858`)
  NE lit AUCUN slot de pile adjacent après l'attente — va DIRECTEMENT à
  un travail sans rapport. **Ceci est une preuve CONTRE la théorie
  `IO_STATUS_BLOCK`**, pas pour elle.
- **DÉCISION** : ce sous-fil (r130-r142, 13 cycles) a tracé un vrai
  crash depuis `sub_821F7C80` (SIGSEGV) à travers 11 mécanismes
  DISTINCTS, chacun MESURÉ EN DIRECT, jusqu'à UNE SEULE feuille :
  `sub_82338388` renvoie du contenu de pile non initialisé parce qu'une
  attente kernel expire et que rien ne remplit la valeur lue ensuite.
  Chaque maillon est étayé par une mesure en direct ; plusieurs
  conclusions intermédiaires de cette investigation ont été attrapées
  et corrigées dans le même cycle ou le suivant (r135 a attrapé sa
  propre erreur d'attribution de frontière de fonction en plein cycle,
  r138 et r140 ont chacun testé et réfuté une hypothèse spécifique en
  direct plutôt que d'accepter une lecture statique plausible, r141 a
  corrigé une prémisse structurante affirmée par r139). C'est un cas
  d'école complet et bien étayé selon les propres standards de ce
  projet.
- **Continuer plus loin dans CETTE SEULE feuille a des rendements
  fortement décroissants** : même une réponse concluante n'expliquerait
  QUE pourquoi la disposition de pile de CETTE recompilation diffère de
  celle du vrai matériel — une question sans technique établie pour y
  répondre sans oracle (explicitement hors périmètre de toute cette
  campagne) — et ne pointerait vers AUCUN correctif concret distinct de
  ce que r130-r131 ont déjà livré (le vrai gain livrable de tout cet
  arc : `NtCreateFile`/`NtReadFile` contre du média réel, et le
  correctif `maximum_size` XDVDFS).
- **Ce qui reste vrai et committé** : les correctifs de r130-r131
  (`NativeGuestMediaService`, `NtCreateFile`/`NtReadFile`, correctif
  XDVDFS) sont réels, testés, et ont RÉELLEMENT fermé le crash original
  de r100 — ce résultat tient indépendamment du reste de ce sous-fil.
  Le nouveau crash `sub_821F7C80` exposé depuis r131 est réel et
  reproductible, mais se termine maintenant dans un comportement de
  contenu de pile du code retail que ce projet ne peut pas résoudre
  davantage sans effort disproportionné ; laissé ouvert, entièrement
  documenté, plutôt que patché avec une supposition non vérifiée.
- **Aucun code source modifié ce cycle** — vérification statique pure
  (grep, lecture de fichiers) + décision. Gates inchangées depuis l'état
  propre et vérifié de r142.
  **Prochain cycle** : PIVOT vers le prochain élément ouvert de
  `NEXT.md` — la traduction `IM_LOAD_IMMEDIATE` Xenos→SPIR-V, la ligne
  de clôture encore ouverte du backlog. Lire son contexte existant
  avant de commencer une investigation fraîche. Voir
  `reports/ac6-retail-native-codegen-gate2-r143-cost-benefit-check-closes-the-data-tbl-subthread-pivoting-to-the-next-gate2-frontier-20260901.md`.

# AC6 retail NTSC-U/J — r142 : le `0` est une lecture de mémoire de PILE JAMAIS ÉCRITE, pas une vraie valeur — `sub_82338388` timeout et son "résultat" n'a jamais été rempli (2026-09-01)

- **Suivant l'instruction de r141** : lecture de `sub_821F7538` — une
  VRAIE boucle d'attente kernel (`NtWaitForSingleObjectEx`). Un premier
  diagnostic non filtré a produit >300 Mo de logs avant crash (ce
  chemin est un point de sondage CHAUD, pas rare comme supposé) — jeté
  SANS ÊTRE LU (discipline de l'instrument), puis resserré au SEUL
  appel pertinent : l'usage par `sub_82338388` du résultat de
  l'attente.
- **Mesuré en direct, décisif** :
  ```
  pre-wait: handle=0x12e pré-existant[pile+88..95]=0
  post-wait: sub_821F4128 renvoie=0x102 (STATUS_TIMEOUT!) [pile+88..95]=0 (INCHANGÉ)
  ```
  DEUX FAITS CERTAINS : (1) **l'attente EXPIRE RÉELLEMENT**
  (`STATUS_TIMEOUT=0x102`), ne réussit PAS ; (2) **`[r1+88]` n'est
  JAMAIS écrit** par cet appel, par notre stub HLE
  `NtWaitForSingleObjectEx`, ni par AUCUNE fonction tracée — sa valeur
  AVANT et APRÈS l'appel est IDENTIQUE dans les deux cas observés.
- **`sub_82338388` renvoie une lecture sign-extendue de `[r1+88]`** —
  de la MÉMOIRE DE PILE PÉRIMÉE, laissée par un appel ANTÉRIEUR sans
  rapport ayant occupé la même zone — PAS un compte, PAS une valeur de
  config, PAS un statut d'attente, PAS quoi que ce soit que
  `sub_82338388` ait calculé.
- **Forme suggestive** : `[r1+80]`/`[r1+88]` ressemble à un local de
  style `IO_STATUS_BLOCK` Windows (`{Status; Information;}`) qu'une
  convention de complétion asynchrone (APC, callback I/O réel)
  remplirait normalement — que nos stubs HLE synchrones ne modélisent
  pas. Plausible, PAS établi — aucun écrivain trouvé dans toute la
  chaîne tracée.
- **Aucun code source modifié ce cycle** — 2 diagnostics temporaires
  (le premier jeté sans lecture pour être disproportionnellement
  chaud, le second bien ciblé), annulés et vérifiés (ctest 9/9,
  139/139 Python après reconstruction propre).
  **Prochain cycle / évaluation coût-bénéfice** : chercher un
  écrivain de `[r1+88]` ailleurs dans le binaire (un autre appelant du
  même import kernel qui écrit à un offset de pile équivalent) ; si
  aucun n'existe nulle part, c'est un comportement de LECTURE DE PILE
  NON INITIALISÉE du jeu retail LUI-MÊME, et la question devient si le
  modèle de pile de ce projet produit un contenu résiduel DIFFÉRENT du
  vrai matériel Xbox 360 à ce point précis. **Étant donné la
  profondeur déjà atteinte (r129-r142) et la nature étroite de cette
  dernière question, une évaluation coût-bénéfice est justifiée avant
  de continuer CE sous-fil précis** — considérer si la chaîne DATA.TBL
  (déjà un cas d'école exceptionnellement complet de la discipline
  d'évidence de ce projet) a plus de valeur comme démonstration
  achevée que comme piste à pousser davantage, face à d'autres
  frontières Gate 2 potentiellement plus productives. Voir
  `reports/ac6-retail-native-codegen-gate2-r142-the-zero-is-a-read-of-stale-uninitialized-stack-memory-not-a-real-value-20260901.md`.

# AC6 retail NTSC-U/J — r141 : CORRIGE r139 — `sub_82338388` NE renvoie PAS le résultat de `sub_82339AA8` directement (2026-09-01)

- **Suivant l'instruction de r140** : instrumentation À L'INTÉRIEUR de
  `sub_82344058` — appelée EXACTEMENT 5 fois dans toute la session,
  renvoyant `1,2,3,4,5` en séquence, **JAMAIS 0**. Ceci contredit à lui
  seul l'affirmation de r139 selon laquelle le `0` observé transiterait
  par `sub_82339AA8 → sub_82344058`.
- **Mesure combinée décisive** (entrée/sortie de `sub_82339AA8` +
  `sub_82344058` + `sub_822834C0` dans LA MÊME session) : pour NOTRE
  requête exacte (`cat=1, réglage=3, idx=4, flags=0`, confirmée par les
  arguments) — `sub_82339AA8 RETURN=0x00000002` (RÉUSSIT, renvoie 2,
  RÉEL et POSITIF) — mais `sub_822834C0` rapporte ENSUITE
  `sub_82338388 returned=0x00000000`. **`sub_82338388` NE renvoie PAS
  le résultat de `sub_82339AA8` directement** — corrige r139.
- **Où la transformation a réellement lieu** : sur le chemin succès,
  `sub_82338388` appelle `sub_821F4128([r1+80], -1)` (PAS sur l'entier
  retourné, sur un TAMPON DE SORTIE que `sub_82339AA8` a rempli via son
  propre argument `r9`), puis lit `[r1+88]` (un AUTRE local 64-bit,
  sign-extend des 32 bits bas) comme vrai retour. Le `2` de
  `sub_82339AA8` (un ID de séquence interne) est ABANDONNÉ sur ce
  chemin.
- **Nouvelle piste principale (nommée, pas affirmée)** : le `0` pourrait
  être l'un des PROPRES PARAMÈTRES D'ENTRÉE de la requête (`p4=0` dans
  la trace) renvoyé en écho via ce tampon de sortie — auquel cas ce
  serait un résultat ENTIÈREMENT ATTENDU et CORRECT, pas un signe de
  ressource vide/non initialisée. Pas encore établi.
- **Corrige une PRÉMISSE structurante** du rapport de r139 (pas
  seulement resserre entre deux possibilités comme r138/r140) — la
  chaîne causale en 10 étapes reste correcte à CHAQUE maillon SAUF
  l'affirmation spécifique sur QUELLE valeur devient ce `0` et
  POURQUOI ; le `0` observable lui-même et tout ce qui en découle
  restent exacts.
- **Aucun code source modifié ce cycle** — 3 diagnostics temporaires,
  annulés et vérifiés (ctest 9/9, 139/139 Python après reconstruction
  propre).
  **Prochain cycle** : lire `sub_821F7538` (le vrai corps derrière
  `sub_821F4128`) — c'est LUI qui produit le `0` final. Identifier ce
  que `sub_82339AA8` écrit dans son tampon de sortie (`r9`/`r1+80` de
  l'appelant) pendant une requête réussie, en le reliant au layout de
  champs de `sub_823455D8` (déjà lu en r139) pour voir si le `0` est
  littéralement `p4=0` de la requête elle-même. Voir
  `reports/ac6-retail-native-codegen-gate2-r141-corrects-r139-sub_82338388-does-not-return-sub_82339aa8s-result-directly-20260901.md`.

# AC6 retail NTSC-U/J — r140 : l'init de la table config s'exécute RÉELLEMENT avant la requête défaillante — réfute l'hypothèse de timing, le mécanisme réel est plus profond (2026-09-01)

- **Hypothèse testée** (issue de r139) : lecture de `sub_82344058`
  (celle qui calcule le retour de `sub_82338388`) montre un COMPTEUR
  MONOTONE à `table+80`, retourné AVANT incrémentation comme "ID". Son
  sibling `sub_82344150(table)` l'initialise à `1` (pas 0). Hypothèse :
  si `sub_82344150(0x82910000)` n'a pas encore tourné au moment de la
  requête défaillante, le compteur resterait à sa valeur `.bss` par
  défaut (0), expliquant le `0` mesuré en r139.
- **RÉFUTÉE par mesure directe** : `sub_82338848` (init maître) ET
  `sub_82344150(0x82910000)` (fixe `[table+80]=1`) S'EXÉCUTENT BIEN,
  dans le bon ordre, AVANT la requête défaillante — ET la requête
  renvoie QUAND MÊME `0`. Écarte complètement "la table n'est jamais
  initialisée".
- **Le mécanisme réel est ailleurs** : `sub_82344058` fait D'ABORD une
  recherche dans un arbre binaire (BST) avant d'atteindre le chemin
  compteur (`loc_8234410C`) — le chemin emprunté dépend de si une
  entrée existe DÉJÀ pour cette clé. Soit un ID=0 a été stocké par une
  requête ANTÉRIEURE (avant que le compteur atteigne 1 — possible si
  cette requête précédente a eu lieu avant `sub_82344150`, ce que ce
  cycle n'a PAS vérifié), soit la lecture de ce cycle du chemin
  "trouvé" est simplement incorrecte — pas assez de preuves pour
  trancher.
- **Deuxième résultat négatif consécutif dans ce sous-fil** (après
  celui de r138) — conservé dans le dossier pour la même raison : une
  hypothèse plausible et bien raisonnée testée en direct et réfutée,
  ce qui RESSERRE où se trouve le vrai mécanisme sans encore le
  trouver.
- **Aucun code source modifié ce cycle** — 3 diagnostics temporaires,
  annulés et vérifiés (ctest 9/9, 139/139 Python après reconstruction
  propre).
  **Prochain cycle** : instrumenter directement L'INTÉRIEUR de
  `sub_82344058` pour distinguer le chemin "arbre vide/compteur frais"
  du chemin "nœud existant trouvé" ; si nœud existant trouvé, dumper
  DIRECTEMENT son champ ID stocké au lieu de l'inférer ; compter
  combien de fois `sub_82344058` tourne au total dans la session pour
  éviter le même type de trou de corrélation déjà attrapé en r135/r138.
  Voir
  `reports/ac6-retail-native-codegen-gate2-r140-the-config-table-init-genuinely-runs-first-refuting-a-timing-hypothesis-real-mechanism-is-deeper-20260901.md`.

# AC6 retail NTSC-U/J — r139 : CHAÎNE COMPLÈTE FERMÉE — une requête de compte renvoie LÉGITIMEMENT zéro et échoue une garde stricte `>0`, produisant la taille garbage EXACTE (2026-09-01)

- **Chaîne d'appel de l'appel #3 défaillant CONFIRMÉE** (compteur
  d'appels + backtrace, exactement l'exigence de r138) :
  `_xstart → sub_821D7DE0 → sub_821D5F48 → sub_821CC288 →
  sub_82222D80(size=0xfefffff9)` — ferme le trou de corrélation que r138
  avait laissé ouvert.
- **Correspondance EXACTE trouvée par calcul** : `sub_82339D10` (appelée
  via `sub_822834C0`→`sub_82338568`, PAS `sub_82338410` — corrige une
  petite erreur de lecture de ce cycle lui-même) exige `compte > 0` ;
  sinon renvoie `0xFEFF0000 | 65529 = 0xFEFFFFF9` — CORRESPONDANCE EXACTE,
  bit pour bit, avec la taille garbage de r137 (pas approximative comme
  la piste réfutée de r138).
- **VÉRIFIÉ EN DIRECT (pas seulement la correspondance statique)** :
  `sub_82338388(catégorie=1, réglage=3, index=4)` renvoie EXACTEMENT `0`
  — LÉGITIME sous son propre contrat (le pool interne, déjà confirmé
  peuplé et fonctionnel en r138, réussit RÉELLEMENT cette requête).
  `sub_822834C0` accepte `0` comme valide (contrôle `>=0`), mais
  `sub_82339D10` exige STRICTEMENT `>0` et rejette `0` avec l'erreur
  codée en dur.
- **CHAÎNE CAUSALE COMPLÈTE EN 10 ÉTAPES, CHAQUE MAILLON MESURÉ EN
  DIRECT** : requête catégorie=1/réglage=3 renvoie légitimement 0 →
  `sub_822834C0` accepte comme valide → `sub_82339D10` exige `>0`,
  rejette avec erreur codée en dur → propagée sans contrôle comme
  "taille" par 3 fonctions successives → `sub_821CC288` ne vérifie
  JAMAIS cette valeur comme code d'erreur → allocation ~4Go rejetée
  (NULL) → `sub_821CC288` ne vérifie PAS l'échec d'allocation → `8`
  stocké comme pointeur descripteur → taille de fichier lue comme 0 →
  `NtReadFile(length=0)` → tampon `DATA.TBL` jamais rempli →
  `sub_82234B88` lit du poison comme en-tête → pointeur sauvage →
  liste de notification corrompue → crash `sub_821F7C80`.
- **Ce n'est PAS de la corruption mémoire, PAS une lecture non
  initialisée, PAS un de nos stubs HLE** — c'est du VRAI code guest
  compilé passant une valeur légitimement zéro à un AUTRE vrai code
  guest avec un contrat différent, et un troisième morceau de code
  guest qui ne vérifie JAMAIS l'erreur qui en résulte.
- **Aucun code source modifié ce cycle** — 4 diagnostics temporaires,
  tous annulés et vérifiés (ctest 9/9, 139/139 Python après
  reconstruction propre).
  **Question finale restante (nommée, pas devinée)** : que représente
  sémantiquement catégorie=1/réglage=3/index=4 ? Est-ce un état RÉEL et
  CORRECT du jeu que cette investigation atteint pour la première fois
  (rien à corriger), ou un trou du runtime natif (une étape
  d'initialisation manquante) ? NE PAS ajouter de correctif défensif à
  `sub_821CC288`/`sub_822834C0`/`sub_82339D10` avant de répondre à
  cette question. Voir
  `reports/ac6-retail-native-codegen-gate2-r139-full-chain-closed-a-count-query-legitimately-returns-zero-and-fails-a-strict-positive-check-20260901.md`.

# AC6 retail NTSC-U/J — r138 : RÉSULTAT NÉGATIF — le chemin d'erreur de lookup config n'est PAS la source de la taille garbage (2026-09-01)

- **Hypothèse testée** (issue de r137) : lecture statique de
  `sub_82338388`→`sub_82339AA8`→`sub_82343F20(0x82910000)` — si CETTE
  dernière renvoie 0, `sub_82339AA8` renvoie une constante d'erreur codée
  en dur `(-16842752)|65528 = 0xFEFFFFF8`, à un bit de la taille garbage
  mesurée en r137 (`0xFEFFFFF9`) — piste plausible, prometteuse.
- **RÉFUTÉE par mesure directe** : `sub_82343F20` n'est PAS un simple
  test "fournisseur enregistré ?" comme d'abord supposé sur sa lecture
  partielle — c'est une opération "POP D'UN POOL" (liste chaînée libre à
  `object+1388`/`+1392`). Mesuré en direct sur TOUS les appels de la
  session : **5 appels, 5 succès** (compteur 16,16,15,14,13, chacun
  renvoie une adresse réelle et valide, jamais 0). **CE chemin d'erreur
  n'est JAMAIS emprunté dans cette session** — écarte cette piste
  spécifique comme source de la taille garbage.
- **Discipline appliquée à soi-même** : la lecture statique qui a motivé
  cette hypothèse était plausible mais fausse — exactement le genre de
  "règle plausible sans contrôle" que ce projet refuse sans vérification
  en direct, et la vérification la réfute.
- **Ce qui N'EST PAS établi** : si l'appel #3 (défaillant, r137) de
  `sub_822834C0` atteint même RÉELLEMENT `sub_82339AA8`/`sub_82343F20` —
  ce diagnostic comptait TOUS les appels de la session sans corréler
  aucun à l'appel spécifique défaillant (le même type d'erreur
  d'attribution que r135 avait auto-corrigée).
- **Aucun code source modifié ce cycle** — 2 diagnostics temporaires,
  annulés et vérifiés (ctest 9/9, 139/139 Python après reconstruction
  propre).
  **Prochain cycle** : établir D'ABORD la vraie chaîne d'appel de
  l'appel #3 défaillant (même technique de compteur d'appels que r135) ;
  SI c'est la bonne chaîne, lire `sub_823455D8` (chemin succès, jamais
  lu) ; SINON, remonter à `sub_82283530`/`sub_822836A8` (appelés dans
  `sub_82283728`, jamais lus). Voir
  `reports/ac6-retail-native-codegen-gate2-r138-negative-result-the-config-lookup-error-path-is-not-the-source-of-the-garbage-size-20260901.md`.

# AC6 retail NTSC-U/J — r137 : la taille demandée par l'allocation qui échoue est du GARBAGE (`0xFEFFFFF9`), PAS 16 octets — corrige r135/r136 (2026-09-01)

- **Corrige la propre hypothèse de travail de cette investigation** : r135
  a caractérisé l'appel comme "une allocation de 16 octets" en prenant au
  pied de la lettre l'immédiat `li r5,16` de `sub_821CC288`. Ce cycle
  trace directement l'usage des paramètres de `sub_82222D80` : **`r5` sert
  au helper `sub_82221C68`** (classe de taille), la VRAIE taille est
  **`r4`**, réglé depuis `r30` — le retour d'un appel, PAS le littéral 16.
- **4 allocations au total dans cette session** (pas une seule) — mesurées
  en direct : #1 classe=5 (LARGE, réussit `0x16f90200`), #2 classe=-1
  (SMALL, réussit `0x16fa0000`), **#3 classe=-1 (SMALL, ÉCHOUE →
  `0x00000000`)**, #4 classe=0 (LARGE, réussit `0x173a0020` — CONFIRME que
  le tampon `DATA.TBL` de r132/r133 EST correctement alloué ; son
  problème reste la lecture de longueur zéro, pas l'échec d'allocation).
- **L'appel #3 n'est PAS unique dans sa classe** (#2 prend le MÊME chemin
  `classe=-1` et réussit) — écarte "cette classe de taille échoue
  toujours".
- **Taille réellement demandée pour l'appel #3, mesurée en direct** :
  `0xFEFFFFF9` (soit `-16777223` en signé) — DU GARBAGE, PAS 16.
  `sub_822834C0` (censé calculer cette taille) N'OPÈRE PAS sur le tampon
  que `sub_82283728` a rempli — il appelle
  `sub_82338388`/`sub_82338568`/`sub_82338410` avec des arguments FIXES
  (1,3,4,0), forme plus cohérente avec une requête de config/état qu'un
  calcul de longueur de chaîne. **L'hypothèse "longueur de chaîne" de ce
  cycle lui-même n'est PAS vérifiée** — nommée explicitement pour ne pas
  être silencieusement portée au cycle suivant (comme "16 octets" l'a
  été).
- **Aucun code source modifié ce cycle** — 2 diagnostics temporaires,
  annulés et vérifiés (ctest 9/9, 139/139 Python après reconstruction
  propre).
  **Prochain cycle** : lire `sub_82283728` et
  `sub_82338388`/`sub_82338568`/`sub_82338410` EN ENTIER sans supposer
  leur sémantique ; comparer les tailles RÉELLES demandées par les appels
  #2 (réussit) et #3 (échoue) — même classe de taille, donc ce qui
  diffère EST l'argument de taille lui-même. Voir
  `reports/ac6-retail-native-codegen-gate2-r137-the-failing-allocation-request-size-is-garbage-not-16-bytes-corrects-r135-r136-20260901.md`.

# AC6 retail NTSC-U/J — r136 : la création du tas RÉUSSIT avec un vrai handle (`0x16F70000`) — l'échec est DANS la logique propre de l'allocateur, pas un tas manquant (2026-09-01)

- **Porte de création du tas mesurée en direct** : `sub_821D5F48` (même
  fonction de boucle de relance tracée depuis r117) garde la création du
  tas sur SON PROPRE arg1. Mesuré : `r31(own arg1)=0x16f70000` (RÉEL,
  non-nul) → `sub_82221DD0(pool=0x16f70000)` RENVOIE `0x16f70000`, stocké
  globalement. **La création du tas RÉUSSIT.** Écarte "le tas n'est
  jamais créé" (une des deux branches de la question ouverte de r135).
- **`0x16F70000` est dans l'espace d'adressage guest réservé** (4GiB
  complet, `GuestAddressSpace::kAddressSpaceSize`, `native_guest_memory.h`)
  — écarte aussi un problème de mapping du runtime natif pour CETTE
  adresse.
- **`sub_82221DD0(pool)` ne prend qu'UN SEUL argument** — pas de
  paramètre de TAILLE. Initialise un bloc de contrôle en place :
  plusieurs têtes de liste-libre à des offsets denses (4,8,12,...,62+),
  TOUTES mises à ZÉRO (vides), plus un pointeur style vtable à l'offset
  0. Aucune réservation/allocation de mémoire de support pour l'arène
  elle-même dans cette init.
- **Conclusion révisée** : puisque TOUTES les listes-libres démarrent
  vides et qu'aucune taille n'est enregistrée, TOUTE allocation
  (y compris nos 16 octets) doit emprunter un chemin "faire grossir le
  tas / committer plus de mémoire" — c'est LÀ, pas dans l'existence du
  tas, que l'échec se situe réellement.
- **Aucun code source modifié ce cycle** — 1 diagnostic temporaire,
  annulé et vérifié (ctest 9/9, 139/139 Python après reconstruction
  propre).
  **Prochain cycle** : lire `sub_82222D80` en entier (seul son
  dispatch de classe de taille a été lu en r135) et `sub_82222908` (la
  branche pour les grandes classes) pour trouver le vrai appel de
  croissance/commit — vérifier s'il atteint un import kernel HLE
  (un vrai trou de ce projet) ou dépend d'un état guest pas encore
  atteint par la sonde. Voir
  `reports/ac6-retail-native-codegen-gate2-r136-heap-creation-succeeds-with-a-real-handle-the-failure-is-inside-the-allocator-itself-20260901.md`.

# AC6 retail NTSC-U/J — r135 : CAUSE RACINE FERMÉE — une allocation de 16 octets sur le tas guest (`sub_82222D80`) renvoie NULL, jamais vérifiée, et se propage à travers 5 fonctions réelles jusqu'au crash (2026-09-01)

- **Corrige r134 sur deux points, dans ce MÊME cycle** : (1) le drapeau
  `0x8293B938` EST bien mis à 2 (non-zéro) — `0x8293B94C` EST bien écrit,
  ce n'est PAS un champ jamais rempli comme r134 le supposait ; (2) une
  première lecture erronée avait attribué le code de vérification du
  drapeau à `sub_821CC508` par proximité de ligne, sans vérifier la
  frontière `PPC_FUNC_IMPL` — un compteur d'appels en direct sur
  `sub_821CC508` (UN SEUL appel, `arg1=0x829ddd80`, réel et non-nul) a
  contredit la mesure précédente et révélé que le code appartient en
  réalité à `sub_821CC288`, une fonction DIFFÉRENTE. Erreur auto-corrigée
  dans le même cycle, avant tout rapport ou commit.
- **`sub_821CC288` ne prend PAS son propre argument dans r31`** — r31 y
  est RÉAFFECTÉ comme valeur de retour de `sub_82222D80` (un ALLOCATEUR
  DE TAS GUEST RÉEL et COMPILÉ, PAS un stub HLE). Mesuré en direct :
  `sub_82222D80(taille=16) renvoie r31=0x00000000` — **L'ALLOCATION
  ÉCHOUE (NULL), et n'est JAMAIS VÉRIFIÉE** avant que `r31+8=8` soit
  stocké comme pointeur de descripteur dans `0x8293B94C`.
- **CHAÎNE CAUSALE COMPLÈTE, CHAQUE MAILLON MESURÉ EN DIRECT** :
  allocation 16 octets échoue (NULL, non vérifiée) → `8` stocké comme
  "pointeur record" → `sub_821CC508` lit `[8+8]=[0x10]` (mémoire
  quasi-nulle) comme taille de fichier → `0` → calcul de taille de chunk
  donne `0` → `NtReadFile(length=0)` → tampon `DATA.TBL` jamais rempli →
  `sub_82234B88` lit des octets de poison comme un vrai en-tête →
  pointeur sauvage → boucle d'échange d'octets écrase la liste de
  notification → `sub_821F7C80` (crash de r131) marche sur la sentinelle
  corrompue → SIGSEGV. **C'est le point le plus profond atteint par cette
  investigation** — un vrai allocateur de tas du JEU LUI-MÊME (code
  compilé réel, pas un de nos stubs) qui échoue.
- **Question ouverte (nommée, pas devinée)** : pourquoi cette allocation
  de 16 octets échoue-t-elle ? Le tas backing cet allocateur est-il
  jamais initialisé par ce runtime natif (une étape que le vrai matériel
  effectuerait plus tôt) ? OU cette investigation atteint-elle
  simplement, pour la première fois, un état RÉEL et CORRECT du jeu (tas
  vide/épuisé par conception à ce stade) — pas un bug à corriger du
  tout ?
- **Aucun code source modifié ce cycle** — 7 diagnostics temporaires,
  tous annulés et vérifiés (ctest 9/9, 139/139 Python après
  reconstruction propre).
  **Prochain cycle** : lire `sub_82222D80` et son appelé
  `sub_82221C68` (recherche de classe de taille) en entier ; tracer
  l'argument "objet tas" jusqu'à son initialisation ; déterminer si ce
  runtime natif doit préparer ce tas plus tôt. NE PAS ajouter de
  vérification défensive à `sub_821CC288` — ce serait patcher un
  symptôme dans du code que ce projet ne possède pas. Voir
  `reports/ac6-retail-native-codegen-gate2-r135-root-cause-closed-a-16-byte-guest-heap-allocation-returns-null-unchecked-20260901.md`.

# AC6 retail NTSC-U/J — r134 : la lecture de longueur zéro tracée jusqu'à un store conditionnel (booléen) JAMAIS pris — pas un import manquant, corrige r133 (2026-09-01)

- **Chaîne d'appel** (`addr2line` sur une capture `backtrace()`) :
  `_xstart → sub_821D7DE0 → sub_821D5F48 → sub_821CC508 → sub_821F4E70 →
  NtReadFile`. `sub_821F4E70` (déjà nommée r124) transmet son ARG3
  (`ctx.r9`, inchangé) comme `Length` de `NtReadFile` — fourni par
  `sub_821CC508`.
- **Calcul de taille de chunk mesuré en direct** : `r30(record)=0x00000008`
  — PAS une vraie adresse guest, une valeur quasi-nulle !
  `filesize_field[r30+8]=0` (lecture près de l'adresse zéro),
  `chunk_cap=262144`, résultat `r27=0`. **Ce n'est PAS un import de taille
  manquant** (hypothèse de r133, maintenant CORRIGÉE) — c'est le POINTEUR
  utilisé pour chercher la taille qui est quasi-nul.
- **`r30` calculé via `[r26-18100]`, où `r26=0x82940000`** (MÊME base que
  la table de r129). `r26-18100 = 0x8293B94C` — EXACTEMENT 16 octets
  après `0x8293B93C` (la table nom-de-fichier/handle de r129) !
- **Un SEUL écrivain trouvé** (même technique de grep que r129) — DANS
  `sub_821CC508` lui-même : un store CONDITIONNEL, gardé par un octet
  drapeau à `0x8293B938` (4 octets AVANT la table de r129). Si le drapeau
  est NON-ZÉRO → écrit `0x8293B94C` (le champ dont on a besoin). Si ZÉRO
  (état actuel observé) → branche ALTERNATIVE qui remplit 4 AUTRES champs
  (`0x8293B950/54/58/5C`) mais JAMAIS `0x8293B94C`.
- **Ce n'est pas un import manquant — c'est du code DÉJÀ exécuté prenant
  la mauvaise branche.** Corrige explicitement la propre hypothèse de r133
  (import `NtQueryInformationFile` manquant), par discipline de correction
  du prédécesseur immédiat.
- **Aucun code source modifié ce cycle** — 3 diagnostics temporaires,
  tous annulés et vérifiés (ctest 9/9, 139/139 Python après reconstruction
  propre).
  **Prochain cycle** : trouver ce que représente le drapeau `0x8293B938`
  (grep des stores vers `-18120(r26)`, même technique) ; déterminer si son
  état actuel (zéro) est correct pour ce point d'exécution ou si c'est un
  vrai trou HLE ; si le drapeau=0 est l'état normal, les champs
  `0x8293B950-5C` (que CETTE branche remplit) sont peut-être les vrais
  champs à lire, pas `0x8293B94C`. Voir
  `reports/ac6-retail-native-codegen-gate2-r134-zero-length-read-traced-to-a-boolean-gated-store-that-never-populates-a-descriptor-field-20260901.md`.

# AC6 retail NTSC-U/J — r133 : L'ÉCRIVAIN EST TROUVÉ — `sub_82234B88` parse un tampon `DATA.TBL` que `NtReadFile` n'a JAMAIS rempli (`length=0`), lit des octets de poison comme un vrai en-tête (2026-09-01)

- **Méthode** : au lieu de deviner quelle fonction instrumenter, un WATCH
  GLOBAL a été ajouté dans `PPC_STORE_U8/U16/U32/U64` (macros partagées,
  `generated/ppc_context.h`, UN SEUL fichier, tous les sites d'appel du
  codebase passent par là) — imprime toute écriture dans
  `[0x823F0C30, 0x823F0C60)`. Évite de refaire les watchpoints GDB peu
  fiables (r128).
- **Écrivain trouvé** : 12 écritures `STORE_U32` séquentielles décalées de
  4 octets, base `0x823F0C32`. Les octets combinés à `0x823F0C4C-0x4F`
  donnent EXACTEMENT `0x00009182` — la valeur corrompue observée en r132.
  Pile d'appels (`addr2line`) : `_xstart → sub_821D7DE0 → sub_821D5F48 →
  sub_821CC508 → sub_82234B88`. Les deux fonctions du milieu sont EXACTEMENT
  la boucle de relance déjà tracée depuis r117.
- **`sub_82234B88`** (lu directement dans le C++ généré) : parse un
  enregistrement depuis `r4` (source) vers `r3` (dest), calcule des bases
  de tableaux à partir d'un champ 16-bit lu dans l'EN-TÊTE de `r4`, puis
  échange les octets en place sur 4 tableaux parallèles — SANS JAMAIS
  référencer `0x823F0C30` littéralement. La corruption dépend ENTIÈREMENT
  du contenu de `r4`.
- **Mesuré en direct** : `r4` (source de `sub_82234B88`) = `0x173a0020`,
  EXACTEMENT l'adresse cible du SEUL appel `NtReadFile` de la session, qui
  demandait `length=0` et a copié `bytes_read=0`. Les octets d'en-tête lus
  (`fe fe fe fe`) sont un motif de poison/mémoire non initialisée
  classique — PAS du contenu réel de `DATA.TBL`.
- **CHAÎNE CAUSALE COMPLÈTE** : lecture de longueur ZÉRO → tampon jamais
  rempli → octets de poison lus comme un vrai champ de stride → pointeur
  de base de tableau SAUVAGE → boucle d'échange d'octets écrase la section
  critique + liste de notification à deux fonctions de distance →
  `sub_821F7C80` (crash de r131) marche sur la sentinelle corrompue →
  SIGSEGV. Déterministe, pas une course.
- **Question restante (nommée, pas devinée)** : pourquoi le jeu demande-t-il
  une lecture de longueur ZÉRO ? `NtCreateFile` de ce projet ne renvoie
  qu'un handle + statut, jamais une taille — un titre réel apprend la
  taille via `NtQueryInformationFile`/`GetFileSizeEx`/un champ IoStatusBlock,
  AUCUN implémenté ici. Hypothèse la plus probable : le mécanisme de
  requête de taille du jeu renvoie 0 (non implémenté), et le jeu demande
  alors exactement ça.
- **Aucun code source modifié ce cycle** — trois instrumentations
  temporaires, toutes annulées et vérifiées annulées (ctest 9/9, 139/139
  Python après une reconstruction PROPRE complète).
  **Prochain cycle** : tracer l'appel entre `NtCreateFile("DATA.TBL")` et
  la lecture de longueur zéro pour trouver QUI détermine la longueur
  demandée ; implémenter le vrai mécanisme de taille (déjà connu du
  runtime via `NativeGuestMediaService`/`locate_xdvdfs_file`, juste pas
  exposé par le bon import) ; relancer la sonde et vérifier EN DIRECT que
  le crash `sub_821F7C80` disparaît. Voir
  `reports/ac6-retail-native-codegen-gate2-r133-writer-found-sub_82234b88-parses-a-zero-length-ntreadfile-buffer-as-a-real-header-20260901.md`.

# AC6 retail NTSC-U/J — r132 : le crash `sub_821F7C80` est une liste de notification CORROMPUE — `NtReadFile` est EXCLU comme cause (mesuré, pas supposé) (2026-09-01)

- **Lecture statique de `sub_821F7C80`** (0x821F7C80-0x821F7CDC, convention
  `__savegprlr`/`__restgprlr` standard MSVC Xbox 360, confirmée par
  symétrie avec `sub_821F7CE0` juste après) : c'est une **diffusion de
  notification** — sous `RtlEnterCriticalSection`/`RtlLeaveCriticalSection`
  (objet `0x823F0C30`), parcourt une liste doublement chaînée intrusive
  (sentinelle `0x823F0C4C`), appelle `*(node+8)` pour chaque nœud.
  `sub_821F7CE0` est la fonction d'insertion/retrait correspondante —
  **UN SEUL site d'appel dans tout le code** (`sub_82383740`), enregistrant
  UN nœud fixe (`0x82915FD8`, pointeur de fonction réel `0x82389BF8`).
  `DumpBytes.java` confirme que `0x823F0C4C` est CORRECTEMENT
  auto-référencé dans l'image XEX statique (init de données, pas de code
  requis) — écarte "jamais initialisé".
- **Mesure en direct** (diagnostic temporaire `AC6_R132_DIAG`, ajouté puis
  ENTIÈREMENT annulé — `grep -c "r132"` retourne 0, ctest 9/9, 139/139
  Python après) : la diffusion réussit proprement ~17 fois de suite
  (sentinelle et nœud corrects), PUIS le dernier appel avant le crash lit
  `head=0x00009182` — NI la sentinelle NI le nœud connu. **La mémoire de la
  sentinelle a été écrasée par une valeur de garbage entre deux appels.**
  `0x9182` correspond EXACTEMENT au registre `rcx` capturé au fault de
  r131. Le changement d'argument (1→0) sur l'appel qui crashe est normal
  (site d'appel légitime distinct dans `sub_82390B18`, `li r3,0x0` visible
  en désassemblage) — PAS un symptôme.
- **`NtReadFile` EXCLU comme écrivain, par mesure directe, pas par
  supposition** : un second diagnostic temporaire trace CHAQUE appel
  `NtReadFile`. Il n'y en a qu'UN SEUL avant le crash, avec `length=0` —
  ZÉRO octet copié, donc AUCUN écrasement possible par ce chemin. Écarte
  définitivement le nouveau code de r130/r131 comme cause.
- **Aucun code source modifié ce cycle** — deux instrumentations
  temporaires, prises, puis annulées et vérifiées annulées.
  **Prochain cycle** : trouver le VRAI écrivain de `0x823F0C4C` (les
  watchpoints GDB sont déjà connus peu fiables sur cette sonde à 18
  threads réels, r128 — ne pas réessayer sans nouvel argument). Approche
  proposée : bissection par snapshots d'entrée/sortie supplémentaires
  dans les fonctions candidates entre le dernier appel sain et celui qui
  crashe. Voir
  `reports/ac6-retail-native-codegen-gate2-r132-sub_821f7c80-crash-is-a-corrupted-notification-list-ntreadfile-ruled-out-as-cause-20260901.md`.

# AC6 retail NTSC-U/J — r131 : le crash original de r100 (`sub_821D6C20`) est CONFIRMÉ DISPARU — nouveau crash déterministe dans `sub_821F7C80` (2026-09-01)

- **Cause réelle du "not found" de r130** : ce n'était PAS un bug de
  recherche dans l'arbre XDVDFS (un diagnostic autonome confirme les 13
  entrées racine trouvées correctement, `DATA00.PAC`/`DATA01.PAC`/
  `DATA.TBL` inclus). C'est `read_xdvdfs_file()` elle-même qui rejette
  tout appel où `maximum_size > 16MiB` — un contrôle sur le PARAMÈTRE du
  CALLER, pas sur la taille réelle du fichier. `NativeGuestMediaService`
  (r130) passait `512MiB`, donc CHAQUE appel échouait, y compris pour
  `DATA.TBL` (14 824 octets).
- **Élever la constante ne suffit pas** : `DATA00.PAC` fait ~2,1GiB et
  `DATA01.PAC` ~633MiB — charger ça entièrement en mémoire à chaque
  `NtCreateFile` est la mauvaise forme, indépendamment du plafond.
- **Correctif** : nouvelle `locate_xdvdfs_file()` (mêmes validations que
  `read_xdvdfs_file`, factorisées, mais SANS copie ni plafond de
  taille — elle ne fait que résoudre offset/taille).
  `NativeGuestMediaService` l'utilise en mode ISO et STREAME chaque
  lecture directement depuis le fichier ISO à la demande, au lieu de
  précharger tout le fichier. `read_xdvdfs_file` elle-même est
  INCHANGÉE dans son contrat (mêmes 3 tests existants passent tels
  quels).
- **Vérifié en direct contre l'ISO qualifié** : les trois
  `NtCreateFile("DATA00.PAC"/"DATA01.PAC"/"DATA.TBL")` réussissent
  maintenant (`-> ok`), alors que r130 rapportait `-> not found` pour
  les trois.
- **LE CRASH ORIGINAL DE r100 (`sub_821D6C20`) NE SE REPRODUIT PLUS** —
  première fois dans tout l'arc r100-r131 que ce site précis n'apparaît
  pas. La sonde progresse bien plus loin (plus de threads, plus
  d'imports Vd).
- **Nouveau crash déterministe** (reproduit identique sur 2 runs `gdb`
  indépendants) : `sub_821F7C80` <- `sub_82390B18` <- `sub_821F8008`
  <- routine du thread `ExCreateThread` (`0x821eede0`). Registres à la
  faute : `rbp=0`, `rdx=0`, `r13=0`, `r15=0` — forme de déréférencement
  de pointeur nul, mécanisme PAS ENCORE établi par désassemblage.
  **Prochain cycle** : désassembler `sub_821F7C80` (vérifier
  complétude via `.pdata` d'abord) pour localiser le champ nul exact.
  Voir `reports/ac6-retail-native-codegen-gate2-r131-xdvdfs-maximum-size-parameter-was-rejecting-every-open-r100s-original-crash-site-is-confirmed-gone-20260901.md`.

# AC6 retail NTSC-U/J — r130 : `NtCreateFile`/`NtReadFile` implémentés contre `NativeGuestMediaService`, appelés en direct avec les vrais noms de fichiers — la recherche XDVDFS échoue encore (cause trouvée et corrigée en r131) (2026-09-01)

- Nouveau service singleton `NativeGuestMediaService`
  (`native_guest_media.h`/`.cpp`, suit le patron existant de
  `native_guest_vd_service()`) : `bind()`, `open_file()`, `read_file()`,
  `close_file()`. Enregistré dans `NativeRuntime::boot()`.
- `NtCreateFile`/`NtReadFile` implémentés dans
  `tools/materialize_native_import_stubs.py` contre la convention
  d'appel et le layout `OBJECT_ATTRIBUTES` déjà confirmés octet par
  octet en r122/r123 (pas re-dérivés). `NtReadFile` complète
  TOUJOURS de façon SYNCHRONE (jamais `STATUS_PENDING`), par
  contrainte nommée en r125/r126 : un stub pending inconditionnel
  transformerait ce crash en boucle infinie.
- **Première vérification en direct de toute cette investigation
  contre le VRAI ISO qualifié** (r105-r129 n'avaient testé que
  `assets/`, qui ne contient que `default.xex`). Trace confirmée :
  `NtCreateFile` est appelé avec EXACTEMENT les noms de fichiers prédits
  par r129 (`DATA00.PAC`, `DATA01.PAC`, `DATA.TBL`) — mais les trois
  rapportent `-> not found` ce cycle (cause diagnostiquée et corrigée
  en r131, voir ci-dessus).
- 139/139 tests Python (était 136/136), 9/9 `ctest`. Voir
  `reports/ac6-retail-native-codegen-gate2-r130-ntcreatefile-ntreadfile-implemented-and-called-with-real-filenames-xdvdfs-lookup-fails-20260901.md`.

# AC6 retail NTSC-U/J — r129 : l'écrivain trouvé — un VRAI nom de fichier `game:\DATA00.PAC`, et ce fichier existe RÉELLEMENT sur l'ISO retail déjà qualifié de ce projet (2026-09-01)

- **Meilleur instrument** : un script Python précis (recherche de la
  séquence exacte `lis(-2104229888)` + `addi(-18116)` sur TOUS les
  fichiers générés) trouve 3 correspondances — 2 dans `sub_821CC508`
  (déjà connu, lecteur) et **1 dans `sub_821CC370`**, une fonction
  différente que l'outil Ghidra général (r128) avait manquée.
- **`sub_821CC370(type, value)` = `table[type] = value`** — L'ÉCRIVAIN.
  Ses SEULS deux appelants sont dans `Function_821D5F48` elle-même,
  TRÈS TÔT (avant GATE1/GATE2) : `sub_821CC370(0, 0x82067d40)` puis
  `sub_821CC370(1, 0x82067d54)`.
- **`0x82067d40` est un VRAI NOM DE FICHIER, PAS un handle** :
  `DumpBytes.java` révèle `"game:\DATA00.PAC"` (0x82067d54 =
  `"game:\DATA01..."`). La table démarre avec des pointeurs de CHAÎNES
  DE NOM DE FICHIER, un par "type" — du code intermédiaire (pas encore
  localisé) doit lire cette chaîne, appeler `NtCreateFile`, et
  remplacer le slot par un VRAI handle ou `INVALID_HANDLE_VALUE` en
  cas d'échec — expliquant PARFAITEMENT la capture de r127
  (`0xFFFFFFFF`, pas la chaîne d'origine).
- **LE FICHIER EXISTE RÉELLEMENT** — TOUTE cette investigation
  (r105-r128) a fait tourner la sonde contre `assets/` (contient
  UNIQUEMENT `default.xex`, confirmé par listing direct) — jamais
  contre le VRAI média. `targets/ntsc-uj.json` cite déjà un ISO
  qualifié par SHA-256 (`204c5e6...`); ce fichier EXISTE à la racine
  du workspace, `sha256sum` confirme une correspondance EXACTE. Un
  `strings` brut sur cet ISO trouve LITTÉRALEMENT `DATA00.PAC` ET
  `DATA01.PAC`. **CE N'EST PAS un blocage de contenu manquant** — le
  contenu existe, sur un média DÉJÀ qualifié par ce projet, simplement
  jamais pointé par cette investigation.
- **Ferme la question pratique de r122/r123/r127** (y a-t-il du vrai
  contenu à lire?) — OUI. Le correctif est maintenant entièrement
  déterminé en FORME (même s'il reste à implémenter) :
  `NtCreateFile` doit traduire les chemins `game:\` via
  `read_xdvdfs_file` existant quand le runtime démarre contre un ISO
  (jamais fait par cette sonde jusqu'ici); `NtReadFile` doit servir de
  VRAIS octets. Aucun code natif modifié (analyse statique pure +
  vérifications de hash/strings hors de l'arbre suivi par git).
  **Prochain cycle** : implémenter `NtCreateFile`/`NtReadFile` contre
  `read_xdvdfs_file`, PUIS relancer la sonde contre l'ISO qualifié (pas
  `assets/`) — nouveau prérequis établi ce cycle. Voir
  `reports/ac6-retail-native-codegen-gate2-r129-writer-found-real-content-exists-the-probe-just-never-used-the-qualified-iso-20260901.md`.

# AC6 retail NTSC-U/J — r128 : les watchpoints GDB sont AUSSI peu fiables sur cette sonde (étend r104) — une piste de première tentative RÉTRACTÉE (2026-09-01)

- **Deux techniques statiques épuisées, toutes deux négatives** :
  `FindPpcAddressMaterialization.java` (read-only) ne trouve AUCUNE
  paire lis+addi/ori construisant `0x8293b93c`; grep exhaustif de
  TOUS les stores utilisant le déplacement `-18116` dans tout le code
  généré trouve exactement deux stores directs, mais tous deux avec
  une base DIFFÉRENTE (`-32099`, pas `-32108`) — ni l'un ni l'autre
  n'écrit notre adresse cible. `DumpBytes.java` confirme aussi que
  l'image statique du XEX à cette adresse est ZÉRO, pas `0xFFFFFFFF` —
  écarte l'hypothèse d'un initialiseur statique.
- **Nouvelle technique tentée : un WATCHPOINT GDB** (passif, pose puis
  `continue` — PAS le pas-à-pas interactif déjà retiré par r104).
  **Première tentative** : déclenché avec `Old value=0xFFFFFFFF` et une
  pile d'appels traversant `sub_82346428` — la fonction même du crash
  de thread d'arrière-plan investigué en r111-r116. Ressemblait à une
  connexion réelle, excitante.
- **RÉTRACTÉ après vérification de reproductibilité** (discipline
  "mesurer l'instrument" de CLAUDE.md) : deux répétitions IDENTIQUES
  déclenchent avec des piles d'appels COMPLÈTEMENT DIFFÉRENTES et
  MUTUELLEMENT INCOHÉRENTES (`wait_event`/`NtSignalAndWaitForSingleObjectEx`,
  puis un futex/condition-variable brut sans frame invité) —
  `New value=<unreadable>` à chaque fois. **Ceci N'EST PAS un vrai
  événement d'écriture capturé trois fois à trois endroits différents
  — c'est GDB produisant des déclenchements FANTÔMES** dans cette
  sonde fortement multi-threadée (18 threads).
- **Étend la découverte de r104** (le pas-à-pas interactif est peu
  fiable) à une DEUXIÈME fonctionnalité GDB : les watchpoints AUSSI.
  La piste `sub_82346428` de la première tentative est explicitement
  RÉTRACTÉE — pas confirmée, pas un résultat établi. L'écrivain de
  `0x8293b93c` reste NON IDENTIFIÉ. Aucun code natif modifié. **Prochain
  cycle** : NE PAS utiliser de watchpoints GDB sur cette sonde sans
  confirmation indépendante; utiliser l'instrumentation build-tree
  fiable (technique validée depuis r105) sur `sub_82346428` (candidat
  encore à vérifier proprement) et le cluster `Function_82390F48`. Voir
  `reports/ac6-retail-native-codegen-gate2-r128-gdb-watchpoints-are-also-unreliable-on-this-probe-a-false-lead-retracted-20260901.md`.

# AC6 retail NTSC-U/J — r127 : le handle de fichier de `sub_821F4E70` est TOUJOURS `INVALID_HANDLE_VALUE` — `NtCreateFile` n'a jamais produit de vrai handle (2026-09-01)

- **Une seule exécution instrumentée** (crash déterministe) sur les
  deux sites d'appel de `sub_821F4E70` dans `sub_821CC508` : le handle
  n'est PAS passé directement — il vient d'une PETITE TABLE GLOBALE,
  indexée par un octet "type" sur l'objet d'état de l'appelant.
- **`type_byte=0`, `table_index=0`, `handle=0xFFFFFFFF`** (×6, décompte
  exact de r121). **`0xFFFFFFFF` EST `INVALID_HANDLE_VALUE`** — le
  sentinelle standard Win32/NT écrit quand la création d'un handle
  échoue. **Décisif à lui seul** : `sub_821F4E70` appelle `NtReadFile`
  avec un handle INVALIDE à CHAQUE tentative — aucune implémentation
  correcte de la sémantique pending/complétion de `NtReadFile` ne peut
  produire une vraie lecture contre un handle qui n'a jamais été
  valide.
- **Confirmé comme une VRAIE écriture délibérée**, pas un zero-fill :
  `GuestAddressSpace` garantit un remplissage à zéro au premier
  contact — `0xFFFFFFFF` (tous bits à 1) ne peut PAS résulter d'une
  adresse jamais touchée. Du code invité a authentiquement écrit
  `INVALID_HANDLE_VALUE` ici, très probablement après l'échec d'un
  `NtCreateFile` antérieur (stub générique, r121) qui n'a jamais écrit
  de vrai handle à son paramètre de sortie.
  `FindPpcAddressMaterialization.java` (read-only) ne trouve AUCUNE
  paire lis+addi construisant cette adresse ailleurs — l'écrivain
  utilise probablement le MÊME registre de base `r24`-style que ce
  cycle a vu réutilisé partout, matérialisé indépendamment dans une
  autre fonction. PAS ENCORE localisé.
- **Reformule/affine le prochain correctif** : le problème n'est pas
  vraiment la sémantique pending de `NtReadFile` — il est EN AMONT, au
  `NtCreateFile` censé peupler l'entrée 0 de la table à `0x8293b93c`.
  Même un `NtReadFile` parfaitement conçu échouerait immédiatement
  avec un handle invalide. Aucun code natif modifié (instrumentation
  restaurée; `ctest` 9/9 reconfirmé). **Prochain cycle** : localiser
  l'écrivain de `0x8293b93c` — est-ce le cluster `Function_82390F48`
  de r122/r123 (partition disque dur), ou un troisième site d'appel
  `NtCreateFile` non encore trouvé? Voir
  `reports/ac6-retail-native-codegen-gate2-r127-the-file-handle-is-invalid-handle-value-ntcreatefile-never-produced-a-real-one-20260901.md`.

# AC6 retail NTSC-U/J — r126 : `RtlNtStatusToDosError` implémenté et vérifié en direct — NÉCESSAIRE mais PAS suffisant seul, exactement comme prédit par r125 (2026-09-01)

- **Implémenté la première moitié du correctif en deux parties de r125.**
  `STATUS_SUCCESS(0)`→`ERROR_SUCCESS(0)`, `STATUS_PENDING(0x103)`→
  `ERROR_IO_PENDING(997)` (les deux valeurs directement tracées depuis
  r109), défaut pour le reste = `ERROR_MR_MID_NOT_FOUND(317)` — le VRAI
  défaut documenté de Windows NT, pas une valeur devinée. Cas par
  défaut tracé via `AC6_NATIVE_IMPORT_TRACE`. 1 nouveau test, suite
  136/136 (134/134 avant).
- **Vérifié en direct — fonctionne exactement comme conçu, ET comme
  prédit** : rejoué avec trace activée — les 43 appels (décompte EXACT
  de r121) convertissent `0xc00000bb` (`kOfflineStatus`, toujours
  depuis le stub `NtReadFile` non implémenté) et retombent
  correctement sur `317` au lieu de le laisser passer inchangé. **Comme
  r125 l'avait explicitement prédit, ceci seul n'arrête PAS le
  crash** — confirmé via capture `gdb --batch` directe : site de crash
  INCHANGÉ, identique à chaque cycle depuis r116 (`sub_821D6C20`).
- Infrastructure réelle, indépendamment justifiée, sans risque de
  masquer quoi que ce soit (les statuts non mappés retombent sur un
  vrai défaut documenté et tracé, pas un sentinelle interne silencieux).
  `NtReadFile` reste délibérément non implémenté — la contrainte de
  conception de r125 (PENDING indéfiniment = boucle infinie) et
  l'incertitude de r122/r123 (quel fichier, le cas échéant) restent à
  résoudre avant d'implémenter sans précipitation.
- Gates : `ctest` 9/9, audit mission01 échoue sur le même échec
  préexistant sans rapport, compteur démo inchangé (185), pytest
  136/136. **Prochain cycle** : tracer l'origine du handle de fichier
  de `sub_821F4E70` (même cluster que r122/r123, ou site séparé?) avant
  de concevoir le correctif `NtReadFile`. Voir
  `reports/ac6-retail-native-codegen-gate2-r126-rtlntstatustodoserror-implemented-verified-live-necessary-not-sufficient-20260901.md`.

# AC6 retail NTSC-U/J — r125 : CHAÎNE COMPLÈTE FERMÉE — deux imports non implémentés (`NtReadFile` ET `RtlNtStatusToDosError`) produisent ensemble la valeur de statut que la boucle ne peut jamais accepter (2026-09-01)

- **`loc_821F4FE4` tracé** : appelle `sub_821F75B8(ctx.r3=statut NTSTATUS
  échoué)` puis retourne 0. **`sub_821F75B8` EST le setter EXACT
  correspondant au getter déjà tracé par r117/r119** (`sub_821F75F0`,
  voisin immédiat en adresse) — écrit dans `[[ctx.r13+256]+352]`,
  l'adresse EXACTE que r119 a instrumentée en direct et que r121 a
  trouvée contenant `0xC00000BB`.
- **Avant d'écrire, appelle `__imp__RtlNtStatusToDosError`** — l'import
  non implémenté LE PLUS appelé de toute la trace (43 appels, r121) —
  qui retombe dans le MÊME stub générique, retournant `kOfflineStatus`
  quel que soit son entrée.
- **`RtlNtStatusToDosError` est une VRAIE API Windows NT documentée** :
  convertit un NTSTATUS en code d'erreur Win32. `STATUS_PENDING=0x103`
  se convertit en `ERROR_IO_PENDING=997` — **LA constante `997`
  poursuivie depuis r109 a maintenant une source confirmée** : ce n'est
  PAS un NTSTATUS direct, c'est la forme CONVERTIE de STATUS_PENDING.
- **Chaîne complète, bout en bout** : vrai matériel — `NtReadFile`→
  STATUS_PENDING(0x103)→`RtlNtStatusToDosError`→ERROR_IO_PENDING(997)→
  champ de statut→boucle reconnaît 997 comme "en cours, pas un échec"→
  continue de sonder jusqu'à complétion réelle. CE harnais —
  `NtReadFile`→kOfflineStatus(0xC00000BB, pas un vrai NTSTATUS)→
  `RtlNtStatusToDosError`→MÊME 0xC00000BB inchangé→champ de statut→
  boucle ne reconnaît RIEN (ni 0, ni 997, ni r30==1)→compteur épuisé→
  abandon -1→`sub_821D5F48` sort tôt (r117)→`0x82935d98` jamais écrit→
  `sub_821D6C20` déréférence le nul→crash de r100.
- **Ni l'un ni l'autre import seul ne suffirait** : `RtlNtStatusToDosError`
  correct n'a rien de réel à convertir sans un vrai STATUS_PENDING de
  `NtReadFile`; `NtReadFile` retournant STATUS_PENDING indéfiniment
  transformerait le crash en boucle INFINIE (le compteur ne décrémente
  que sur un échec RECONNU, pas sur "en attente") — pas encore un
  correctif fonctionnel. Aucun code natif modifié. **Prochain cycle** :
  implémenter `RtlNtStatusToDosError` (petite table sans état,
  faible risque); concevoir `NtReadFile` pour que la lecture se
  termine RÉELLEMENT (pas juste "en attente" pour toujours) — mérite
  son propre cycle dédié avec tests (façon r108/r116). Voir
  `reports/ac6-retail-native-codegen-gate2-r125-entire-chain-closed-two-unimplemented-imports-ntreadfile-and-rtlntstatustodoserror-20260901.md`.

# AC6 retail NTSC-U/J — r124 : connexion confirmée EN DIRECT — `sub_821F4E70` dispatche directement vers `NtReadFile`, et son appelant attend explicitement `STATUS_PENDING` comme issue normale (2026-09-01)

- **Instrumentation d'UNE seule exécution** (crash déterministe depuis
  r116) sur le site `bctrl` de `sub_821F4E70` : `dispatch
  target=0x823d035c` (×6, correspond EXACTEMENT au décompte de 6 appels
  `NtReadFile` de r121). **`0x823d035c` EST l'adresse d'import de
  `NtReadFile`** — connexion établie EN DIRECT, pas par inférence
  statique. Ce n'est PAS le cluster `Function_82390F48`/`Function_82391A40`
  de r122/r123 — un site d'appel séparé, plus central.
- **Mapping complet des registres confirmé**, correspondant exactement
  à la signature réelle de `NtReadFile` (r122) : `r3=Handle(=r30),
  r4=[r31+16](Event), r5=0, r6=0/r31, r7=r31(&IoStatusBlock),
  r8=Buffer, r9=Length, r10=&ByteOffset`. `r31+0` (Status) mis à `259`
  (`STATUS_PENDING`) JUSTE AVANT l'appel — idiome standard de driver NT.
- **L'appelant reconnaît EXPLICITEMENT `STATUS_PENDING` comme issue
  NORMALE** : `if (r3<0) goto ...; if (r3==259) goto MÊME endroit; else
  succès`. Ceci confirme, depuis la LOGIQUE MÊME du jeu (pas une
  inférence), que toute la lecture "997/pending, retry" établie depuis
  r109 est correcte. Notre harnais retourne `kOfflineStatus`
  (0xC00000BB) qui ne correspond à AUCUNE des issues reconnues.
- **Reformule encore le correctif** : puisque `STATUS_PENDING` est déjà
  une issue attendue par le code réel, un simple "statut d'échec
  différent" (hypothèse de r123) semble moins prometteur — le
  candidat le plus prometteur devient : retourner `259` au premier
  appel (comme le vrai matériel), puis faire évoluer l'`IoStatusBlock`
  vers un vrai statut de complétion avant l'épuisement du compteur de
  nouvelle tentative. PAS encore conçu ni implémenté.
- Aucun code natif modifié (instrumentation restaurée; `ctest` 9/9
  reconfirmé). **Prochain cycle** : tracer `loc_821F4FE4` (issue non
  reconnue); concevoir (sans implémenter) un stub `NtReadFile` correct.
  Voir
  `reports/ac6-retail-native-codegen-gate2-r124-connection-confirmed-sub_821f4e70-directly-dispatches-to-ntreadfile-20260901.md`.

# AC6 retail NTSC-U/J — r123 : `OBJECT_ATTRIBUTES` entièrement résolu — la lecture est à décalage fixe sur une partition disque dur peut-être absente (2026-09-01)

- **Structure `OBJECT_ATTRIBUTES` Xbox 360 (réduite, 3 champs, 12
  octets) confirmée octet par octet** : `RootDirectory=0`,
  `ObjectName`→pointeur `ANSI_STRING` (la MÊME chaîne statique
  `"\Device\Harddisk0\Partition1"` déjà vérifiée en r122),
  `Attributes=0x40` (= `OBJ_CASE_INSENSITIVE`, convention NT standard).
  **`ObjectName` pointe DIRECTEMENT vers le chemin de périphérique
  statique — le tampon sprintf du nom de fichier par appel n'est
  JAMAIS référencé par ObjectAttributes.**
- **La lecture est à décalage/longueur FIXES** (0x800/0x400), pas
  pilotée par un chemin — reformule l'hypothèse : ceci ressemble à une
  sonde de structure système fixe sur une PARTITION DISQUE DUR, pas à
  un chargeur d'actifs générique. Un titre disque-uniquement comme AC6
  n'a AUCUNE garantie de disposer de `\Device\Harddisk0\Partition1` —
  sur le vrai matériel, cet échec pourrait être une défaillance PROPRE
  et ATTENDUE (pas de disque dur), pas un vrai chargement de fichier.
- Graphe d'appel tracé un niveau plus loin : `Function_82390F48` ←
  `Function_82391A40` (qui appelle AUSSI directement `NtCreateFile`/
  `NtReadFile`) ← un seul appelant. PAS ENCORE connecté à la table de
  dispatch INDIRECTE (`bctrl`) de `sub_821F4E70` — `FindDirectCallsTo.java`
  ne voit que les appels directs.
- **Reformule le correctif potentiel** : si confirmé, la bonne
  correction pourrait être bien plus petite que "implémenter la
  lecture de fichier" — juste retourner un VRAI statut d'échec NT
  (au lieu de `kOfflineStatus`) pour que le chemin d'erreur déjà écrit
  du jeu s'exécute normalement. Hypothèse concrète, PAS encore établie.
  Aucun code natif modifié (travail read-only; script jetable
  supprimé). **Prochain cycle** : établir la connexion (ou son absence)
  entre ce cluster et la table de dispatch de `sub_821F4E70`; si
  connecté, tester l'hypothèse "statut d'échec" en direct. Voir
  `reports/ac6-retail-native-codegen-gate2-r123-object-attributes-resolved-and-the-read-is-fixed-offset-on-a-possibly-absent-hdd-partition-20260901.md`.

# AC6 retail NTSC-U/J — r122 : convention d'appel réelle de `NtCreateFile`/`NtReadFile` vérifiée par désassemblage — implémentation différée (2026-09-01)

- **Sept sites d'appel réels trouvés** (`FindDirectCallsTo.java`,
  read-only), tous groupés dans une seule petite région — un unique
  wrapper CRT-style, pas d'appels dispersés. La plus petite fonction
  contenant LES DEUX appels, `Function_82390F48` (307 octets),
  désassemblée intégralement (`DumpRange.java`, read-only).
- **Convention d'appel confirmée directement depuis le code machine
  réel** (pas supposée d'une spec externe) : `NtCreateFile(r3=&Handle,
  r4=DesiredAccess, r5=&ObjectAttributes, r6=&IoStatusBlock,
  r7=AllocationSize, r8=FileAttributes, r9=ShareAccess,
  r10=CreateDisposition)`; `NtReadFile(r3=Handle, r4=Event,
  r5=ApcRoutine, r6=ApcContext, r7=&IoStatusBlock, r8=Buffer,
  r9=Length, r10=&ByteOffset)` — correspond exactement à l'ordre réel
  NT/XDK (8 premiers registres seulement; 9e argument jamais fourni ici).
- **`ObjectAttributes` partiellement résolu** : une `ANSI_STRING`
  réelle (`{Length:u16, MaxLength:u16, Buffer:ptr}`) pointant vers une
  chaîne statique confirmée par lecture d'octets (`DumpBytes.java`) :
  `Length=28` correspond EXACTEMENT à `"\Device\Harddisk0\Partition1"`
  (28 caractères) — un chemin de partition Xbox 360 plausible et
  bien formé.
- **PAS assez pour implémenter en sécurité** : quel offset est
  `RootDirectory` vs `ObjectName`, et le nom de fichier par appel
  (construit via sprintf séparément) restent à résoudre. Décision
  délibérée de DIFFÉRER l'implémentation plutôt que de risquer le
  motif "implémenté avant vérification complète" que r115 avait déjà
  signalé sur lui-même. Aucun code modifié (travail read-only pur;
  un script Ghidra jetable supprimé avant ce rapport). **Prochain
  cycle** : résoudre le layout complet d'`OBJECT_ATTRIBUTES` avant
  d'implémenter. Voir
  `reports/ac6-retail-native-codegen-gate2-r122-ntcreatefile-ntreadfile-calling-convention-verified-from-disassembly-implementation-deferred-20260901.md`.

# AC6 retail NTSC-U/J — r121 : CAUSE RÉELLE TROUVÉE — `NtReadFile`/`NtCreateFile` ne sont pas implémentés, le champ de statut n'est jamais mis à jour (2026-09-01)

- **Auto-correction en cours de route** : r117/r119 avaient LU À
  L'ENVERS la branche de `sub_821F75F0` (`bne cr6,loc_821F7608` sur
  `r13+336≠0` → retourne 0; SUR `==0` → lit la valeur indirectée —
  l'inverse de ce que les deux rapports décrivaient). Le premier tour
  d'instrumentation a reproduit cette même inversion et affiché un
  sentinelle placeholder — repéré immédiatement, corrigé, reconstruit
  dans le MÊME cycle plutôt que rapporté comme résultat.
- **Avec la logique corrigée** : `ctx.r13=0x0f000000`,
  `p256=PPC_LOAD_U32(r13+256)=0x0f001000`,
  `indirected=PPC_LOAD_U32(p256+352)=0xC00000BB`. **`0xC00000BB` EST
  `kOfflineStatus`** — la constante EXACTE que
  `materialize_native_import_stubs.py` renvoie pour TOUT import sans
  stub dédié. Le stub générique n'écrit JAMAIS en mémoire invité (juste
  `ctx.r3`) — sa présence en mémoire à `0x0f001160` signifie que du
  CODE INVITÉ a lui-même copié le retour d'un import non implémenté
  dans ce champ.
- **`ctx.r13 = kProbePcrAddress`** (`0x0f000000`), une constante
  DÉLIBÉRÉE du harnais (`native/src/ac6recomp_main.cpp:17-30`) —
  n'initialise QUE quelques offsets; `kProbeThreadAddress+0x160`
  (=`0x0f001160`) n'en fait PAS partie.
- **Confirmé via `AC6_NATIVE_IMPORT_TRACE` (mécanisme déjà existant,
  aucune nouvelle instrumentation)** : `NtReadFile` (6 appels) ET
  `NtCreateFile` (4 appels) — la paire canonique d'E/S fichier
  asynchrone — tombent TOUTES DEUX dans le stub générique. Le vrai
  contrat de `NtReadFile` écrit le statut dans un `IO_STATUS_BLOCK`
  fourni par l'appelant; le stub générique n'y touche jamais. **La
  boucle de nouvelle tentative attend RÉELLEMENT et CORRECTEMENT une
  lecture de fichier qui n'a jamais été implémentée** — cohérent avec
  TOUTE la chaîne (997/ERROR_IO_PENDING, retry-puis-abandon, ce champ
  de statut).
- Aucun code natif modifié (deux tours d'instrumentation restaurés;
  `ctest` 9/9 reconfirmé) — implémenter `NtCreateFile`/`NtReadFile`
  mérite son propre cycle dédié (façon r108). **Prochain cycle** :
  implémenter contre l'infrastructure XDVDFS/média native existante,
  vérifier la convention d'appel réelle avant d'implémenter, puis
  re-vérifier en direct. Voir
  `reports/ac6-retail-native-codegen-gate2-r121-real-cause-found-ntreadfile-ntcreatefile-are-unimplemented-status-field-never-updated-20260901.md`.

# AC6 retail NTSC-U/J — r120 : `sub_821D4988` poste un message asynchrone dans un tampon circulaire, ce n'est PAS un appel de log — spéculation de r119 corrigée (2026-09-01)

- r119 décrivait `sub_821D4988` comme "ressemblant à un appel de
  log/diagnostic" — pure spéculation d'après la FORME de l'appel,
  jamais vérifiée. Adresse de la chaîne présumée calculée
  (`0x8275a414`) et lue via `DumpBytes.java` (read-only) : **128 octets
  de zéros**, pas une chaîne.
- **Lu la fonction elle-même** : ce n'est PAS un logger — elle empaquette
  `{buffer, flags}` dans un TAMPON CIRCULAIRE protégé par
  `RtlEnterCriticalSection`/`RtlLeaveCriticalSection` (mutex RÉEL depuis
  r116), gère un curseur d'écriture (mod 64) et un compteur, puis
  appelle `sub_821F5988` qui vérifie un résultat de la FAMILLE
  `258`/`STATUS_TIMEOUT` déjà vue ailleurs dans ce code
  (`wait_event()`). C'est un motif PRODUCTEUR classique (poster un
  travail + attendre), pas un `printf`.
- **Ne change pas la chaîne causale de r119** (le compteur à `+22896`
  décide toujours) — corrige une description non vérifiée avant
  qu'elle n'induise en erreur un futur cycle cherchant une chaîne
  d'erreur lisible qui n'existe pas. Aucun code modifié (lecture
  statique pure, aucune instrumentation cette fois). **Prochain
  cycle** : les prochaines étapes de r119 restent inchangées —
  instrumenter `ctx.r13` et le compteur `+22896`. Voir
  `reports/ac6-retail-native-codegen-gate2-r120-sub_821d4988-posts-an-async-message-not-a-log-string-r119s-speculation-corrected-20260901.md`.

# AC6 retail NTSC-U/J — r119 : l'octet de mode global vaut 2 (pas 0 ni 1) — le -1 vient d'une boucle de nouvelle tentative bornée sur un état par thread (2026-09-01)

- **Correction directe, ENCORE** : avant d'instrumenter, ce cycle avait
  RE-supposé l'octet à 0 par déduction plutôt que par lecture — la même
  erreur que r118 venait de corriger. Instrumentation directe :
  **`octet de mode global = 2`**, ni 0 ni 1. Nommé explicitement comme
  un second piège de déduction évité de justesse.
- **Chemin réel** : avec octet=2, `loc_821CC800` tombe en fallthrough
  (PAS vers `loc_821CCD4C`) dans une TROISIÈME table de dispatch,
  distincte des deux précédentes (base `-32227/-14244`).
- **Origine du -1 tracée précisément** : une SEULE assignation `-1`
  dans tout ce switch, à `loc_821CCCD8`, atteinte depuis
  `loc_821CCAB0` — un compteur de nouvelles tentatives (`+22896` de
  l'objet) : s'il atteint 0, appelle `sub_821D4988` (forme d'un
  log/diagnostic) puis retourne -1; sinon décrémente et boucle
  (état=3). La condition de succès/échec réutilise le MÊME couple
  `sub_821F4E70`/`sub_821F50A0`→`sub_821F75F0` que r117 avait déjà lu
  (lecture d'un champ PAR THREAD à `ctx.r13+336`) — mal attribué au
  mauvais switch à l'époque, mais le mécanisme lui-même était juste.
- **C'est une boucle de nouvelle tentative bornée qui abandonne** —
  cause exacte (ce que `ctx.r13+336` contient réellement, valeur
  initiale du compteur) non établie ce cycle, deviner refusé. Aucun
  code natif modifié (instrumentation restaurée; `ctest` 9/9
  reconfirmé). **Prochain cycle** : instrumenter `ctx.r13` lui-même et
  le compteur `+22896`; lire la chaîne de `sub_821D4988` pour nommer
  directement le sous-système en échec. Voir
  `reports/ac6-retail-native-codegen-gate2-r119-real-path-is-a-bounded-retry-loop-checking-per-thread-status-at-ctx-r13-plus-336-20260901.md`.

# AC6 retail NTSC-U/J — r118 : la cible "case 3" de r117 était fausse — le vrai bailout est contrôlé par un octet de mode global, jamais examiné (2026-09-01)

- **Correction nommée de r117** : r117 avait cité "état=3" (repris de
  r109, JAMAIS revérifié sous le build actuel r108/r116-corrigé) sans
  reconfirmer la valeur elle-même — seulement le retour (-1) et la
  branche de secours. Ré-instrumenté les deux (lecture de l'état ET
  corps du case 3) dans la MÊME exécution : `state(+324)=0`, PAS 3.
  **La valeur d'état n'est pas fixe** — elle dépend de tout ce qui
  s'exécute avant, et les corrections de r108/r116 l'ont déplacée.
- Tracé l'état 0 : `loc_821CC5BC` (case 0) tombe en fallthrough à
  travers `loc_821CC5E0→...→loc_821CC5EC`, écrivant l'état à 2 puis 7
  puis 3, retombant dans le MÊME corps "case 3" que r117 avait lu. Un
  print placé dans ce corps n'a JAMAIS déclenché dans les deux
  exécutions instrumentées — preuve directe qu'il n'est pas atteint.
- **Résolu en relisant `sub_821CC508` depuis son entrée** : DEUX portes
  précèdent le dispatch par état, toutes deux déjà présentes dans la
  transcription de r109 mais jamais suivies : (1) porte "déjà fait" sur
  bit0 de `320(r31)`; (2) un OCTET GLOBAL (`lis r21,-32108` / déplacement
  `-18120`) — si ≠1, saute vers `loc_821CC800` (PAS le switch par état);
  si ≠2 là aussi, saute vers `loc_821CCD4C` — une TROISIÈME région
  jamais examinée. Le dispatch par état n'est atteint QUE si cet octet
  vaut exactement 1.
- Aucun code natif modifié (deux tours d'instrumentation sur deux
  fichiers, tous restaurés; `ctest` 9/9 reconfirmé). L'ABSENCE de sortie
  du print case-3 est elle-même la preuve qui écarte le mécanisme de
  r117. **Prochain cycle** : lire/instrumenter cet octet global et
  confirmer que `loc_821CCD4C` est bien emprunté avant de le tracer.
  Voir
  `reports/ac6-retail-native-codegen-gate2-r118-r117s-case3-target-was-wrong-real-bailout-gated-by-a-global-mode-byte-20260901.md`.

# AC6 retail NTSC-U/J — r117 : chaîne causale COMPLÈTE fermée — le site d'écriture de `0x82935d98` identifié, jamais atteint à cause du bailout déjà caractérisé par r109 (2026-09-01)

- **`FindPpcAddressMaterialization.java` (read-only) trouve UNE SEULE
  matérialisation** de `0x82935d98` dans tout le XEX : `821d6be0
  lis r11,-0x7d6d ; addi r11,r11,0x5d98`. Le code généré correspondant
  (`stw r3,0(r11)`) se trouve DANS `sub_821D5F48` lui-même — la même
  fonction de ~1700 lignes que r105-r109 avaient déjà caractérisée
  (GATE1-5, boucle post-GATE2) — à ~1100 lignes APRÈS cette boucle.
- **Vérifié en direct en UNE SEULE exécution** (le crash étant
  désormais déterministe grâce à r116, plus besoin de balayage répété) :
  `post-GATE2 dispatch ret=-1` puis `BAILOUT sub_821D5F48 exits early
  via loc_821D6138 -- 0x82935d98 NEVER written`. **Chaîne causale
  complète fermée, bout en bout** : `sub_821CC508` retourne -1 pour
  l'état 3 (déjà capturé par r109) → branche de secours `loc_821D6138`
  → `return;` GENUINE ~1100 lignes avant l'écriture → `sub_821D7DE0` ne
  vérifie/n'abandonne pas (r102) → `sub_821D6C20` lit `0x82935d98`
  toujours nul → crash.
- **Ce n'est PAS un nouveau mécanisme** — c'est le MÊME échec d'état 3
  de `sub_821CC508` que r109 avait déjà capturé, maintenant compris
  comme la cause racine RÉELLE du crash original de r100, pas un
  problème séparé. Premier coup d'œil statique sur `case 3`
  (`loc_821CC5EC`) : routine substantielle d'allocation/init de pool
  de tampons (`sub_821F4170`+`sub_821F3BF0`), pas encore tracée
  jusqu'à son retour exact.
- Aucun code natif modifié (instrumentation restaurée; `ctest` 9/9
  reconfirmé). **Prochain cycle** : tracer `case 3` de `sub_821CC508`
  jusqu'à son point de retour exact — vérifier d'abord une lacune du
  harnais (façon r108) avant de supposer un bug du code invité. Voir
  `reports/ac6-retail-native-codegen-gate2-r117-full-causal-chain-closed-write-site-to-crash-20260901.md`.

# AC6 retail NTSC-U/J — r116 : les vraies sections critiques éliminent complètement les crashes r111-r115 — elles masquaient le null global ORIGINAL de r101, maintenant déterministe (2026-09-01)

- **D'abord testé la question ouverte de r115.** Trace ajoutée (gated
  `AC6_NATIVE_IMPORT_TRACE`, même motif que `DbgPrint`) : **AUCUN** des
  dix-huit threads ne demande `CREATE_SUSPENDED` — l'hypothèse centrale
  de r114/r115 est réfutée directement. La correction de r115 était
  correcte mais un no-op pour ce jeu précis.
- **Le vrai bug** : `RtlEnterCriticalSection`/`RtlLeaveCriticalSection`
  étaient de purs no-ops, sous l'hypothèse (maintenant réfutée par ce
  projet lui-même, r111-r115) d'"un seul thread invité". Avec dix-huit
  vrais threads concurrents et AUCUNE exclusion mutuelle réelle, tout
  ce que le jeu comptait protéger courait sans protection.
- **Corrigé** : de vrais `std::recursive_mutex` par objet, clés sur
  l'adresse invité de l'objet `RTL_CRITICAL_SECTION` (même motif que
  `g_events`). 4 tests nouveaux/mis à jour, suite 135/135.
- **Vérifié en direct — résultat majeur** : balayage de 25 exécutions —
  **ZÉRO crash de thread d'arrière-plan** (`sub_82346428`,
  `sub_821D4C20`) — élimination complète, pas une réduction. MAIS le
  thread principal plante maintenant DÉTERMINISTIQUEMENT, à CHAQUE
  exécution (25/25), dans `sub_821D6C20` — le site ORIGINAL de r100.
  Capturé : `rbp = 0x82935d98` — **exactement le global nul que r101
  avait identifié tout au début de cet arc d'investigation**, jamais
  réellement corrigé, jusqu'ici masqué par la course que r111-r115 ont
  caractérisée sans savoir qu'elle cachait un bug plus ancien déjà
  diagnostiqué.
- Amélioration nette réelle et indépendamment justifiée (élimine un
  vrai bug de multithreading, ne ferme pas Gate 2). Gates : `ctest`
  9/9, pytest 135/135, compteur démo inchangé (185). **Prochain
  cycle** : rouvrir directement la découverte de r101 (`0x82935d98`),
  vérifier si des commits antérieurs à cette session (DPC/interruption
  graphique, "the real waiter is an equality wait on a sequence
  counter") sont pertinents avant de re-dériver du travail déjà établi.
  Voir
  `reports/ac6-retail-native-codegen-gate2-r116-real-critical-sections-eliminate-the-race-expose-r101s-original-null-global-deterministically-20260901.md`.

# AC6 retail NTSC-U/J — r115 : `ExCreateThread` respecte maintenant `CreationFlags` — réduit mais N'ÉLIMINE PAS les crashes de r112-r114 (2026-09-01)

- **Implémenté l'étape suivante nommée par r114.** Confirmé d'abord que
  `NtResumeThread`/`KeResumeThread` sont de VRAIS imports utilisés par
  ce XEX (`ppc_recomp_shared.h` lignes 19516/19667) avant d'implémenter
  quoi que ce soit. `ExCreateThread` lit maintenant `creation_flags =
  ctx.r9.u32` (7e argument entier PPC, convention XDK publique — signalé
  comme connaissance externe non vérifiée dans ce dépôt, à recontrôler).
  Si le bit `CREATE_SUSPENDED` (0x4) est posé, le thread `std::thread`
  généré bloque via un nouveau `park_until_resumed()` (attente NON
  bornée, délibérément distincte du `wait_event()` borné à 2ms de r91 —
  réutiliser ce dernier aurait laissé le code invité s'exécuter après
  2ms même sans reprise réelle) avant d'exécuter le shim invité.
  `NtResumeThread`/`KeResumeThread` signalent l'événement associé au
  handle du thread (même compteur `g_next_handle` partagé, aucune
  collision de clé).
- Tests : 3 nouveaux (dont un vérifiant EXPLICITEMENT que
  `park_until_resumed` n'utilise PAS `wait_for`, garde contre l'erreur
  de borner l'attente par erreur), 1 test existant mis à jour pour la
  nouvelle liste de capture du lambda. Suite complète 134/134 (131/131
  avant ce cycle).
- **Vérifié en direct, résultat honnête** : rejouée la même sonde
  `gdb --batch` répétée 15 fois — `sub_82346428` (1er site nommé par
  r111-r114) N'A PAS planté cette fois, mais `sub_821D4C20` et le crash
  ORIGINAL `sub_821D6C20` (r100) ont TOUS DEUX planté encore une fois
  chacun (2/15, contre 4/10 dans le lot comparable de r112 — direction
  seulement, pas un taux quantifié avec confiance sur si peu
  d'échantillons). **PAS une correction complète.** Deux explications
  ouvertes, non tranchées : (1) tous les dix-huit threads ne sont peut-
  être pas créés avec `CREATE_SUSPENDED`; (2) l'ordonnancement de la
  reprise elle-même pourrait avoir sa propre course indépendante de la
  suspension. Deviner refusé.
- Gates : `ctest` 9/9, audit mission01 échoue sur le même échec
  préexistant sans rapport (`retail_session.cpp`), compteur soumodule
  démo inchangé (185), pytest 134/134. Voir
  `reports/ac6-retail-native-codegen-gate2-r115-suspended-thread-creation-implemented-reduces-but-does-not-eliminate-crashes-20260901.md`.

# AC6 retail NTSC-U/J — r114 : CAUSE RACINE TROUVÉE — appel via un pointeur de fonction invité NUL, et le stub `ExCreateThread` ne lit jamais `CreationFlags` (2026-09-01)

- `sub_821D4C20` (deuxième site nommé par r112) capturé avec
  désassemblage complet : **instruction et valeur de `r12` IDENTIQUES**
  à `sub_82346428` (r112/r113) — trop précis pour être une coïncidence
  de mémoire réutilisée. `objdump` statique confirme : `r12` est une
  **constante figée à la compilation** (`movabs $0xffffffff7e980000`),
  PAS une lecture mémoire runtime.
- Correspond exactement à la macro `PPC_CALL_INDIRECT_FUNC`
  (`rex/ppc/context.h:126-131`) : `PPC_LOOKUP_FUNC(x,y) =
  *(PPCFunc**)(x + PPC_IMAGE_BASE + PPC_IMAGE_SIZE +
  (uint64_t(uint32_t(y)-PPC_CODE_BASE)*2))`. Le compilateur replie la
  partie constante dans `r12`; `y*2` (= `rax*2`) est la seule partie
  variable. **`rax=0` capturé aux deux crashes signifie `y=0`** — le
  code invité appelle un pointeur de fonction NUL. Sans vérification
  de nullité, `uint32_t(0)-PPC_CODE_BASE` déborde en arithmétique
  32-bit non signée, produisant l'adresse hôte non mappée observée.
  **Mécanisme de crash entièrement expliqué**, plus une hypothèse.
- **Cause du pointeur nul** : le vrai stub `ExCreateThread`
  (`tools/materialize_native_import_stubs.py:271-301`) lit r3
  (Handle), r6 (XapiThreadStartup), r7 (StartAddress), r8
  (StartContext) mais **JAMAIS r9 (`CreationFlags`, qui porte
  `CREATE_SUSPENDED` sur le vrai matériel)** — signature XDK publique
  bien documentée, à revérifier contre une source faisant autorité.
  Les dix-huit threads démarrent donc TOUS immédiatement, quelle que
  soit la demande du jeu — explique le mécanisme ET la sensibilité au
  timing documentée depuis r110.
- **Corrige r112** ("lecture vtable non synchronisée" → en réalité
  déterministe une fois `y=0` connu, pas une vraie course mémoire) et
  affine r113 (`r12` n'a jamais été une valeur non initialisée — c'est
  le SLOT de pointeur de fonction invité, plus en amont, qui est
  encore nul). Aucun code modifié — un changement de ce poids
  (infrastructure de cycle de vie des threads) mérite son propre
  cycle avec couverture de tests dédiée. Voir
  `reports/ac6-retail-native-codegen-gate2-r114-root-cause-found-null-guest-function-pointer-plus-excreatethread-ignores-creationflags-20260901.md`.

# AC6 retail NTSC-U/J — r113 : `r12` confirmé mémoire non mappée, ET le crash original `sub_821D6C20` du thread principal se produit toujours par intermittence après r108 (2026-09-01)

- **`r12` capturé au crash `sub_82346428`** : `0x7ffe75980000`,
  **confirmé NON MAPPÉ** (`gdb`: "Cannot access memory at address").
  Pas un pointeur nul, pas un pointeur valide vers le mauvais objet —
  une valeur véritablement invalide, cohérente avec (sans le prouver)
  une origine réellement non initialisée, même classe que la
  découverte `MmQueryStatistics` de r108 mais pour un champ différent
  et non identifié.
- **Découverte séparée et plus importante** : une capture a montré le
  crash ORIGINAL `sub_821D6C20` (celui de r100) se produire à nouveau,
  cette fois sur le THREAD PRINCIPAL (`main → __xstart →
  sub_821D7DE0 → sub_821D6C20`), PAS sur un thread d'arrière-plan. Ceci
  ne contredit pas la découverte spécifique de r108 (l'allocation GATE2
  réussit vraiment, vérifié en direct et reproductible) mais montre que
  "la chaîne de crash est fermée" n'a JAMAIS été une affirmation
  universelle établie — le site continue de planter par intermittence.
  Non reproduit dans 15 tentatives de suivi (même fenêtre de timing
  étroite et sensible à la charge que documentée en r112).
- Aucun code natif ni généré modifié (investigation `gdb --batch` pure).
  **Prochain cycle** : recapturer `sub_821D6C20` avec désassemblage
  complet pour l'instruction exacte; envisager un balayage plus long en
  arrière-plan plutôt que des lots interactifs courts, vu le taux de
  reproduction ~10-20% observé pour les deux sites. Voir
  `reports/ac6-retail-native-codegen-gate2-r113-r12-is-unmapped-garbage-and-the-original-main-thread-crash-still-happens-intermittently-20260901.md`.

# AC6 retail NTSC-U/J — r112 : dix-huit threads démarrent en parallèle; le crash est un appel indirect via un pointeur apparemment pas encore prêt (2026-09-01)

- Un lot de dix répétitions `gdb --batch` (même technique passive que
  r111) montre **dix-huit lignes `[New Thread ...]` par exécution** —
  bien plus qu'un seul thread d'arrière-plan. **Deux sites de crash
  distincts et reproductibles**, tous deux atteints via le même
  trampoline d'entrée de thread `sub_821F8008` : `sub_82346428` (via
  `sub_823453E8`, 2/10) et `sub_821D4C20` (directement, 2/10). 6/10
  n'ont produit aucun crash dans la borne de 15s.
- **Instruction de crash capturée en direct** : `call *(%r12,%rax,2)`
  avec `rax=0` — un appel indirect via le pointeur situé en `[r12]`,
  suivi IMMÉDIATEMENT (si atteint) d'un `__imp__RtlEnterCriticalSection`
  sur le même objet — code SUR LE POINT de dispatcher via un pointeur
  façon vtable AVANT d'acquérir le verrou censé le protéger. Forme
  cohérente avec une lecture non synchronisée en course avec l'écriture
  d'un autre thread, sans preuve directe (valeur de `r12` non capturée
  — le taux de reproduction du crash a fluctué de façon marquée, de
  2/10 dans un lot à 0/18 dans le suivant, empêchant une nouvelle
  capture dans le budget de ce cycle).
- Aucun code natif ni généré modifié (investigation entièrement par
  `gdb --batch`/`objdump`, aucune édition). `ctest` 9/9 (vérification
  de routine, rien édité). **Prochain cycle** : recapturer `r12`;
  tracer `sub_821D4C20` (crash plus proche du trampoline, potentiellement
  plus simple); vérifier si le harnais `ExCreateThread` devrait lancer
  ces dix-huit threads avec un ordre/rythme (ex. threads suspendus
  jusqu'à reprise explicite) que le stub actuel ne modélise pas. Voir
  `reports/ac6-retail-native-codegen-gate2-r112-eighteen-threads-spawn-concurrently-crash-is-an-unsynchronized-vtable-read-20260901.md`.

# AC6 retail NTSC-U/J — r111 : la source du non-déterminisme de r110 est une VRAIE course entre threads — `ExCreateThread` lance un vrai thread hôte qui segfault en code invité (2026-09-01)

- Instrumentation de 46 points de contrôle dans la région GATE1-5, puis
  **capture directe du crash via `gdb --batch -ex run -ex bt`** — usage
  passif (laisser tourner jusqu'au signal), distinct du pas-à-pas
  interactif que r103/r104 avaient jugé peu fiable pour cette sonde.
  Deux exécutions sur quatre reproduisent un SIGSEGV identique :
  ```
  [New Thread ...]
  Thread 15 "ac6recomp" received signal SIGSEGV
  #0 __imp__sub_82346428 ()
  #1 __imp__sub_823453E8 ()
  #2 __imp__sub_821F8008 ()
  #3 libstdc++.so.6 (?? )
  #4 start_thread
  #5 __clone3
  ```
- **Ce crash n'est PAS sur le thread principal** — `[New Thread ...]` et
  les cadres `start_thread`/`__clone3` montrent un VRAI thread OS
  séparé. Vérifié directement (`tools/materialize_native_import_stubs.py`
  lignes 271-301) : `ExCreateThread` lance réellement un `std::thread`
  détaché exécutant du code invité, avec une pile de seulement 64 Ko
  (`g_next_thread_stack.fetch_sub(0x10000u)`) — pas un stub no-op.
  **Résout le non-déterminisme de r110 sans le contredire** : le
  "moment" où le thread principal semble bloquer variait réellement
  d'une exécution à l'autre, mais la CAUSE est une course avec ce
  second thread qui plante, pas un non-déterminisme dans
  `sub_821D5F48` lui-même.
- Cause exacte du crash dans `sub_82346428` (offset +414) non établie
  ce cycle — deviner refusé. Aucun code natif modifié (instrumentation
  restaurée; `ctest` 9/9 reconfirmé). **Prochain cycle** : tracer
  `sub_82346428` statiquement pour localiser l'instruction exacte, puis
  évaluer si c'est un vrai bug de code invité (façon r108) ou une
  limitation du harnais (pile de 64 Ko potentiellement insuffisante).
  Voir
  `reports/ac6-retail-native-codegen-gate2-r111-nondeterminism-source-is-a-real-background-thread-race-20260901.md`.

# AC6 retail NTSC-U/J — r110 : la sonde est non-déterministe d'une exécution à l'autre, MÊME sans GDB — r108/r109 ont besoin d'une réserve (2026-09-01)

- En traçant `sub_821D7DE0` (prochaine étape nommée par r108), r110
  corrige une erreur de portée dans r109 : "la fonction retourne
  proprement" avait été DÉDUITE de l'absence de prints supplémentaires,
  jamais observée directement — `sub_821D5F48` fait 1700 lignes
  (18586-20291), et tout le travail r105-r109 (GATE2, la boucle
  `loc_821D6358`) se trouve à l'INTÉRIEUR de cette seule fonction, pas
  dans des fonctions séparées. Des marqueurs posés directement sur les
  deux chemins possibles (`loc_821D6138` return vs. fallthrough) ont
  confirmé que la branche de sortie EST bien prise — la conclusion de
  r109 sur la boucle elle-même tient.
- **Mais** cinq exécutions successives du MÊME binaire, mêmes entrées,
  même instrumentation, bornées à 20s chacune, donnent des résultats
  DIFFÉRENTS : 3 sur 5 restent bloquées juste après l'entrée de
  `sub_821D7DE0` (à l'intérieur de `sub_821D5F48`, AVANT même d'atteindre
  la boucle post-GATE2); 2 sur 5 progressent jusqu'à la boucle de
  préchauffe. Une exécution isolée a même segfault. **Non-déterminisme
  réel, sans GDB** — distinct de l'instabilité GDB déjà retirée par r104.
- **Conséquence** : les conclusions "GATE2 réussit sans crash" (r108) et
  "la boucle se résout proprement" (r109) décrivent le résultat d'UNE
  seule exécution chacune, pas le comportement typique de la sonde.
  Aucune cause identifiée ce cycle (candidats : une autre lecture de
  pile réellement non initialisée dont le contenu dépend de la mise en
  page du processus hôte malgré la garantie zero-fill de
  `GuestAddressSpace`, ou une vérification sensible au timing) —
  deviner refusé, même discipline que les cycles 1111/1113. Aucun code
  natif modifié (trois tours d'instrumentation, tous restaurés; `ctest`
  9/9 reconfirmé). **Prochain cycle : toute conclusion sur le
  comportement de la sonde doit désormais s'appuyer sur PLUSIEURS
  exécutions, pas une seule.** Voir
  `reports/ac6-retail-native-codegen-gate2-r110-probe-is-run-to-run-nondeterministic-without-gdb-20260901.md`.

# AC6 retail NTSC-U/J — r109 : la boucle de dispatch post-GATE2 se résout en un seul appel — le vrai blocage reste en aval (2026-09-01)

- Après r108, la lecture de `Function_821D5F48` vers l'avant révèle une
  boucle ressemblant exactement au motif déjà documenté (poll mémoire
  direct, cf. le suivi démo superseded r1829-1833) : `loc_821D6358`
  appelle `sub_821CC508(r29)` en boucle tant qu'il retourne 0.
  `sub_821CC508` contient lui-même un dispatch par table de fonctions
  indexé sur un champ à `r29+324`, sans aucun appel `__imp__*Wait*`.
  **Vérifié en direct plutôt que supposé** à partir de la seule forme
  (discipline "instrument avant de faire confiance") : deux tours
  d'instrumentation temporaire (`AC6_R109_DIAG`, restaurée après
  usage) — quatorze points de contrôle après chaque `bl` entre le
  succès de GATE2 et la boucle, puis la boucle elle-même.
- **Résultat : la boucle ne tourne PAS** — `sub_821CC508` retourne `-1`
  dès le premier appel (état `3`), ce qui prend immédiatement la
  branche `blt→loc_821D6138` (la même sortie de secours partagée) et
  `Function_821D5F48` retourne proprement `r3=0` à `sub_821D7DE0`, sans
  boucler ni planter. **Hypothèse réfutée par mesure directe** — la
  forme du code seule aurait facilement pu être rapportée comme "la"
  réponse sans être exécutée.
- Aucun code natif modifié (deux tours d'instrumentation, tous deux
  restaurés; `ctest` 9/9 reconfirmé). **Le vrai blocage de 30s reste en
  aval**, dans du code que ce cycle n'a pas atteint — prochain candidat :
  `sub_821D7DE0` lui-même, ce qu'il fait du retour `r3=0`. Voir
  `reports/ac6-retail-native-codegen-gate2-r109-post-gate2-dispatcher-resolves-cleanly-real-stall-still-downstream-20260901.md`.

# AC6 retail NTSC-U/J — r108 : la chaîne de crash GATE2 (r100-r107) est fermée — `MmQueryStatistics` n'écrivait jamais son tampon de sortie (2026-09-01)

- **r106 s'est trompé de cadre.** Le pointeur mystère (`0x8feffd1c`)
  n'est pas hors de toute chaîne d'appel tracée : `Function_821D5F48`
  passe `r1+96` en argument à `sub_821F4820` (`addi r3,r1,96; bl
  0x821f4820`), qui écrit à travers ce pointeur (`r31+12` dans son
  propre corps généré, `ppc_recomp.27.cpp`) — exactement `r1+108`, le
  slot lu juste après. Une recherche d'écritures indexée sur `ctx.r1`
  dans l'appelant ne peut pas voir une écriture faite via un autre
  registre dans une fonction appelée : une évasion de pointeur de cadre.
- **Cause racine identifiée** : `sub_821F4820` obtient sa valeur d'un
  appel à `__imp__MmQueryStatistics`, un stub HLE matérialisé par
  `tools/materialize_native_import_stubs.py` qui, avant ce cycle,
  n'avait aucun cas dédié et tombait dans le générique
  (`ctx.r3.u64 = kOfflineStatus;`) — qui ne touche JAMAIS la mémoire
  invité pointée par le tampon de sortie. La valeur "non initialisée"
  de r105/r106 était les octets périmés déjà présents dans le tampon de
  pile de `sub_821F4820`, jamais du territoire noyau/chargeur.
- **Corrigé** : ajout d'un cas `MmQueryStatistics` écrivant les deux
  champs réellement lus (`+4`, `+12`) avec des valeurs dérivées de la
  RAM physique unifiée bien documentée de la Xbox 360 (512 Mo, granularité
  de page 4 Ko déjà établie par le `rlwinm`-12 de l'appelant) :
  `0x20000` pages totales, `0x18000` (384 Mo) disponibles. Les champs
  non lus ailleurs sont mis à zéro plutôt que devinés.
- **Vérifié en direct, pas seulement par lecture du désassemblage** :
  instrumentation temporaire (`AC6_R108_DIAG`, restaurée après usage) —
  `field+12=0x18000000`, puis `alloc result r3=0x16f70000` (non nul :
  l'allocation RÉUSSIT désormais, contre échec à chaque cycle précédent).
  Sonde d'entrée bornée rejouée sans instrumentation : atteint la borne
  de 30s SANS crash — une amélioration stricte par rapport à tous les
  cycles depuis r100 (qui se terminaient tous par un SIGSEGV dans
  `sub_821D6C20`).
- Corrige explicitement la conclusion de r106 ("rien dans la chaîne
  tracée ne possède cette mémoire") — nommée par cycle, per la
  discipline du projet. Le nouveau point d'arrêt de la sonde (bloquée à
  la borne, aucun crash) reste à investiguer : c'est la frontière
  d'attente/ordonnanceur post-IB déjà nommée par le plan en cours. Voir
  `reports/ac6-retail-native-codegen-gate2-r108-mmquerystatistics-was-the-uninitialized-source-fixed-20260901.md`.

# AC6 retail NTSC-U/J — r107 : la taille de pile déclarée du XEX écartée — analysée mais jamais utilisée (2026-09-01)

- Hypothèse concrète et vérifiable : ce projet analyse DÉJÀ un champ XEX
  déclaré (`kHeaderDefaultStackSize`, tag `0x00020200`) dans
  `native_xex.cpp` — pourrait-il expliquer la valeur mystère de r106?
  Ajouté UNE ligne de diagnostic temporaire dans le fichier hôte
  maintenu à la main (`ac6recomp_main.cpp`, tracké, restauré après
  usage; `ctest` 9/9 reconfirmé), imprimant `stack_size`.
- **Résultat** : `stack_size=0x40000` (256 Ko). **Écarté** : (1) ce
  harnais analyse ce champ mais ne l'utilise JAMAIS nulle part ailleurs
  (`r1` codé en dur à `0x8ff00000` sans égard à la valeur déclarée du
  titre — une vraie lacune de fidélité, indépendante, notée mais pas
  corrigée); (2) 256 Ko n'explique PAS de façon plausible une quantité de
  4 Mo/8 Mo — écarté par mesure directe, pas par déduction supplémentaire.
- **Résultat négatif documenté**, dans l'esprit de la discipline du
  projet (CLAUDE.md cite les cycles 1111/1113 pour ce même motif) :
  élimine proprement une hypothèse plausible plutôt que de la laisser
  comme supposition non testée pour un futur cycle. Aucun code natif
  modifié. Voir
  `reports/ac6-retail-native-codegen-gate2-r107-xex-stack-size-ruled-out-parsed-but-unused-20260901.md`.

# AC6 retail NTSC-U/J — r106 : le slot de pile non initialisé localisé hors du cadre de `_xstart` lui-même (2026-09-01)

- Même technique que r105 (instrumentation temporaire du source généré,
  jamais commitée, restaurée après usage; `ctest` 9/9 reconfirmé). Ajouté
  un `fprintf` UNIQUE au site de lecture lui-même, imprimant l'adresse
  invité résolue, la valeur lue et `ctx.r1.u32` — élimine tout risque
  d'erreur de calcul manuel.
- **Résultat direct** : `guest_addr=0x8feffd1c value=0x400000
  (ctx.r1.u32=0x8feffcb0)`. Concorde EXACTEMENT avec le calcul de cadre
  fait à la main UNE FOIS corrigé : r1 initial du harnais (`0x8ff00000`)
  moins le prologue de `_xstart` (`-0x1f0`) moins celui de
  `sub_821D7DE0` (`-0x70`) moins celui de `Function_821D5F48` (`-0xf0`)
  plus 108 = `0x8feffd1c`. **Cette adresse est SOUS `0x8feffe10` — hors
  du cadre de `_xstart` LUI-MÊME**, pas seulement hors de celui de
  `Function_821D5F48`/`sub_821D7DE0`. `_xstart` étant le point d'entrée
  XEX (aucun appelant invité), rien dans la chaîne d'appel PPC tracée ne
  possède cette mémoire.
- **Erreur de calcul auto-corrigée avant de tromper le rapport** : un
  premier calcul manuel (avant l'instrumentation directe) oubliait le
  propre prologue de `_xstart`, donnant une adresse fausse
  (`0x8fefff0c`) lue à zéro dans l'ancien core dump du plantage r100/101
  — ce qui semblait d'abord contredire ce cycle. Recalculé correctement
  et relu la BONNE adresse dans le même core dump : zéro AUSSI —
  cohérent, pas contradictoire : le core dump capture l'état à un moment
  BEAUCOUP plus tardif (le plantage `sub_821D6C20`), après que cette
  région de pile a presque certainement été réutilisée. Pas de preuve de
  non-déterminisme réel (`GuestAddressSpace` utilise `MAP_ANONYMOUS`,
  garanti zéro par Linux, vérifié directement dans le source).
- **Toujours aucun correctif implémenté** : l'origine de cette valeur
  nécessite soit de tracer une séquence de boot noyau/chargeur PLUS
  LARGE que ce que ce harnais reproduit (`initialize_probe_thread` est
  minimal), soit d'accepter cela comme une limitation de portée du
  harnais. Aucun code natif modifié. Voir
  `reports/ac6-retail-native-codegen-gate2-r106-uninitialized-stack-slot-pinpointed-outside-xstart-own-frame-20260901.md`.

# AC6 retail NTSC-U/J — r105 : cause racine trouvée — une pile non initialisée pilote une allocation surdimensionnée (2026-09-01)

- Suivant la recommandation r104, instrumenté DIRECTEMENT le source
  généré (`ppc_recomp.23.cpp`, jamais maintenu à la main, dans l'arbre de
  build ignoré par git) avec des `fprintf` temporaires gardés par une
  variable d'env (`AC6_R105_DIAG`), reconstruit, exécuté NATIVEMENT (zéro
  GDB, zéro `ptrace`), puis RESTAURÉ depuis une sauvegarde avant tout
  commit. `ctest` 9/9 reconfirmé après restauration, `git status` propre.
- **Résultat reproductible sur 2 runs identiques** : GATE2
  (`sub_821F4078`) échoue réellement, causant le bailout puis le SIGSEGV
  déjà connu. **Ceci rétracte la lecture GDB de r103** (`r3=0x8feffcb0`
  non nul) — exactement le type d'artefact que r104 avait mis en garde
  contre, sans encore le prouver concrètement.
- Tracé GATE2 jusqu'à sa vraie cause : son `bl 0x823d012c` est en réalité
  le nom résolu par XenonRecomp pour l'import noyau
  **`MmAllocatePhysicalMemoryEx`**. Instrumenté ses arguments/retour :
  parmi 17 appels de ce cycle, UN SEUL échoue, demandant
  **`r4=0xffc00000`** (`-4 Mo` signé, soit ~4,09 Go non signés) — refusé
  à juste titre par `allocate_guest()`.
- **Origine de la taille tracée jusqu'à sa source** : `r11 - 0x800000`
  (8 Mo), où `r11` vient de `lwz r11,108(r1)` — un slot de pile LOCAL.
  Grep exhaustif du corps entier de `Function_821D5F48` (1700+ lignes) :
  **aucune écriture** à cet offset nulle part. Vérifié aussi l'appelant
  unique confirmé (`sub_821D7DE0`, r102/r103) : n'écrit rien non plus
  avant son `bl`. **C'est de la mémoire de pile non initialisée dans ce
  harnais** — 4 Mo au lieu d'au moins 8 Mo attendus.
- **Décidé de ne PAS implémenter de correctif** : deux explications
  restent compatibles (dépendance cachée à une séquence d'appels réelle
  du matériel véritable qui laisse une valeur résiduelle appropriée là,
  vs. une vraie lacune du harnais mono-thread) et ce cycle ne peut pas
  trancher — deviner une valeur synthétique masquerait potentiellement
  une vraie lacune plutôt que de la fermer. Aucun code natif modifié
  (toutes les modifications étaient dans l'arbre de build généré,
  restaurées). Voir
  `reports/ac6-retail-native-codegen-gate2-r105-crash-root-cause-uninitialized-stack-oversized-allocation-20260901.md`.

# AC6 retail NTSC-U/J — r104 : le traçage GDB en direct sur cette sonde est peu fiable; les deux lectures statiques s'accordent (2026-09-01)

- Désassemblé le code HÔTE COMPILÉ (x86, pas juste le PPC invité) à
  l'adresse de retour de GATE2 (220 instructions) : confirme exactement
  la même absence de branche de sortie entre GATE2 et GATE3 que la
  lecture PPC de r103. Un fait nouveau : `RtlInitializeCriticalSection`
  (un stub d'import natif réel, pas une fonction générée) est appelé
  DEUX fois dans cet intervalle — noté pour un futur cycle, pas poursuivi.
  Écarté l'ambiguïté de symbole comme explication : `sub_821CC508` et
  `__imp__sub_821CC508` résolvent à la MÊME adresse.
- **4 sessions GDB en direct, 3 résultats DIFFÉRENTS**, rapportées sans
  tri sélectif : (1) `gates2.gdb` — reproductible 2x, GATE1+GATE2
  seulement; (2) `gate3_check.gdb` non conditionné — expiré 240s sans
  rien; (3) `gates3.gdb` — contenait une vraie erreur de script (adresse
  hôte fixe d'une session précédente, invalide sous PIE), expiré après
  GATE1 seul; (4) `gates4.gdb` — version corrigée de (3), **N'ATTEINT
  MÊME PAS GATE2** cette fois. Cet éventail est LA découverte : le
  traçage `ptrace` de GDB change mesurablement le comportement
  d'exécution de cette sonde précise, run après run — pas une nouvelle
  découverte sur le jeu invité, une découverte sur la fiabilité de
  l'instrument (même discipline que CLAUDE.md : "mesurer l'instrument
  avant de lui faire confiance").
- **Décidé d'arrêter le traçage GDB en direct pour cette question** —
  disqualifié après 3 résultats différents sur 4 tentatives sans bug de
  script commun. La preuve STATIQUE (PPC invité ET x86 hôte, indépendantes,
  concordantes) reste le signal fiable : aucune sortie de contrôle entre
  GATE2 et GATE3. La question "pourquoi GATE3 n'a jamais été vu en
  direct" de r103 est mieux expliquée comme un artefact GDB que comme un
  comportement réel. Le modèle structurel r102 reste valable; QUEL garde
  échoue (si un garde échoue du tout) reste non établi — ni confirmé ni
  réfuté ce cycle. Aucun code modifié. Voir
  `reports/ac6-retail-native-codegen-gate2-r104-gdb-live-tracing-unreliable-static-evidence-shows-no-skip-20260901.md`.

# AC6 retail NTSC-U/J — r103 : l'hypothèse r102 corrigée, la vraie divergence reste ouverte (2026-09-01)

- Tracé en direct les 5 gardes de `Function_821D5F48` avec des breakpoints
  filtrés par adresse d'appelant (évite le problème de callee ambigu de
  r102) : reproductible sur 2 runs identiques, seuls GATE1
  (`sub_82338300`) et GATE2 (`sub_821F4078`) sont atteints avant
  `REACHED_CRASH_SITE`.
- **Corrigé l'hypothèse r102** : la valeur de retour réelle de GATE2
  (`r3=0x8feffcb0`, lue via un breakpoint temporaire sur l'adresse de
  retour, offset `PPCContext::r3` = 8 confirmé depuis `rex/ppc/context.h`
  et `types.h`, pas deviné) est NON NULLE — son propre test
  (`cmplwi cr6,r31,0; beq cr6,bailout`) ne se déclenche donc PAS. Le
  modèle "un des 5 gardes échoue par son propre test" est trop simple.
- Relu intégralement le bloc entre le test de GATE2 et l'appel de GATE3
  (~115 instructions, 3 structures locales convergentes) : **aucune
  branche ne sort de cette plage**. Par cette lecture, GATE3 devrait être
  atteint sans condition — pourtant il ne l'est jamais sur 2 runs
  reproductibles.
- Tentative de casser sans condition sur `sub_821CC508` seul : expiré à
  240s sans le moindre coup, ni crash. **Pas interprété comme "jamais
  appelé"** — ce projet a déjà documenté (r100/r101) que les sessions GDB
  attachées se comportent très différemment en timing des runs natifs sur
  cette sonde précise, et le code a un historique connu de comportement
  multi-thread sensible au timing (r90/r91). Refusé de conclure sans
  preuve plus solide qu'un hang de 240s. Aucun code modifié. Voir
  `reports/ac6-retail-native-codegen-gate2-r103-r102-gate-hypothesis-corrected-real-divergence-still-open-20260901.md`.

# AC6 retail NTSC-U/J — r102 : chaîne causale complète — un bailout partagé que l'appelant n'honore pas (2026-09-01)

- Identifié la fonction contenant le site de construction (r101) via
  `GetFuncBounds.java` : `Function_821D5F48` (`0x821d5f48`-`0x821d6c1b`,
  ~820 instructions) — c'est EXACTEMENT le premier appel de
  `sub_821D7DE0` (`0x821d7dec bl 0x821d5f48`). r101 était donc imprécis :
  le chemin ATTEINT bien la fonction contenante, pas nécessairement le
  bloc précis.
- Dumpé la fonction entière (822 lignes) : **aucun `blr`**, exactement
  DEUX sorties (branches partagées vers l'épilogue `0x82382a48`). Le
  bailout précoce (`0x821d6138`, `r3=0`) est la cible PARTAGÉE de 5
  gardes internes distinctes (après `bl 0x82338300`, `0x821f4078`,
  `0x821cc508`, `0x821d28c8`, `0x821d5600`) — n'importe laquelle en échec
  saute directement au bailout, contournant la construction du singleton
  à `0x821d6be8`, atteignable seulement si les 5 réussissent.
- **`sub_821D7DE0` distingue bien l'échec** (appelle un diagnostic
  `sub_821F5B18` seulement si le retour est 0) **mais ne s'arrête pas** —
  les deux chemins reconvergent à `0x821d7e0c` et continuent
  inconditionnellement vers `sub_821D6C20`. Chaîne causale complète, sans
  deviner QUEL garde échoue.
- Tentative en direct (breakpoints GDB) partiellement infructueuse,
  rapportée honnêtement : casser sur les 5 fonctions-garde est ambigu
  (appelées ailleurs aussi); casser sur `Function_821D5F48` elle-même
  (appelant unique confirmé) + `finish` a expiré à 90s (probablement le
  surcoût `ptrace`, pas une preuve que la fonction bloque — le crash
  prouve qu'elle retourne). Piste nommée sans l'affirmer : si
  `sub_821F5B18` est fatal sur le vrai matériel, un stub natif qui
  retourne au lieu d'arrêter expliquerait tout. Aucun code modifié. Voir
  `reports/ac6-retail-native-codegen-gate2-r102-crash-chain-traced-to-shared-bailout-and-swallowed-failure-20260901.md`.

# AC6 retail NTSC-U/J — r101 : le plantage `sub_821D6C20` est un singleton de service lu avant construction sur ce chemin (2026-09-01)

- Tracé mécaniquement le plantage r100 jusqu'à sa cause exacte, sans
  deviner de correctif. `sub_821D6C20` (`DumpRange.java`, projet `ac6-us`)
  répète ~24 fois le motif : charger le pointeur global `0x82935d98`,
  déréférencer sa vtable, appeler le slot `0xE4`. `FindInstructionScalar.java
  0x5d98` : 26 occurrences dans TOUT l'exécutable, toutes des LECTURES,
  toutes dans cette seule fonction.
- **Valeur runtime au moment du plantage lue directement dans le core dump
  `apport`** (pas seulement l'image statique) : `0x00000000` — pointeur
  réellement nul, pas un artefact de formatage. C'est la cause mécanique
  exacte du `bctrl` sauvage (déréférencer une vtable nulle produit une
  cible d'appel indirect invalide).
- **Trouvé le site de construction réel** (`FindPpcAddressMaterialization.java`) :
  un motif singleton paresseux classique juste avant le prologue de
  `sub_821D6C20` (`0x821d6be8 stw r3,0x0(r11)` — le SEUL site d'écriture
  dans tout l'exécutable), qui réussit un appel virtuel différent (slot
  `0xE0`) juste après — l'objet est réel et constructible.
- **`sub_821D7DE0`** (`FindDirectCallsTo.java` : seul appelant de
  `sub_821D6C20`, confirme la trace r100) **n'atteint jamais ce site de
  construction** avant d'appeler `sub_821D6C20` — vérifié par dump complet
  de son corps. Trois explications restent compatibles avec cette preuve
  (boot plus large non atteint par la sonde, race multi-thread que la
  sonde mono-thread ne peut pas gagner, stub d'import/XAM encore no-op) —
  aucune n'est distinguée par ce cycle. **Refusé de deviner** — même
  discipline que r97. Aucun code natif modifié. Voir
  `reports/ac6-retail-native-codegen-gate2-r101-crash-root-cause-uninitialized-service-singleton-20260901.md`.

# AC6 retail NTSC-U/J — r100 : rejet prédicat retiré (évidence bornée), nouveau plantage d'appel indirect dans `_xstart` (2026-09-01)

- Trace statique de `Function_821E6280` commencée (r97-r100, 5 niveaux de
  profondeur) puis abandonnée sur avis externe : la question réelle n'était
  pas "quand ce contenu émet-il le paquet prédiqué" (déjà répondu par
  l'observation runtime) mais "que doit faire le DÉCODEUR", question que la
  trace statique ne pouvait pas trancher plus loin.
- **RETIRÉ le rejet `native_xenos.cpp:169-171`** (`predicated TYPE3 packets
  are not supported`) sur 4 preuves indépendantes déjà établies : (1) les
  DEUX bits prédicat captures sont des constantes figées à la compilation
  (r99), jamais une valeur calculée; (2) le recensement complet des 281
  paquets TYPE3 (r96) ne contient AUCUN opcode de positionnement de
  prédicat, et ce fichier n'en définit aucun; (3) le backend Vulkan
  (`WaitPacket`) ignorait déjà le bit prédicat, ne vérifiant que le
  sélecteur; (4) la branche alternative `WAIT_REG_MEM` (r99) retourne
  directement dans le gestionnaire d'interruption déjà caractérisé
  (r85-r89), pas un second état à modéliser. Politique de décodage
  documentée comme telle (pas une revendication matérielle), lacune
  explicitement énoncée dans le commentaire.
- **Deuxième bug pré-existant trouvé et corrigé** (sans rapport avec le
  prédicat) : `indirect_buffer_decode_error_reports_real_hex_address`
  (introduit r95/r96) fixait `ring[2]=1u` (compte de dwords IB) alors que
  le contenu invité faisait 4 dwords — le décodeur tronquait avant
  d'atteindre le contrôle de plage de registre que le test voulait
  exercer. Vérifié pré-existant (git stash sur l'arbre r99 non modifié,
  même échec). Corrigé (`ring[2]=4u`).
- Vérifié en direct : la sonde dépasse largement le record précédent
  (`vd publish write=49` contre 37 en r96), nouvelles allocations
  jusqu'à 2 Mo (échelle texture/vertex, jamais vue), un `vd swap commit`,
  quatre cycles de publication avec une valeur de fence qui s'incrémente
  (7,9,11,13). **Puis SIGSEGV** (pas l'expiration habituelle) : appel
  indirect corrompu dans `sub_821D6C20` (`call *(%rcx,%rax,1)` avec
  `rax` dans la plage d'un pointeur hôte, pas une adresse invité
  plausible), atteint via `_xstart → sub_821D7DE0 → sub_821D6C20`.
  Nouvelle frontière nommée, pas encore caractérisée. Tous les gates
  requis passent (9/9 ctest, 130/130 pytest scoped à `tests/`, audits
  standards; N2 pré-existant inchangé). Voir
  `reports/ac6-retail-native-codegen-gate2-r100-predicate-decode-fixed-new-indirect-call-crash-20260901.md`.

# AC6 retail NTSC-U/J — r99 : le prédicat rejoint la lacune callback d'interruption déjà connue, pas une inconnue indépendante (2026-09-01)

- TRACÉ les DEUX paquets prédiqués du tampon jusqu'à leur site de
  construction réel (`FindInstructionScalar.java`, pas deviné) :
  `DRAW_INDX_2` (`0x821e14a0`, `Function_821E1248`) — bit prédicat CONSTANTE
  figée à la compilation, jamais calculée à ce site. `WAIT_REG_MEM`
  (`0x821e637c`, `Function_821E6280`, MÊME cluster 0x821E6xxx que
  `sub_821E60A8`) — bit prédicat GENUINEMENT conditionnel (`rlwinm.
  r10,r5,0,0x1d,0x1d; beq` sur bit 2 de l'argument r5), et le chemin
  prédiqué lit `object+0x2a94` et compare contre `0xBADF00D` — EXACTEMENT
  les champs du callback imbriqué déjà caractérisés r87/r88. La branche
  alternative (drapeau clair) retourne immédiatement; l'instruction
  SUIVANTE est l'entrée de `sub_821E63F0` lui-même (le gestionnaire
  d'interruption déjà entièrement désassemblé r87/r88).
- CONCLUSION : ce n'est PAS deux occurrences indépendantes de prédication
  matérielle — le chemin `WAIT_REG_MEM` prédiqué est directement lié à la
  lacune callback d'interruption déjà documentée sur cinq cycles
  (r85-r89) : stub natif no-op, callback jamais invoqué, pointeur
  imbriqué nul par conception. **Ceci renforce, plutôt qu'il ne résout,**
  le refus r97 de deviner — un correctif "prédicat toujours vrai" risque
  maintenant d'interagir mal avec cette lacune déjà connue, pas seulement
  de deviner une sémantique matérielle isolée.
- DÉCISION : toujours aucun correctif de prédicat implémenté. Aucun
  changement de code natif ce cycle.
- OUVERT, deux options plus précisément cadrées que r97 : (1) tracer
  l'appelant de `Function_821E6280` pour voir ce qui détermine le bit 2 de
  r5 et s'il corrèle avec l'enregistrement du callback déjà confirmé
  (r87) — question statique bornée, pas une supposition matérielle;
  (2) le bit prédicat de `DRAW_INDX_2`, constante figée sans condition
  calculée à son site, est le plus sûr des deux si un correctif étroit
  spécifique à cet opcode est voulu — mais n'unbloquerait pas seul le
  tampon (`WAIT_REG_MEM` prédiqué apparaît plus loin, non résolu).

Preuve : `reports/ac6-retail-native-codegen-gate2-r99-predicate-connects-to-interrupt-callback-gap-20260901.md`.

# AC6 retail NTSC-U/J — r98 : opcodes 0x45/0x46 implémentés depuis code réel vérifié; le prédicat reste le blocage vivant (2026-09-01)

- TRACÉ (méthode `FindInstructionScalar.java`, PAS deviné) : `0x45` —
  un seul site statique (`0x821eb918`, dans `sub_821EB8B8`) construit
  exactement le header `0xC0054500` observé, écrivant une forme fixe de
  6 dwords 256 fois, empaquetant 3 tableaux uint16 parallèles fournis par
  l'appelant. Le producteur de ce tableau (`sub_821F00C0`) le remplit par
  division d'un compteur par 127/255 — forme de rampe linéaire/table de
  quantification, cohérente avec (pas prouvée identique à) une table de
  gamma d'affichage ou de tramage.
- `0x46` — construit dans `sub_821EBB40`, qui atteint le MÊME motif
  dépassement curseur/limite → `sub_821E60A8` déjà entièrement
  caractérisé (r93/r94) pour la famille d'écrivains de paquets d'anneau,
  avant d'écrire header `0xC0004600` payload `0xF` (correspond au paquet
  réel exactement) — même famille productrice, même forme structurelle
  que `INVALIDATE_STATE` (0x3B) déjà implémenté.
- CORRIGÉ (implémentation étroite, suivant des précédents établis dans ce
  code, pas devinée) : `kOpcodeSetGammaOrDitherTable`=0x45 traité comme
  `SET_BIN_MASK`/`SELECT` (accepté structurellement, count==6, payload non
  modélisé sémantiquement); `kOpcodeInvalidateStateExtended`=0x46 traité
  IDENTIQUEMENT à `INVALIDATE_STATE` (accepté, count==1, aucun effet
  d'état supplémentaire modélisé).
- VÉRIFIÉ : tests unitaires reproduisant les paquets réels capturés,
  décodage réussi. MAIS sonde vivante INCHANGÉE — le décodage bute
  toujours sur le `DRAW_INDX_2` prédiqué à l'offset 239 (r97, non résolu
  délibérément); 0x45/0x46 se trouvent après ce point dans le flux réel,
  donc ce correctif, bien que correct et vérifié, n'a aucun effet
  observable sur la sonde tant que la question du prédicat n'est pas
  résolue — énoncé clairement plutôt que sous-entendu.
- CTest 9/9 (incluant le nouveau test), pytest 130/130. Gate mission01
  échoue toujours sur le même mismatch N2 préexistant.
- OUVERT : la question du prédicat (r97) reste le seul blocage vivant
  restant sur ce tampon spécifique; identité du nouvel objet bloqué
  `sub_821E6AC8`/`sub_821F03B0` (r94) toujours non lue.

Preuve : `reports/ac6-retail-native-codegen-gate2-r98-opcodes-0x45-0x46-implemented-from-verified-code-20260901.md`.

# AC6 retail NTSC-U/J — r97 : sémantique de prédicat a besoin d'une source externe; 2 blocages + 257 paquets non implémentés cartographiés (2026-09-01)

- CARACTÉRISÉ (pas de code modifié, cycle d'investigation délibéré) : sur
  les 281 paquets TYPE3 du tampon `0x125c0000` déjà capturé (r96), SEULS
  2 portent le bit prédicat (`DRAW_INDX_2`=0x36 offset 239,
  `WAIT_REG_MEM`=0x3c offset 294) — TOUS DEUX déjà des opcodes
  implémentés. Le premier opcode VRAIMENT inconnu (`0x46`) apparaît
  seulement à l'offset 400, après les deux paquets prédiqués; `0x45` se
  répète ensuite 257 fois — l'échelle réelle du travail restant.
- REFUSÉ DE DEVINER : `grep -rn "predicat"` ne trouve QU'UN SEUL résultat
  dans tout le code — le message de rejet lui-même. Aucun registre de
  prédicat, aucun drapeau d'activation, aucun état de requête
  d'occlusion. Le test préexistant (avant cette session) valide
  explicitement ce rejet comme comportement correct — lecture d'un choix
  délibéré, pas un oubli. La règle plausible ("prédicat jamais activé →
  exécuter sans condition", cohérente avec les conventions PM4/CP
  générales) N'EST PAS vérifiée contre ce code, ce contenu, ou une
  documentation Xenos citée dans cet espace de travail — deviner ici
  risquerait une corruption VISUELLE SILENCIEUSE (pire que le rejet
  bruyant actuel), exactement le type de règle non contrôlée que la
  discipline du projet refuse. Politique oracle du projet : "non" pendant
  toute la campagne.
- OUVERT, deux options indépendantes, aucune ne dépend de l'autre :
  (1) obtenir une source vérifiée pour la sémantique de prédicat PM4
  Xenos, correctif alors étroit (`native_xenos.cpp:169-171`); (2) étudier
  les opcodes `0x45`/`0x46` (257+1 paquets) depuis le désassemblage
  invité réel — cible probablement plus haute valeur, et sans le risque
  de corruption silencieuse (un opcode non implémenté échoue bruyamment).
- Aucun changement de gate mission01/ctest ce cycle (pas de code natif
  modifié).

Preuve : `reports/ac6-retail-native-codegen-gate2-r97-predicate-semantics-need-external-source-20260901.md`.

# AC6 retail NTSC-U/J — r96 : registre Xenos élargi depuis contenu réel; TYPE3 prédicat nommé comme prochaine lacune (2026-09-01)

- SCANNÉ (méthode vérifiée) : dump GDB complet du tampon indirect
  2840 dwords à `0x125c0000` (capture base fiable, entrée brute), parsé
  en Python avec la logique de décodage PM4 EXACTE du projet
  (`type_of`/`count_of`/`low_register_of`/`one_reg_of`, copiée, pas
  devinée). Flux propre bout en bout : 371 `TYPE0` + 281 `TYPE3`, aucune
  désynchronisation. Registre maximum réellement touché : `0x5002`.
- CORRIGÉ : `XenosState::kRegisterCount` passé de `0x4000` à `0x8000` —
  borne dérivée du FORMAT (`low_register_of` masque à 15 bits, `0x8000`
  est la plage complète adressable par ce champ), pas une constante
  matérielle devinée. Couvre le besoin observé (`0x5003`) avec marge.
- VÉRIFIÉ EN DIRECT : rejet de plage de registres disparu; le compteur de
  publication d'anneau AVANCE de 31 à 37 dwords (plus de contenu invité
  soumis que jamais observé auparavant) avant un NOUVEAU rejet distinct :
  `predicated TYPE3 packets are not supported (IB 0x125c0000, header
  0xc0003601)` — une garde délibérée (`header & 1`), travail de
  fonctionnalité réel (sémantique d'exécution prédicée), pas un bug, pas
  entrepris ce cycle.
- Tests : `indirect_buffer_decode_error_reports_real_hex_address` (r95)
  utilisait le registre `0x4800`, maintenant dans la plage — corrigé pour
  utiliser un dépassement de compte à la limite haute; nouveau test
  `register_count_covers_type0_full_field_width` reproduisant le paquet
  réel de r95 (accepté maintenant).
- CTest 9/9, pytest 130/130. Gate mission01 échoue toujours sur le même
  mismatch N2 préexistant.
- OUVERT : implémenter l'exécution prédiquée des paquets TYPE3; identité
  du nouvel objet bloqué `sub_821E6AC8`/`sub_821F03B0` (r94, non lue).

Preuve : `reports/ac6-retail-native-codegen-gate2-r96-register-count-widened-predicate-gap-named-20260901.md`.

# AC6 retail NTSC-U/J — r95 : formatage hex du décodeur corrigé; plage de registres nommée précisément, pas corrigée (2026-09-01)

- TROUVÉ ET CORRIGÉ : l'annotation d'erreur `INDIRECT_BUFFER` formatait
  l'adresse IB et le header via `std::to_string()` (DÉCIMAL) sous un
  préfixe littéral `"0x"` — l'alerte r94 sur une adresse IB dépassant
  32 bits (`0x308019200`) était un artefact de formatage : c'est le
  décimal `308019200`, en vrai hex `0x125c0000` — une adresse guest
  ordinaire, valide, présente littéralement dans le dump d'anneau déjà
  capturé par r94. Header : décimal `346112` = vrai hex `0x00054800`.
  Corrigé (`to_hex()`, `snprintf("0x%08x", ...)`), vérifié sur le binaire
  produit réel après resynchronisation de la copie build-tree.
- CARACTÉRISÉ PRÉCISÉMENT (pas corrigé) : header `0x00054800` décodé
  (type=0/TYPE0, base=`0x4800`=18432, count=6) — un besoin RÉEL et
  légitime d'écrire 6 registres consécutifs au-delà de
  `XenosState::kRegisterCount` (`0x4000`=16384). Pas un artefact de
  désynchronisation de décodage (adresse hex maintenant résolvable et
  valide, champs TYPE0 bien formés). Ne PAS agrandir `kRegisterCount` sans
  base vérifiée (borne matérielle Xenos réelle non disponible ici, ou
  scan du tampon indirect complet pour trouver le registre maximum
  réellement touché) — deviner une constante serait exactement le type de
  règle non contrôlée que la discipline du projet refuse.
- CTest 9/9 (incluant le nouveau test), pytest 130/130 (inchangé). Gate
  mission01 échoue toujours sur le même mismatch N2 préexistant.
- OUVERT : scan du tampon indirect `0x125c0000` (2840 dwords) pour une
  borne `kRegisterCount` vérifiée; identité du nouvel objet bloqué
  (r94, non lue).

Preuve : `reports/ac6-retail-native-codegen-gate2-r95-decode-error-hex-fixed-register-range-named-20260901.md`.

# AC6 retail NTSC-U/J — r94 : double bswap EVENT_WRITE_SHD corrigé; le blocage r85-r93 est réellement résolu (2026-09-01)

- TROUVÉ ET CORRIGÉ : `drain_locked()` (`native_guest_vd.cpp`) faisait
  passer les écritures `EVENT_WRITE_SHD` par `gpu_swap()` PUIS
  `store_guest_word()` — le premier émule l'unité d'échange d'octets du
  GPU (son résultat EST déjà le motif d'octets final), le second applique
  SON PROPRE bswap (correct pour une valeur logique normale, faux ici) —
  deux échanges qui se composent au lieu d'annuler une seule
  transformation voulue. Vérifié algébriquement (Python, arithmétique
  exacte) AVANT toute modification, contre les DEUX valeurs vivantes
  observées : `raw=5` → octets actuels `05 00 00 00` (correspond
  EXACTEMENT aux lectures r89/r91/r93, jamais expliquées) → octets
  corrigés `00 00 00 05` (=5); `raw=0x162e00d4` → octets actuels
  `d4 00 2e 16` (pas une adresse valide) → octets corrigés
  `16 2e 00 d4` (=0x162e00d4, une adresse propre dans la plage anneau déjà
  tracée). Correctif : nouvelle fonction `store_guest_bytes_raw()` (memcpy
  brut, pas de second bswap), utilisée à la place de `store_guest_word()`
  pour ce chemin uniquement.
- VÉRIFIÉ EN DIRECT, isolé par reconstruction A/B (fichier unique
  git-stashé puis restauré) : SANS le correctif, comportement r93 identique
  (une seule séquence publish/accepted, pas de boucle). AVEC le correctif,
  comportement RÉELLEMENT différent : la même étape se répète en boucle
  (dizaines de milliers de fois/30s), maintenant REJETÉE
  (`decode_ok=0 code=5 TYPE0 register range exceeds Xenos state, IB
  0x308019200`) — un nouveau problème séparé, non caractérisé ce cycle.
- **PROUVÉ (GDB, thread principal) : le blocage r85-r93 est RÉELLEMENT
  RÉSOLU.** Le thread principal est de nouveau dans
  `sub_821E6AC8←sub_821E61A8` mais atteint par une chaîne d'appel
  ENTIÈREMENT DIFFÉRENTE et jamais vue auparavant :
  `←sub_821F03B0←sub_8234F558←sub_8233E0A8←sub_8233B5A0` (contre
  `←sub_821E64A8←sub_821E65B0←sub_8234F2C8` à chaque cycle précédent). Le
  thread est revenu de l'ancienne chaîne, a exécuté du code invité réel
  jamais atteint auparavant, et bloque maintenant sur une NOUVELLE
  instance du même mécanisme d'attente — objet probablement différent,
  non identifié ce cycle.
- TROUVÉ (incident, en vérifiant que le nouveau test s'exécuterait
  réellement) : les 8 fichiers de test natifs retail utilisaient
  `assert()` SANS garde `#ifdef NDEBUG/#error` — exactement la classe de
  défaut déjà corrigée pour la piste demo mais jamais appliquée au retail.
  CMake force déjà `-UNDEBUG` sur ces cibles (pas de bug actif), mais à un
  seul retrait de flag CMake près. Corrigé (garde ajoutée aux 8 fichiers);
  `audit_test_assert_liveness.py` passe maintenant propre (suites=8
  vacuous=0).
- CTest 9/9 (incluant le nouveau test), pytest 130/130 (inchangé, aucun
  changement Python ce cycle). Gate mission01 échoue toujours sur le même
  mismatch N2 préexistant, sans rapport.
- AUCUNE revendication de boot/titre/gameplay — la sonde expire toujours;
  ceci est une progression réelle confirmée au-delà d'un blocage
  spécifique longuement investigué, pas un nouveau jalon atteint.
- OUVERT : identité du nouvel objet bloqué (non lue); nature du nouveau
  rejet de décodage `TYPE0`/`IB 0x308019200` (adresse >32 bits, suspect,
  à vérifier en premier) — frontière concrète du prochain cycle.

Preuve : `reports/ac6-retail-native-codegen-gate2-r94-double-endian-swap-fixed-main-thread-unblocked-20260901.md`.

# AC6 retail NTSC-U/J — r93 : voie threading épuisée; passage 76-sites démarré, pas fermé (2026-09-01)

- RÉGLÉ : la voie threading hôte (r90-r92) est épuisée comme piste vers le
  jalon Gate 2. Sonde 60s (`AC6_NATIVE_IMPORT_TRACE`) : mêmes 100 hits
  génériques qu'à 20-25s. Sonde 40s (`AC6_NATIVE_VD_TRACE`) : même
  séquence qu'établie en r11 (`PM4_ME_INIT` 19 dwords + lot IB 12 dwords),
  rien de plus. `__imp__VdSwap` : 0 hit/30s — attendu, pas nouveau
  (`poll_once()`/`poll_loop()` publie le premier tampon avant tout appel
  VdSwap, per commentaire déjà présent dans `native_guest_vd.cpp:135-138`).
- DÉMARRÉ (pas fermé) : le passage différé sur les 76 sites d'appel de
  `sub_821E60A8` (décliné en r79, r88, r89). `FindDirectCallsTo.java`
  confirme 76 sites; 10 dans le cluster 0x821E6xxx/0x821F1xxx déjà
  caractérisé, vérifiés en premier.
- TROUVÉ (nouveau) : 6 de ces 10 sites (dont `sub_821E64A8` lui-même,
  déjà dans la chaîne d'attente tracée) partagent le motif
  `if (*(object+0x30) > *(object+0x38)) sub_821E60A8(object)` — un
  filet de sécurité de dépassement, pas un déclencheur d'interruption.
  **`sub_821E64A8` est en réalité un ÉCRIVAIN de paquets d'anneau** :
  écrit deux dwords (`0x5c8`, `0x00020000`, forme d'en-tête/charge PM4) au
  curseur via `stwu`, avance et réécrit le curseur dans `object+0x30`,
  puis appelle `sub_821E61A8(object,4)` — l'attente déjà tracée — si
  `object+0x2a9c` (=7, r85) est non nul.
- PROUVÉ (vivant, capture base fiable à l'entrée brute, méthode r85/r88) :
  `object+0x30`=`0x162e017c` (correspond r85), `object+0x38`=`0x162eff60`
  (correspond indépendamment au `limit=` déjà imprimé par le trace VD
  existant depuis l'ère r11/r75 — champ maintenant identifié),
  `object+0x2af8`=0 (déjà satisfait), `*(0x164e0000)+0`=`05 00 00 00`
  inchangé depuis r91. Curseur ~65 Ko sous la limite : le filet de
  dépassement n'est PAS actuellement déclenché — deuxième mécanisme
  indépendant confirmé non-actif, sans rouvrir la réfutation r88/r89 du
  callback d'interruption.
- DÉCISION : 10/76 sites vérifiés, pas 76. Les 66 restants (majoritairement
  0x821Dxxxx, territoire allocateur générique r79) ne sont pas vérifiés —
  inconnue honnête, pas supposée sans danger. Prochain : passage par lot
  sur les 66 restants, OU vérifier si la boucle appelante de
  `sub_821E65B0` (1112 octets) est elle-même conditionnée par quelque
  chose que ce runtime pourrait faire avancer — piste plus étroite que le
  callback d'interruption déjà réfuté.
- CTest 9/9 (aucun code natif modifié ce cycle). Gate mission01 échoue
  toujours sur le même mismatch N2 préexistant, sans rapport.

Preuve : `reports/ac6-retail-native-codegen-gate2-r93-threading-avenue-exhausted-76-site-pass-started-20260901.md`.

# AC6 retail NTSC-U/J — r92 : appelants événements réglés (négatif), visibilité `DbgPrint` ajoutée (2026-09-01)

- RÉGLÉ (statique, `FindDirectCallsTo.java` sur les adresses guest de
  `NtSetEvent`=0x823D015C/`NtClearEvent`=0x823D017C) : question ouverte de
  r91. `NtClearEvent` a UN appelant direct (`sub_821F4210`), lui-même
  appelé depuis **neuf** sites distincts à travers six fonctions parentes
  ordinaires de tailles variées (80-368 instructions) — motif « effacer un
  drapeau de statut » diffus, pas une boucle dégénérée. `NtSetEvent` a
  ZÉRO appelant direct — atteint uniquement via le dispatch de callbacks
  indirect déjà caractérisé en r91 (`sub_821F7C80`). **Négatif** : aucun
  spin invité trouvé dans le graphe d'appel statique; le volume mesuré
  s'explique par l'absence de régulateur de fréquence d'image dans cette
  sonde offline, pas par un défaut.
- AJOUTÉ : gestionnaire `DbgPrint` (gated `AC6_NATIVE_IMPORT_TRACE=1`,
  lecture brute bornée à 512 octets via `PPC_LOAD_U8`, sans substitution
  varargs — jugée non vérifiable en sécurité depuis seulement deux sites
  d'appel statiques). Site d'appel 1 (`sub_821EF458`) pré-formate déjà via
  un helper type `vsnprintf` avant `DbgPrint` (aucun vararg vivant au point
  d'appel réel); site 2 (`sub_821F5ED0`) passe une chaîne littérale + un
  entier brut. Vérifié SÛR (compile, 17/17 tests) mais **zéro ligne
  `[DbgPrint]` sur une sonde bornée de 25s** — négatif honnête, aucun des
  deux sites n'est atteint dans cette fenêtre; gardé pour une sonde future
  plus longue.
- CTest 9/9, pytest 130/130 (129+1 nouveau). Gate mission01 échoue
  toujours sur le même mismatch N2 préexistant, sans rapport.
- OUVERT : aucune piste étroite restante sur ce fil événements; prochain
  cycle devrait soit continuer le survol par fréquence mesurée
  (`AC6_NATIVE_IMPORT_TRACE`), soit évaluer un régulateur de cadence
  minimal.

Preuve : `reports/ac6-retail-native-codegen-gate2-r92-event-callers-settled-dbgprint-added-20260901.md`.

# AC6 retail NTSC-U/J — r91 : `wait_event` bloque désormais réellement; l'impact agrégat reste ouvert (2026-08-31)

- TROUVÉ (statique) : `sub_821F7C80` (appelé par `sub_821F8008`, un des
  threads worker activés par r90) parcourt une liste chaînée à adresse
  statique fixe et invoque le pointeur de fonction de chaque nœud via
  `bctrl` — un motif ordinaire de dispatch de callbacks/notifications, pas
  une boucle manifestement cassée.
- PROUVÉ (mesuré, `strace -f -c`, sonde bornée 15s, runtime multi-thread) :
  **1 547 456 appels `futex`** (619 105 en erreur), tous imputables au
  mutex `g_event_mutex` partagé par `create_event`/`set_event`/
  `clear_event`/`wait_event` — même famille de constat que r90
  (busy-spin mesuré), mais sur la famille d'attente d'événements, déjà
  identifiée comme lacune connue avant même le début de cette
  investigation (`wait_event()` retourne immédiatement au lieu de
  bloquer).
- CORRIGÉ : `wait_event()` bloque maintenant réellement (jusqu'à 2 ms) sur
  un `std::condition_variable`, réveillé par `set_event`/`create_event`
  signalé — contrat inchangé pour l'appelant (toujours `STATUS_TIMEOUT` si
  non satisfait). Vérifié en direct (GDB) : un thread réellement dans
  `pthread_cond_wait`, pas en spin.
- MESURÉ (points d'arrêt GDB comptés, fenêtre 12s) : la famille
  événements est réellement sollicitée à un rythme soutenu — `NtSetEvent`
  13 873, `NtClearEvent` 11 619, `wait_event` 34 509, `NtCreateEvent` 24,
  `KeSetEvent`/`KeResetEvent`/`NtPulseEvent` 0.
- NON ÉTABLI, explicitement laissé ouvert : le volume `futex` agrégat
  (`strace`) après correctif (1 501 150, 254 477 erreurs) est
  statistiquement indiscernable d'avant (1 547 456) — comparer ce chiffre
  au taux mesuré par points d'arrêt GDB (~5 000/s) n'est pas valide, les
  deux instruments ayant des surcoûts non comparables. Ni "le correctif a
  éliminé le spin" ni "le correctif n'a rien changé" n'est une conclusion
  tenue par la preuve — refusé comme règle plausible sans contrôle.
- CTest 9/9, pytest 129/129 (128+1 nouveau). Gate mission01 échoue
  toujours sur le même mismatch N2 préexistant, sans rapport.
- OUVERT : ce trafic `NtSetEvent`/`NtClearEvent` est-il une signalisation
  moteur légitime par image (rien ne cadence encore à une fréquence cible
  dans cette sonde offline) ou un motif de spin côté invité — nécessite une
  trace statique des appelants réels, non entreprise ce cycle.

Preuve : `reports/ac6-retail-native-codegen-gate2-r91-wait-event-blocks-aggregate-impact-open-20260831.md`.

# AC6 retail NTSC-U/J — r90 : un busy-spin `NtReleaseMutant` bloquait la progression des threads worker, corrigé (2026-08-31)

- AJOUTÉ : diagnostic permanent `AC6_NATIVE_IMPORT_TRACE=1` (imite
  `AC6_NATIVE_VD_TRACE`) sur le chemin générique `kOfflineStatus` de
  `materialize_native_import_stubs.py` — quels imports sans gestion
  spécifique sont réellement atteints à l'exécution.
- PROUVÉ (sonde bornée, 20s, runtime multi-thread par défaut) : sur
  1 263 904 appels au chemin générique, **631 941 `NtReleaseMutant`** et
  **631 902 `RtlNtStatusToDosError`** — un busy-spin réel et mesuré (pas
  inféré), `NtReleaseMutant` n'ayant aucune gestion spécifique et
  retournant systématiquement un statut d'échec.
- CORRIGÉ : `NtReleaseMutant`/`NtReleaseSemaphore` retournent maintenant le
  succès immédiatement (même idiome déjà établi pour
  `RtlEnterCriticalSection`/`RtlLeaveCriticalSection` — un seul thread
  invité tant que le scheduler n'est pas migré, aucune contention réelle
  modélisée). Vérifié : la même sonde tombe de 1 263 904 à **100** appels
  génériques après correction.
- PROUVÉ (GDB, tous threads) : **10 threads maintenant actifs** (contre
  très peu auparavant), plusieurs avec des piles d'appel jamais vues
  (`sub_821F4210`, `sub_821D4C20`/`821D4F20`, `sub_821F8008`) et une
  contention réelle sur `g_event_mutex`. Le thread principal reste
  **inchangé**, toujours arrêté dans exactement la même chaîne
  `sub_821E6AC8←sub_821E61A8←sub_821E64A8←sub_821E65B0` fermée par r85-r89 —
  ce correctif ne contredit ni ne rouvre ce négatif borné, c'est un blocage
  séparé sur d'autres threads.
- CTest 9/9, pytest 128/128 (126+2 nouveaux). Gate mission01 échoue toujours
  sur un mismatch N2 préexistant, sans rapport
  (`reconstruction/ace-combat-6/src/retail_session.cpp`, déjà modifié avant
  ce cycle, piste N2 abandonnée).
- OUVERT : ce que font réellement les threads worker nouvellement actifs
  n'est pas établi — cible naturelle d'un prochain passage statique.

Preuve : `reports/ac6-retail-native-codegen-gate2-r90-mutant-release-busy-spin-20260831.md`.

# AC6 retail NTSC-U/J — r89 : le verrou de complétion confirme r87/r88, négatif borné (2026-08-31)

- TROUVÉ (statique, `FindStoresAtDisplacement.java 0x2abd`) :
  `sub_821E60A8` (0x821e60a8-0x821e61a0, corps complet vérifié) contient sa
  propre structure verrou/relecture, jusqu'ici non tracée : trois portes
  séquentielles (`object+0x2abc` bit 0x80 clair, un drapeau global non nul,
  `object+0x2abd` bit 0x2 clair) gardent un appel DIRECT à
  `sub_821E61A8(object, limite-2, 0)` — la même fonction d'attente que le
  thread bloqué — suivi d'un verrou de complétion à usage unique
  (`object+0x2abd` bit 0x2, posé en sortie).
- PROUVÉ (vivant, sonde mono-thread, lecture octet-par-octet à l'arrêt
  confirmé dans `__imp__sub_821E6AC8`) : `object+0x2abd` = `0x00` — le
  verrou n'a JAMAIS été posé pour cet objet. `object+0x540c` = tout zéro
  (rôle non établi ce cycle). `*(0x164e0000)+0x0` = octets bruts
  `05 00 00 00`, volontairement NON interprété numériquement — ce champ
  n'est écrit par aucun site connu de `native_guest_vd.cpp`
  (`drain_locked()` écrit `+0x3c`, pas `+0x0`), donc son sens grand-boutien
  vs. hôte reste ouvert plutôt que deviné.
- DÉCISION : ces lectures CONFIRMENT r87/r88 (le callback d'interruption ne
  se déclenche jamais → `sub_821E60A8` n'atteint jamais son verrou pour cet
  objet) par une preuve mémoire vivante indépendante, sans ouvrir de
  nouveau mécanisme de déblocage. Négatif borné pour ce sous-fil
  (`sub_821E6AC8`/`0x10001a00`) : cinq cycles (r85-r89) ont établi ce qui
  est prouvé et ce qui ne l'est pas; le passage statique sur les 76 sites
  d'appel de `sub_821E60A8` reste hors de portée pour l'instant. Retour à
  la liste scheduler/kernel/VFS/XAM plus large de NEXT.md au prochain
  cycle.
- HYGIÈNE SESSION : job cron ponctuel dupliqué `833b1b98` supprimé (restait
  actif en parallèle du job récurrent `1874bb39`); arrêt des rappels
  `ScheduleWakeup` en fin de cycle (mécanisme du mode dynamique, pas du
  mode à intervalle fixe) — les deux ensemble doublaient la cadence réelle
  de la boucle.

Preuve : `reports/ac6-retail-native-codegen-gate2-r89-latch-confirms-r87-bounded-negative-20260831.md`.

# AC6 retail NTSC-U/J — r88 : la chaîne r87 n'atteint pas l'écriture de déblocage (2026-08-31)

- CORRIGÉ : relecture précise de `sub_821E63F0` — l'offset `+0x10` du bloc
  `object+0x2a94` N'EST PAS un sentinel séparé, c'est le POINTEUR DE
  FONCTION du sous-callback lui-même (le test `0xBADF00D` est une garde
  anti-poison, pas une condition normale); `+0x14` est son contexte. Quand
  ce pointeur est NUL (confirmé : c'est le cas ici, le bloc est fraîchement
  `memset`é à zéro par `sub_821E65B0` et jamais réécrit — seul écrivain via
  déréférencement dans toute l'image : le handler lui-même, à l'offset
  +0x0, pas +0x10/+0x14), le gestionnaire d'interruption SAUTE
  intentionnellement l'appel du sous-callback.
- PROUVÉ (runtime, confirmation indépendante n°3 de l'identité d'objet) :
  `ctx.r3=0x821E63F0`, `ctx.r4=0x10001a00` au point d'entrée réel de
  `VdSetGraphicsInterruptCallback`.
- DÉCISION : même une correction du stub natif de cet import
  n'atteindrait PAS `sub_821E60A8`/`sub_821E5D60` par ce chemin — la
  chaîne causale de r87 ne tient pas telle que tracée. Rétractée comme
  mécanisme de correction (les faits bruts restent valables). Après quatre
  cycles à resserrer puis fermer des mécanismes spécifiques sans trouver
  la réponse finale, la prochaine étape doit reconsidérer la portée plutôt
  que proposer une cinquième hypothèse : soit documenter un négatif borné
  pour ce sous-fil et revenir à la liste plus large de NEXT.md
  (scheduler/kernel), soit s'engager dans un passage statique
  substantiellement plus coûteux (les 76 sites d'appel de
  `sub_821E60A8`) seulement si jugé utile.

Preuve : `reports/ac6-retail-native-codegen-gate2-r88-nested-callback-is-null-by-design-20260831.md`.

# AC6 retail NTSC-U/J — r87 : le callback d'interruption Vd retail n'est jamais invoqué (2026-08-31)

- PROUVÉ : `VdSetGraphicsInterruptCallback` a deux appelants statiques.
  `0x821f1220` enregistre réellement `callback=0x821E63F0, context=self`;
  `0x821f15fc` désenregistre (0,0), suivi d'un appel de fermeture à
  `sub_821E65B0` (chemin d'arrêt, sans rapport).
- PROUVÉ : `sub_821E63F0` est le vrai gestionnaire d'interruption Vd/CP —
  garde `source==1`, valide un sentinel de corruption (`0xBADF00D`, trap
  sinon), charge un SOUS-callback enregistré à `context+0x2a94+0x14` et
  l'invoque via `bctrl`, puis efface un bit de statut sous verrou.
- PROUVÉ : le stub natif `VdSetGraphicsInterruptCallback`
  (`materialize_native_import_stubs.py`) est un no-op complet — il
  n'enregistre rien et n'invoque jamais le callback. `sub_821E63F0` n'est
  donc JAMAIS appelé par ce runtime, en aucune circonstance.
- OUVERT : la chaîne du sous-callback (`context+0x2a94+0x14`) jusqu'à
  `sub_821E60A8`/`sub_821E5D60` (l'écriture de déblocage déjà identifiée)
  n'est pas encore tracée instruction par instruction — seule la forme est
  établie.
- DÉCISION : toujours aucune implémentation — tracer le sous-callback
  d'abord. Si confirmé, le correctif est de faire enregistrer et invoquer
  ce callback par le service Vd natif (via `PPC_LOOKUP_FUNC`, comme le
  stub `ExCreateThread`) lors d'une VRAIE progression d'anneau/IB observée
  dans `drain_locked()` — jamais sur un minuteur fixe ni inconditionnellement.

Preuve : `reports/ac6-retail-native-codegen-gate2-r87-interrupt-callback-never-fires-20260831.md`.

# AC6 retail NTSC-U/J — r86 : le pipeline PM4/Vd fonctionne; l'attente bloquée est un sous-allocateur adjacent (2026-08-31)

- PROUVÉ (trace `AC6_NATIVE_VD_TRACE=1`, diagnostic déjà existant dans
  `native_guest_vd.cpp`) : le pipeline PM4/Vd natif fonctionne — objet
  `0x10001a00` découvert, `PM4_ME_INIT` (19 dwords) puis le lot IB (12
  dwords) tous deux ACCEPTÉS. Rien n'est bloqué côté anneau graphique; ceci
  correspond exactement à ce qui était déjà établi (r56-r75, rapport r11).
- PROUVÉ : le readback réel écrit par `drain_locked()` est
  `object+0x2a90` (=`0x164e0000`) **+ 0x3c** = `0x164e003c`. Mais
  `sub_821E61A8` déréférence l'offset **+0x0** de ce même bloc — un champ
  différent. Séparément, `object+0x2a9c` (la "limite" comparée) est stable
  à **7**, sans rapport avec les indices d'écriture réels de l'anneau (19
  puis 31).
- REFORMULATION : l'attente générique `sub_821E61A8`/`sub_821E64A8`/
  `sub_821E65B0` (dont les faits de flot de contrôle de r77-r83 restent
  valables) est un **sous-allocateur séparé, adjacent** — probablement un
  tampon de mise en scène de liste de commandes partageant le même objet
  "périphérique graphique" que l'anneau Vd, mais drainé par un mécanisme
  différent, non encore identifié. Ce n'est probablement PAS un blocage du
  pipeline graphique lui-même.
- DÉCISION : ne rien implémenter. Prochaine étape : trouver quel code
  retail (pas le côté attente) écrit `object+0x2a90+0x0` directement (pas
  via le bloc readback `+0x3c`) pour comprendre ce compteur de mise en
  scène et son mécanisme de drainage réel.

Preuve : `reports/ac6-retail-native-codegen-gate2-r86-vd-pipeline-works-generic-wait-is-separate-20260831.md`.

# AC6 retail NTSC-U/J — r85 : CORRECTION, r77-r84 ont tracé le mauvais objet (2026-08-31)

- CORRIGÉ (majeur) : r78 a dérivé "l'objet bloqué" via `frame 1` + `print
  $rbp` pendant que GDB était arrêté dans `sub_821E6AC8` (frame 0) — une
  technique non fiable sur un binaire sans info de debug (le `%rbp` affiché
  reflétait probablement l'usage LOCAL de `sub_821E6AC8` lui-même, pas une
  valeur correctement dérouler pour la frame appelante). Cela a donné
  `0x1a0010`, jamais recoupé avant ce cycle.
- PROUVÉ : une méthode fiable (breakpoint à l'entrée BRUTE de
  `sub_821E64A8`, lecture directe de `ctx.r3` via `$rdi`, aucun changement
  de registre encore effectué) donne **`object = 0x10001a00`** — qui
  correspond EXACTEMENT à l'objet Vd/PM4 déjà établi dans
  `reports/ac6-retail-native-codegen-gate2-r11-20260831.md` (tranche r53,
  "l'objet à 0x10001a00, le ring 0x162d0000 et le readback"), une
  investigation totalement indépendante d'avant ce fil r77.
- PROUVÉ : avec l'objet correct, `+0x2a90` déréférence exactement l'adresse
  de readback déjà connue `0x164e0000`, et `+0x30` est un curseur proche de
  l'anneau déjà connu `0x162d0000`; `+0x2a9c=7`. Les trois champs sont
  STABLES sur 200 itérations de la boucle (sonde à thread unique,
  déterministe) — aucun des constats "objet null"/"contradiction" de
  r77-r84 ne s'applique à cet objet.
- DÉCISION : rétracter l'identité `0x1a0010` et tout ce qui en dépend
  directement (valeurs de champs, récits "allocation NULL"/"ordre
  d'init"/"contradiction de porte"). Les faits de flot de contrôle
  (structure de `sub_821E64A8`/`sub_821E61A8`/`sub_821E6AC8`/
  `sub_821E5D60`/`sub_821E6A08`) restent valables, lus depuis le
  désassemblage indépendamment de l'objet. Reconnecte l'investigation à
  l'item déjà ouvert r53 : "le consommateur PM4/Vd natif n'est pas encore
  relié" — prochaine étape : vérifier ce lien avec
  `native/src/native_guest_vd.cpp` avant toute nouvelle piste.

Preuve : `reports/ac6-retail-native-codegen-gate2-r85-wrong-object-corrected-20260831.md`.

# AC6 retail NTSC-U/J — r84 : bug d'endianness dans la méthode, chronologie réelle établie (2026-08-31)

- EXÉCUTÉ : la variante de sonde à thread unique proposée par r83 (no-op
  temporaire et non committé de `ExCreateThread`, changement révoqué après
  usage, CTest 9/9 revérifié après revert).
- CORRIGÉ (méthode) : `x/1xw` de GDB affiche la mémoire invitée (big-endian)
  comme si elle était little-endian. La lecture "0x05000000" de ce cycle
  était en réalité **5** une fois corrigée — cohérent avec l'init
  `sub_821E65B0` (= 3 si ancien était zéro, nouvellement lu ce cycle) plus
  un push via `sub_821E5D60` (+2, r79). Les lectures à zéro de r80-r83
  restent valables (zéro est invariant par endianness) — aucune conclusion
  antérieure à corriger.
- PROUVÉ : le premier appel à `sub_821E65B0` (objet frais, +0x2a9c=0) saute
  bien l'appel à `sub_821E64A8` (confirmé octet par octet sur le code x86
  compilé); le deuxième appel, avec +0x2a9c=5 (réel), entre dans l'appel —
  c'est CE MÊME appel qui finit bloqué.
- OUVERT (resserré, pas résolu) : les deux seuls écrivains statiques de
  +0x2a9c (`sub_821E5D60`: +=2; `sub_821E65B0`: =3 si zéro) n'écrivent
  jamais zéro. Comment le compteur redevient zéro plus tard dans le même
  appel bloqué reste sans écrivain identifié.
- DÉCISION : noter la correction d'endianness pour toute lecture GDB
  future de cette investigation. Prochaine étape : relire +0x2a9c à
  plusieurs points À L'INTÉRIEUR du même appel bloqué (pas seulement à la
  porte) avec la correction appliquée, sur la sonde à thread unique.

Preuve : `reports/ac6-retail-native-codegen-gate2-r84-endian-misread-and-timeline-20260831.md`.

# AC6 retail NTSC-U/J — r83 : l'escalade n'est que télémétrie; l'adresse de l'objet reste inexpliquée (2026-08-31)

- RÉFUTÉ : `sub_821E6A08` (l'escalade appelée au timeout de
  `sub_821E61A8`) ne touche aucun des champs `object` — c'est un compteur
  de performance pur (accumulation `mftb`, callback de profiling optionnel).
  N'explique pas la contradiction r82.
- NOUVEAU FAIT : `object=0x1a0010` (reproduit 5+ fois) est entièrement hors
  de l'image XEX statique (`0x82000400..0x82ac2dc7`) ET sous la plage de
  l'allocateur bump `NtAllocateVirtualMemory` (`0x10000000..0x7f000000`,
  r47). N'explique pas encore la contradiction, mais exclut une
  coïncidence avec les régions bootstrap PCR/thread (r49) et resserre
  l'hypothèse vers un pointeur du tas propre au jeu.
- DÉCISION : arrêter d'itérer des hypothèses une à une sur cette
  contradiction (rendements décroissants, même mise en garde que le scan
  d'appels à 76 sites de r79). Prochaine étape concrète : construire une
  variante de sonde à thread unique (no-op temporaire, non committé, du
  spawn `ExCreateThread`) pour éliminer les ~16-18 threads hôtes
  concurrents qui ont rendu les watchpoints peu fiables (r82), puis
  relire la porte de `sub_821E65B0` au point exact du check.

Preuve : `reports/ac6-retail-native-codegen-gate2-r83-escalation-is-telemetry-20260831.md`.

# AC6 retail NTSC-U/J — r82 : hypothèse r81 réfutée, nouvelle contradiction ouverte (2026-08-31)

- RÉFUTÉ : `sub_82331CA8` et le tronçon de `sub_821D5F48` autour de
  `0x821d6008` sont entièrement linéaires, sans branche conditionnelle.
  L'hypothèse r81 ("point d'entrée gardé prématurément satisfait") ne
  tient pas.
- OUVERT (contradiction non résolue) : la porte de `sub_821E65B0`
  (`0x821e65c4-0x821e65dc`) exige `object+0x2a9c!=0` ET `object+0x30!=0`
  pour atteindre `sub_821E64A8` — exactement la frame observée sur la pile
  de Thread 1 à chaque capture (r78/r80/r81, reproduit 5+ fois). Pourtant
  la lecture live de r80, plus profonde dans la même pile, montre ces deux
  champs à zéro, sans écriture intermédiaire trouvée ni autre thread
  touchant ces fonctions.
- INSTRUMENTS ESSAYÉS, NON CONCLUANTS : point d'arrêt conditionnel sur
  l'entrée de `sub_821E64A8` (jamais déclenché en 20s, probablement trop
  lent pour la fenêtre de blocage); watchpoints matériels/logiciels sur les
  trois champs (déclenchements bruyants attribués à des threads sans
  rapport — `NativeGuestVdService::poll_loop`, `NtClearEvent`,
  `NtWaitForSingleObjectEx` — cohérent avec une émulation de watchpoint
  logiciel peu fiable sur ~16-18 threads hôtes concurrents; écarté comme
  instrument inadapté ici, pas comme négatif fiable).
- DÉCISION : ne pas répéter ces deux expériences telles quelles. Prochaine
  étape : réduire la sonde à un seul thread vivant (stub temporaire de
  spawn `ExCreateThread`) avant de retenter, OU revérifier statiquement si
  la porte de `sub_821E65B0` opère sur de la mémoire fraîchement allouée
  potentiellement non significative la première fois, indépendamment de la
  question du handle de tas `0x8293B970`.

Preuve : `reports/ac6-retail-native-codegen-gate2-r82-gate-contradiction-open-20260831.md`.

# AC6 retail NTSC-U/J — r81 : le push d'anneau tourne avant la création de son propre tas (2026-08-31)

- PROUVÉ (runtime) : le global `0x8293B970` (handle de tas lu par
  `sub_821D74A8`) est entièrement à zéro au moment du blocage.
- PROUVÉ (statique) : le seul site d'écriture de `-0x4690(r31)` dans toute
  l'image est `sub_821D5F48:0x821d6200` — et `sub_821D5F48` est déjà une
  frame ancêtre dans notre propre pile capturée. L'appel qui descend vers le
  push d'anneau (`0x821d6008: bl sub_82331CA8`) précède l'écriture du handle
  (`0x821d6200`, via `bl sub_82222d80` à `0x821d61c8`) de 488 octets dans le
  MÊME flot d'exécution linéaire, sans boucle entre les deux.
- OUVERT : ce n'est probablement pas un bug d'ordre réel du jeu retail
  (peu plausible pour un titre shippé) — plus probablement, `sub_82331CA8`
  est un point d'entrée générique normalement gardé par un état/drapeau que
  notre couche HLE satisfait prématurément. Ce garde n'est pas encore
  localisé.
- DÉCISION : toujours aucune implémentation — écrire une valeur non-nulle
  synthétique masquerait un vrai bug d'ordonnancement plutôt que de le
  corriger. Prochaine étape statique : trouver ce qui garde `sub_82331CA8`
  (ou un ancêtre jusqu'à `_xstart`) de s'exécuter avant `0x821d6008` en
  exécution réelle.

Preuve : `reports/ac6-retail-native-codegen-gate2-r81-heap-init-ordering-20260831.md`.

# AC6 retail NTSC-U/J — r80 : l'allocation de l'objet bloqué a échoué (NULL), preuve runtime (2026-08-31)

- PROUVÉ (runtime, GDB sur la sonde bornée existante) : l'objet réellement
  bloqué est `0x1a0010` (reproduit identiquement sur deux runs
  indépendants), base mémoire invité `0x7ffef7000000`. `PPC_CONFIG_NON_VOLATILE_AS_LOCAL`
  est actif : r31/"object" vit dans `%rbp` hôte (établi depuis le prologue
  de `sub_821E61A8`), la base dans `%r14`.
- PROUVÉ : TOUS les champs pertinents de cet objet sont à zéro au moment du
  blocage — `+0x2abd=0x00`, `+0x540c=0`, `+0x34bc=0`, **`+0x2a90=0`**,
  `+0x2a9c=0`, `+0x30=0`, `+0x38=0`, `+0x2af8=0`, `+0x2a94=0`,
  `+0x3a44=0`, `+0x3a48=0`. L'objet n'a jamais été initialisé, ce n'est pas
  un anneau partiellement avancé.
- DIAGNOSTIC : `+0x2a90` (le pointeur déréférencé par l'attente) étant NULL,
  la comparaison `sub_821E61A8` est `0 >= 4`, faux par construction — la
  mémoire réservée non mappée se lit comme zéro sans fault. Le seul site
  d'écriture (`sub_821E65B0:0x821e6740`) stocke sans vérification le retour
  de l'allocateur `sub_821D74A8`; un retour NULL explique exactement l'état
  observé. Tracé un niveau plus loin : `sub_821D74A8` est un wrapper de pool
  avec verrou, qui appelle l'allocateur réel `sub_82222d80` via un handle de
  tas global (`-0x4690(r11)`); ce dernier n'est pas encore examiné.
- DÉCISION : ne rien implémenter. Prochaine étape : `sub_82222d80` et le
  handle de tas global — statique d'abord, puis vérifier si cela aboutit à
  un import kernel (`ExAllocatePool`-shaped) dont le stub générique
  `kOfflineStatus` de `materialize_native_import_stubs.py` serait la cause
  racine. N'écrire aucune valeur non-nulle synthétique dans `+0x2a90` avant
  cette identification.

Preuve : `reports/ac6-retail-native-codegen-gate2-r80-null-ring-allocation-20260831.md`.

# AC6 retail NTSC-U/J — r79 : l'objet est un allocateur générique, pas l'anneau Vd (2026-08-31)

- PROUVÉ : le seul site qui écrit `*(object+0x2a90)` avec la valeur qui
  débloquerait `sub_821E61A8` est `sub_821E5D60` (appelé depuis
  `sub_821E5E48`, lui-même appelé depuis `sub_821E60A8`), et cette écriture
  est conditionnelle à `object+0x540c == 0` ET un bit de `object+0x2abd`.
- PROUVÉ : la porte `sub_821E54B8` (appelée avant l'avance du curseur) n'est
  pas un simple booléen mais un **suballocateur** (bump-allocation depuis un
  pool `object+0x34bc`, appel indirect de secours, budget `+0x3a44/+0x3a48`);
  en échec il positionne le bit `0x20` de `object+0x2abd`.
- REFORMULATION : `sub_821E60A8` a 76 sites d'appel statiques répartis sur
  quasi tout le binaire (de `0x820fd2e8` à `0x821f1780`). L'objet tracé
  depuis r77/r78 est donc un allocateur de tampon de commandes générique
  réutilisé par le moteur, pas démontrablement l'anneau Vd/PM4.
- DÉCISION : arrêter le traçage statique du graphe d'appel (rendements
  décroissants sur un point d'entrée à 76 sites); la prochaine étape est une
  lecture runtime (GDB sur la sonde bornée déjà existante) des champs
  `object+0x2abd`, `+0x540c`, `+0x34bc`, `*(object+0x2a90)` et `+0x2a9c` au
  point d'arrêt déjà connu, pas une nouvelle recherche statique.

Preuve : `reports/ac6-retail-native-codegen-gate2-r79-generic-allocator-reframe-20260831.md`.

# AC6 retail NTSC-U/J — correction r78 : Thread 1 attend l'espace anneau, pas la fence (2026-08-31)

- CORRECTION : r77 a caractérisé le spin terminal `object+0x2AF8` de
  `sub_821E64A8` comme "le blocage réel" sans vérifier contre une trace
  capturée. Le GDB déjà existant pour r51
  (`artifacts/retail-us-native-build-gate2-r51-thread/entry-gdb-all/gdb.log:162-165`)
  montre Thread 1 (le thread invité, distinct des 16 workers hôtes) dans
  `sub_821E6AC8 → sub_821E61A8 → sub_821E64A8` — mais la pile confirme qu'il
  est dans l'appel conditionnel `bl 0x821e61a8` (espace anneau insuffisant),
  pas dans le fallthrough vers le spin `+0x2AF8`.
- PROUVÉ : `object+0x2a90` n'est pas un curseur mais un pointeur vers un
  bloc alloué de 0x60 octets (seul site d'écriture statique :
  `sub_821E65B0:0x821e6740`, juste après un appel allocateur). `sub_821E61A8`
  déréférence l'offset 0 de ce bloc et le compare à `object+0x2a9c`; la
  condition n'est jamais satisfaite, donc la boucle rappelle
  `sub_821E6AC8` sans jamais sortir.
- OUVERT : qui écrit l'offset 0 du bloc pointé par `object+0x2a90` n'est pas
  trouvé (recherche de déplacement plate insuffisante, la cible est
  `*(ptr)+0`); et si cet anneau est spécifique au graphisme ou un mécanisme
  générique réutilisé (8 appelants de `sub_821E64A8` très dispersés,
  `0x82172184`..`0x821f36xx`).
- DÉCISION : toujours aucun changement de code. Prochaine étape : suivre le
  pointeur retourné par l'allocateur (`bl 0x821d74a8` dans `sub_821E65B0`)
  pour trouver qui d'autre l'utilise, plutôt qu'un nouveau scan de
  déplacement plat.

Preuve : `reports/ac6-retail-native-codegen-gate2-r78-actual-wait-frame-20260831.md`.

# AC6 retail NTSC-U/J — Gate 2 la frontière post-IB est une fence, décrément non localisé (r77, 2026-08-31)

- PROUVÉ (statique, `ghidra-projects/ac6-us`) : le nom porté depuis r51
  ("sub_821E6AC8 → sub_821E61A8 → sub_821E64A8, attente queue/ring") est
  vérifié pour la première fois sur le binaire US retail lui-même (et non
  par analogie avec le cycle 295 PAL, qui concernait un binaire différent où
  la même valeur hex n'était pas une fonction). `sub_821E64A8` pousse un
  paquet de deux dwords dans l'anneau puis boucle sans borne ni retry sur
  `lwz r11,0x2af8(r31) / cmpwi r11,0 / bne` : c'est ce spin exact qui
  bloque la sonde.
- PROUVÉ : `object+0x2AF8` est un compteur/fence ajusté par `sub_821E5FD0`
  (verrou + `add r11,r11,r26`). Les quatre sites d'appel direct trouvés dans
  toute l'image (`0x821e5f80`, `0x821e6130`, `0x821ef2ac`, `0x821ef2f4`)
  chargent tous un delta de 0 ou +1 : aucun décrément direct n'existe
  statiquement. La seule écriture inconditionnelle à zéro est le chemin
  d'abandon `sub_821EFAF0`, atteint uniquement depuis le *second* wait
  (`sub_821E61A8`, borné à 5000 ticks via `sub_821E6AC8`), pas depuis le
  spin de `sub_821E64A8`.
- OUVERT : le décrément réel est donc indirect (probablement un callback
  d'interruption graphique/CP enregistré, cohérent avec r53 "le consommateur
  PM4/Vd natif n'est pas encore relié") ou passe par un chemin non encore
  localisé; `FindDirectCallsTo` ne voit pas les dispatchs `bctrl`.
- DÉCISION : ne pas implémenter de décrément tant que le déclencheur retail
  réel n'est pas localisé — écrire un décrément maintenant serait l'état
  synthétique que r53 a déjà refusé pour le readback ring. Prochaine étape :
  scanner les dispatchs indirects (`FindVirtualDispatchSlot.java`,
  `FindPpcBranchesTo.java`) autour de l'installation du callback
  d'interruption graphique plutôt qu'un nouveau scan d'appels directs.
- Aucun code natif, codegen ni test modifié ce cycle; CTest reste **9/9**
  (non affecté, aucune source C++/Python touchée).

Preuve : `reports/ac6-retail-native-codegen-gate2-r77-fence-frontier-20260831.md`.

# AC6 retail NTSC-U/J — Gate 2 runtime natif, codegen/liaison fermés (2026-08-31)

- PROUVÉ : receipt codegen r11 `pass` avec XenonAnalyse/XenonRecomp US,
  81 fichiers générés, 62 629 029 octets, zéro diagnostic et
  `unrecognized_instructions=[]`; les 70 intervalles switch et deux cibles
  externes sont configurés depuis la qualification US.
- PROUVÉ : guest généré compilé en 79 objets, lié avec
  `ppc_func_mapping.cpp` et 229 définitions d'imports offline; le probe forcé
  vérifie un symbole guest, `PPCFuncMappings` et la sentinelle, puis sort avec 0.
- PROUVÉ : build natif `gate2-codegen-linked`, CTest 9/9 et
  `tools/validate.py --target ntsc-uj --runtime native` passent.
- PROUVÉ : `ac6recomp` natif, `--self-test`, installation `bin/ac6recomp` et
  audit d'installation temporaire passent; le parseur XEX2 valide
  `assets/default.xex`, avec déchiffrement AES-CBC, compression basic et
  validation PE.
- PROUVÉ : `GuestAddressSpace` réserve 4 GiB virtuels, valide les bornes 32-bit
  et permet une vue à `0x82000000`; `NativeRuntime::boot()` exige cette
  réservation et écrit l'image XEX décodée.
- PROUVÉ : lecteur XDVDFS natif borné extrait `default.xex` de l'ISO US en
  mémoire, avec chemins insensibles à la casse, bornes et cycles rejetés.
- PROUVÉ : loader XEX2 natif déchiffre le payload AES-CBC retail, développe les
  blocs basic et mappe l'image `0xa98000` à `0x82000000`; probes assets et ISO
  sortent avec 0, validation PE Xenon incluse. Compression normal/LZX reste
  fail-closed.
- PROUVÉ : `ac6recomp` lie le guest r11 et exécute `sub_8209C0B4` avec base
  guest alignée après mapping XEX; smoke ABI neutre uniquement, aucun point
  d'entrée retail ni gameplay synthétique.
- PROUVÉ : table `PPC_LOOKUP_FUNC` host est peuplée pour 19 832 mappings
  générés, et l'entry `0x821f5ed0` se résout à `_xstart`; aucun appel entrypoint.
- TRACE BORNÉE : avec stubs TLS, fréquence Xenon, création handle et timeout
  événement offline, `_xstart` atteint `sub_821F9E10` en 12 s; la trace reste
  non qualifiée et le chemin normal n'appelle pas l'entrypoint.
- TRACE ÉTENDUE : même probe opt-in borné à 60 s finit `timeout`/124 sans
  retour; scheduler/état SDK restent la frontière causale ouverte.
- PROUVÉ r47 : le binding `NtAllocateVirtualMemory` réserve et zero-remplit
  une plage guest bornée `0x10000000..0x7f000000`; les bindings CRT pool et
  chaînes BE sont couverts par les tests ciblés, sans allocation hôte exposée.
- PROUVÉ r49 : le probe initialise un PCR/thread guest déterministe et place
  `r13` sur ce PCR; CTest 9/9 et pytest 122/122 restent verts.
- PROUVÉ r50 : `VdRetrainEDRAM`/HSIO et le cycle Vd d'initialisation renvoient
  le succès natif borné; la sonde dépasse la première attente EDRAM.
- PROUVÉ r51 : `ExCreateThread` valide les cibles générées, clone le contexte
  Xenon (r13 conservé), assigne une pile guest bornée et lance le shim dans un
  worker host; GDB observe 16 workers, tandis que le thread principal attend
  encore la queue/ring.
- PROUVÉ r52 : après synchronisation de la copie `native-source`, le contexte
  réellement exécuté entre avec `r1=0x8ff00000`, `r13=0x0f000000` et un PCR
  guest initialisé; le correctif n'est plus seulement présent dans le source.
- PROUVÉ r53 : `MmAllocatePhysicalMemoryEx` fournit les blocs guest du ring;
  `VdEnableRingBufferRPtrWriteBack` reçoit `0x164e003c` dans un bloc valide,
  avec readback 0 et write pointer 5 observés. Le consommateur PM4/Vd natif
  n'est pas encore relié, donc aucun pointeur synthétique n'est injecté.
- PROUVÉ r64–r75 : le service Vd observe le champ primaire qualifié
  `object+10952` (et non le curseur secondaire `+10908`), publie des indices
  dwords et écrit le readback exact `state+60`. La mémoire guest complète les
  IB; le paquet `PM4_ME_INIT` (19 dwords) et le lot bootstrap (12 dwords) sont
  décodés/acceptés sans ReXGlue. `PM4_REG_RMW`, `INVALIDATE_STATE`, binning,
  `EVENT_WRITE_SHD`, `IM_LOAD_IMMEDIATE` et `NOP` ont des enveloppes bornées.
- PROUVÉ r75 : build/CTest natif **9/9**, pytest retail **126/126**, audit
  d'installation et validator ordinaire passent. La sonde `_xstart` bornée
  (SDL dummy) expire après le lot IB; aucune entrée guest retournée, frame,
  mission ou save n'est revendiquée.
- PROVISOIRE r76 : les handles d'événements offline ont maintenant une table
  bornée (set/clear/pulse, auto/manual reset, wait et signal-and-wait sans
  blocage hôte). La sonde conserve l'acceptation des deux lots GPU mais expire
  encore au même point; cette tranche ne qualifie pas le scheduler Xenon.
- PROVISOIRE : les stubs build-only couvrent TLS, fréquence 50 MHz, mémoire
  virtuelle, pool/chaînes, PCR, handles/événements, Vd, mémoire physique et
  workers; ils ne remplacent pas le scheduler/kernel, le consommateur PM4 ou
  les bindings SDK qualifiés.
- OUVERT : les définitions d'imports sont des stubs HLE génériques, pas encore
  le runtime SDK Xenon/XAM/Vd/XMA; scheduler/événements restent bloqués après
  le lot IB, et la traduction `IM_LOAD_IMMEDIATE` vers SPIR-V n'est pas
  qualifiée. Aucun boot guest retail complet, gameplay M01, renderer
  présentable, campagne ou release n'est qualifié.
- DÉCISION : fermer le sous-gate codegen/liaison; migrer maintenant contexte,
  mémoire, scheduler/kernel, VFS/save, XAM/input, XMA/XAudio et Vd par
  tranches testées, puis atteindre M01 par code invité naturel. Le patch outil
  et le code généré restent ignorés, hors installation.

Preuve : `reports/ac6-retail-native-codegen-gate2-r11-20260831.md`.

# AC6 retail NTSC-U/J — Gate 2 corrections statiques, codegen ouvert (2026-08-30)

- PROUVÉ : PM4 hardware natif corrigé (TYPE0/TYPE1/TYPE2/TYPE3, prédicat,
  WAIT, DRAW_INDX, SET_CONSTANT, IB et XE_SWAP), avec capsule mise à jour.
- PROUVÉ : assertions des cinq tests C++ restent actives en Release; CTest 5/5,
  pytest retail 113/113 et `validate.py --runtime native` passent.
- PROUVÉ : ABI VMX128 à 128 registres, réservations PPC alignées/invalidation
  et CR0, saves case-insensitive; spans Vd possédés par le bridge.
- PROUVÉ : census statique de la table US: 229 imports uniques; la famille
  réseau est déclarée offline-error sans création de socket, sans binding natif
  encore installé.
- OUVERT : génération XenonRecomp directe; export Ghidra US 8 163 fonctions,
  sélection 89 propriétaires switch. L'essai complet expire à 20 min; l'essai
  propriétaire seul finit avec 2 881 diagnostics, puis le croisement mapping
  expire à 20 min (exit 124).
- DÉCISION : aucun code généré n'est compilé, lié ou installé; boot/gameplay,
  census des 229 imports et gates campagne/release restent bloqués par Gate 2.

Preuve : `reports/ac6-retail-native-gate2-static-correction-20260830.md`.

# AC6 retail NTSC-U/J — Gate 2 codegen r3 ouvert (2026-08-30)

- PROUVÉ : XenonAnalyse/XenonRecomp US direct produced 84 ignored files in a
  bounded r3 run; no oracle code linked.
- PROUVÉ : helpers ABI US `save/restgpr`, `save/restfpr`, `save/restvmx` et
  `save/restvmx_64` qualifiés par bytes; r3 conserve la réduction de huit
  diagnostics.
- PROUVÉ : `qualify_xenon_helpers.py` produit huit occurrences uniques et
  alignées dans `artifacts/retail-us-native-codegen-gate2/helpers.json`.
- OUVERT : receipt r3 `open-diagnostics`, 1 831 diagnostics (1 824 switch
  boundary errors, `dcbst`/`mulhdu`/`frsqrte` unrecognized). Output remains
  unqualified.
- DÉCISION : no generated C++ enters native runtime until helper/function
  boundaries are qualified and diagnostics reach zero.

Preuve : `reports/ac6-retail-native-codegen-gate2-r3-20260830.md`.

# AC6 retail NTSC-U/J — Gate 1 statique fermé, capture runtime ouverte (2026-08-30)

- PROUVÉ : profil `native` et capsule `ac6.xenos-capsule.v1` passent; PM4,
  MMIO, ring/IB, endian, EDRAM, shader boundary, services offline, kernel
  handles/timebase, ABI PPC et `NativeRuntime` lifecycle audit `nm` fail-closed.
- VALIDATION : 106 tests Python retail, CTest native 5/5, validator native
  pass avec `release_ready=false`.
- OUVERT : capture oracle read-only et replay Vulkan réel; aucun gameplay ou
  release n'est revendiqué.

Preuve : `reports/ac6-retail-native-gate1-20260830.md`.

# AC6 retail NTSC-U/J — Gate 0 fermé, Gate 1 actif (2026-08-30)

- DÉCISION : feuille active revenue à `recompilation/ace-combat-6-retail`;
  N2 de `reconstruction/ace-combat-6` abandonné pour cette feuille, sans
  restauration ni fusion de ses sources.
- PROUVÉ : identités US scellées (XEX, ISO, projet Ghidra `ac6-us`,
  AC6_recomp, route M01) et route `771a77…b6043`.
- PROUVÉ : XenonRecomp `ddd128…6ace` et XenosRecomp `990d03…69d1`
  restaurés en checkouts détachés propres, hors produit.
- CONSTAT : catalogue `.tools/knowledge-base/architecture-v1/catalog.json`
  absent; aucune assertion générique n'en est dérivée.
- VALIDATION : 94 tests Python retail passent.
- GATE ACTIF : capsule `ac6.xenos-capsule.v1` et renderer Xenos/Vulkan
  autonome, fail-closed, sans ReXGlue.

Preuve : `reports/ac6-retail-native-gate0-20260830.md`.

# Native US 2026-08-30 — N1b verte, N2 prêt

- PROUVÉ : deux processus natifs NTSC-U/J exécutent 3 600 ticks chacun avec
  replay, huit captures Vulkan et reçu identiques octet par octet.
- PROUVÉ : scène complète (cité, terrain, eau, F-16, HUD, pose/caméra live),
  `complete_render_scene=true`, `jv_eligible=true`, deux exits 0.
- PROUVÉ : CTest 88/88, scène US explicite, cache 926/926 et frontières
  source/ELF verts.
- DÉCISION : N1b fermée verte. N2 premier objectif est la prochaine frontière.

Preuve :
`reports/native-us-n1b-m01-b-20260830.md`.

# Native US 2026-08-30 — N1a verte, N1b prête

- PROUVÉ : deux processus natifs NTSC-U/J exécutent 1 800 ticks avec pose et
  caméra joueur live, terrain, cité, eau, F-16 et HUD Vulkan.
- PROUVÉ : pitch, roll, yaw, throttle et frein changent chacun le frame ; sept
  PPM, le manifeste et le replay sont identiques octet par octet entre runs.
- PROUVÉ : deux exits 0, reçu commun `74392499…869012c`, CTest 88/88 et scène
  US store-backed verte.
- DÉCISION : N1a fermée verte. N1b est la prochaine frontière ;
  `jv_eligible=false` demeure obligatoire jusque-là.

Preuve :
`reports/native-us-n1a-free-flight-20260830.md`.

# Native US 2026-08-30 — N0 vert, N1a actif

- PROUVÉ : cache NTSC-U/J complet, 926/926 entrées, index
  `d7071928…1a34df5b`, 5 409 550 519 octets décodés.
- PROUVÉ : contrat US séparé identité/route/payloads, build complet et 88/88
  CTest sans échec.
- PROUVÉ : frontière source/ELF et paquet 96 entrées verts, sans ReXGlue,
  Xenia, C++ généré ni octet retail.
- DÉCISION : N0 fermé vert. Gate actif N1a free-flight 1 800 ticks ;
  `jv_eligible=false` reste obligatoire.

Preuve :
`reports/native-us-n0-baseline-20260830.md`.

# Native US 2026-08-30 — R0 fermé négativement, N0 ouvert

- PROUVÉ : rebuild NTSC-U/J `46e30186…41e5a1c`, validation statique 16/16,
  cache Vulkan 193/193 et route unique qualifiée.
- PROUVÉ : `1AB6` atteint `PRESENT` et contient le HUD vert, mais la capture
  stable conserve un monde noir.
- ÉCHEC BORNÉ : après `sync-log`, Escape fait passer `world=1` à `world=0`;
  le wait post-edge expire, puis le runner force le teardown sans fatal guest.
- DÉCISION : aucun second run ReXGlue. Le produit actif devient
  `reconstruction/ace-combat-6`; gate N0 NTSC-U/J.

Preuve :
`reports/retail-us-r0-frontbuffer-validation-20260830.md`.

# Retail US 2026-08-30 — 0869 fermé, frontbuffer guest restauré en source

- PROUVÉ : le payload `08694231e8d17665:2+12` remplace les dérivées
  divergentes par LOD0 sur deux ressources `mip_range=0..0`; `tf10` reste
  inchangé.
- PROUVÉ : le run cache 193 `ca04c848…c4b5` conserve une scène non noire après
  `0869` et le dernier `50D9`, termine 86/86 et s'arrête proprement.
- PROUVÉ : l'ordre guest est `0311 → HUD/UI → resolve 1AB6 → PRESENT`.
- CORRIGÉ EN SOURCE : le fallback presenter, actif par défaut, substituait
  `1B9C` à `1AB6` et supprimait le HUD. Il est désormais opt-in.
- CORRIGÉ : `sync-log` interdit au wait post-Escape de consommer un ancien
  marqueur stable. 57 tests passent.
- NON VALIDÉ : le nouveau défaut presenter n'est pas encore rebuildé; aucun
  reçu gameplay/debrief/save n'est promu.

Preuve :
`reports/retail-us-0869-lod-present-frontbuffer-fix-20260830.md`.

# Retail US 2026-08-28 — service runtime fermé, contrat campagne v3 prêt

- PROUVÉ : le walker CRT atteint `0x823CB708`, qui construit `0x829BAF30`
  via `0x82120FD8` avant sa publication dans `0x823F9B28`.
- CORRIGÉ : le vptr de base est `0x8205CFC4`, non `0x8275CFC4`; le vptr
  effectif `CNuSound` est `0x8205D1A4` et `+0x168 -> 0x82124930` dépend de
  l'état `objet+0xCECC`. Aucun patch d'initialisation forcée.
- IMPLÉMENTÉ : gate/audit v3, débrief/visuel v2 automatisés, replay strict,
  save/cache chaînés et orchestrateur frais M01–M15. 84 tests passent.
- DÉCISION : le reçu M01 v2 devient historique (`gameplay_pass=0`). Le gate
  courant est un rebuild puis une seule observation read-only du service,
  renderer, heartbeat et teardown.

Preuve :
`reports/retail-us-mission01-campaign-service-owner-reachability-static-20260828.md`.

# Retail US 2026-08-28 — loader campagne statiquement qualifié

- PROUVÉ : dans le projet Ghidra US canonique, `0x8218F4F0` ne fait
  `state=0->1` qu'après résolution `0x821D3028` puis retour `>0` de
  `0x821D28C8`; l'agrégateur reste à zéro tant qu'une requête/enfant est
  incomplète et renvoie `-1` sur erreur.
- PROUVÉ : la tranche US `DATA.TBL[9]`/FHM enfant scénario 0 est complète et
  qualifiée ; l'hypothèse « scénario US absent » est fermée, sans preuve de
  complétude runtime du graphe.
- DÉCISION : l'absence de `state=1->2` dans le run analogique expiré ne permet
  pas de distinguer appel non effectué et ressources encore pendantes. Aucun
  nouveau runtime identique.
- PROCHAIN : qualifier statiquement ownership/ordre de vie du registre et du
  service `vtable+0x168`, puis `FlightActive`, postprocess/resolve et teardown.

La chaîne source `FlightActive`/heartbeat et le paquet MnK sont aussi qualifiés
(activation par timestamp `<300 ms`, wrappers physiques joueur, layout
`LT/RT/LX/LY/RX/RY`), mais aucun heartbeat runtime n'a été observé.
Le global US `0x823F9B28` pointe initialement `CAce6Sound`; son slot
`vtable+0x168` cible `0x823A2C30` (`li r3,0; blr`). La statique qualifie aussi
les sites d'écriture vers `0x829BAF30` (`0x821D6380`) puis un vptr candidat
`0x8275CFC4` (`0x823CE6AC`). Leur reachability normale, le slot terminal
effectif et l'ordre de vie restent ouverts ; ne pas généraliser le résultat du
vtable initial au runtime.

Preuves : `reports/retail-us-mission01-campaign-loader-static-20260828.md`,
`reports/retail-us-mission01-campaign-service-owner-static-20260828.md`,
`reports/retail-us-mission01-flightactive-static-20260828.md`.

# Retail US 2026-08-28 — sonde analogique bornée, frontière toujours ouverte

- PROUVÉ : le rebuild `job-mtct7k4h-0be226ed` est validé et installé sous le
  binaire `e93d9fbe…bd8115` (37 804 064 octets), ReXGlue/Vulkan, 16/16 tests,
  `bin/bin` absent.
- ÉCHEC BORNÉ : `job-mtctdvii-e9c22860` atteint seulement `2/96` sur la route
  longstart; `type28=30` manque après 249,461 s, aucune ligne analogique,
  `game_status=-9`, zéro fatal/trap.
- DÉCISION : ce run n’apporte aucune preuve d’entrée. La route originale
  qualifiée est maintenant explicitement admise au harness diagnostic, sans
  modifier le gate produit; une corrélation read-only unique est en cours sous
  `job-mtctm90d-3d748f51`.
- ÉTAT : M01 gameplay v2, débrief/save checkpoint, missions 02–15, parité
  shader/lumière et `release_ready` restent non promus; PAL reste bloqué.

Preuve : `reports/retail-us-mission01-gameplay-candidate-20260828.md`.

# Retail US 2026-08-28 — corrélation analogique originale expirée

- ÉCHEC BORNÉ : `job-mtctm90d-3d748f51` a expiré à 20 min (`exit 124`) avec
  le binaire `e93d9fbe…bd8115`, la route originale scellée et le seed complet;
  aucun `RESULT.json` final n’a été écrit.
- PROUVÉ : l’artefact brut contient 50 captures, `5 587 PRESENT`, `94 type28`
  et des boutons XAM non nuls (`0x0004/0x0010/0x1000/0x2000`), mais aucun
  `[ac6-campaign-transition] state=1->2`, aucune ligne analogique et aucune
  phase `world=1/hud=1`.
- DÉCISION : corrélation non concluante, sans attribution renderer/input;
  aucune répétition. Reprendre statiquement transition campagne, `FlightActive`,
  postprocess/resolve et teardown; M01 v2, débrief/save, missions 02–15 et PAL
  restent bloqués.

Preuve : `reports/retail-us-mission01-gameplay-candidate-20260828.md`.

# Retail US 2026-08-28 — gameplay v2 candidat, sortie cinématique ouverte

- PROUVÉ : le binaire `b004ee70…4d70` est buildé/validé et porte les
  observables read-only du scheduler (`0x822ED310→0x82267160`) et de
  l'objectif (`0x82256490→0x8226C068`).
- PROUVÉ : la sonde HUD diagnostique atteint `cinematic=0 world=1 hud=1
  stable=30` et produit cinq contrôles visuellement différents.
- ÉCHEC BORNÉ : deux essais vierges calent à `type28=30`; le run cache hôte
  atteint l'étape 75 mais manque la phase stable après l'edge `Escape` fixe.
- ÉTAT : le manifeste de campagne garde le reçu gameplay historique attaché
  à `d8b7b7b3…`; `debrief_pass=0`, `release_ready=false` et PAL reste bloqué.
- PROCHAIN : une unique route diagnostique adaptative, puis seulement un
  replay v2 et l'audit; pas de correction globale renderer, trace globale,
  A/B, guest write ou ouverture des missions 02–15 avant succès M01.

Preuve : `reports/retail-us-mission01-gameplay-candidate-20260828.md`.

# Retail US 2026-08-28 — frontière scheduler US qualifiée

- PROUVÉ : le projet Ghidra canonique `ghidra-projects/ac6-us` (`default.xex`,
  XEX `6eefba42…67cbbbc`) borne `0x822ED310..0x822ED467` comme handler du
  signal `-2`, avec gardes contexte/objet puis appel de `0x82267160`.
- PROUVÉ : `0x82267160..0x8226723B` avance le pas selon le délai du scénario,
  remet le compteur de temps à zéro et retourne `1` à l'épuisement ; les
  appels directs US observés sont `0x82258F8C` et `0x822ED408`.
- PROUVÉ : `0x822ED708` et `0x8226E158` sont des offsets internes, non des
  entrées de fonction ; aucune frontière scheduler ne reste ambiguë.
- OUVERT : l'activation native objet/arme/compteur qui doit atteindre `-2` en
  vol ; aucun runtime ni gameplay/debrief n'est promu par cette lecture.
- DÉCISION : préparer une seule trace runtime bornée et read-only des gardes,
  du signal, du retour d'avance et du premier compteur M01.

Preuve : `reports/retail-us-mission01-scheduler-static-20260828.md`.

# Retail US 2026-08-28 — timer M01 sans entrée, auto-complétion écartée

- PROUVÉ : avec le binaire candidat `54dd900a…5711f9`, la route
  `mission01-roundtrip-timer-candidate.steps` atteint le HUD stable puis reste
  sans entrée pendant `300 s` ; 96/96 étapes et 28 captures sont produites.
- PROUVÉ : arrêt propre après `755,014796 s`, `game_status=0`, aucune
  correspondance fatale et aucun trap/timeout ; `selector=1 value=1` et les
  transitions campagne `0→1→2` restent visibles.
- NON PROUVÉ : aucun `[ac6-post-mission]`, terminal `0x822E3248`, setter niveau
  2, débrief ou checkpoint de sauvegarde pendant la fenêtre temporisée.
- DÉCISION : l'hypothèse « le scénario s'auto-complète au repos » est fermée.
  Le gate reste le round-trip M01 ; qualifier statiquement les objectifs,
  unités et compteurs US avant toute nouvelle entrée de vol ciblée.

Preuve : `reports/retail-us-mission01-roundtrip-timer-diagnostic-20260828.md`.

# Retail US 2026-08-27 — handoff corrigé atteint le HUD, visuel encore ouvert

Après intégration du settle/hold Campaign dans le harness diagnostique, un run
unique `--mission-cinematic-handoff` atteint briefing, cinématique et la phase
`cinematic=0 world=1 hud=1 stable=30`. Le résultat est
`diagnostic-capture-ready`, `23` captures, arrêt propre (`game_status=0`) ; le
centre HUD est non noir (`mean=0.301670`, `stddev=0.302621`, nonblack `1.0`).
L’avion et les hautes lumières restent surexposés, et aucun contrôle ou débrief
n’est crédité. Le gate round-trip M01 reste ouvert.

Preuve : `reports/retail-us-mission01-cinematic-handoff-candidate-20260828-r2.md`.

# Retail US 2026-08-28 — bytes scénario US qualifiés par tranche

- PROUVÉ : `DATA.TBL[9]` de l’ISO US qualifiée a été lu sur sa seule plage
  `offset=16908288`, `length=13234635` ; le payload décompressé et l’enfant FHM
  0 ont été vérifiés par SHA-256.
- PROUVÉ : l’enfant scénario (`51c10abe…ac6d45`) passe le probe natif avec
  `root_slots=10`, 230 unités, 434 objets, quatre sous-missions et
  `reader_runs=666`, `reader_failure=null`.
- DÉCISION : l’absence de ressource US est fermée pour Mission 01 ; aucune
  preuve PAL n’est transférée et aucun payload n’est copié dans le produit.
- OUVERT : progression naturelle, débrief, setter niveau 2 et round-trip save.

Preuve : `reports/retail-us-mission01-scenario-static-qualification-20260828.md`.

# Retail US 2026-08-27 — confirmation campagne qualifiée par settle/hold

La route ciblée `mission01-campaign-confirm-candidate.steps` a utilisé une
seule différence d’entrée nommée : settle `8 s`, puis edges `space` tenus
`0,6 s` sur les écrans de configuration. Elle produit des captures distinctes
pour difficulté/contrôles/langue, le getter
`selector=1 value=1 lr=0x821BA918`, puis les transitions campagne
`state=0->1` et `state=1->2`. Le diagnostic Vulkan a été arrêté après sa
capture (`RESULT.status=fail` technique, `game_status=-9`) sans fatal/trap ; il
ne constitue pas un reçu gameplay.

La couture d’entrée est donc fermée et sa recette est réutilisable dans le
round-trip naturel M01. Aucun override renderer, A/B ou guest write n’est
autorisé ; `debrief_pass=0` reste inchangé. Preuve :
`reports/retail-us-mission01-campaign-confirm-candidate-20260828.md`.

# Retail US 2026-08-27 — handoff campagne figé après profil

La variante `--mission-cinematic-handoff` a franchi le profil/Game Data avec le
candidat Vulkan `54dd900a…5711f9`, puis est restée sur `CAMPAIGN / NEW GAME`
après la langue. Aucun `[ac6-campaign-transition] state=1->2` n'est apparu ;
`4 610` `PRESENT` ont été produits et deux captures à 12 s sont identiques
(`62a77bf1…`). L'arrêt cgroup contrôlé porte le statut `15`, sans fatal/trap.

Ce diagnostic borne une seconde couture d'entrée, sans attribution au renderer
et sans autoriser A/B, guest write ou répétition identique. Le gate reste le
round-trip naturel M01 ; preuve :
`reports/retail-us-mission01-cinematic-handoff-candidate-20260828.md`.

# Retail US 2026-08-27 — amorce longue figée, gate round-trip conservé

La route diagnostique `mission01-flight-long.steps` a été exécutée une seule
fois avec le candidat Vulkan `54dd900a…5711f9`. Après `8 min 25 s`, le movie
worker produisait encore des réveils et `349` `PRESENT`, mais aucun marqueur de
campagne, `type28` ou `selector44` n'était publié ; seules deux phases
`world=0`/`stable=0` sont apparues. Les captures Xvfb à `23:07:40` et `23:12:21`
ont le même SHA-256 `87a84b42…`. Le scope a été arrêté borné avec le statut
`15`.

Ce résultat est un diagnostic fail-closed de l'amorce, pas un reçu de gameplay
et pas une attribution à Vulkan. Il n'autorise ni override renderer, ni A/B,
ni nouvelle exécution identique. Le gate reste le round-trip naturel M01 ; le
rapport complet est `reports/retail-us-mission01-flight-long-candidate-20260828.md`.

# Retail US 2026-08-27 — tick monde vivant, phase visuelle encore instable

La capture diagnostique bornée du candidat `54dd900a…5711f9` confirme
`0x8226CEA0`/caméra actifs et une charge frontier Vulkan substantielle. Les 93
échantillons montrent `object:0`, `camera:1` sur 85 appels après
initialisation ; l'update objet `0x822704A0` n'est pas appelé par le tick et son
unique référence directe reste la route `0x82256490`. La phase visuelle ne
tient jamais 30 frames ; aucune correction renderer globale n'est retenue.

Le gate reste le round-trip M01 naturel (débrief, setter niveau 2, save
quiescent, relecture fraîche niveau 2). Le diagnostic est enregistré dans
`reports/retail-us-mission01-world-owner-diagnostic-20260827.md` et ne ferme
pas le gameplay.

# Retail US 2026-08-27 — candidat round-trip Mission 01 prêt

Quatre observables read-only qualifiés sont liés : entrée InterMissionSelect,
setter de niveau filtré aux callsites `0x821A64D0/0x821A64E8`, transitions
brutes du save manager et lectures de niveau sur changement. Build sans
codegen et validation statique réussis : Python 65/65, natif 16/16, Vulkan,
SDL dummy, zéro D3D12, `bin/bin` absent. Le candidat
`54dd900a…5711f9` n'est pas encore promu ; le round-trip runtime reste à faire.

Preuve : `reports/retail-us-mission01-persistence-observables-build-20260827.md`.

# Retail US 2026-08-27 — chaîne débrief/progression Mission 01 qualifiée

La fin monde `0x822E3248` est reliée canoniquement à Debriefing,
DemoIntermission, Unlock puis InterMissionSelect. `0x821A6400` porte
l'incrément de niveau `1 -> 2`, enregistre son `CSelectSaveLoadManager` via
`0x82158D00`, l'actualise via `0x82158D90`, puis publie MissionTitle. La
persistance effective et la relecture après redémarrage restent non prouvées ;
le gate courant devient un round-trip runtime borné, sans A/B ni trace globale.

Preuve : `reports/retail-us-mission01-debrief-progression-static-20260827.md`.

# Retail US 2026-08-27 — quinze routes DPL/DATA.TBL qualifiées

La frontière statique campagne US est fermée sans corriger artificiellement
Ghidra : `0x821CD168` est une feuille appelée directement entre les entrées
`.pdata` `0x821CD0D0` et `0x821CD2E8`. Le loader actif flag-2 publie la borne du
`DATA.TBL` à `0x8293B950`, puis la feuille range les DPL directs inchangés.
L'extraction XDVDFS du seul `DATA.TBL` depuis l'ISO exacte donne 14 824 octets,
926 entrées et le SHA-256 `bad3a157…863b2f`, distinct du PAL.

Le manifeste promeut les quinze routes statiques `1..15 → 9..23 → 9..23`.
Validation : Python 65/65, natif 16/16, `validate.py` pass ; campagne
`static_qualified=15`, `gameplay_pass=1`, `debrief_pass=0`, release non prête.
Aucun codegen ni runtime n'a été lancé. Le gate courant devient la qualification
statique de la fin Mission 01, du débrief et de l'écriture de progression.

Preuve : `reports/retail-us-campaign-manifest-static-20260827.md`.

# Retail US 2026-08-27 — manifeste 15 missions, frontière DPL ouverte

Le sélecteur US canonique `0x821B6EE8` lit `0x820657B0` et qualifie les
missions `1..15` vers les DPL `9..23`. Le manifeste campagne fail-closed est
intégré à `validate.py` : 15 sélecteurs qualifiés, 0 route DATA.TBL complète,
1 gameplay, 0 débrief, release non prête. La requête `0x821D1190` et le loader
`0x821CC288` sont qualifiés ; le call target `0x821CD168` doit encore être
réconcilié avec la `.pdata` canonique, puis le `DATA.TBL` US extrait de l'ISO
exacte doit être identifié. Aucun runtime ni codegen n'a été lancé.

Preuve : `reports/retail-us-campaign-manifest-static-20260827.md`.

# Retail US 2026-08-27 — gate v2 construit, capture D5B4 suivante

Le gate NTSC-U/J v2 est construit et validé statiquement avec le binaire
`b869e2b1…61fde55`. Il impose 96/96, 27 captures, une phase HUD stable hors
cinématique, un centre de monde non noir et cinq effets de contrôle. Aucun
runtime n'a suivi le build dans cette session lourde. Le prochain gate est une
capture RenderDoc unique `cinematic-d5b4` dans une nouvelle session. PAL reste
fermé.

# Retail US 2026-08-27 — F556 qualifié, resolve inconcluant

Le premier draw F556 reçoit un fetch `tf0` valide (format 6, endian 2, tuilé,
256×256), clamp edge et bordure noire : la piste de bordure blanche est
fermée. `0x1C95E000` est une destination de resolve GPU; les zéros d'une
lecture CPU sont stale. Le probe de contenu resolve n'a pas quitté le hangar,
donc aucune correction n'est retenue. Le gate US reste fermé et le PAL bloqué.

La route 96 a aussi été reclassée : ses captures `step-84`/`87`/`90`/`93`/`96`
sont la cinématique pré-mission, pas le vol. Seule la route longue
`flight-long2/step-87` montre un HUD/radar retail plausible au début du
gameplay, mais sur un monde entièrement noir.

Preuve : `reports/retail-us-f556-sampler-resolve-20260827.md`.

# Retail US 2026-08-27 — qualification des surfaces blanches

La cinématique montre désormais le relief et les avions, mais l'aplat D5B4
orientation-dépendant et les taches blanches de la passe point-list F556 restent
distincts. D5B4 a une cible et un fetch BC3 valides; F556 passe par une
expansion point-sprite Vulkan correcte, avec le fetch/sampler `tf0` reçu. La
prochaine preuve doit qualifier le contenu resolve/load sans confondre cette
cinématique avec le HUD de gameplay.

Preuve : `reports/retail-us-white-texture-static-boundary-20260827.md`.

# Retail US 2026-08-27 — water-gradient inconcluant

La sonde unique `ac6_fix_water_line=false` réutilise le binaire validé et
termine proprement 89/89 opérations, sans fatal ni trap. Elle ne produit
toutefois aucune vue de vol : après `step-69-post-weapon-confirm`, les captures
restent identiques au hangar « Deploy with this selection? ». Elle ne confirme
ni n'exclut donc la passe water-gradient; aucune nouvelle session n'est lancée
sur ce seul résultat.

Preuve : `reports/retail-us-stock-water-runtime-20260827.md`.

# Retail US 2026-08-27 — textures blanches : dé-swizzle et FBO écartés

Le bras comparatif `--ac6_fix_deswizzle=false` conserve les surfaces blanches
selon l'orientation; le correctif AC6 reste activé. Les observations BC3/D5B4
restent valides (format 20, tuilé, ressource présente), ce qui déplace la cause
vers la lumière/composition. Un bras unique `render_target_path_vulkan=fbo`
produit une image initiale mais n'atteint pas `type28=30` en 120 s; FSI reste
le chemin qualifié. Aucun reçu gameplay ni PAL n'est ouvert.

Preuve : `reports/retail-us-stock-deswizzle-runtime-20260827.md`.

# Retail US 2026-08-27 — FSI/post-process/frontbuffer borné

Le tooling renderer utilisateur comprend désormais `glslc` (shaderc 2026.1-1),
en plus de SPIR-V Tools/glslang et de la couche Khronos. Un probe distinct de
séquence complète (96 opérations) a atteint la campagne mais n'a pas produit
`[ac6-campaign-transition]` et a été interrompu à `type28=30`; il n'ajoute
aucun reçu gameplay. La route scellée 96/96 reste seulement mécanique : ses
captures de vol ne montrent pas le HUD de cockpit. PAL reste donc fermé.

Le binaire US `a150da4022425d0477e20ecfaa06cd73162fa44074f409dac8edaffa36974de9`
est statiquement validé avec FSI ReXGlue/Vulkan. Les outils SPIR-V installés
en espace utilisateur valident les 93 shaders (`spirv-val` 93/93 et réflexion
`spirv-cross` 93/93). Une route longue propre atteint la cinématique 3D
surexposée, puis le monde noir avec HUD invité ; aucun reçu Mission 01 de
gameplay complet n'est encore obtenu.

La frontière restante est bornée aux passes post-process après le compose et
au handoff final `0x1B9C0000` → `0x1AB60000`. Le fetch swap est chargé sans
erreur, mais la sortie présentée devient noire. Cette observation ne suffit
pas à choisir un shader ou une barrière ; pas de nouveau rebuild/codegen et
PAL reste fermé.

Preuves : reports/retail-us-fsi-handoff-logged-runtime-20260827.md et
reports/retail-us-flight-long2-runtime-20260827.md.

# Retail US 2026-08-26 — observable propriétaire validé

Le rebuild sans codegen passe et installe `64f34acf…46af5b8`. Les cinq wrappers
CModeTaskGame/tick/objet/caméra/radio sont liés ; la cvar et les marqueurs sont
exigés par la validation. 16/16 tests, Vulkan-only, SDL dummy, zéro D3D12 et
zéro `bin/bin`. Le prochain runtime causal attend une nouvelle session lourde.

Preuve : reports/retail-us-world-owner-observable-rebuild-20260826.md.

# Retail US 2026-08-26 — propriétaires US qualifiés

Ghidra canonique US et les corps générés convergent sur CModeTaskGame
`0x8219A510`, tick monde `0x8226CEA0`, objet `0x822704A0`, caméra `0x822638B0`
et radio `0x82271908`; `0x8219A170` est réfuté. Étendues `.pdata`, octets,
références et appels sont reçus. Un observable read-only borné est prêt ; son
rebuild sans codegen attend une nouvelle session lourde.

Preuve : reports/retail-us-world-owner-static-qualification-20260826.md.

# Retail US 2026-08-26 — soumission du monde effondrée au handoff

Le hangar est franchi et le probe atteint proprement le HUD noir sans panneau.
La cinématique soumet 969–975 draws, 91 resolves et 32 pointlists ; le handoff
chute de 1 197/84/31 à une signature stable 106/1/0 jusqu'au HUD. Le viewport et
la `guest_swap_texture` restent 1280x720. La rupture est en amont du swap, dans
l'activation/dispatch/soumission du monde. Runtime consommé : 19.

Preuve : reports/retail-us-flight-world-submission-collapse-20260826.md.

# Retail US 2026-08-26 — divergence bornée au hangar sans panneau

Le binaire validé termine proprement le probe et le panneau vert est absent.
L'audit visuel montre toutefois le même hangar `Deploy` des étapes 70 à 87 : le
premier A immédiat n'est pas accepté. Les étapes antérieures, le focus et les
présentations restent corrects. Une attente de deux secondes précède désormais
l'unique A du hangar. Runtime consommé : 18.

Preuve : reports/retail-us-no-panel-hangar-divergence-20260826.md.

# Retail US 2026-08-26 — rebuild résumé persistant sans panneau

Le rebuild unique réutilise le code généré et passe avec un binaire installé
`60c9fc6a…15da94`. Les 16 tests ciblés, Vulkan-only, SDL dummy, l'absence de
D3D12 et de `bin/bin` sont validés. Le dernier résumé non vide est conservé et
le panneau vert est opt-in, donc masqué par défaut. Le prochain runtime, dans
une nouvelle session lourde, journalisera les signatures bornées sans panneau.

Preuve : reports/retail-us-last-meaningful-rebuild-20260826.md.

# Retail US 2026-08-26 — résumé de frame non autoritatif

Le probe atteint 87 opérations et les captures cinématique 3D/HUD noir, mais
reste un échec fermé (`game_status=-9`, arrêt non propre). Les compteurs sont
nuls même sur la frame 3D visible : la frontière PRESENT vide écrase le dernier
résumé utile. Les appels générés sont bien liés aux wrappers. La source conserve
maintenant la dernière frame non vide ; aucun second job lourd dans cette
session. Ce correctif et le défaut qui masque le panneau vert attendent le
prochain rebuild unique.

Preuve : reports/retail-us-black-world-frame-summary-20260826.md.

# Retail US 2026-08-26 — HUD atteint, monde noir

Le seuil 180 PRESENT stabilise le démarrage. La route atteint le mode vol :
les primitives HUD sont visibles après A/Start, mais le monde 3D est noir.
Le run de 87 opérations termine proprement sans fatal ni trap. Le prochain
probe active uniquement le résumé de frame borné afin de compter draws,
clears, resolves, RT et viewport sur cette frame noire.

Preuve : reports/retail-us-hud-black-world-20260826.md.

# Retail US 2026-08-26 — seuil 1 PRESENT réfuté

Après attente du premier PRESENT, l'Escape initial arrête immédiatement toute
présentation : une seule frame totale, aucun type28, aucune capture, fatal,
trap ou erreur audio. Le run est interrompu par TERM au done_when et reste un
échec. Le prochain probe attend les 180 PRESENT historiquement qualifiés avant
toute entrée.

Preuve : reports/retail-us-cinematic-hud-one-present-20260826.md.

# Retail US 2026-08-26 — probe HUD non exercé

La tentative de handoff HUD s'est figée avant type28 après 1 630 PRESENT,
sans capture, fatal, trap ni erreur audio. Le focus et les impulsions restaient
actifs. Le timeout 124 sans RESULT.json classe la session en échec interrompu.
Le prochain probe attend obligatoirement le premier PRESENT avant toute entrée,
ce qui fixe la divergence temporelle nommée.

Preuve : reports/retail-us-cinematic-hud-startup-stall-20260826.md.

# Retail US 2026-08-26 — cinématique de lancement atteinte

La confirmation A depuis la carte tactique lance la cinématique 3D du F-16.
Les captures à 15 et 35 s montrent l'avion; à 65 s, la transition reste
floutée sans HUD. Le run termine proprement sans fatal ni trap. Le prochain
probe applique la recette historique A puis Start et s'arrête au premier HUD
candidat, sans contrôle de vol.

Preuve : reports/retail-us-tactical-map-confirm-20260826.md.

# Retail US 2026-08-26 — premier hangar franchi

Le probe exact confirme A depuis le premier hangar et atteint la carte tactique
Mission 01. Celle-ci reste stable jusqu'à 65 secondes avec A OK : elle attend
une seconde confirmation explicite. Le run est propre, sans fatal ni trap.
Le prochain probe ajoute uniquement cet A sur la carte.

Preuve : reports/retail-us-first-hangar-confirm-20260826.md.

# Retail US 2026-08-26 — divergence au premier hangar

Les captures ordonnées prouvent que Space progresse avion → armes → premier
hangar, puis que Shift renvoie aux armes. Le Space qualifié jusque-là comme
« lancement » ne faisait que revenir au hangar. Aucun probe précédent n'a
donc confirmé A depuis ce hangar. Le prochain probe s'arrête à l'opération 69,
confirme A une fois, puis reste passif.

Le panneau vert vient de l'appel Show() de l'application. Le nouveau cvar
ac6_graphics_diagnostics est désactivé par défaut dans la source, mais le
binaire installé n'est pas encore rebâti.

Preuve : reports/retail-us-hangar-route-divergence-20260826.md.

# Retail US 2026-08-26 — pression courte Mission 01 réfutée

Le diagnostic US atteint le hangar Mission 01 et observe la transition campagne
0→1→2. Après le seul Space de lancement tenu 0,1 s, les captures à 50, 70 et
100 s restent au hangar A OK / B CANCEL. La pression courte n'est donc pas une
confirmation suffisante. Le prochain probe borne uniquement ce même bouton à
0,6 s; aucune autre touche post-lancement n'est autorisée.

Preuve : reports/retail-us-mission-launch-short-press-20260826.md.

# Cycle 1849 — timeline marque normale, frontière enfant fermée

La négative à 3 200 ticks est requalifiée : le MovieController racine
`0x2E3CDD10` possède 2 220 frames et n'atteint sa dernière frame qu'autour du
tick 6 882. Son avancement est produit normalement par `0x82323BB8`, cadencé
par `0x82322438`; la promotion parent/enfant `+0xE4/+0xE8 -> +0xF4` existe
déjà. L'instantané `0x2E3CED10+0xDC=0` ne démontre donc aucun cue manquant et
ne contrôle pas EndMode.

La jointure exacte de la dernière entrée parent au cue Startup reste statique,
mais elle n'est pas requise pour le prochain test causal : un probe unique,
sans entrée, jusqu'à 7 200 ticks doit observer Title après le film naturel.
Aucun patch produit ni état de succès n'est promu.

Preuve : `reports/cycle-1849-demo-brand-movie-timeline-requalified.md` et
`artifacts/cycle-1849/`.

# Cycle 1848 — MovieController atteint, cue enfant ouvert

Le probe PAL démo HSIO=1 sans entrée atteint 3 200 ticks avec ring actif
(`1 182` soumissions, `1 089` présentations), sans frontend, mission ni
terminal. Le parent MovieController `0x2E3CDD10` avance, alors que son enfant
`0x2E3CED10` reste sur `DC=0` pendant la fenêtre de trace, malgré `D5=1` et
`D6=0`. Aucun appel EndMode n'apparaît.

Le garde connu de `0x82323BB8` est donc permissif; aucun patch de ces champs
n'est causal. La suite est une jointure Ghidra statique parent→enfant pour
nommer le cue/producteur réel, sans autre runtime ni modification produit.

Preuve : `reports/cycle-1848-demo-moviecontroller-runtime-negative.md` et
`artifacts/cycle-1848/`.

# Cycle 1847 — dispatch render post-fence non causal

Le plateau `0x822E559C -> 0x822F8848` est maintenant qualifié comme un update
render/context du main loop : `0x822E559C` est le LR du dispatch indirect dans
`0x822E5540`, non une entrée ni un wait. Son corps ne touche pas la chaîne
StartUp/EndMode/manager; aucun correctif n'est causal.

La frontière revient au flux guest SWG déjà nommé : le MovieController concret
de Startup doit avancer de frame0 vers frame1/list1 pour sélectionner EndMode.
La suite prépare une observation native unique de cette jointure, sans écrire
l'état invité.

Preuve : `reports/cycle-1847-demo-render-dispatch-noncausal.md` et
`artifacts/cycle-1847/`.

# Cycle 1846 — prédicat post-fence négatif borné

La chaîne invitée `EndMode -> listener StartUp -> manager+0x18 -> Title` est
qualifiée, mais n'est pas démontrée atteinte sous HSIO=1. Le garde
MovieController `0x82323BB8` est aval et non atteint; aucune correction de
son état n'est causale. Le dernier site guest démontré est l'indirect
`0x822E559C -> 0x822F8848` (objet `0x82934280`, vtable `0x8202A488`, slot
`+0x10`), dont le receiver n'est pas encore joint au listener StartUp.

La négative ferme le gate sans patch ni runtime. La suite est une jointure
Ghidra statique du receiver et des writers de `+0x0C`.

Preuve : `reports/cycle-1846-demo-post-fence-predicate-bounded-negative.md`
et `artifacts/cycle-1846/`.

# Cycle 1845 — fence scratch runtime réfuté

Le seul probe effectif PAL démo HSIO=1 sans entrée atteint 3 200 ticks sans
Title, mais le ring reste sain (1 182 soumissions, 1 089 présentations). Les
192 couples de trace source 1 montrent tous `0x16AE2000: 4 → 0` après retour
du dispatcher `0x821B9710`, y compris les callbacks `0x821C5190`,
`0x822E4240` et `0x822E4268`.

Le `pending_wait=4` terminal est donc un instantané de borne, pas un fence
bloqué. Aucun correctif Xenos, host, import ou PPC n'est justifié; la sonde
temporaire est retirée. La suite doit identifier statiquement le premier
prédicat invité encore actif avant `EndMode → manager+0x18 → Title`.

Preuve : `reports/cycle-1845-demo-scratch-fence-runtime-refuted.md` et
`artifacts/cycle-1845/`.

# Cycle 1844 — producteur scratch zéro qualifié statiquement

Le wait qui maintient StartUp est maintenant qualifié jusqu'à sa retraite
attendue : `SCRATCH_REG0` publie `4` à `0x16AE2000`, puis
`PM4_INTERRUPT(4)` livre la source 1 au dispatcher invité `0x821B9710`.
Après retour du callback actuel `0x822E4240`, ce dispatcher doit effacer le
bit CPU 2, donc `4 → 0`. Le host ne produit pas ce zéro.

Le contrat est déjà présent statiquement dans le processeur Xenos, le ring et
le cycle de vie. Aucun patch n'est causalement justifié. La seule suite est
une trace native bornée de cette retraite exacte, sans A/B ni entrée.

Preuve : `reports/cycle-1844-demo-scratch-fence-producer-qualified.md` et
`artifacts/cycle-1844/`.

# Cycle 1843 — Title naturel non atteint, writer scratch zéro ouvert

La correction texture existante passe les validations OFF 27/27 et ON 26/26.
Le run HSIO=1 sans entrée atteint 1 182 soumissions ring à 3 200 ticks, mais
StartUp reste actif et aucune frame Title n'existe. Le dernier wait CP lit le
dword `0x16AE2000` à 4 et attend 0; le producteur du retour à zéro n'est pas
nommé. La branche s'arrête sans patch produit ni cycle mission.

Preuve : `reports/cycle-1843-demo-title-natural-not-reached.md` et
`artifacts/cycle-1843/`.

# Cycle 1842 — divergence HSIO bornée, première frontière dure nommée

Le sélecteur invité est qualifié : `0x821C64E8` produit le cache
`0x827AD310`, `0x821BA780` garde `device+0x2ABD.bit1`, puis `0x821B9BC8`
sélectionne le ring sous HSIO=1. Ce choix est intentionnel et reste inchangé.
Le plateau worker est non causal et le `WAIT_REG_MEM` profond est conforme au
contrat CP testé. La première frontière dure de la longue exécution est le
profil texture BC3 320×160 rejeté au tick 4 911; sa prise en charge minimale
et son test existent déjà dans l'arbre courant. Aucun runtime ni patch produit
n'a été ajouté pendant ce cycle.

Preuve : `reports/cycle-1842-demo-first-post-startup-divergence.md` et
`artifacts/cycle-1842/`.

# Maintenance post-cycle 1841 — handoffs opérationnels nettoyés

`NEXT.md` et `RESUME.md` sont désormais current-only : 7 350 lignes cumulées
ont été ramenées à 49, avec un seul statut courant et aucun faux gate actif.
L'historique reste dans `STATE.md`, `EVIDENCE.md`, `reports/` et `artifacts/`.
`AGENTS.md` impose ce contrat, exige la lecture des ancres exactes avant patch
et nomme explicitement `recompilation/ace-combat-6-demo` comme arbre PAL démo
actif. Aucun code produit, artefact runtime ou résultat de gate n'a changé.

# Cycle 1841 — contrat threading déjà satisfait, branche bloquée sans patch

`RÉFUTÉ` : ni le vol auto-reset ni la famine d'un worker runnable n'explique
l'échec Title. Le backend courant réserve déjà le waiter via `granted_thread`
et sert tous les threads runnable par passe. Dans le run existant, t15 exécute
1 878 dispatches jusqu'au tick 2 997 pendant le spin massif de t1; à la borne,
seul t1 est runnable et les 22 autres threads sont bloqués.

Aucun changement produit, oracle retail ou nouveau runtime. Les validations
post-retrait existantes restent PASS (OFF 27/27, ON 26/26), mais les critères
finaux restent t1=5 et aucune transition `StartUp→Title` à 3 000 ticks malgré
1 114 soumissions ring. La branche demandée est arrêtée au blocker : une suite
doit ouvrir un gate distinct sur un prédicat invité ou service hôte post-StartUp.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

Preuves : `artifacts/goal-playable/threading-contract-review-20260825/`,
`reports/cycle-1841-demo-threading-contract-already-satisfied.md`.

# Cycle 1840 — les publications existent : deux sémaphores, origine invitée

`PROUVÉ` dans le projet Ghidra canonique PAL démo : `0xE000005C` et
`0xE0000130` sont créés par `NtCreateSemaphore` puis publiés par
`NtReleaseSemaphore`. Le census antérieur `set/pulse` ne pouvait pas les voir.

- `0xE0000130` : producteurs `0x820FF710/788/7F8/A88/B50`, cinq callsites
  vers `0x822E1E70 -> 0x821A6950`; garde `[obj+0x60d0] < 256`.
- `0xE000005C` : producteur partagé `0x822EF750`, release à `0x822EF7B4`;
  bootstrap `0x822EF7D0 -> 0x822EF750` à `0x822EF804`, sans garde de release.

Les atlas existants prouvent respectivement une et cinq publications avant le
ledger d'attente. Les deux origines sont `guest-import`; aucun post hôte ne
doit être ajouté et l'oracle retail n'est pas nécessaire. Frontière active :
contrat partagé scheduler/waiters après publication invitée.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

Preuves : `artifacts/goal-playable/thread-producer-static-20260825/`,
`reports/cycle-1840-demo-semaphore-producers-qualified.md`.

# Cycle 1839 — rotation scheduler seule réfutée et retirée

`RÉFUTÉ` : la variante round-robin déterministe échoue dans l'unique run PAL
démo HSIO=1 borné à 3 000 ticks : thread 1 reste à 5 handshakes (critère
`>5`), le ring atteint 1 114 soumissions, et `StartUp→Title` reste absente.
Le reçu d'exit manque après interruption du contrôleur ; le run n'est pas
qualifié de succès.

Le curseur, le helper et leur test ont été retirés. Validation post-retrait :
builds codegen-OFF/ON PASS, CTest OFF 27/27 et ON 26/26. Aucun correctif
scheduler n'est conservé. Frontière active : producteurs invités des posts
`0xE000005C` et `0xE0000130`. `frontend=false`, `mission=false`,
`terminal=false`, `supported=false`.

# Pivot retail après échec clos du cycle 1850 (26 août 2026)

Le probe PAL démo codegen-ON s'est arrêté fail-closed après 5811 ticks sur
`unsupported Xenos draw shape`, avec 1959 présentations, sans frontend ni
mission. Ce gate et l'architecture runtime démo artisanale sont clos et
supersédés; aucun nouveau build ou long run n'est autorisé.

Le produit actif devient `recompilation/ace-combat-6-retail`, fondé sur le
sous-module BSD-3-Clause AC6_recomp
`09144bb092ad871584808aeead69c395edbd5200` et le renderer Xenos/Vulkan complet
de ReXGlue. Le bootstrap NTSC-U/J est le gate courant. PAL retail reste la cible
finale et est bloqué jusqu'au succès gameplay US. Preuve :
`reports/cycle-1850-demo-runtime-closed-superseded.md`.

Preuves : `artifacts/goal-playable/scheduler-rotation-20260825/`,
`reports/cycle-1839-demo-scheduler-rotation-refuted.md`.

# Cycle 1838 — les draineurs sont nommés : worker de file et pool de complétion

`PROUVÉ` (statique pure, basefile qualifié `b98a9ac1…14218` + atlas canonique,
zéro run) : les deux cibles de spin ont leurs décrémenteurs.

1. File de rendu (`0x82386CC0`, `[obj+24784]/[24788]`) : producteurs
   `sub_820FF710/788/7F8/A88/B50` ; initialiseur `sub_820FF9E0` qui crée le
   worker dédié via `0x822E1D30` (unique appelant) ; draineur **`sub_820FFCA0`**
   — pop, ++`[24788]` @0x820FFD78, dispatch `sub_820FEFA8` (mesuré thread 25).
   À vide le worker **spinne**, il ne dort jamais sur un événement.
2. Contexte de complétion (`0x82928B80`) : bootstrap `sub_822EF850` crée
   l'événement `[+152]` puis **trois threads** de corps thunk
   `0x822EF848 = b 0x822EF5B0` : attente infinie sur `[+152]` (wrapper
   `0x821A6AF0`), au réveil ++`[ctx+8]` @0x822EF5E0, pop de la file
   incorporée, dispatch vtable des items. Mesuré thr15 (handle 0x60 x24).

Attente côté thread 1 : corps de tick `sub_822DA9C0 -> sub_822E40E8`
(r3=`0x82928B80`) -> spin `sub_822EF7D0`. Ledger tick 260 : t15/t16/t17
bloqués sur l'événement `0xE000005C`, t25 (worker de rendu) sur
`0xE0000130`, et ces deux handles ne reçoivent **aucune publication dans les
deux régimes** — la contrainte liante est le post manquant, pas seulement le
partage de tranches. Sous HSIO=0, la causalité en miroir se confirme
(draineurs servis, Title au tick 2369).

Prochain gate : spécifier AVANT implémentation la correction minimale —
rendre les tranches aux non-primaires sans horloge murale (rotation de
l'ordonnanceur hôte), contre les observables fixés (handshakes t1 > 5,
soumissions ring > 0, StartUp→Title). Preuve :
`artifacts/goal-playable/drain-writer-census-20260825/RESULT.md`,
`reports/cycle-1838-demo-drain-writers-named.md`.

# Cycle 1837 — la garde des deux fenêtres : le thread 1 ne revient jamais au poll

`PROUVÉ` (statique pure + journaux existants) : la machine de soumission de
streaming est nommée — `sub_8219DF00(req)`, phases sur `[req+12]`, démarrage
tant que `[req+16] < 860`, poll vers `sub_8219AF20` tant que `[req+36]==0`,
enregistrement via `E1E28/E1E18` vers le pool `0x8261EBC8`, anneau
`sub_821A1B50` qui pose l'événement `28`. Le thread 1 y arrive par bctrl
depuis `sub_8219F5D0` : 99 848 polls sur ticks [66..222].

La garde liante n'est pas dans la machine : dès le tick ~206, le thread 1 est
capté par des spins de drain sans fin — `sub_820FF8D8` bouclant sur
`[obj+24784]/[obj+24788]` (n=1,86 M d'itérations mesurées jusqu'au tick 2997),
famille `0x822EC03C→F5CE8`, et `EF7D0 while([ctx+8]>0)`. Rien ne se draine
quand le thread 1 mange toutes les tranches ; pas de troisième poll, pas de
troisième fenêtre. Sous HSIO=0, ses handshakes bloquants rendent le CPU aux
draineurs — la charge avance jusqu'à 2369. Les deux régimes confirment la
même causalité en miroir.

Prochain gate : qui décrémente `[obj+24784/88]` et `[0x82928B80+8]`
(threads/sites de complétion), puis choix de la correction minimale contre les
observables fixés. Preuve :
`artifacts/goal-playable/streaming-guard-static-20260825/RESULT.md`,
`reports/cycle-1837-demo-streaming-guard-is-a-drain-spin.md`.

# Cycle 1836 — kickers morts dans les deux régimes ; pipeline de streaming nommé

`RÉFUTÉ` (négative bornée, trois preuves indépendantes) : le sous-système
kicker (`sub_821A1928`, pool `0x82774630`) n'exécute rien pendant le bootstrap,
**ni sous HSIO=1 ni sous HSIO=0** — zéro arête indirecte sur 3000 ticks pour
toute sa fermeture d'appelants, zéro publication des handles `04..3C` des deux
côtés du journal 1024. Il n'est pas le levier.

Instrument nouveau (observation pure) : backchain des LR sauvés aux shims
d'événements (`AC6_DEMO_WATCH_EVENT_CALLER`). Deux chaînes fermées :
- **Paceur** : `NtSetEvent(40)` x259 ← `sub_821C5090`+232 ← callback
  d'interruption `sub_821B9710`+176 — estampille et pacing dans la même
  fonction, une fois par tick.
- **Streaming** : thread 1 soumet via `sub_8219AF20`+332 → `sub_821A1B50`+128
  (anneau `0x82774898+152i`) → set `28` ; thread 9 attend/draine et clôt par
  clear/pulse `2C` (`sub_821A1D10`). **Deux fenêtres seulement en 260 ticks**
  (66→72, 205→219) ; sous HSIO=0 les lectures vont jusqu'au tick 2369.

La frontière suivante est la garde qui borne les soumissions de streaming du
thread 1 à deux occurrences. Preuves :
`artifacts/goal-playable/kicker-caller-attribution-20260825/RESULT.md`,
`reports/cycle-1836-demo-kickers-dead-streaming-pipeline-named.md`.

# Cycle 1835 — la découple de cadence est réfutée avant implémentation

`RÉFUTÉ` (statique pure, trois balayages parallèles, zéro run) : le correctif
envisagé au cycle 1834 — délivrer l'interruption graphique au rythme du temps
réel plutôt que par tick — tombe sur les trois points du gate. (1) Inventaire
clos : famille de pacing complète WORK `{40,44,48,4C}` / GATE `{54,58}` /
perf `0x82935270` (`50`) / complétion `0x82928B80` / pool workers
`0x82774630` ; l'interruption part une fois par tick *avant* les tranches
(`lifecycle.hpp:417-422`) et les journaux d'arêtes n'ont aucun plafond.
(2) Neuf fonctions lisent le timebase : estampilles, stats, un spin de
timeout — **aucun rendez-vous** ; découpler ne change aucune sémantique de
fence. (3) Les soumissions ring sont des stores MMIO invités, rien ne les
couple à la coïncidence interruption/tick ; un pacing temps réel casserait en
revanche le déterminisme des replays.

Le maillon nommé remplace la frontière : sous HSIO=1, les workers 4..11
dorment depuis le tick 0 faute de kick (`sub_821A18D0`, six appelants statiques
dont aucun ne s'exécute après la phase précoce), donc `EF7D0` spinne dans le
thread 1, donc tout le chemin titre affame. À 60 Hz matériels, le producteur
EST libéré chaque frame — ce qui manque au port, c'est la préemption par le
temps et les complétions asynchrones, pas la cadence.

Prochaine expérience unique : watcher opt-in sur `sub_821A18D0` et ses six
appelants, 260 ticks headless ; identifier l'appelant kicker et sa garde, ou
négative bornée. Aucune correction scheduler/renderer avant ce verdict.

Preuve : `artifacts/goal-playable/pacing-inventory-static-20260825/RESULT.md`,
`reports/cycle-1835-demo-interrupt-decouple-premise-refuted.md`.

# Cycle 1834 — l'interruption satisait d'avance chaque attente

`PROUVÉ` (statique + un run 260 ticks, instruments opt-in, zéro ligne changée) :
les cinq handshakes du thread 1 se répartissent sur **deux** objets — la file
de `WORK` (`{48,4C}`, via `4018`/`4080`) et le *gate* global (`{54,58}` à
`0x82933F98`, via `EEE68`). Dernier appel au tick 177, comme sur 3000 ticks.

Pourquoi ça cesse de bloquer : `sub_822E4268` (dispatch d'interruption
graphique) estampe un timebase **et** fait `NtSetEvent([obj+104])`. Sous
HSIO=1 l'interruption libère donc le producteur (`t12` attend `0xE0000040`)
une fois par tick : publication x259, compteur de gate basculant 0/1, et
chaque condition du consommateur déjà vraie à l'arrivée. Le handshake étant
le yield de l'invité, le thread 1 ne dort plus, épuise 2998/3000 tranches et
affame le streaming (fenêtres thread 9 aux ticks 66/205 seulement). Sous
HSIO=0, personne ne libère `40`, le thread 1 dort 5611 fois, l'horloge avance
par timeouts. Deux régimes faux en miroir.

Référence : Xenia Edge délivre l'interruption depuis un worker temps réel
(`graphics_system.cc:171-206,385-403`), indépendante de l'ordonnanceur ; le
port la tire une fois par tick — même variable que l'horloge.

Corrige : cycle 1833 (égalité sur le gate, pas sur la file ; la file n'a que
`4080` de bloquant), cycles 1829/1830 (estampilleur = paceur, `[obj+24]`
+ événement). Preuve :
`artifacts/goal-playable/gate-waiter-attribution-20260825/RESULT.md`,
`reports/cycle-1834-demo-interrupt-pacing-pre-satisfies-every-wait.md`.

Prochain gate : lister statiquement les paceurs dérivés de `[obj+104]`, puis
décider la découple cadence d'interruption / tick avant toute correction,
baseline ring (502 soumissions) comme garde-fou.

# Cycle 1826 — aucun événement ne manque : le thread 1 ne va plus se coucher

`RÉFUTÉ`, et c'est une affirmation de ce dépôt qui tombe : le journal
`event_publications` était un `std::array<…, 32U>` cessant d'enregistrer au 33ᵉ
événement, et les deux côtés de l'A/B l'avaient saturé. Porté à 1024, le relevé
complet montre les deux chemins publiant **la même chose** :

```
site invité      HSIO=1   HSIO=0        clés distinctes : 61 contre 60
0x821A61F0          52       52
0x821A688C           2        2
0x821A6AC4         970      970
```

Les seize handles `0xE0000088…0xE00000C8` sont publiés une fois chacun des deux
côtés. Seule `0xE0000050` est exclusive (27 contre 0) : clé d'attente du thread
13, réveillé par le régime d'interruption — signature attendue.

Tombent avec : « les dix-sept publications disparaissent » (cycle 1826), « le
streaming s'arrête » (55 lectures contre 61, compteurs non plafonnés), et la
lecture du cycle 1807 des seize handles comme publication en trop.

Ce qui survit, mesuré hors plafond :

```
thread 1, NtSignalAndWaitForSingleObjectEx @0x821A69CC
   HSIO=1      5 appels, dernier au tick 177
   HSIO=0   5611 appels, dernier au tick 2999
```

Le thread 1 ne dort pas faute d'être réveillé : il ne va plus se coucher. La
frontière est côté invité — quel prédicat fait sauter le handshake après le
tick 177.

Preuve : `artifacts/goal-playable/publication-ab-cap1024-20260824/RESULT.md`.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Cycle 1825 — le ring et la progression s'excluent sur un seul mot

`PROUVÉ` à une variable près : `VdIsHSIOTrainingSucceeded` décide seul si la
démo affiche quelque chose ou si elle progresse.

```
HSIO=1 (HEAD)  ring 502 soumissions   mode bloqué à StartUp, Title jamais publié
HSIO=0         ring 0 soumission      Title publié au tick 2369, état 1 au 2385
```

Le Title publié porte la vtable `0x820113E4`, le vrai
`CModeTaskTitleDemoOffline`; les ticks 2369/2385 reproduisent exactement
l'observation du cycle 1793, qui avait donc été faite sous HSIO=0.

Signature ordonnanceur : HSIO=0 → 23 threads bloqués, 0 runnable, **215**
épuisements de tranche sur 3000. HSIO=1 → 22 bloqués, 1 runnable, **2998**.
Sur le chemin ring, l'invité attend des événements que le port ne rend pas.

`RÉFUTÉ` par mesure : régression de binaire entre le 22 et le 24 août (le
témoin conservé et le binaire courant donnent des résultats rigoureusement
identiques) ; différence de store (byte-identique au store neutre) ; différence
de code invité (même manifeste codegen). Et rétrospectivement l'objet de
plusieurs cycles : « quel producteur naturel arme `manager+0x18` depuis
START » n'avait pas de réponse parce que la question n'avait pas d'objet —
sous HSIO=0, StartUp→Title se produit seule, sans START.

Décision : `kVdHsioTrainingSucceededResult` **reste à 1**. Passer à 0
échangerait un blocage contre un autre et masquerait le défaut. Le correctif
est de servir, sur le chemin ring, ce que l'invité attend.

Preuves : `reports/cycle-1825-demo-hsio-arbitrates-ring-against-progression.md`,
`artifacts/goal-playable/hsio-mode-transition-tradeoff-20260824/RESULT.md`.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Cycle 1824 — START est pressé à 42 % du film de marque

`CONFIRMÉ` (hypothèse utilisateur) : l'écran affiché quand START est envoyé est
le splash éditeur **Bandai Namco Games**, pas un titre interactif. Image
inspectée humainement :
`artifacts/goal-playable/title-start-timing-capture-20260824/png/frame-live-shape-inv.png`.

`RÉFUTÉ` — et c'est la première conclusion de ce cycle qui tombe : la timeline
SWG **n'est pas gelée**. Sur le binaire courant, l'owner racine `0x2E3CDD10`
passe de la frame 0 (tick 225) à la frame 85 (tick 480), soit exactement
`(480-225)/3`. Les owners `0x2E3CE490` et `0x2E3CED10` ont des tables de 1 et 2
frames : leur immobilité est **conforme**, pas une panne. Mesure :
`artifacts/goal-playable/swg-frame-advance-current-20260824/`.

Arithmétique qui recadre tout le problème :

```
racine 0x2E3CDD10 = 2220 frames        (end-begin)/8
cadence            = 1 frame / 3 ticks (frame 0 au tick 225)
fin du film        ≈ tick 6882
START au tick 3000 = frame 925/2220    soit 42 % du film
```

L'écran figé n'est donc pas un jeu bloqué : le film demande ensuite le handle
`0x0E000059` (attendu au tick ~1125) et le renderer échoue fail-closed sur son
fetch BC3 1280×720 ; la dernière image réussie persiste. Cela réconcilie la
contradiction avec `brandlogo-list2-window-runtime-20260823` sans régression.

Oracle Xenia, même XEX démo, session humaine
`ac6_demo_work/instrumentation-xenia/20260815-134231` : chargement à 15 s,
silence disque de 20 à 58 s pendant que le film joue, START humain à 58,55 s,
puis 514 lectures. La progression est pilotée par l'horloge du film, ni par le
disque ni par un événement externe. Écran cible capturé :
`ac6_demo_work/xenia-runtime-results-20260815-final/screenshots/xenia-title-check.png`.

Décision utilisateur : le renderer est « functional-enough », on n'y touche pas.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Cycle 1824 (première rédaction, corrigée ci-dessus) — START sur un splash

`CONFIRMÉ` (hypothèse utilisateur) : l'écran affiché quand START est envoyé est
le splash éditeur **Bandai Namco Games** — sigle à trois lobes, « Games », `™` —
et non un écran titre interactif. Image inspectée humainement :
`artifacts/goal-playable/title-start-timing-capture-20260824/png/frame-live-shape-inv.png`
(canal R étiré puis inversé, transformation de lecture seule ; aucun changement
renderer).

`PROUVÉ` : le readback est **byte-identique** à celui du cycle 1811 à la borne
1160 ticks (`changed_pixels=0`). L'instrument est calibré : il détecte un pixel
modifié d'un niveau. 429 draws pour 8 frames uniques ; seules les textures
`0x57` (64×64) et `0x58` (512×512) sont tirées, jamais le wordmark `0x59`.

`PROUVÉ` indépendamment (guest, sans instrument partagé) : le Title reste à
l'état interne 1 du tick 2452 au tick 8000, soit 5548 ticks immobiles
(`analysis/demo/ac6-demo-title-natural-current-8000-v1.json`).

Conséquence : la question « quel producteur naturel arme `manager+0x18` depuis
START » était mal cadrée. Il n'existe pas d'écran titre prêt à recevoir START.
La chaîne cohérente est : timeline SWG figée sur frame 0 → le splash ne se
termine jamais → aucun `EndMode` → listener `0x8217C890` jamais appelé →
`manager+0x18` jamais armé. Le cycle 1789 avait nommé cette frontière avant que
la campagne parte sur la couleur pour treize cycles.

Décision utilisateur : le renderer est « functional-enough », on n'y touche
pas ; focus sur la progression titre → menus → gameplay.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Cycle 1823 — consolidation, hygiène verte, pivot gameplay

**Redirect de priorité utilisateur (2026-08-24)** : focus sur l'atteinte du
début du gameplay ; toute run produit des captures pour validation automatique
et humaine.

Le fil couleur `BF` du logo titre (cycles 1810–1822, treize cycles dont onze
statiques) est **parké par redirect — ni réfuté, ni supersédé par preuve**. Sa
frontière exacte (callsites `0x82322438`/`0x82324118`) est conservée pour
reprise. Motif : les trois contrats mission01 portent
`"visual_parity_out_of_scope": true`; aucun contrat n'exige le fond blanc, et
le `done_when` « fond blanc » du cycle 1811 était auto-imposé.

Hygiène rétablie : `ctest` **26/26** (était 25/26 — `ac6-demo-complexity`
échouait, donc aucun commit n'était légal, donc l'arbre a grossi jusqu'à 51
fichiers suivis modifiés). La baseline de complexité est re-pinnée sur les
tailles atteintes ; les limites globales (220/1200/1000) sont **inchangées** et
le cliquet `aggravated over-budget baseline` continue d'interdire toute
aggravation. Gate mission01, artefacts contrats, adresses contrats, assert
liveness, numéros de CLAUDE.md et reset microexec : tous verts.

Aucune avancée causale guest dans ce cycle. Hypothèse à tester en premier au
cycle suivant, issue de l'utilisateur : *START était envoyé pendant les splash
screens*. Aucune capture n'a jamais été produite au tick du pulse (3000), et
`STATE.md` note par ailleurs que le logo Namco précède nettement le prompt
« PRESS START ». Un run naturel sans entrée, 8000 ticks, une capture par
présentation qualifiée, est lancé pour établir la chronologie réelle.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Cycle 1822 — ligne alpha réduite au transform fourni

- `PROUVÉ` — `0x82323BB8` conserve `r3` dans `r31` et son argument `r4`
  dans `r30`; les stores `+0x14c/+0x15c` lisent donc le transform fourni.
- `PROUVÉ` — avec un record actif, les deux champs valent
  `transform[0x4c]*T[0x0c]` et
  `transform[0x5c]*T[0x0c]+T[0x1c]`; sans record, le bloc complet
  `transform+0x40..+0x5f` est copié.
- `PROUVÉ` — pour le type 4, `T[0x0c]=1` et `T[0x1c]=0`; les deux branches
  donnent donc la même entrée alpha
  `transform[0x4c]+transform[0x5c]`.
- `RÉFUTÉ` — la présence du record ne peut expliquer `BF` pour ce draw.
- Frontière : qualifier le builder du transform `r4` aux appelants externes
  `0x82322438` et `0x82324118`.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Cycle 1821 — transform type 4 constant

`PROUVÉ` : l'entrée big-endian `0x8264CDF8[4]` vaut `0x82322EC8`, qui retourne
inconditionnellement `0x8264CDA0`. Ce global contient les floats
`[1,1,1,1,0,0,0,0]`; donc `T+0x0c=1` et `T+0x1c=0`. Les cinq champs couleur
passés à `0x820EB200` se réduisent exactement à
`MovieController+0x140/+0x144/+0x148/+0x14c/+0x15c`. L'octet `BF` vient de
`quantize255(clamp(C+0x14c + C+0x15c,0,1))`, pas du record type 4.
Prochaine frontière : stores/prédicats de `C+0x14c/+0x15c` dans
`0x82323BB8`. Aucun runtime ni changement renderer. `frontend=false`,
`mission=false`, `terminal=false`, `supported=false`.

# Cycle 1820 — producteur local de la matrice couleur

`PROUVÉ` : `r5` à `0x820EB200` est la matrice locale `M=r1+0x50` construite
par `0x82326420`, transmise en `r6` au handler type 0 `0x82325E70`, puis
replacée en `r5` avant le slot 7 renderer. Les champs `M+0x40/+0x44/+0x48`
sont `T[0/+4/+8] * MovieController[0x140/+0x144/+0x148]`; `M+0x4c` vaut
`T[0x0c]*MovieController[0x14c]`; `M+0x5c` vaut
`MovieController[0x15c]*T[0x0c]+T[0x1c]`. `T` vient du trampoline
`0x823237B8 -> 0x8264CDF8[type]`. La limite de cinq batches laisse
`0x8264CDF8[4]` comme pivot statique unique. Aucun runtime ni changement
renderer. `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.

# Cycle 1819 — pack couleur du record titre fermé

`PROUVÉ` : l'unique store vers `record+0x18` est `0x821DF018` dans
`0x821DEED8`. Il assemble quatre composantes quantifiées depuis
`parameters+0x3c/+0x30/+0x34/+0x38` lorsque `parameters+0x40 & 0x20`; sinon il
écrit le défaut `0xffffffff`. `0x820EB200` construit ces composantes depuis
`r5+0x40/+0x44/+0x48` et la somme bornée `r5+0x4c + r5+0x5c`. Son mot de flags
inclut toujours `0x860`, donc le bit `0x20` est actif. L'hypothèse d'une copie
depuis un champ unique est `RÉFUTÉE`. Le mot observé `BFFF0000` implique les
octets amont quantifiés `[BF,FF,00,00]`; la valeur flottante exacte produisant
`BF` reste inconnue. Aucun changement renderer ni runtime.

# Cycle 1818 — constructeur exact du nœud P1

`PROUVÉ` : `0x821185A8` est réfuté comme producteur du nœud source ; il copie
`source+0x24` vers `owner_de_travail+0x18` en aval. Le véritable producteur
est `0x82095DF0`, qui réserve `0x70` octets, appelle `0x821DEED8`, puis lie le
nœud par `owner+0x20`/`owner+0x24` et `tail+0x10`. Ce sont exactement les
champs consommés par `0x82119488`. La frontière est maintenant le store
interne de `record+0x18` dans `0x821DEED8`, puis son champ source dans le bloc
`r1+0x50` construit par `0x820EB200`. Aucun runtime ni changement renderer.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.
Preuve :
`artifacts/goal-playable/title-p1-record-constructor-static-20260824/RESULT.md`.

# Cycle 1817 — le writer P1 reçoit un nœud de liste

`PROUVÉ` : `0x82119488` charge chaque owner depuis `r31+4+4*i`, prend la tête
à `owner+0x20`, parcourt les records par `record+0x10`, puis passe le record
courant en `r5` à `0x82118650`. Le dispatcher le transmet en `r4` au writer
sélectionné par `0x82009E78 + *(record+0x14)*0x14 + 0x0C`; le `r4+0x18` lu par
`0x82118D18` est donc exactement `record+0x18`. `0x821185A8` contient un
store candidat `0x8211860C: stfs fr12,0x18(r31)`, mais son `r31` n'est pas
encore joint à cette liste et `fr12` n'est pas relié à `BFFF0000`. Aucun
runtime ni changement renderer. `frontend=false`, `mission=false`,
`terminal=false`, `supported=false`. Preuve :
`artifacts/goal-playable/title-p1-record-color-source-static-20260824/BLOCKER.md`.

# Cycle 1816 — slots exacts du dispatch P1

`PROUVÉ` sur le `xex-basefile.bin` relié explicitement au XEX demo PAL : deux
descripteurs de `0x14` octets contiennent `0x82118D18` à `entry+0x0C`, aux
entrées `0x82009E8C` et `0x82009EB4`. Leurs tuples bruts sont respectivement
`(0000000D,09000001,826F61C0,82118D18,821187A8)` et
`(0000000D,09000003,826F61C0,82118D18,821187A8)`. Aucune branche PPC directe
ne cible le writer et aucune instruction ne construit les cellules exactes :
le consommateur charge une base et indexe la table. Callsite, règle d'index,
type de `r4`, store `r4+0x18` et provenance de `BFFF0000` restent ouverts.
Preuve :
`artifacts/goal-playable/title-p1-dispatch-xref-static-20260824/BLOCKER.md`.
Aucun runtime ni changement renderer. `frontend=false`, `mission=false`,
`terminal=false`, `supported=false`.

# État autoritaire courant — cycle 1814 (24 août 2026)

La contradiction P/Q est fermée. Le shader titre consomme le fetch constant
95/P1 : `103FB893 100A9002`, base `0x103FB890`. Ses quatre couleurs guest
valent `BFFF0000`. Le mode 3 `k16in32`, l'unpack `FMT_8_8_8_8` et le swizzle
`zyxw` produisent exactement `(191,0,0,255)`, rouge opaque à environ 75 %.

Le slot 94/Q1 analysé au cycle 1812 est un garde adjacent non consommé ; son
décodage rouge était correct mais ne qualifiait pas `vf0`. Aucune copie Q->P
ni erreur de mapping du bridge n'est présente sur ce draw. La frontière remonte
au record source du writer `0x82118D18`, qui copie `r4+0x18` sans
transformation aux quatre champs `vertex+0x30`. `frontend=false`,
`mission=false`, `terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1813 (24 août 2026)

La chaîne couleur du renderer titre est fermée du fragment shader au PPM : le
PS calcule `texture * couleur vertex`, le render target titre est un
`R8G8B8A8` direct, et son readback n'emprunte ni l'EDRAM ni
`copy_dest_swap`. Le rouge existe donc avant la copie et la présentation ; une
nouvelle permutation de canaux au readback serait une double correction.

La preuve vertex du cycle 1812 est toutefois qualifiée pour le fetch constant
94, alors que le VS titre consomme `vf0`, joint au slot 95 par le bridge. La
valeur exacte du slot 95 au même draw n'est pas encore conservée. La nouvelle
frontière est cette réconciliation P/Q : décoder les deux dwords du slot 95 au
draw qualifié avant de remonter le producteur de `vertex+0x30` ou de modifier
une sémantique couleur. `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.

# État autoritaire précédent — cycle 1812 (24 août 2026)

La sémantique du fetch couleur du titre est fermée. Le slot 94 vaut
`104A4893 100087F2`; ses bits endian valent 3, soit `k16in32` et non
`k8in32`. Les octets guest `FF FF 00 00` deviennent `0xFFFF0000` après
échange des demi-mots, sont décompactés en `(0,0,255,255)`, puis le swizzle
VS `zyxw` produit le rouge opaque `(255,0,0,255)` observé.

Le décodeur vertex, le swizzle de présentation et le readback sont donc
réfutés comme source du fond non blanc. La frontière remonte au producteur du
dword à `vertex+0x30` dans le buffer Q1, ou à un état de composition antérieur
qui doit établir le blanc. Aucun correctif produit n'est ajouté dans ce gate.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1811 (24 août 2026)

Le swizzle du fetch de présentation est maintenant appliqué au readback : le
descripteur demande `B,G,R,1`, complément exact du `copy_dest_swap=1`. Cette
correction transforme la capture bleue précédente en rouge et prouve que le
readback restitue désormais les canaux présents dans le frontbuffer invité.

La validation visuelle échoue toujours : la capture 1280x720 est rouge plein
écran, avec les lobes et « Games » plus sombres; ses coins valent `(255,0,0)`
et seuls les pixels du canal rouge sont non nuls. Les traces de draws montrent
un premier quad plein écran dont les quatre couleurs vertex valent
`0xFFFF0000`, puis le quad du logo en `0xBFFF0000`. La nouvelle frontière est
donc la sémantique de `FMT_8_8_8_8`/endianness et de la modulation couleur au
premier draw, avant la présentation. Après cinq batches, aucune seconde
correction n'est tentée. `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.

# État autoritaire précédent — cycle 1810 (24 août 2026)

Le contrat HSIO est corrigé : `VdIsHSIOTrainingSucceeded` retourne 1. Le run
codegen-on à 1160 ticks restaure exactement 502 soumissions, 4405 dwords et
407 présentations renderer. Retourner 0 activait le fallback logiciel invité
`0x821BA130` et contournait le ring; ce n'était pas une réparation valide.

La capture 1280×720 contient le logo complet mais échoue l'oracle : le fond
attendu blanc est bleu et tous les pixels ont R=G=0. La frontière est le
décodage/export de la couleur vertex `FMT_8_8_8_8` vers l'interpolateur du
pixel shader, avant toute nouvelle capture. `frontend=false`, `mission=false`,
`terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1809 (24 août 2026)

Le témoin positif du 22 août fournit désormais 53 tuples `NtReadFile` complets
à 1160 ticks, avec 502 soumissions et 407 présentations. Sa capture obligatoire
1280x720 est uniformément noire, vérifiée automatiquement et humainement.

Le côté courant de l'A/B est non qualifié : `build/` était codegen-off et a
refusé le probe à tick 0. Après cinq batches, le gate s'arrête avant toute
conclusion comparative. Le tracepoint minimal est prêt; la reprise exacte est
un rebuild `build-codegen-on` puis le seul run courant. `frontend=false`,
`mission=false`, `terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1808 (24 août 2026)

L'export Ghidra canonique ferme le site `0x821A61EC` : c'est le slot `+0x10`
de la table file-I/O globale `0x823C2D2C`, appelé par `Function_821A6168`,
avec traitement de `STATUS_PENDING` (`0x103`) et attente éventuelle via
`NtWaitForSingleObjectEx`. Le bridge courant joint ce chemin à `NtReadFile` et
publie immédiatement l'événement de complétion après sa lecture synchrone.

La piste d'un `NtSetEvent` direct ou d'une création du scheduler est réfutée.
Le binaire positif contient le même helper de publication auto-reset et
l'appelle également depuis sa branche `NtReadFile`; une différence locale de
contrat est donc réfutée. La frontière exacte devient le premier état d'entrée
`NtReadFile` divergent : fichier/offset/longueur, événement ou waiter.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1807 (24 août 2026)

La première divergence en amont du ring est maintenant événementielle. Le
binaire courant publie seize handles `0xE0000088…0xE00000C8` depuis le LR
invité `0x821A61F0`, absents du témoin ancien, puis bloque ses 23 threads. Le
témoin ancien garde le thread principal runnable et publie le ring.

L'atlas qualifié place le site dans `Function_821A6168`, mais son pseudocode
n'est pas conservé. Après cinq batches, le gate s'arrête sur l'export Ghidra
ciblé de cette seule fonction. `frontend=false`, `mission=false`,
`terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1806 (24 août 2026)

Le binaire codegen-on conservé du 22 août restaure à 1160 ticks 502
soumissions ring, 4405 dwords et 407 présentations renderer. Le binaire
courant reste à zéro aux mêmes entrée et borne. Le hook
`AC6_PPC_STORE_U32(0x7FC80714) -> apply_xenos_mmio_write` existe dans les deux
binaires : sa suppression est réfutée et la frontière remonte à la route
guest/scheduler qui atteint ce store.

Le readback obligatoire du témoin ancien est 1280x720 mais uniformément noir,
vérifié automatiquement et humainement. Les présentations ne valident donc
pas encore le frontend. `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.

# État autoritaire précédent — cycle 1805 (24 août 2026)

Le binaire courant rejoué exactement jusqu'au tick 1160 reste à
`RPTR=WPTR=0`, zéro soumission, zéro dword et zéro présentation renderer. Le
run positif historique produisait à cette même borne 502 soumissions, 4405
dwords et le logo Namco visible.

La régression native antérieure à `CP_RB_WPTR` est maintenant prouvée ; un
reset tardif des compteurs est réfuté. Aucun readback courant n'a été produit.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1804 (24 août 2026)

Un A/B natif strict avec le même binaire courant et la seule différence START
prouve que neutre et START restent tous deux à zéro soumission PM4, zéro dword
et zéro présentation renderer malgré 24 draws typés et 2894 `VdSwap`.

START et le changement de frame SWG sont réfutés comme cause de l'écran noir.
Le contrôle positif historique est visuellement confirmé : logo Namco non noir
en 1280×720. La frontière remonte à une régression ring/MMIO du binaire
reconstruit après ce run. `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.

# État autoritaire précédent — cycle 1803 (24 août 2026)

Le HIR Xenia Edge qualifié prouve que `0x822DA568` stocke son callback `r3` à
`node+0x08` et Q (`r4`) à `node+0x0C`; `0x822E35E8` les relit et appelle le
callback. Pour le record naturel type 1, la case `0x82009E9C` vaut exactement
`0x821187A8`. La chaîne Q→callback est fermée.

La frontière devient le premier événement postérieur à `0x821B4D80` qui
produit un kickoff GPU dans Edge mais manque au natif. Les screencaps natives
restent noires et `supported=false`.

# État autoritaire précédent — cycle 1802 (24 août 2026)

Le record naturel `0x827B3A80` du draw `0x12` est maintenant joint au premier
slot de queue. Avec `[0x826F61B8]=0x2E8B8C20` et `record+0x14=1`, il passe
`0x82119488 -> 0x82118FA0 -> 0x821185A8`, crée le slot `0x8270F598`, puis
atteint `0x821186B0`.

Les appels worker `0x821187A8 -> 0x821B4D80` observés au même tick précèdent
cette ingestion et ne sont pas attribués au lot courant. La frontière unique
est le transfert asynchrone `Q 0x8270F598 -> 0x822DA568/0x822E35E8 -> P ->
0x821187A8`. Le ring reste à zéro soumission et zéro dword. Quinze screencaps
1280×720 sont uniformément noires, dont une inspectée humainement.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1801 (24 août 2026)

Le chemin naturel `list13/draw_index=0x12` est fermé jusqu'à la création du
record renderer. Le handle exact est `0x0E000071`; les seize draws passent
`0x820EB200 -> 0x820EA9A0 -> 0x82095DF0 -> 0x821DEED8`, qui alloue les records
de `0x70` octets. La piste d'un index 0x12 invalide est réfutée.

La run Vulkan conserve 24 draws typés et `present_count=0`. Elle a cette fois
produit 21 screencaps Xvfb : toutes sont 1280×720, une couleur, moyenne 0, et
une inspection humaine confirme le noir uniforme. La frontière est désormais
le consumer du record puis son encodage PM4/Xenos ou sa condition de rejet.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1800 (24 août 2026)

Le chemin naturel `list_index=13` est fermé jusqu'au premier consumer
renderer. `table[13]={type=0, offset=0x1998}` désigne une liste de 16 records
type 0 à `0x2DCB2BB8`; ils portent `draw_index=0x12` et rejoignent tous
`0x82325E70 -> slot 7 (LR 0x82325ED4) -> 0x820EB200`.

Le run compte 24 draws typés mais `present_count=0`. Ni la screencap d'audit
ni le readback PPM n'ont été produits : aucune validation visuelle n'est donc
revendiquée. La frontière est l'entrée ABI 0x12 de `0x820EB200`, son handle de
draw/queue, puis le producteur de ressource présentable. `frontend=false`,
`mission=false`, `terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1799 (24 août 2026)

Le chemin type 4 est fermé statiquement jusqu'à la table relocalisée : pour
le sous-enfant `0x2E3F1350`, `list_index=13` désigne
`table[13]=[0x2DD796A4,0x2DD796AC)`. Les deux mots et le record cible sont
absents des artefacts statiques qualifiés ; l'égalité disque/heap complète de
`TitleUS` n'est pas établie. `TitleUS` est bien le bundle anglais présent dans
la démo PAL, distinct de `Title`, et non une preuve NTSC-U.

La frontière est un snapshot read-only unique au tick 3001 du descripteur,
de la liste et de ses records bornés. `frontend=false`, `mission=false`,
`terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1798 (24 août 2026)

La branche enfant post-START est jointe exactement : frame 0 de
`0x2E3F8C50` = type 5 / index objet `0x42`, créant `0x2E3F1350` ; frame 0 du
sous-enfant = type 4 / `list_index=13`. Aucun bytecode VM n'est exécuté par
cette branche au tick 3001.

La frontière est `list_index=13 -> 0x82326420 -> record -> premier consumer
renderer/draw`. `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.

# État autoritaire précédent — cycle 1797 (24 août 2026)

Le run naturel post-START conservé prouve qu'au tick 3001 l'enfant
`0x2E3F8C50` n'exécute aucun offset VM : les deux `execute_raw` observés sont
liés au parent `0x2E3EDA90`. Son premier effet est la construction du
sous-enfant `0x2E3F1350`, avec `MovieMemory=0x2E3F1590`.

La frontière est désormais la seconde factory du même tick : qualifier ses
arguments `B/D`, la frame 0 du sous-enfant et le premier effet sortant de la
hiérarchie. `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.

# État durable — AC6 démo PAL native

## Gate statique objet→shader — classe mapparts fermée (24 août 2026)

Dans la démo canonique, 169 des 170 objets `mapparts` portent la clé matériau
brute `0x30000010`. `Function_822E8668` résout `material+0x00` dans le même
registre `0x8296BF80` que `Function_822E0950` peuple depuis les descripteurs
NSXR. La clé rejoint exactement `vsCstCT.updb + psCT.updb`. `NU_FLAG1` varie
entre 0 et 1 sans changer cette paire ; les 4 312 descripteurs portent le
format vertex `06/13`, construit séparément par `Function_822EDF60`.

La variante `0x30040010` contient la même paire sous `Common_HDR`, mais le
writer de `material+0x14` reste à fermer. L'objet exceptionnel porte
`0x30000090`, absent des 51 NSXR. `NU_HASH` comme clé NSXR et les familles
`psMapCTF2_*` pour `0x30000010` sont réfutés. Aucun runtime n'a été utilisé.

Preuves : `research/object-shader-static-boundary.md` et
`artifacts/shader-selection-xrefs.txt`.

## État autoritaire courant — cycle 1795 — promotion et premier tick enfant fermés

`0x82323BB8` ferme la promotion recherchée : au début du tick parent, il copie
la tête pending `A+0xE4` dans `A+0xF4` puis vide `A+0xE4/A+0xE8/A+0xEC`.
Après les handlers de frame, il parcourt la nouvelle liste `A+0xE4` et
s'appelle récursivement sur chaque enfant. L'enfant construit par
`0x82323468` reçoit donc son premier tick dans la même invocation parentale,
puis devient la tête active du parent au tick suivant.

La publication différée n'est plus une frontière. Le prochain gate part du
premier tick enfant et cherche son premier effet persistant vers la VM, un
callback natif ou un propriétaire de transition qualifié. Aucun effet
`EndMode`, listener Title, `manager+0x18` ou frame frontend n'est encore
établi. `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.

## État autoritaire courant — cycle 1794 — enfant pending, promotion active ouverte

Le corps canonique `0x82323468..0x823235CF` ferme le consumer immédiat du
`MovieController` enfant observé au cycle 1793. Après construction ou
réutilisation, le résultat `C` est enregistré par le slot virtuel 4 puis
inséré dans la liste `A+0xE4/A+0xE8`. Le corps ne publie jamais `C` dans
`A+0xF4`; sa seule écriture à ce champ retire un ancien candidat actif.

`swg::MovieController` est traité comme contrôleur de timeline UI scriptée,
distinct d'un décodeur vidéo et de `CModeTaskTitleMovie`. La route active
qualifiée appelle `0x82323BB8(A+0xF4)`, où les offsets de `MovieMemory` sont
remis à la VM. La frontière est donc la promotion pending→active de
`A+0xE4/A+0xE8` vers `A+0xF4`, puis le premier tick de l'enfant.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

## État autoritaire courant — cycle 1793 — MovieController enfant observé, frontend ouvert

Le gate renderer reste fermé naturellement depuis le cycle 1786 : deux cold
runs sélectionnent `list 2 / draw_index 2 / handle 0x0E000059`, produisent Q1
côté guest, passent `921600/921600` samples et donnent le même logo Namco
visible. Aucun draw, handle, Q1, scheduler ou pixel n'est forcé.

Le contrat HLE fail-closed HSIO du cycle 1791 reste qualifié et la transition
naturelle Startup → Title reste fermée : au tick 2369, le manager publie le
vrai `CModeTaskTitleDemoOffline` de vtable `0x820113E4`, dont l'état atteint 1
aux ticks 2384/2385. Le pulse START tick 3000/release 3001 atteint ensuite les
globales logiques current/pressed à `0x10`, deux dispatchs virtuels et les cinq
callbacks GetCurrentLevel, GetCurrentMission, GetCurrentMode, SendMsgI(M102)
et OnVoice2D. L'input n'est plus la frontière.

Dans `[2990,3041)`, aucun appel `0x820EA4A8`, writer `manager+0x18`, changement
de tâche ou mutation du Title n'est observé. La statique ferme les writers
`MovieController+D5`, les quatre lanceurs SWG et la chaîne commune vers
`menu_endMode` sans trouver de producteur post-TitleUS joint au listener
`0x8217C890`. Elle ne permettait toutefois pas d'exclure un nouvel objet
`MovieController` construit par un événement guest distinct.

La qualification statique ferme désormais le caller naturel `0x82322300`,
l'agrégat `A`, le storage incorporé `B=A+0x08` et la sélection de `D` depuis
les tables de B jusqu'à la factory virtuelle `0x820D18C8`. Le décompresseur
global `0x82278F78`, `swg::MovieMemory` et ses slots Add/Count/GetAt/Clear
restent correctement séparés des helpers globaux ACC
`0x823233B0/0x82323468`. Le writer statique de record #2/draw 2/handle 59
reste une question d'auteur distincte ; il ne réouvre pas le gate renderer
déjà fermé par l'exécution naturelle.

Une trace observation-only qualifiée observe au tick 3001, sous le vrai Title,
le callsite enfant `0x8232356C` appeler la méthode virtuelle `0x820D18C8`,
puis l'initialiseur d'instance `0x82323808`. Le résultat est un
`MovieController` de vtable `0x820304D8`; `A`, `B=A+8`, `D`, `s=-1` et
`MovieMemory` concordent avec l'ABI statique. Cela réfute l'ancienne hypothèse
d'absence de reconstruction post-START.

La portée reste bornée : `0x8232356C` est une adresse de retour/callsite, pas
une fonction. L'observateur s'arrête au retour de cette factory enfant et ne
prouve ni publication racine dans `A+0xF4`, ni remplacement, ni `EndMode`, ni
listener Title, ni requête mode-manager. Les appels génériques ultérieurs à
`0x82323808` ne sont pas corrélés à cette invocation. Le report finit à
`max_ticks=3407` avec `frontend=false`, `mission=false`, `terminal=false`; le
backend headless ne qualifie aucune frame visible.

Le prochain gate reste statique : qualifier le corps contenant le callsite
`0x8232356C`, le prédicat de construction, la source de `D` et le
consumer/publicateur du résultat. Seulement depuis cette chaîne qualifiée,
chercher un réarmement D5, `EndMode`/`menu_endMode`, le listener
`0x8217C890` ou une requête `manager+0x18`. Une autre trace exige d'abord un
nouveau prédicat causal et un `done_when` plus précis. Le gate frontend reste
état guest persistant + frame visible post-transition. `supported=false`.

Preuves autoritaires :
`artifacts/goal-playable/title-start-edge-runtime-final-20260823/RESULT.md`,
`artifacts/goal-playable/swg-acc-caller-join-static-20260823/RESULT.md`,
`artifacts/goal-playable/title-post-start-moviecontroller-join-runtime-20260823/RESULT.md`,
`analysis/demo/ac6-demo-structural-types-v1.json` et
`reports/cycle-1793-demo-post-start-child-moviecontroller.md`.

Les sections suivantes sont historiques ; leurs anciennes frontières restent
supersédées par le cycle 1793.

## Historique cycle 1788 — vrai Title et frame SWG supposée post-START

Le gate renderer est fermé. Deux cold runs naturels identiques sélectionnent
`list 2 -> draw_index 2 -> handle 0x0E000059`, produisent un Q1 invité non
nul et guest-owned, font passer `921600/921600` samples, puis affichent un
logo Namco centré reconnaissable dans le readback 1280x720. La trace est
identique octet par octet entre les deux runs (SHA-256
`389ba502569afa3c9b7956b84f98f5a777aa311a446dda7955cc5daed5bb889b`),
comme le PPM (`6c0ab7ac...`). Les couleurs restent imparfaites, mais aucun
draw, handle, Q1, scheduler ou pixel n'est forcé. Preuves :
`artifacts/goal-playable/title-vertex-window-runtime-20260823/RESULT.md` et
`artifacts/goal-playable/title-vertex-window-runtime-20260823-r2/RESULT.md`.

La reprise Ghidra canonique ferme ensuite `CTaskModeManager` :
`0x8218E970` sélectionne une factory globale, `0x8218EA88` est le setter
d'instance `manager+0x10`, `0x82190B18` est la méthode de transition qui
range finalement le nouvel objet dans `+0x08` et l'insère via
`[*0x82822F08]->slot+0x0C`, et `0x827435F8` est statiquement prouvé
`CTaskModeManager*`. Aucun shim n'est justifié. Preuve autoritaire :
`artifacts/goal-playable/frontend-mode-manager-canonical-ghidra-20260823/RESULT.md`.

Le vrai objet titre est maintenant séparé sans ambiguïté : la factory globale
`0x82192680` alloue `0x78` octets et appelle le constructeur d'instance
`0x8218DC10`; `0x820113E4` est la vtable primaire globale de
`CModeTaskTitleDemoOffline` et `0x82011384` sa vtable secondaire
`CSwgListener` à `this+0x68`. La méthode listener `0x8217C890` appelle le slot
parent `+0x48 = 0x8218AB98`, écrit `Title+0x44=3` et `Title+0x0C=2`; trois
updates `0x8218A7A8` plus tard, le slot `+0x4C = 0x8218AA30` précède
l'écriture de `manager+0x18=1`. Cette chaîne est qualifiée **si**
`menu_endMode` atteint le listener ; elle n'est pas une jonction naturelle
depuis START.

`CModeTaskMissionTitle` est un type distinct (`RTTI 0x823918C4`, vtable
`0x8200E5C4`, update `0x82185198`) et n'est pas le propriétaire du vrai titre
démo. Toute la famille `CModeTask{StartUp,Title,Loading,Game}DemoOffline` est
propre à la démo et absente des prototypes retail Preview; aucun layout,
constructeur, slot, attribut ou nom de méthode n'en est transféré. Le listener
global `0x826DF804` et `E000004C/0x821A8C88` sont également réfutés comme
arête manquante vers le manager.

Le SWG `brandLogo` contient bien `EndMode`, mais le cadrage
`M102=0/lookup 0x0B -> prédicat EndMode` est maintenant réfuté. Les deux
valeurs sont évaluées dans un item déjà ajouté à `MovieMemory`; l'audio est un
second item indépendant. Le vrai sélecteur est le handler type 6
`0x82322A80`, qui enfilerait `EndMode` uniquement pour une commande exacte
`{type=6,payload=0x00000E04}`.

Dans la fenêtre START qualifiée, le chemin d'initialisation republie la plage
du `swg::MovieController`, de deux entrées à une entrée type 4. Le même LR
`0x82323860` prouve ce chemin, mais ne distingue pas à lui seul une nouvelle
instance d'une adresse réutilisée, ni lequel de storage `B` ou descriptor `D`
change. Le handler type 6 disparaît, alors que la couche parallèle reste
vivante par
`0x82326608 -> 0x82326420 -> 0x82325E70 -> 0x820EB200`. La frontière jouable
ne se situe toutefois pas dans ce consommateur : RTTI et vtable prouvent que
`0x820EB200` est le slot 7 d'une instance `CSwgRenderer`, et le CFG ne fait
que produire un record CPU de layout `0x70`, ensuite sérialisé et mis en
file. `0x82325E70` est un callback de table globale, pas une méthode prouvée.

L'analyse exhaustive des douze descripteurs `brandLogo` couvre 2 351 frames
et onze éléments type 6, sans payload brut littéral `0xE04`. Cette absence ne
ferme pas la relocation vers le buffer heap décompacté. Les trois bords
statiques ouverts sont donc la carte brut→heap, le producteur de `B/D` à
START et l'état persistant de `CModeTaskTitleDemoOffline` pouvant déclencher
une transition indépendante. Aucune trace de `M102`, `0x0B` ou du renderer
n'est justifiée. Le logo Namco appartient à une époque nettement antérieure
au prompt visible « PRESS START ».

Preuves autoritaires :
`artifacts/goal-playable/title-start-route-static-20260823/RESULT.md`,
`artifacts/goal-playable/start-title-object-rtti-static-20260823/RESULT.md`,
`artifacts/goal-playable/title-swg-post-start-static-correction-20260823/RESULT.md`,
`artifacts/goal-playable/title-frame-descriptor-static-20260823/RESULT.md`,
`artifacts/goal-playable/title-parallel-matrix-consumer-static-20260823/RESULT.md`
et `reports/cycle-1788-demo-title-swg-frame-switch-correction.md`.

Les sections ci-dessous sont l'historique antérieur ; lorsqu'elles décrivent
le sélecteur SWG ou la publication du mode comme « gate courant », elles sont
supersédées par l'état ci-dessus.

## Gate BrandLogo — rupture au sélecteur de liste SWG (23 août 2026)

La trace observation-only ciblée ferme la fenêtre titre 190..320 : l'owner
SWG imbriqué `0x2E3CE490` a une frame valide mais unique, sélectionne
`list_index=0`, puis une liste d'un seul record `draw_index=0` raccordé au
handle `0x57`. Aucun record `draw_index=2/handle59` n'est publié. L'owner
parent avance simultanément de la frame 0 à 31, ce qui réfute un gel global du
scheduler ou de la task. La perte du logo est donc antérieure au renderer,
au worker et à PM4 : elle se situe au producteur/script qui doit changer la
frame ou la liste imbriquée vers l'entrée `draw2`. Preuve :
`artifacts/goal-playable/brandlogo-draw2-selector-runtime-20260823/RESULT.md`.

## Gate renderer titre — blend fermé, logo sélectionné en amont (23 août 2026)

Le pipeline Vulkan titre applique maintenant le blend Xenos naturellement
observé (`RB_BLENDCONTROL0=0x00010706`) sous une qualification fail-closed;
les autres pipelines restent en overwrite. Le build et six tests ciblés
passent. Un replay borné à 400 ticks produit 66 captures 1280x720 et des
readbacks évolutifs, mais chaque capture reste un aplat bleu. La perte n'est
donc plus le blend ni le resolve.

Le même payload invité enregistre naturellement trois NTXR : `0x57` (64x64),
`0x58` (512x512) et `0x59` (`003_NTXR`, wordmark Namco 1280x720). Les draws
atteints sélectionnent `0x57`, puis brièvement `0x58`, mais pas encore `0x59`.
La frontière active est exclusivement le sélecteur/producteur du record
`draw2 -> 0x59`; aucune substitution de ressource n'est autorisée. Preuves :
`artifacts/goal-playable/title-blend-runtime-20260822/RESULT.md` et
`artifacts/goal-playable/brandlogo-resource-entry-runtime-20260822/RESULT.md`.

## Gate BrandLogo — constructeur de commandes atteint (22 août 2026)

Le probe borné `brandlogo-consumer-runtime-20260822` ferme le consommateur de
liste et son worker : la garde `0x826F61B8` est valide, le record type 1 avec
ressource `0x0E000057` passe par `0x82119488 -> 0x82118FA0 -> 0x821185A8 ->
0x821186B0`, puis le thread 14 appelle naturellement `0x821187A8 ->
0x821B4D80` avec le contexte renderer `0x10041A00`. Pourtant le ring reste à
`RPTR=WPTR=0`, sans soumission ni paquet PM4. La frontière ouverte est donc le
cursor/flush après `0x821B4D80`, pas la ressource ou la liste BrandLogo. Voir
`artifacts/goal-playable/brandlogo-consumer-runtime-20260822/RESULT.md`.

## Gate BrandLogo — géométrie publiée, consommation aval ouverte (22 août 2026)

Le probe natif borné `acc-resource-runtime-20260822` ferme les deux gardes
amont : les slots `0x0E000057/58` ne sont pas le sentinel `-1`, la géométrie
est non nulle, puis le guest atteint naturellement `0x82095DF0 -> 0x821DEED8`
et publie des records de 112 octets sous l'owner `0x8281EAF0`. Le noir ne
vient donc ni du mapping ACC immédiat ni de `0x820EA9A0`. Le premier contrat
ouvert est le consommateur/flush aval de cette liste et son raccord à la
soumission Xenos; aucun record, draw ou pixel synthétique n'est justifié.
Voir `artifacts/goal-playable/acc-resource-runtime-20260822/RESULT.md`.

## Gate SWG callback — appelé mais non armé (22 août 2026)

La trace native bornée `artifacts/goal-playable/swg-callback-arm-20260822/`
ferme l'ambiguïté du dispatch SWG : `CSwgCallback` (`vtable 0x820061FC`)
atteint naturellement son slot `+0x14 → 0x820CDF30`, avec `this+8=1`. Le
champ `this+9` reste nul, car les mots lus via
`*(u32*)0x823C27E0 + 0x23980/+0x23988/+0x2398C/+0x23990` sont des bitsets
d'état logique/action (et non une table de ressources) et restent nuls; la
garde `0x826DFC48` vaut également zéro. Aucun draw normal, PM4 visuel ou pixel
RT0 n'est produit; le frontbuffer reste `0/921600` non nuls à 3000 ticks.

Le VFS lit bien `DATA.TBL` et `DATA00.PAC` au démarrage (`vfs2/`), mais cette
passe est bornée avant de pouvoir attribuer le chargement SWG. Les écrivains
qualifiés de ces bitsets sont `0x821DE990` et `0x821DE6E0`; le bord causal
restant est leur état SWG/script ou le consommateur de la garde globale, pas
le renderer. Aucun bit, callback, record ou pixel n'a été synthétisé. Voir
`artifacts/goal-playable/swg-callback-arm-20260822/RESULT.md` et
`BLOCKER.md`.

## Gate courant — entrée de jeu non publiée (22 août 2026)

La passe `task-loading-publication` ferme aussi le faux bord de terminaison :
`0x8218CE20`/`0x8218CCD0` ne font que sonder et terminer `CTaskLoading`,
`0x82259D10` ne met à jour que les tâches déjà insérées, et
`0x82259E18`/`0x82259FF8` gèrent insertion/initialisation de listes. `0x821929A8`
ne fait qu'avancer les champs de requête du `CTaskModeManager` et sonder son
loader embarqué. Aucun de ces points ne publie `CModeTaskGameDemoOffline` ou
n'appelle `0x8217C678`; le prochain bord causal est donc la factory amont qui
construit le mode puis l'insère. Preuves :
`artifacts/goal-playable/task-loading-publication/RESULT.md` et `BLOCKER.md`.

Le follow-up owner/r8 ferme la provenance AVI : `0x8200B6FC/+0x04` cible
`0x82165CC0`, mais aucun constructeur ne publie `base+0xC` comme receiver et
aucun producteur de `r8` n'est identifié. Le writer type 1 reste inaccessible
sans état invité synthétique. Preuve :
`artifacts/goal-playable/owner-r8-followup/RESULT.md`.

Le slice statique `game-entry-static` ferme l'absence de pont partagé
qualifié : `0x8217C678` installe `0x8217C4D8`, mais
`CModeTaskGameDemoOffline` n'est jamais publié. Le thunk START `0x820D32D0`
reste un dispatch virtuel `vtable+112` dépendant de l'objet; le remplacer ou
forcer `0x8217C4D8(-3)` serait un état guest synthétique. Provider, listener et
réveil `E000004C` sont déjà qualifiés. Le prochain discriminateur est la
terminaison/publication de `CTaskLoading` vers le mode manager autour de
`0x8218CE20`, `0x82259E18/0x82259FF8`, `0x821929A8`. Preuves :
`artifacts/goal-playable/game-entry-static/RESULT.md` et `BLOCKER.md`.

## Gate courant — route du logo Namco (22 août 2026)

La sonde native bornée du 22 août (`runtime-current-20260822`) a atteint le
timeout à 1622 ticks sans produire de record type 1–4, AVI receiver ou paquet
PM4; les seules écritures observées concernent les pointeurs de contrôle de
queue, dont les slots restent nuls. Les tranches ACC et AVI parallèles ferment
également les deux jonctions candidates : `0x8219EE40`/`0x8219F5D0` ne
publient pas le descripteur consommé par `0x8219E580`, et
`0x82166550`/`0x821674A8` n'exposent pas leur receiver ajusté à
`0x82165CC0`. Aucun changement runtime n'est justifié par ces résultats.
Preuves : `artifacts/goal-playable/runtime-current-20260822/`,
`artifacts/goal-playable/acc-provider-deep/RESULT.md` et
`artifacts/goal-playable/avi-record-deep/RESULT.md`.

La dernière tranche statique autour de `0x8218BFB0`/`0x8219BFB0` est
négative : `0x8218BFB0` sélectionne des clés/tables de providers et
`0x8219BFB0` n'est qu'un `lwz r4,0x20(r31)` dans `0x8219BF40`. Aucun des deux
ne relie `DATA.TBL[170/171]`, `brandLogo` ou `texture_id 2` au `source+0x20`
consommé par `0x8219E580`. Le gate reste donc bloqué au raccord contenu→ACC;
aucun mapping synthétique n'est autorisé. Preuve :
`artifacts/goal-playable/acc-guest-join-next/RESULT-final.md`.

La route du premier logo est qualifiée comme un film/cue SWG
(`0x820E8F90 → 0x820EA4A8 → 0x821728C0`) et son payload disque est maintenant
identifié : `DATA.TBL[170/171]` contient `FHM → FHM → SWG "brandLogo"`
(118244 octets) avec huit feuilles NTXR soeurs, chacune portant `GIDX`
`0x08000000`. Le tableau de textures du SWG sélectionne précisément
`texture_id 2 → 003_NTXR.ntxr`, dont le décodage est le mot-symbole rouge
`namco®` sur fond blanc ([artefact visuel](artifacts/goal-playable/brandlogo-swg-refs/png/0002_1280x720_fmt14_mip1.png)).
Cette preuve nomme enfin les octets Namco de la démo, mais ne relie pas encore
le SWG/NTXR exact à la table ACC guest consommée par `0x8219E580`.
Le renderer existant conserve toutefois la chaîne réutilisable
`0x8219E580 → 0x821A00E8 → 0x8219F080 → 0x820EB200 → … → 0x822F5BC8`.

Le runtime reste donc sans image visible : aucun draw guest non-bootstrap ne
change RT0 et aucune capture 1280×720 non noire n'est produite. Aucun logo
host-peint, appel forcé, record synthétique ou nouveau chemin Xenos n'est
autorisé. La provenance immédiate de l'ACC est maintenant qualifiée
(`0x8219E580 ← 0x821A02C0 → 0x8219E768`, ressource parallèle
`0x821A0180 → 0x8219F080/0x8219F1C0 → 0x8219E428`), mais la jointure vers
PAC/FHM/DATA.TBL et les handles `0x0E000057..0x0E00005E` reste ouverte.
Preuve : `artifacts/goal-playable/namco-logo-route/RESULT.md`,
`artifacts/goal-playable/brandlogo-swg-refs/RESULT.md` et
`artifacts/goal-playable/brandlogo-loader-join/RESULT.md`.

La dernière tranche statique ferme la fausse jonction par le callback de
démarrage : `0x820E8F90 → 0x820EA4A8 → 0x821728C0` ne fait que déléguer à la
tâche propriétaire et passer son état à 2. Elle ne construit ni ACC ni
record. Les consommateurs restent séparés (`CResourceLoaderSwg` vtable
`0x82012DC4/+0x14 → 0x8219E580` et `CSelectAircraftSetupManager`
`0x8200A254/+0x04 → 0x82127D40`). Aucun write qualifié vers `+0x18`, `+0x1c`
ou `+0x20` ne relie encore ces routes au payload `brandLogo`; aucun patch
runtime ou renderer n'est donc justifié. Preuve :
`artifacts/goal-playable/acc-guest-join-next/RESULT-followup.md`.

La passe du registre DPL ferme négativement la piste `0xCB..0xCF` : ces
valeurs deviennent les clés `DPL::[cb..cf,0]`, rejoignent seulement
`0x8219E768`, et ne sont reliées ni à `0x8219E428` ni à PAC/FHM/DATA.TBL/SWG.
Le producteur ACC et la jointure `brandLogo → ACC` restent donc le seul
blocage statique nommé. Preuves :
`artifacts/goal-playable/logo-key-registry/RESULT.md` et
`artifacts/goal-playable/swg-payload-static/RESULT.md`.

## Gate courant — owner/r8 et premier record guest (22 août 2026)

Le gate statique/runtime courant ne qualifie encore aucun payload visuel. Les
constructeurs initialisent `0x8200B6FC` à `base+0xC`, mais aucune publication
qualifiée de cet adjusted receiver vers `0x82165CC0` n'est connue. L'îlot
`0x82374AD0` est classifié comme initialiseur de la base `0x82731A30`, sans
publication AVI. Le writer type 1 est relié à la queue
(`0x821075A0 → 0x82117410 → 0x820FF788`) et exige
cinq ressources non nulles; le worker utilise le service global `0x82386C58`,
slot vtable `+0x08`, dont la valeur-flow reste inconnue.

La passe titre complémentaire a identifié les writers `0x820B3E80` et
`0x82321E18` pour des champs `+0xE8`, mais aucun join statique vers l'owner de
`0x820D29E0`, l'AVI ou les records n'est fermé.

La passe AVI finale confirme que `0x82166550` et `0x821674A8` sont les seuls
constructeurs qualifiés qui écrivent `0x8200B6FC` à `objet+0xC`; aucune
publication vers `0x82165CC0` n'est qualifiée. Une trace bornée (ticks 220:223,
plage stores
`0x2e000000..0x2f000000`, 300 ticks) termine avec 192 PRESENT mais zéro
soumission ring/PM4 et aucun owner ciblé. Elle confirme le blocage sans
autoriser de shim ou de changement renderer.

La passe provider sépare les contrats : `0x82165AF8` sélectionne dans
`DAT_826F6188`; `0x821080D0` convertit les cinq clés issues du payload en IDs
pour `DAT_826F6124`; `FUN_821ee130` alimente seulement `record+0x108`. Le
dernier join statique est la population des entrées de `DAT_826F6188` par
`0x82165040`/sa voie jumelle et la cible du service `0x82386C58`.

La trace bornée START (ticks 3000/3001, fenêtre 2990:3040) atteint
`0x820FEFA8` avec `record_type=0`, sans `0x8210A1C0`, `0x82117410` ou
`0x82165CC0`, et reste pré-frontend avec zéro paquet PM4. Aucun état Xenos,
shim ou record synthétique n'est autorisé. Le détail est dans
`artifacts/owner-r8-static/RESULT.md` et `BLOCKER.md`.

Validation native du checkpoint : codegen-ON CTest `26/26`; codegen-OFF
build + CTest `27/27` (refus CLI guest inclus); installation au préfixe
portfolio réussie avec `test ! -e bin/bin`. Le prochain gate doit qualifier
statiquement le service/vtable et les ressources avant toute extension du
renderer.

## Gate statique limité — producteur IB non observé par conception

La watch historique ne couvre que `0x1274A000..0x1274CF54`; elle exclut donc
les adresses heap du bootstrap. Aucun PC de producteur ne pouvait apparaître
dans les anciens runs. La prochaine observation est une watch native d'un
tick sur le seul IB `0x16AE0980..0x16AE0A40`.

## Gate limité — lot bootstrap de 24 draws

Le lot précoce de 24 `PointList` est précédé de `RB_COLOR_MASK = 0` et ne
livre ni payload vertex ni état RT0 qualifiant. Il ne peut pas être promu
comme writer visible, mais l'absence d'effet interne Xenos ne se déduit pas
de cette seule observation. Reprendre par le producteur de cet IB.

## Gate fermé — contrat RT0 vers lecture

Le draw normal qualifié produit le buffer RGBA consommé par la résolution
neutre, avec le draw de copie et le PRESENT déjà qualifiés. Le harness EDRAM
couvre des sources uniforme et structurée, et rejette une sortie altérée. Ce
contrat n'établit pas encore de pixel non noir ni de frontend.

## Gate fermé — sélection du draw RT0 qualifié

La construction Vulkan conserve désormais le rectangle normal et le rectangle
de copie après qualification exacte de leurs registres, avant le court-circuit
du cache de pipelines. La compilation et les tests configurés passent. La
frontière suivante est le contrat de lecture RT0, non le shader ou le rendu.

## Gate limité — owner transmis au slot virtuel

`0x820D29E0` est le slot `+0x0C` de la vtable `0x820064D8`. Ses constructeurs
écrivent cette vtable mais pas l'owner `+0xE8`; l'unique appelant direct du
constructeur est à nouveau une entrée de table. La reprise doit trouver le
site qui invoque ce slot et y remonter `r3` et le second argument.

## Gate limité — installation du contexte SWG

`0x82324188` installe conditionnellement `owner + 0xE8` dans
l'interpréteur puis appelle son slot `+4`; il n'initialise pas la vtable. Son
unique appelant construit l'interpréteur mais reçoit l'owner en argument. La
reprise statique remonte donc les appelants de `0x820D29E0` pour qualifier cet
owner avant de chercher le producteur de `+0xE8`.

## Gate fermé — handlers SWG post-START

Les indices 12, 19, 26 et 68 sont des thunks vers les slots `+0x80`, `+0xC8`,
`+0x64` et `+0xA8` de la vtable de `context + 0x0C`. L'indice 50 lit un mot
de flux puis appelle le slot `+0xCC`. Aucun handler ne touche directement le
sélecteur global. La prochaine frontière statique est l'installation de cette
vtable et ses cinq cibles concrètes.

## Gate fermé — consommateur de table SWG

`0x82325160` consomme les mots du flux à `context + 0x14` et appelle
`DAT_8264CEA8 + mot * 4`. Les cibles post-START sont les indices 12, 19, 26,
50 et 68. Son CFG n'accède ni au sélecteur global `0x823C27E0` ni au setter
`0x82171988`; le prochain slice statique est donc les handlers indexés.

## Gate limité — tables SWG post-START

Les neuf cibles relevées après START sont référencées par des tables de
données; six n'ont pas de fonction Ghidra. Les xrefs ne relient pas ces
entrées au producteur du sélecteur global et ne prouvent pas l'absence d'un
chemin indirect. La reprise statique doit reconstruire la table
`0x8264CED8..0x8264CFB8` et son consommateur.

## Gate fermé — divergence virtuelle SWG au START

La sonde bornée aux ticks 3000–3035 relève 22 appels sélectionnés au tick
3000, puis 69 au tick 3001. Au site `0x823251B8`, START introduit notamment
`0x82324548`, `0x82324440`, `0x823244B8`, `0x823248C8`, `0x82324C20`,
`0x820D7768`, `0x820DAFA8`, `0x820DD0B8` et `0x820DD158`. Le callback
`0x8218AB98` n'apparaît pas dans le périmètre des trois sites instrumentés;
cette absence n'est pas une conclusion globale. Reprendre statiquement par
ces cibles exclusives au tick 3001.

## Gate fermé — dispatcher native du titre borné

`0x820EA238` appelle le slot du contexte SWG; le chemin neutre atteint
`0x8217C890`, puis `0x8218AB98`. Aucun CFG ni xref direct de cette chaîne ne
mène au setter mission global `0x82171988`. Le choix START/neutre reste dans
le film SWG; la capture suivante est définie et ne doit relever que la cible
virtuelle, son contexte et son retour.

## Gate limité — objet titre distinct du sélecteur global

Le callback titre `0x8218AB98` stocke dans `this + 0x70` de son objet
`CModeTaskTitleDemoOffline`. Les producteurs de mission utilisent séparément
`PTR_DAT_823c27e0 + 0x70`. L'égalité de déplacement ne prouve aucune identité
d'objet; reprendre par le dispatcher titre et ses xrefs, sans runtime.

## Gate fermé — vocabulaire compact du loader

`0x82278F78` est qualifié statiquement comme décompresseur bitstream :
réservoir de bits, tables de nœuds, émission de littéraux et copies arrière.
Les mots hors `0..7` du flux sont des valeurs de sortie; leur source compacte
reste à relier au clip titre.

## Gate fermé — décodage borné du flux titre

La décompilation de `0x823246C0` et la reconstruction des octets déjà écrits
par `0x82278F78` expliquent les 23/23 appels box observés dans
`0x2DCB2438..0x2DCB2680` (opcode `0`, avec 13 opcode `7` intercalés). Le
premier mot hors du vocabulaire `0..7` est `0x2DCB2448 = 0x1A`; la suite doit
qualifier le format compact du loader, sans nouveau runtime par défaut.

## Gate fermé — chargeur de flux relié au contexte titre

Le chargeur `0x82278F78` écrit les 8 192 octets de `0x2DCB2000..0x2DCB3FFF`
au tick 2435. Au tick 3001, les PC du contexte titre sont dans cette même
plage. Le flux de titre est donc qualifié; reste à décoder localement la
fenêtre d'opcodes qui produit les requêtes mission/niveau.

## Gate fermé — frontière statique du branchement de film

Le flux de statements est un buffer heap interprété par `0x823246C0`, chargé
par décompactage depuis une entrée non reliée statiquement au contexte titre.
Les tables de natives ne révèlent pas ce flux. La suite requiert donc une
capture bornée chargeur → buffer → interpréteur titre, avec ses critères déjà
définis.

## Gate fermé — consommateurs statiques du triplet sélectionné

La table de commandes du titre lie `GetCurrentMission` et `GetCurrentLevel`
aux natives démo `0x820EA550` et `0x820EA598`. Leurs CFG appellent les getters
de mission/niveau qualifiés; le second remappe 6 et 7. Les lecteurs sont donc
attribués au script titre, mais pas encore au branchement de film post-START.

## Gate fermé — écrivain direct de mission au START écarté dans la fenêtre

Le slot mission réellement sélectionné est `0x823C2F14`, non l'ancienne cible
déréférencée. Avec START injecté au tick 3000, il reste inchangé aux ticks
3000–3004 et la capture de stores bornée sur ce slot ne relève aucune écriture.
Les neuf appelants directs du setter ne l'atteignent donc pas dans cette
fenêtre précise; cela n'établit ni les appels après le tick 3004 ni le chemin
de progression du film.

## Gate fermé — producteurs statiques de mission bornés

Neuf appelants directs du setter mission démo sont qualifiés. Ils couvrent des
valeurs littérales, calculées et issues d'une table UI, mais leur exécution au
START et l'écrivain de niveau ne sont pas prouvés. Une capture ciblée devra
relever uniquement le PC appelant et la valeur de mission écrite.

## Gate fermé — bornes START dans le XEX démo

Dans `ace-combat-6-demo`, les quatre observables START appartiennent aux
fonctions attendues : `0x82095B80` lit la mission et `0x82171988` écrit le
même champ sélectionné; `0x820E9290` lit le niveau voisin. Reprendre par le
producteur statique de l'index et des champs, sans nouveau runtime.

## Gate fermé — entrée du codegen démo qualifiée

`build-codegen-on` consomme `demo-game-file/extracted/stfs-root/Default.xex`
et son manifeste Ghidra démo dédié. Les observables du runtime codegen relèvent
donc de `ace-combat-6-demo`, pas du corpus retail `ace-combat-6`. La prochaine
preuve qualifie les adresses START dans ce projet démo.

## Gate fermé — réconciliation du corpus Ghidra canonique

Le corpus `ace-combat-6` / `default.xex` contient les fonctions qualifiées,
mais ses bornes démontrent que les adresses du code généré du demo ne portent
pas leur sémantique dans ce PAL. Ne pas transférer les noms ou CFG générés vers
le corpus canonique. La provenance de `gs+112` reste non établie pour le demo
tant que son entrée binaire n'est pas qualifiée statiquement.

## Gate fermé — cible du setter de mission au START réfutée

Dans la fenêtre START 3000–3004, l'état de jeu actif reste `0x82774B00`, mais
son champ `+112` vaut `0x8201DFDC`, table de méthodes et non sous-objet de
slots. Aucun store n'atteint l'ancienne adresse `0x82775234`; cette absence ne
qualifie donc pas `0x82171988`. Reprendre par la provenance statique du champ
`gs+112` et des getters réellement exécutés, sans nouveau runtime.

## Gate fermé — valeurs SWG après START

Les getters START renvoient `0 / 0 / 2`; monde et clip SWG ne changent pas et
`menu_endMode` ne réapparaît pas. Les substitutions ciblées déjà faites ne
modifient pas ce résultat. Le prochain causal est le producteur de cet état ou
le bytecode du clip, jamais un draw RT0.

## Gate fermé — sélecteur SWG post-START du titre

Le monde SWG est publié par l'unique écrivain `0x820CE368`; ses wrappers ne
portent aucun choix entre attract et START. Le choix est dans le bytecode du
film ou ses valeurs lues. La prochaine preuve est une capture bornée des trois
getters et du contexte, pas un trace renderer.

## Gate fermé — avancement neutre de l'attract title

Le callback SWG neutre atteint `0x8218AB98`, qui écrit l'état titre à `+0x70`;
l'état 2 mène ensuite à la transition de tâche. START ne produit pas de draw :
il choisit des handlers distincts et supprime ce callback. La prochaine
frontière est donc le sélecteur SWG, avant tout examen RT0.

## Gate fermé — consommateur statique du compteur de titre

`0x820D3AC8` n'a qu'une référence de données, sa case de vtable. Aucun appelant
direct ne permet de rattacher statiquement le compteur heap à une transition.
Le burst SWG observé ne devient pas persistant : il ne prouve aucun draw RT0
non nul. La recherche revient aux producteurs de contenu après progression
guest qualifiée.

## Gate fermé — récepteur virtuel du START pendant le titre

Le slot `+0x70` des deux objets de titre cible `0x820D3AC8`. Cette routine
appelle le slot `+0x5C` d'un contexte puis soustrait son résultat à un compteur
guest. Le chemin est une évaluation de VM de script; aucune transition menu ou
mission n'en découle encore de manière prouvée.

## Gate fermé — dispatch START pendant le titre

La boucle atteint le titre avant que START puisse être consommé. À cette phase,
`0x820D32D0` est un thunk PPC : vtable de `r3`, slot `+0x70`, puis `bctr`.
Ce n'est ni une fonction métier ni un producteur d'état. La prochaine frontière
est le récepteur dynamique de ce slot, avec une capture bornée de son objet et
de sa cible seulement.

## Gate fermé — producteur PM4 du draw RT0

`0x821B55C0` construit le `DRAW_INDX_2` RT0 atteint et `0x821B9BC8` publie le
ring via le WPTR Xenos. Ce CFG n'écrit pas EDRAM depuis le CPU : un contenu
RT0 non nul ne peut être produit qu'à l'exécution Xenos du draw. La prochaine
frontière est une progression guest qui atteint un contenu non nul.

## Gate fermé — reflection varying du draw normal

Le vertex shader atteint exporte position et `interpolator0`, mais ses trois
couleurs et l'entrée `color0` du pixel shader sont nulles. Le geometry shader
de test perd les interpolateurs hors position, sans pouvoir expliquer la
noirceur de ce draw précis. Le prochain causal est le premier écrivain non nul
de RT0/EDRAM, sans runtime.

## Gate fermé — interface vertex du draw normal

Le record `XenosDrawCommand` ne retient aucun descripteur fetch ni octet guest,
mais le backend lie séparément le descripteur shared-memory qui couvre la plage
du rectangle normal. Le `vkCmdDraw` de trois sommets ne réfute donc pas une
entrée vertex au shader traduit. La frontière est désormais limitée à
l'interprétation shader/format/lane, sans pixel ni frontend promu.

## Gate fermé — disposition d'installation native

L'installation temporaire du build courant fournit le binaire sous `bin/` et
sa configuration sous `share/`; l'invariant d'absence de `bin/bin` passe.
Cette preuve est limitée à la disposition de l'artefact installé.

## Gate fermé — intégration locale du bridge natif

Le build courant de `recompilation/ace-combat-6-demo` compile et ses 27 CTest
passent avec l'audio dummy. Cette validation couvre l'intégration locale du
bridge, sans promouvoir de frontend, mission ou readback.

## Gate en pause — producteur de `VdGlobalDevice+0x4084`

`0x821C64E8` initialise le device, enregistre `0x821B9710` via
`VdSetGraphicsInterruptCallback`, puis publie ce device dans
`VdGlobalDevice`. L'écrivain du champ interne `+0x4084`, lu ensuite par
`0x821C5190`, appartient à l'interface Vd externe et reste non qualifié.
Après cinq slices statiques, aucun runtime.

## Gate fermé — interface du callback indirect graphique

`0x821C5190` appelle conditionnellement `VdGlobalDevice+0x4084` sous le
spinlock `+0x4130`, avec un record local de six mots provenant des compteurs
du même objet. La cible concrète reste inconnue, mais l'interface est bornée
statiquement ; aucun effet PM4, pixel ou frontend n'est attribué.

## Gate fermé — contrat CPU du callback XAudio

Le contrat local de sélection CPU du callback XAudio compile dans le bridge
actuel et son test ciblé passe. Cette vérification ne promeut aucune exécution
audio, frontend ou mission ; le choix CPU explicite reste borné par les
preuves déjà consignées.

## Gate en pause — écrivain non nul du record render

`0x820FF710` initialise explicitement le champ de contrôle du slot puis avance
le producteur ; le worker recopie le record de 96 octets. La recherche des
appels directs vers ce producteur ne donne aucun appel `bl` : son entrée reste
indirecte ou atteinte par saut. Après cinq observations statiques, aucun
écrivain non nul ni consommateur de payload n'est qualifié. Aucun runtime.

## Gate fermé — faux lecteur de payload render `0x820FEFA8`

Le worker `0x820FFCA0` copie le slot de 96 octets dans sa pile puis le passe à
`0x820FEFA8`. Ce callee remplace immédiatement `r3` par le retour de
`0x82327104`, sans préserver l'argument du slot ; il ne peut donc pas lire ce
payload. La suite est le premier écrivain statique non nul du record, sans
runtime.

## Branche en pause — lecteur fournisseur `+0x20`

Les écritures `0x822FFCC8`/`0x822FFDB8` vers `fournisseur+0x20` sont établies,
mais leur lecteur n'est pas identifiable par le scan D-form seul : 1 066
candidats `r3`/`r4` restent sans provenance d'objet. Le runtime n'est pas
autorisé pour combler cette frontière statique.

## Gate fermé — corps des slots `+0` de fournisseurs non-bootstrap

- Les trois entrées sont des feuilles PPC sans frontière Ghidra reconnue et sans appel direct : `0x822F2F20` retourne immédiatement `0x80004001` (`E_NOTIMPL` dans le header SDK local), `0x822FFCC8` copie 16 octets de `r4` vers `r3+0x20` puis retourne zéro, et `0x822FFDB8` copie 16 mots (64 octets) au même offset puis retourne zéro.
- Avant leurs `blr`, aucun de ces corps ne fait de call/dispatch, n'accède à un ring, ni n'écrit de MMIO ou d'état GPU global. Ils réfutent donc la liaison ou soumission shader immédiate des fournisseurs du profil.
- Le seul effet non nul est l'état local `fournisseur+0x20`; le prochain gate doit trouver son premier consommateur statique. Aucun runtime n'est justifié.

## Gate fermé — vtables des fournisseurs non-bootstrap

- Dans le projet Ghidra canonique `ace-combat-6-demo`, module `Default.xex`, le balayage RTTI qualifie sept vtables `Shader::Parameter*` : `0x8202ADF8`, `0x8202AE14`, `0x8202AE30`, `0x8202AE4C`, `0x8202AE68`, `0x8202AE84` et `0x8202AEA0`.
- Leurs slots `+0` ne sont pas homogènes : ils ciblent exactement `0x822F2F20`, `0x822FFCC8` ou `0x822FFDB8`. Le dispatch `0x822E0B90` doit donc conserver cette indirection à trois implémentations.
- La zone d'initialisation `0x822FB2B8–0x822FB610` installe ces vtables, mais aucune frontière de fonction n'y est reconnue : aucun pseudocode n'est retenu comme preuve. Le prochain gate qualifie seulement les trois corps de slot `+0`; aucun runtime n'est justifié.

## Gate fermé — effet de fournisseur `0x822E0B90`

- Le CFG PPC de `0x822E0B90` charge `*(u32 *)r3`, puis `*(u32 *)vtable`, place ce mot dans `CTR` et exécute `bctr`.
- Cette routine est une dispatch virtuelle de slot `+0` en queue. Elle n’écrit ni buffer local, ni état GPU, ni PM4.
- Les trois fournisseurs du profil atteignent donc une interface exacte dont l’implémentation de slot `+0` porte tout effet ultérieur. Le prochain gate doit qualifier cette vtable; aucun runtime n’est justifié.

## Gate fermé — lecteurs du profil global non-bootstrap `0x829D0800`

- Le seul lecteur distinct de l’initialiseur est la trampoline `0x822DEBD8`, qui passe le global `0x829D0800` à `0x822E9BD8`.
- Le prologue de `0x822E9BD8` conserve ce premier argument dans `r29`. Sa branche principale charge `r29+0xc`, `r29+8` puis `r29+0x10` aux sites `0x822E9D80/90/9C`, et appelle `0x822E0B90` pour chacun.
- Le CFG prépare ensuite un contexte et appelle `0x822F5608`, sans écrire de PM4 lui-même. La prochaine frontière est le callee commun `0x822E0B90`; aucun runtime n’est justifié.

## Gate fermé — provenance statique de l’enregistrement de profil non-bootstrap

- Dans le projet Ghidra canonique `ace-combat-6-demo`, module `Default.xex`, `0x822E9968–0x822E996C` matérialise `r3 = 0x829D0800`, puis `0x822E9974` pose le drapeau à `+0` et `0x822E9978` appelle `0x822E9A18`.
- `0x822E9A18` copie ce `r3` dans `r31` et y conserve les fournisseurs. `0x8232710C` est seulement l’aide de prologue qui préserve les registres non volatils.
- Le profil non-bootstrap a donc un stockage global exact `0x829D0800`. Aucun consommateur shader/PM4 n’est encore qualifié; le prochain gate doit partir exclusivement de ses lecteurs.

## Gate fermé — consommateur du profil de fournisseurs non-bootstrap

- Dans le projet Ghidra canonique `ace-combat-6-demo`, module `Default.xex`, `0x822E9A18` écrit les trois fournisseurs résolus dans un enregistrement retourné via `0x8232710C`.
- L’accesseur est un stub vide et ses appelants sont transversaux, y compris de nombreux sites du cluster `0x822E…`. Il ne fournit donc ni identité statique de cet enregistrement, ni xref sélective vers un consommateur.
- Aucun CFG qualifié ne relie les trois champs à un shader, microcode ou paquet PM4. La frontière reste opaque à cette indirection; aucun changement natif ni runtime n’est justifié.

## Gate fermé — implémentation de la slot de résolution `+0x2c`

- Le constructeur `0x822F8E68` installe la vtable `0x8202AAB4` de `Shader::ShaderContextXenon`. Son mot `+0x2c` cible `0x822F2B88`.
- `0x822F2B88` parcourt deux listes de fournisseurs puis appelle `0x822F89D0`; ce dernier applique un prédicat sur l’identifiant demandé et, en cas de succès, renvoie le pointeur du fournisseur. La slot résout donc une ressource, sans charger de microcode ni écrire de PM4.
- Dans l’initialiseur `0x822E9A18`, les trois valeurs retournées sont conservées dans l’enregistrement de profil. Le prochain lien utile est leur premier consommateur; aucun changement natif ni runtime n’est justifié.

## Gate fermé — sélection du pixel shader non-bootstrap

- Dans le projet Ghidra canonique `ace-combat-6-demo`, module `Default.xex`, le CFG de `0x822E3858` appelle le bootstrap puis `0x822E9948`; ce dernier pose cinq drapeaux globaux et appelle cinq initialiseurs. Aucun de ces deux CFG ne construit une clé de technique, ne charge un shader pixel ou n’émet un `IM_LOAD`.
- Le résolveur partagé `0x822E0B10` obtient un objet depuis le gestionnaire global puis délègue la résolution à la slot virtuelle `+0x2c`. Cette dispatch est la frontière exacte entre les profils initialisés et un éventuel shader consommé.
- La question est donc bornée statiquement, sans lien établi vers un pixel shader non-bootstrap. Aucun changement natif ni runtime n’est justifié.

## Gate fermé — origine EDRAM / readback du draw normal démo

- La chaîne locale est fermée : `execute_vulkan_neutral_resolve` passe les octets `normal.resolved_rgba8` au resolve, puis son certificat différentiel refuse tout writeback dont les données tuilées ne restituent pas exactement cette entrée.
- La matérialisation EDRAM n’est pas la divergence : son oracle isolé couvre un motif uniforme non nul, son placement EDRAM et la préservation du canari. La copie et la conversion qui suivent ne peuvent donc pas transformer silencieusement une source nulle en image exploitable.
- Pour le draw normal démo atteint, le contrat qualifié indique une entrée de readback entièrement nulle. La noirceur est antérieure à EDRAM, à la copie et au writeback; aucun changement natif ni runtime complet n’est justifié.

## Gate fermé — enveloppe de soumission pilote `0x821BFBA8`

- Le buffer passé à `NtWriteFile` est celui du canal sélectionné; sa longueur est calculée en unités de `0x200` octets. Le CFG ajoute un en-tête de `0x10`, copie conditionnellement `0x38` octets depuis `device+0x3584`, puis ajoute conditionnellement `0x600` octets avant de publier la longueur.
- `0x821A66E0` transmet le pointeur et cette longueur à `NtWriteFile` sans décoder le buffer. `0x821BA880` ne fournit pas de layout supplémentaire : il sauvegarde un curseur et déclenche le drain.
- Aucun champ observé ne relie cette enveloppe à un WPTR ou à un descripteur PM4. Le format interne est donc opaque derrière le pilote; aucun changement natif, test synthétique ou runtime n'est justifié.

## Gate fermé — contrat natif de soumission de `0x821BFBA8`

- Le rapprochement réfute l'isomorphisme supposé : le CFG démo de `0x821BFBA8` assemble un buffer de canal puis appelle `0x821A66E0` / `NtWriteFile`; il ne publie pas directement un WPTR MMIO.
- Le point partagé natif est séparé : le store guest à `0x7FC80714` est routé dans `guest_bridge.cpp` vers `apply_xenos_mmio_write`, qui consomme le ring et met à jour le read pointer seulement après consommation.
- Les tests ring couvrent la progression, le wrap et le rejet avant writeback. Aucun écart dans le publisher Xenos n'est donc établi et aucun changement de code n'est justifié. Le format de l'enveloppe pilote demeure une frontière statique distincte.

## Gate fermé — premier producteur PM4 non-bootstrap

- La prémisse est réfutée : les cinq initialiseurs appelés par `0x822E9948`, dont `0x822E9A18` et `0x822EA010`, créent un objet via `0x822E6A38`, le configurent par dispatch virtuel, puis résolvent trois handles par `0x822E0B10`.
- Ils n’écrivent aucun buffer PM4, ring ou MMIO. `0x822E0B10` lui-même délègue la résolution à la slot virtuelle `+0x2c`; ce chemin est une frontière de ressources, non une soumission Xenos.
- `FUN_8232710c` est un stub sans corps dans cette analyse et ne permet pas de typer le receveur des stores relatifs. La première jointure ressource → PM4 doit donc être cherchée chez les consommateurs des handles, sans assimiler ces initialiseurs au producteur PM4.

## Gate fermé — effets de rendu bootstrap et transition non-bootstrap

- Les `IM_LOAD` du template entrent dans `XenosCommandProcessor`, créent deux `XenosShaderLoadCommand`, puis alimentent l’état vertex/pixel recopié dans chaque `XenosDrawCommand`; `GuestBridge::apply_xenos_typed_batch` place ces commandes dans la boîte renderer.
- Le CFG démo de `0x822E3858` appelle sans condition `0x822F0340` (bootstrap) puis `0x822E9948` (cinq initialiseurs de profils NSXR non-bootstrap). Cette séquence initialise des globaux distincts, sans sélectionner un shader ni émettre un paquet PM4.
- La première émission PM4 qui consomme un profil non-bootstrap n’est pas jointe par ce CFG ; elle reste une frontière statique séparée. Aucun runtime ne serait discriminant avant l’identification de ce producteur.

## Gate fermé — format PM4 du buffer indirect de `0x821B1D58`

- Le template compte 74 DWORDs (indices 0–73) : `INVALIDATE_STATE` (`0x3B`, un mot), deux `IM_LOAD` (`0x2B`, 26 puis 11 mots), puis des Type 0 vers des registres jusqu’à `0x2312`.
- Les deux `IM_LOAD` ont les formes natives exactes : stage sommet, taille 24 ; stage pixel, taille 9. L’invalidation exige aussi exactement un mot.
- Les Type 0 restent sous la capacité native de `0x8000` registres, et leurs indices ne franchissent aucune frontière opaque avec une valeur non qualifiée. Le scanner du ring conserve toutes ces formes sémantiques.
- Aucun écart statique n’est établi : ni test isolé ni runtime ne sont requis.

## Gate fermé — opérandes indirectes de `0x821B2BC8`

- `0x821B2BC8` écrit `0xC0013F00`, puis `device+0x35c4` comme adresse et `device+0x35c0 & 0x00ffffff` comme nombre de DWORDs.
- `0x821B20A0` alloue le buffer de 0x2000 octets via `MmAllocatePhysicalMemoryEx`; son implémentation locale aligne l’adresse au moins à la page. L’encodage de `+0x35c4` préserve donc les deux bits bas nuls et le `& ~3` de `capture_indirect` est une identité.
- `0x821B1D58` produit exactement 73 DWORDs (dernier store à l’index 72) et `0x821B20A0` les place dans `+0x35c0`. Le masque natif `& 0xFFFFF` préserve donc aussi le compte et l’unité est le DWORD des deux côtés.
- Aucun écart statique n’est établi : ni test isolé ni runtime ne sont requis.

## Gate fermé — paquets écrits après le drain `0x821BA780`

- Après un drain, `0x821B0D20` écrit les quatre Type 3 à un mot `0x60`, `0x62`, `0x61` et `0x63`, que le processeur natif qualifie comme sélecteurs de bin.
- `0x821B2BC8` écrit un Type 3 `0xC0013F00` à deux opérandes ; le bridge le reconnaît comme descripteur indirect et développe son contenu avant de l’envoyer au processeur de commandes.
- Les en-têtes et longueurs coïncident avec les deux décodeurs natifs. Aucun écart statique, test synthétique ou runtime n’est requis.

## Gate fermé — consommateur de la publication de `0x821BA780`

- Les appelants démo `0x821B0D20`, `0x821B2BC8`, `0x821C57D0` et `0x821C64E8` appellent `0x821BA780` quand leur curseur de commandes dépasse la limite courante, puis continuent à écrire le lot suivant.
- Le flux déjà qualifié de `0x821BA780` vers `0x821BA058`, puis `0x821B9BC8`, relie donc ce seuil au publisher WPTR du ring système.
- `0x821C64E8` appelle aussi `0x821BAAD0` sur son chemin d’initialisation, mais cette co-occurrence ne remplace pas la jointure de drain ci-dessus et ne nécessite aucun runtime.

## Gate fermé — sémantique native du segment de plage optionnel

- Les deux segments optionnels émis par `0x821B9DB0` ont la même forme : Type 0 vers `0xA31`, Type 0 de deux mots vers `0xA2F–0xA30`, puis Type 3 `WAIT_REG_MEM` (`0x3C`) à cinq mots.
- Le décodeur natif accepte les Type 0 dans les bornes du fichier de registres; `0xA2F`, `0xA30` et `0xA31` ne font pas partie de sa frontière opaque. Il traite aussi exactement le `WAIT_REG_MEM` cohérent de cinq mots.
- Il n’existe donc aucun écart statique entre ce contenu et le décodeur : aucun changement ni test synthétique n’est justifié, et aucun runtime n’a été utilisé.

## Gate fermé — entrées de contenu de `0x821B96B8`

- `0x821B96B8` échange atomiquement la paire 64-bit `device+0x2e28` contre le sentinelle et retourne ses moitiés basse et haute.
- `0x821C6928` initialise cette paire; `0x821B9648` la met à jour atomiquement depuis les chemins `0x821B8090` et `0x821B86F8`.
- `0x821B9DB0` consomme les deux moitiés pour émettre son segment optionnel de plage; elles ne changent ni le contrat de descripteur ni le publisher WPTR.

## Gate fermé — producteur de contenu de `0x821B9DB0`

- `0x821B9DB0` alloue lui-même le contenu par `0x821B9AE0`, l’écrit localement, puis renvoie son adresse matérialisée dans la première sortie.
- Sa seconde sortie est une longueur de `11` ou `22` dwords, déterminée par deux états privés du device.
- `0x821BA780` transmet ensuite exactement cette adresse et cette longueur à `0x821BA058`; la valeur optionnelle insérée dans le contenu provient de `0x821B96B8`.

## Gate fermé — source des descripteurs indirects du publisher

- Dans le CFG démo, `0x821BA780` obtient la longueur et l’adresse du contenu auprès de `0x821B9DB0`, réserve l’en-tête par `0x821B9AE0`, puis appelle `0x821BA058`.
- `0x821BA058` forme un descripteur local d’un élément et le transmet à `0x821B9BC8`; le publisher reste ainsi séparé de la production du contenu.
- `0x821B2BC8` peut appeler `0x821BA780` lorsque la capacité courante l’exige, mais cette relation ne prouve pas que le template qu’il écrit ensuite est le descripteur alors drainé.

## Gate fermé — contrat natif du publisher WPTR

- Le bridge valide l’écriture `CP_RB_WPTR`, parcourt le ring de façon circulaire, puis décode le triplet Type-3 `0xC0013F00` démontré pour `0x821B9BC8` avec adresse alignée et compte borné.
- Après le décodage du batch, il publie le WPTR et le pointeur de lecture local au même point partagé ; ce contrat correspond à la publication guest après écriture du triplet.
- La construction locale et les suites `ac6-demo-core-tests` et `ac6-demo-xenos-command-tests` passent. L’inclusion du traceur graphique est maintenant commune aux variantes générée et non générée, sans changement de la sémantique du ring.

## Gate fermé — publication du ring système

- Dans le projet Ghidra démo `ace-combat-6-demo` / module `Default.xex`, `0x821BAAD0` appelle `MmGetPhysicalAddress` puis `VdInitializeRingBuffer`, et initialise base, masque et producteur du ring aux offsets `+0x3a18`, `+0x3a1c` et `+0x2ac8`.
- `0x821B9BC8` relit ces trois champs, insère chaque triplet `0xC0013F00`, adresse, longueur dans le ring avec retour circulaire, puis écrit l’index final à `0x7FC80714` avant les barrières `sync/eieio/sync`.
- La jointure ring initialisé → writer WPTR est donc statique et directe. Aucun runtime n’a été utilisé.

## Gate fermé — contrat d’initialisation du ring natif

- Le wrapper `VdInitializeRingBuffer` reçoit la base physique et le logarithme calculé par `0x821BAAD0`; `configure_xenos_ring` conserve ces valeurs.
- `xenos_ring_snapshot` calcule `1 << (size_log2 + 1)` DWORDs. Cette capacité donne exactement le masque guest `(taille_en_octets >> 2) - 1` initialisé à `+0x3a1c`.
- `VdEnableRingBufferRPtrWriteBack` transmet l’adresse de writeback au bridge; la complétion publie le RPTR final à cette adresse. Les tests core couvrent initialisation, passage circulaire et writeback.

## Gate fermé — writer WPTR du ring système

- `0x821BAAD0` initialise le ring système : base `device+0x3a18`, masque
  `device+0x3a1c` et producteur `device+0x2ac8`, puis appelle
  `VdInitializeRingBuffer`.
- `0x821B9BC8` encode des triplets dans ce ring, met à jour
  `device+0x2ac8`, puis publie cet index à `0x7FC80714` entre
  `sync`, `eieio`, `sync`.
- La jointure est par les mêmes champs du device ; aucun appel direct entre
  l'initialiseur et le publisher n'est requis ni observé.

## Gate fermé — enveloppe pilote de `0x821BFBA8`

- Dans le binaire démo, `0x821BFBA8` passe par `0x821BA368` puis
  `0x821BA130` avec `VdGlobalDevice` et le mode `6` avant `NtWriteFile`.
  Cette chaîne réserve/synchronise le buffer global ; elle n'encode pas un
  payload PM4 documenté.
- Au point d'appel, handle et buffer sont lus par index dynamique dans le
  sous-objet de canal, et la longueur est dérivée/arrondie. Les offsets de ce
  canal ne sont pas couverts par le layout public `D3DDevice` localement
  disponible.
- L'enveloppe `NtWriteFile` est donc une interface pilote opaque, distincte
  du ring PM4. Aucun changement du bridge MMIO natif n'est justifié.

## Objectif

Faire fonctionner `recompilation/ace-combat-6-demo` sur l’unique
`Default.xex` démo PAL qualifié, du cold boot au menu visible, puis à une
mission jouable avec succès et échec endogènes, replays déterministes,
capsules et readbacks guest non noirs.

Identité canonique : Xbox 360 Xenon/Xenos, SHA-256 XEX
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`,
projet Ghidra `ace-combat-6-demo`.

## Faits établis

- Le cold path natif atteint 3 036 ticks avec START au tick 3 000 et relâche
  au tick 3 001, sans trap : 23 threads guest et 2 899 appels `VdSwap`.
- `VdSwap` publie un frontbuffer guest `0x1374A000`, format 6, `1280×720`.
- Le second ring primaire est `0x126CA000`, capacité 131 072 dwords. Dans le
  run final il reste à 0 soumission, RPTR 0, WPTR 0.
- Le batch primaire de boot historique de 25 dwords est intégralement
  consommé par l'état courant : deux soumissions de 22 puis 3 dwords, sans
  attente pendante. L'ancien constat `RPTR=7/WPTR=25` ne décrit donc pas le
  processeur de commandes actuel.
- Le debugger du runtime généré observe un label nommé `sub_821B9BC8` avec
  `device+10941=0x16` et `device+21508=0` après l'init finale. Ce reçu est
  limité au binaire généré : il n'est pas encore une preuve PAL.
- Dans Ghidra PAL canonique, `0x821B9BC8`, `0x821BA058` et `0x821B9D24`
  appartiennent à l'unique fonction/PData `0x821B99B8–0x821BA273`, et
  `0x821B9D24` est un `bctrl`, pas le store MMIO prétendu par le généré.
  L'identité du XEX générateur doit être réconciliée avant toute autre
  interprétation de ses labels ou de son contrôle.
- La provenance est maintenant résolue comme divergente : le codegen lié est
  qualifié pour `ace-combat-6-demo` / `Default.xex`, alors que le projet
  Ghidra canonique actif est `ace-combat-6` / `default.xex`. Les deux chaînes
  ne peuvent pas être croisées à adresse égale.
- Pour la démo, le draw normal est bien rasterisé avec couverture complète,
  puis résolu et écrit en mémoire guest, mais ses pixels sont tous noirs. Le
  pixel shader exact du draw est un bootstrap : il n'a ni entrée couleur ni
  fetch de texture et écrit RGBA zéro. Le problème n'est donc ni le copy, ni
  le tiling, ni l'absence de rasterisation.
- Le source Xenia Edge `e4b13738c3c461b2c06241fa3f54b5a669b6a304`
  transmet à `EnableReadPointerWriteBack` uniquement l’adresse fournie. Le
  runtime natif fait désormais de même.
- Les packets PM4 atteints `0x61`, `0x62`, `0x63` et les valeurs dynamiques
  qualifiées de `EVENT_WRITE_SHD` sont acceptés avec tests bornés.
- Build codegen-on, CTest `26/26`, installation et absence de `bin/bin` sont
  validés. Détails et hashes :
  `analysis/demo/ac6-demo-ring-readback-frontier-v1.json`.

## Résultats négatifs importants

- Vulkan valide 2 modules, mais crée 0 pipeline raster, 0 normal draw,
  0 writeback guest et aucun screenshot d’audit.
- Aucun packet Xenos `PRESENT`, readback guest non noir, frontend qualifié,
  mission ou terminal n’est atteint.
- Les 2 899 notifications `VdSwap` ne constituent pas une preuve visuelle.
- Écrire le timebase hôte dans l’inconnu `KTHREAD+0x58` est inutile : retiré.
  Les digests headless/Vulkan finaux restent identiques sans cette écriture.
- Le faux writeback hôte à `adresse_RPTR-0x3c` touchait un champ guest distinct
  et n’est pas conforme au source Edge : retiré.

## Hypothèses ouvertes

- La chaîne démo qualifiée gouverne le runtime ; le projet Ghidra canonique
  reste une chaîne statique distincte et ne peut pas lui être joint par
  adresse égale.
- La condition qui fait quitter le batch bootstrap noir pour atteindre un
  pixel shader non bootstrap reste inconnue. Dans le projet Ghidra démo,
  `0x822E9948` initialise sans condition cinq profils NSXR non-bootstrap ; le
  bootstrap est initialisé par `0x822F0340 -> 0x822F8078`. Aucune jointure
  statique qualifiée ne relie encore ces profils à une émission PM4 ou au draw
  observé.
- Le prévol GDB NSXR n'a pas lancé le programme : son script appelait
  `continue` sans `run`. Le symbole `__imp__sub_822E0B10` est néanmoins résolu
  par GDB ; cette erreur hôte ne porte aucune information guest.
- La fenêtre GDB corrigée reste bloquée avant le guest si elle emploie un store
  temporaire vierge : le montage VFS exige le marqueur `.ac6-demo-store` et
  l'ensemble des neuf fichiers qualifiés. Le chemin d'import natif est le seul
  moyen retenu pour dériver un store isolé sans modifier la référence.
- L'import natif a publié un store démo isolé sous
  `/fastdata/lavaulta/tmp/ac6-nsxr-import.inm9D9/store`. Son inventaire contient
  uniquement le marqueur et les neuf fichiers qualifiés ; il peut servir à la
  fenêtre GDB bornée.
- La fenêtre GDB NSXR a observé au tick 0 les 12 lookups attendus : bootstrap
  `-14`, puis les profils `-13` à `-9`, avec les LR des wrappers démo. Elle
  prouve l'initialisation complète, non une consommation de profil par PM4.
- `0x8232710c` est un thunk sans effet et ne désigne pas un registre de profils.
  L'initialiseur global `0x822E3858` a un seul appelant statique démo connu,
  `0x822DA7F0`, qui devient le pivot de la prochaine analyse de consommateur.
- Le pivot `0x822DA7F0` ne consomme pas un profil pour PM4 : il orchestre des
  constructeurs, puis appelle `0x822E3858`. Son seul appelant direct connu est
  l'orchestrateur global `0x821A3C30`.
- L'atlas des appels directs de `0x821A3C30` contient `0x822DA7F0`, mais aucun
  des producteurs Xenos déjà qualifiés. Cet orchestrateur ne joint donc pas
  directement l'initialisation NSXR au PM4.
- `0x821B1D58` construit un template PM4 dans son second argument et y copie
  le microcode pixel bootstrap à l'offset `0x80`. Cette fonction ne soumet pas
  elle-même ce template ; son appelant direct connu est `0x821B20A0`.
- L'atlas inverse relie `0x821B20A0` à `0x821C64E8`, qui initialise le device
  global et appelle ce constructeur après ses prérequis. Cette jointure reste
  une initialisation : aucune soumission du template n'y est démontrée.
- `0x821B20A0` réserve un bloc `0x2000`, construit le template bootstrap à son
  début, puis publie adresse et longueur dans le device aux offsets `+0x35bc`,
  `+0x35c0` et `+0x35c4`. Les autres templates partagent ce bloc ; aucune
  soumission n'est présente dans ce CFG.
- `0x821B2BC8` consomme `+0x35c4` et `+0x35c0`, les émet dans le buffer de
  commandes courant, puis appelle `0x821B1F28`. Le passage template→buffer est
  établi ; le commit final de ce buffer reste à qualifier.
- `0x821B1F28` et son enfant `0x821AE1E8` ajoutent uniquement des paquets
  d'état au même curseur `device+0x30` (avec extension par `0x821BA780`). Ils
  ne publient pas ce curseur vers le ring.
- Le chemin bootstrap est appelé par `0x821C5DB8` pendant l'initialisation du
  device, avec les paramètres `(0,0,0)`. L'autre appelant direct,
  `0x821BFBA8`, est distinct et constitue le prochain pivot statique runtime.
- `0x821BFBA8` construit un batch puis appelle `0x821BF720`; celui-ci avance
  un index et un offset de canal (`+0x254`, `+0x17c`) et pose le bit
  `+0x258:0x08` au retour au canal zéro. Il ne publie pas directement le ring.
- L’ordre asynchrone du CP dans Edge peut expliquer une partie de la chaîne
  d’attente D3D, mais son impact dans ce runtime reste spéculatif.
- Les packets construits par `VdSwap` existent dans des buffers guest ; la
  condition qui doit les joindre au ring final reste inconnue.

## Décisions

- Utiliser le code source Xenia Edge comme référence statique ; ne pas lancer
  son runtime et ne pas utiliser Wine.
- Ne faire aucune optimisation avant affichage natif du début de mission.
- Ne pas lancer d’A/B par défaut ; seulement pour une ambiguïté causale nommée.
- Ne jamais promouvoir un visuel sans pipeline/readback guest et RGB non nul.
- Conserver les frontières inconnues fail-closed et ne jamais modifier le C++
  généré.
- Aucun CPJ, worker automatique ou sous-agent.

## État Git exact du checkpoint

- Branche : `main`, upstream `origin/main`.
- Base fonctionnelle avant les documents de checkpoint :
  `aa9b05347ad0c26dba16e1ca77bef902a664d47d`, alors synchronisée `+0/-0`.
- Les documents sont publiés par le commit contenant ce fichier ; le retrouver
  par `git log -1 --format=%H -- STATE.md`.
- Modifications préexistantes non incluses dans le checkpoint :
  `recompilation/ace-combat-6-demo/{CMakeLists.txt,src/guest_bridge.cpp,src/guest_bridge/dynamic_object_vtable_trace.hpp,src/guest_bridge/graphics_dispatch.hpp,src/guest_bridge/graphics_interrupt_trace.hpp,src/guest_bridge/import_journal.hpp,src/guest_bridge/lifecycle.hpp,src/guest_bridge/transition_memory_trace.hpp,src/guest_bridge_resources.cpp,tests/test_xam_return_chain.py,tools/map_xam_return_chain.py}` et
  `reports/AC6_DEMO_XAUDIO_CALLBACK_CPU_FRONTIER.md`.
- Artefacts préexistants non suivis :
  `analysis/demo/ac6-demo-{graphics-interrupt-gate-v1,ring-writeback-boundary-v1,title-doorbell-start-v1,title-imports-queue-ab-v1,title-matrix-consumer-ab-v1,title-queue-ab-v1,title-selector-ab-v1,title-task-list-ab-v1,title-xma-kick-v1,vdswap-corridor-v2,vulkan-xenos-frontier-v1,xaudio-cpu-ab-v1}.json`,
  `analysis/demo/ac6-demo-post-resume-ab/sha256/940637146a447e48fc1619471b9910278c962ca0b261017a269c3cc4affca0c8/`,
  `analysis/demo/ac6-demo-xam-return-chain-ab/sha256/{bcf3382c64ec0d415110ffdbc309161df26fe226ce84fc88e03ccc90ab19bee4,c939f016eec34118ffd46df7a4df81e1b916bd0353a26f788705e5ab1dce2e14}/`
  et `recompilation/ace-combat-6-demo/install/`.
## Checkpoint statique — drain de batch

`0x821BF720` travaille sur le sous-objet `device+0x5494` : son bit `+0x258:0x08` est donc `device+0x56ec:0x08`. Son premier consommateur qualifié, `0x821BF7D8`, lit ce bit pour sélectionner la borne de drain mémorisée, sans l'effacer ni publier directement le ring. Les appels suivants `0x821A6530` et `0x82337E38` restent la frontière de publication.
## Checkpoint statique — frontière NtWriteFile

Le drain `0x821BF7D8` atteint `0x821A66E0`, wrapper synchrone de `NtWriteFile`. Il lui passe le handle du sous-objet, un descripteur local et une longueur `0x800`; aucune écriture MMIO n'est visible dans ce wrapper. L'identité et le producteur du handle restent à qualifier avant de conclure à une soumission graphique.

`0x821A6530` ne fait que positionner ce handle par dispatch virtuel. `0x82337E38`
appelle `NtQueryInformationFile` puis deux fois `NtSetInformationFile`; aucun
des deux appels n’est une publication de batch. La frontière est donc bien le
wrapper `NtWriteFile` et la provenance de son handle.

Le wrapper obtient ce handle par `FUN_8232710C`, qui est un stub sans corps
dans le projet démo. Cette provenance est donc une frontière indirecte ; la
suite statique utile est le producteur du buffer de `0x800` octets.

Le buffer est alloué dans la pile de `0x821BF7D8`. Seuls ses 56 premiers
octets sont remplis explicitement par la copie `FUN_82327D90` depuis
`device+0x3584`; aucun accès PPC D-form direct à cet offset n’existe. Son
producteur devra être recherché par les adresses composées, sans l’identifier
comme PM4.

La recherche par constante relie `device+0x3584` à `0x821BFBA8` et
`0x821C57D0`. Le premier le copie dans un batch ring ; le second le transmet
à deux chemins qui construisent des commandes/scaler sans le remplir. Le
producteur reste indécidable dans cette fenêtre statique.
## Checkpoint statique — producteur du handle

`0x821BEFF0` construit les deux canaux à `device+0x5494`; `0x821BEE60` leur alloue de la mémoire physique mais ne renseigne pas leur champ `+0x14`. Les accès D-form voisins ne montrent aucune écriture directe à ce champ. Son producteur reste une route d'alias ou d'initialisation indirecte, sans preuve de device Xenos.
## Checkpoint statique — alias du champ de handle

Les scripts existants ne relient aucun store au champ `device+0x54a8` : ni le scan D-form du sous-objet, ni la recherche des accès indexés avec l'offset complet ne fournissent de producteur. La provenance du handle est indécidable avec ce projet Ghidra et cet outillage; aucune conséquence de publication GPU n'en est tirée.
## Checkpoint statique — chaîne PM4 vers I/O

`0x821BFBA8` est la chaîne qualifiée de publication : il appelle `0x821B2BC8` pour émettre le template PM4, construit le buffer de canal, puis le transmet via `0x821A66E0` à `NtWriteFile`. Les appelants `0x821A1170` et `0x822FA5A0` sont des voies I/O génériques sans ce contexte PM4.
## Checkpoint natif — frontière pilote

Le point natif partagé `GuestBridge::apply_xenos_mmio_write` publie directement le WPTR MMIO `0x7FC80714`, capture le ring circulaire puis en applique le stream. La chaîne démo qualifiée remet à la place un buffer de canal à `NtWriteFile`. Sans décodage de l'enveloppe pilote, ces contrats ne sont pas comparables instruction pour instruction; aucun changement natif n'est autorisé.
# Gate fermé — entrées de contenu de `0x821B96B8`

- Dans le projet Ghidra démo `ace-combat-6-demo` / `Default.xex`, `0x821B96B8` échange atomiquement le qword de `state+0x2e28` contre le sentinelle `-1`, puis retourne ses moitiés basse et haute à ses deux sorties.
- `0x821B9DB0` consomme ces sorties lorsque le qword n’était pas sentinelle : la moitié basse devient la base alignée à la page et la moitié haute borne la longueur alignée.
- Le même CFG réserve 11 mots par plage et écrit sa séquence de commande, dont `0xC0043C00`; il retourne l’adresse du buffer et le nombre de mots au consommateur amont.
# Revalidation — producteur de contenu de `0x821B9DB0`

- La décompilation démo actuelle de `0x821B9AE0` confirme une réservation de `param_2 × 4` octets et le comptage associé dans `state+0x3a3c`.
- Celle de `0x821BA780` confirme que les deux sorties de `0x821B9DB0` sont transmises sans transformation à `0x821BA058`, après réservation de l’en-tête de quatre dwords.
# Revalidation — source des descripteurs indirects du publisher

- `0x821BA058` prend un curseur d’en-tête, une adresse et une longueur ; sur son chemin direct, il forme localement `{ longueur & 0x00ffffff | 0x81000000, adresse }` et appelle `0x821B9BC8` avec un seul descripteur.
- `0x821B9BC8` consomme ces deux dwords comme longueur puis adresse et les écrit dans le ring derrière l’en-tête indirect Type-3 `0xC0013F00`.
- `0x821BA780` fournit à `0x821BA058` l’adresse et la longueur de `0x821B9DB0`; `0x821B2BC8` ne rejoint `0x821BA780` que sur sa branche de capacité, sans identité statique avec son template ultérieur.
## Gate fermé — ABI publique XMA Create

Le SDK qualifie `XMACreateContext` comme un `HRESULT` avec unique sortie `PXMACONTEXT*`; le PAL fournit le slot `entry+64` et le relit. L'adaptateur peut donc écrire seulement ce pointeur opaque et le statut. Taille/cardinalité du contexte, MMIO et décodage restent hors preuve et ne constituent pas une preuve de contenu RT0.
## Gate fermé — cycle XMA sans lien RT0

La trace bornée des trois `XMAReleaseContext` consomme les pointeurs du corridor local, puis atteint une frontière séparée; elle ne montre ni paquet ni écriture RT0/EDRAM. Ce cycle n'est donc pas un producteur de contenu graphique. La reprise commence au quatrième store XMA et à son consommateur PAL.
## Gate fermé — quatrième store XMA

Le CFG PAL de `0x82357240` produit le store du quatrième kick par la même boucle de création : index physique divisé par 64, bit one-hot, aperture XMA et barrière I/O. Le consommateur établi est matériel, pas guest; aucun lien RT0/EDRAM n'est établi.
## Gate fermé — handoff scheduler de `E000004C`

Le PAL crée `E000004C` par `NtCreateEvent` et le signale par `NtSetEvent`. Les traces jointes montrent 351 set→wake→reprise au même tick sur neutral et START. Le scheduler n'est pas la source du gel; aucun réveil synthétique n'est autorisé.
## Gate fermé — livraison START, frontière post-dispatch

START est qualifié dans le snapshot guest puis dans le mapper logique (`0x10 → 0x400 → 0x10`), mais les consommateurs menu ne sont pas atteints dans la fenêtre. Les contrôles tardifs excluent un second bouton manquant. La divergence est post-START dans l'état/dispatch guest.

## Gate fermé — consommateurs mission/niveau natifs

Dans le projet Ghidra démo `ace-combat-6-demo` pour `Default.xex`,
`0x820EA550` et `0x820EA598` sont désormais couverts nativement dans
`guest_mission_consumers.inl`. Build, audit, CTest (26/26), symboles et
absence de divergence des fichiers générés passent. Une sonde bornée à 120
ticks reste en état guest sans milestone et n'atteint aucun des deux wrappers;
la frontière de progression mission reste ouverte.

## Gate fermé — getters mission/niveau et validity gate natifs

Dans le projet démo qualifié, `sub_82095B80`, `sub_820E9290` et
`sub_820E9300` concordent entre Ghidra et le corps généré, puis sont couverts
par le bridge natif avec les wrappers mission/niveau déjà présents. Build,
audit, CTest (26/26) et symboles forts passent. La sonde bornée à 120 ticks
reste `guest/max_ticks` sans milestone ni entrée getter : reachability encore
indécidable, sémantique statique fermée.

## Gate fermé — écrivain de validity mission natif

Le balayage Ghidra démo trouve sept appels directs à \`0x820E9300\`. Le corps
\`0x821714C0\` est maintenant couvert nativement : branche de validité, écriture
du champ sélectionné \`+0x6c8\`, publication globale \`+0x20\`, puis appels aux
helpers de table encore générés. Build, audit, CTest (26/26) et symbole fort
passent; la reachability runtime reste une question distincte.

## Gate fermé — helpers de publication de validity natifs

Les helpers \`sub_8218E088\` et \`sub_8218EA88\` sont qualifiés dans le projet
Ghidra démo et concordent avec les corps générés. Ils sont maintenant natifs
dans \`guest_mission_consumers.inl\`; build, audit, CTest (26/26) et symboles
forts passent. La prochaine frontière statique est l'appelant \`sub_8216DB10\`.

## Gate fermé — appelant validity \`sub_8216DB10\` natif

\`sub_8216DB10\` et son helper de table \`sub_8218DF70\` concordent entre
Ghidra et le PPC généré : prédicat validity, publication globale et écritures
\`-0x24=3\` et \`-0x5c=2\`. Les deux fonctions sont natives; build, audit,
CTest (26/26) et symboles forts passent. La prochaine frontière est
\`sub_8216D760\`.

## Gate fermé — parent validity \`sub_8216D760\` natif

Dans le projet Ghidra démo \`ace-combat-6-demo\`, le corps PPC qualifie \`r3\`
comme objet d'entrée (l'appel \`0x8232710C\` est \`__savegprlr_29\`), publie la
table \`sub_8218DF70\` via \`sub_8218EA88\`, puis appelle les slots virtuels 13
et 14 aux LR \`0x8216D79C\` et \`0x8216D7E8\`. Il sélectionne \`mission+207\` ou
\`223\`, exécute \`sub_8219EE40\`/\`sub_8219EC88\`, et écrit \`+0xc=0\`,
\`+0x44=4\` et \`+0x60930=0\`. Le corps est maintenant natif; le dispatcher
indirect ne qualifie que ces deux LR et slots. Build, audit, CTest (26/26),
invariants statiques et symbole lié passent. La prochaine frontière est la
qualification et le remplacement des deux enfants récursifs.

## Gate fermé — enfants récursifs de \`sub_8216D760\` natifs

Dans le projet Ghidra démo \`ace-combat-6-demo\`, \`sub_8219EAA0\` parcourt une
liste (élément, suivant) depuis \`head\`, appelle le slot virtuel 1 au LR
\`0x8219EAE8\` et renvoie le nœud dont l'identifiant égalise la clé.
\`sub_8219EC88\` marque \`object+0x24=1\`, parcourt \`object+0x1c\`, appelle
le slot 3 au LR \`0x8219ECBC\` et se récurse lorsque le résultat signé vaut 1.
\`sub_8219EE40\` essaie d'abord \`EAA0(key,head)\`, puis le même slot 3 au
LR \`0x8219EE94\` et la récursion avec la clé au LR \`0x8219EEB0\`; il renvoie
le premier élément trouvé ou zéro. Les trois corps sont maintenant natifs et
les trois sites virtuels sont qualifiés par LR/slot exacts. Build, audit,
CTest (26/26), whitespace et symboles forts passent. La reachability runtime
et les cibles concrètes des slots restent des questions distinctes.

## Gate fermé — mutateur partagé des listes mission \`sub_8219ECF8\` natif

Le slicing statique des 249 appels à \`sub_8219EE40\` réduit le prochain point
commun à \`sub_8219ECF8\` : il appelle \`sub_8219EAA0\`, manipule la tête
\`object+0x1c\` et recycle la free-list globale \`0x827745ec\`. Son corps
généré qualifie neuf dispatchs aux slots 1, 5, 14, 12 et 11, avec les LR exacts
\`0x8219ed34\` à \`0x8219ee34\`; ces neuf sites sont maintenant enregistrés
dans le dispatcher et le mutateur est natif. Build, audit, CTest (26/26), diff
et symboles forts passent. Les wrappers producteurs et les cibles concrètes
des slots 1/3 restent la prochaine frontière statique.

## Gate fermé — wrappers producteurs et cibles virtuelles mission natifs

Le projet Ghidra démo et les corps générés qualifient `sub_8219EEE8`,
`sub_8219F080`, `sub_8219F1C0`, `sub_8219F300` et `sub_8219F468` : allocations
des éléments et nœuds, constructeurs, free-list `0x827745e0`, insertion à
`object+0x20`, et retours anticipés sont concordants. Les cinq wrappers sont
maintenant natifs avec conservation `r27..r31`/LR. Les 24 dispatchs de ces
wrappers sont enregistrés par LR et slot exacts; les cibles communes des slots
1 et 3 (`0x82311960` et `0x820d2c60`) sont aussi natives. Build, audit, CTest
(26/26), comparaison de contrats et symboles forts passent. La prochaine
frontière est le slicing statique des appelants de ces producteurs; aucune
sonde runtime n'est nécessaire à ce stade.

## Gate fermé — appelants directs des wrappers producteurs mission natifs

Dans le projet Ghidra démo canonique `ace-combat-6-demo` (`Default.xex`), les
xrefs directs donnent 36 appels répartis sur 21 appelants des cinq wrappers
producteurs. Les contrats des helpers `sub_8219E428` et `sub_8219E768` sont
concordants; six appelants (`sub_821A00E8`, `sub_821A0180`, `sub_821A0240`,
`sub_821A02C0`, `sub_821A0338`, `sub_821A03A8`) sont maintenant natifs avec
modes, registres, offsets de nœud, scratch guest, restauration callee-saved/LR
et retours slot 1 exacts. Les six LR virtuels `0x821A014C`, `0x821A01F0`,
`0x821A02A0`, `0x821A0324`, `0x821A0398` et `0x821A0410` sont qualifiés dans
le dispatcher. Les assertions de contrats, le build, l'audit de complexité,
CTest (26/26) et l'installation passent; aucune reachability runtime n'est
encore déduite. La prochaine frontière est une trace bornée vers Mission 01.
## Gate fermé — runtime borné: alias free-list avant Mission 01

L'import qualifié puis la sonde `SDL_AUDIODRIVER=dummy` sous Xvfb s'arrêtent au
tick 61 sur `LR=0x8219ECBC`, cible nulle. Les écritures hôte bornées montrent
que `0x827745E0` reçoit `0x18BB0300`, puis que `ac6_mission_take_node` écrit
`0x18960180` dans le mot 0 de cet objet; le snapshot de dispatch confirme le
vtable/slot 3 mappé mais nul. Le corps généré de `0x8219F080` concorde avec
l'aide native, donc aucune garde ou modification du dispatcher n'est justifiée.
Mission 01 reste non atteinte. La prochaine frontière est le producteur et le
chaînage qui introduisent l'adresse de tas dans la free-list `0x827745E0`.

## Gate fermé — resolver mission: sélecteur hors bornes

La correction de `ac6_mission_take_node` (retour de l'élément alloué, non de la
tête de free-list) supprime le trap du tick 61 et porte la sonde au tick 67.
Dans la chaîne qualifiée `0x821EDF40` → `0x821EE0F8` → `0x82278160`, le second
descripteur observé possède `count=2`, table `0x18BD1014`, mais reçoit l'index
`0x30`. La recherche retourne donc zéro par la branche statique hors bornes;
`0x82278160` transmet ce zéro à `0x821EDF40`, qui lit `r4+4` et déclenche le
trap à l'adresse `0x4`. Ce n'est ni un slot virtuel nul ni une raison d'ajouter
une garde au dispatcher. Build, CTest et installation codegen-on repassent.
La prochaine frontière est le producteur statique du sélecteur `0x30` et des
données du descripteur `0x18BD1000`.

## Gate fermé — producteur statique de l'index resolver `0x30`

Le slicing qualifié du projet démo canonique relie `sub_8219E7B0` aux champs
`object+0x14` (racine), `object+0x18` (descripteur) et `object+0x20` (index).
`sub_821A03A8` écrit son `r8` dans `object+0x20`; `sub_82191088` fournit ce
`r8` par le retour d'un appel virtuel à l'offset de vtable 160 (`0x82191148`),
avant l'appel à `0x821A03A8` (`0x82191164`). Le tick 67 est expliqué jusqu'à
cette frontière: `0x30` n'est pas un immédiat produit par le wrapper natif.
Le contrat local est fermé, l'origine amont reste indécidable sans qualifier
le service virtuel; aucune garde resolver n'est justifiée. La prochaine
frontière est le producteur concret de cet appel virtuel ou la documentation
explicite de cette limite de service.
## Gate fermé — cible virtuelle de l’index (offset 160)

La chaîne constructeur qualifiée `sub_821908B8` → `sub_8218EA98` appelle
`sub_8223D4D0` sur `objet+0x222D60`; ce constructeur écrit le vptr
`0x820210D0`. La vtable Ghidra qualifie l’offset 156 vers `0x8223D4C0` et
l’offset 160 vers `0x8223D4C8`. Le premier stocke `r4` à `receiver+3164`, le
second le relit; `sub_82191088` écrit donc `48` puis récupère exactement
`0x30`, avant que `sub_821A03A8` ne le place dans `resolver+0x20`. La frontière
virtuelle est locale et fermée; aucune garde resolver n’est ajoutée.

La prochaine frontière est le contrat statique du resolver après l’index
`0x30`: déterminer si le descripteur `count=2` attend réellement cet index ou
si le producteur/layout est incohérent.
## Gate fermé — contrat du resolver après l’index `0x30`

`sub_821EE0F8` est un lookup borné : il lit `count` à `+0`, compare
l’index signé avant tout accès, lit la table à `+12`, puis renvoie
`base(+4)+entry` seulement pour une entrée non nulle. `sub_821EDF40`
construit exactement ce descripteur; `sub_82278160` abandonne si sa
construction retourne zéro. Avec `count=2` et `index=0x30`, le retour nul
est donc le comportement PPC attendu, pas une garde manquante.

Le gate ferme la question du resolver et réfute l’hypothèse d’un bug de borne.
L’incohérence restante est le contrat amont du champ `resolver+0x20`
(valeur `0x30` consommée comme index de deux éléments). La prochaine
frontière est de réconcilier ce champ avec ses producteurs et le layout de
l’objet réellement exécuté.

## Gate fermé — setter object+8 et cible virtuelle slot 16

Le contrat statique de `sub_820D2C68` est un store direct de `r4` vers
`object+8`; les constructeurs concernés l'initialisent à zéro et
`sub_8219E580`/`sub_8219E7B0` le consomment sans garde. Le probe borné confirme
que le slot 16 (`lr=0x8219F594`) reçoit `0x18BC0100` pour l'objet
`0x189600C0`, puis que le consommateur `0x8219E7B0` relit cette même valeur au
tick 67. Le trap restant vient d'un autre objet (`0x18960100`) consommé par
`0x8219E580`, dont `object+8` vaut zéro. Le setter et l'aliasing sont donc
réfutés comme cause; aucune garde null n'est ajoutée. La validation finale
build/CTest/installation/layout passe.

La prochaine frontière est le producteur statique et le constructeur de l'objet
`0x18960100`/vtable `0x82012DC4`, afin de qualifier pourquoi son champ `+8`
reste nul avant `sub_8219E580`.

## Gate fermé — producteur E550 et argument du setter F42C

Le RTTI qualifie `0x82012DC4` comme le vtable E550 : le slot 16 (`+0x10`)
vise `sub_820D2C68` et le slot 20 (`+0x14`) vise `sub_8219E580`. Le setter
écrit directement `r4` vers `object+8`; F300 lui transmet `memory.load_u32(out)`
au retour `0x8219F42C`. La trace bornée
`artifacts/object8-null-producer-gate/F42C-zero-context.txt` montre trois
objets E550 construits au tick 61 puis des stores F42C de zéro à `+8`.
L'hypothèse « aucun setter n'est attendu » est réfutée; aucun garde ni patch
du constructeur n'est justifié. La prochaine frontière est le producteur de
`out` (`sub_8219EE40` ou le fallback allocator) avant F42C.

## Gate fermé — cadre ABI de F300 et producteur de `out`

Le slicing PPC qualifie `sub_8219F300` comme une fonction à cadre de 128
octets; `sub_821E1CD8` réserve elle-même `-128(r1)`. Le remplacement natif ne
réservait pas ce cadre, et la capture ciblée montre `out` non nul après les
slots 8/28/36 et ECF8 puis nul exactement après E1CD8 taille `0x20`. F300
réserve/restaure maintenant le cadre PPC attendu. La relecture bornée donne
trois valeurs `out` non nulles à E1CD8/E550/F42C et aucun trap nul.

Le probe atteint 100 ticks mais reste `max_ticks` avec le scheduler épuisé
avant le jalon mission; aucune preuve de gameplay visible n'est encore acquise.
La validation canonique build/CTest (27 tests)/installation/layout est verte.
La prochaine frontière est la progression du scheduler vers le jalon mission,
sans modifier davantage F300.

## Gate fermé — attente scheduler après F300

La fenêtre bornée à 300 ticks ne produit aucun trap et conserve le contrat
`out`/F42C corrigé, mais le rapport reste `max_ticks` avant mission. Les
compteurs compacts donnent `runnable=0`, `blocked=23`, `finished=0` et
`slice_exhaustions=201`; la frontière est explicitement « all started guest
threads blocked before mission milestone ». Le prochain test doit qualifier
le wait/événement qui maintient ces threads bloqués, sans élargir la trace.

## Gate fermé — frontière statique des réveils d'événements mission

Le slicing des attentes et publications est fermé dans
`artifacts/mission-wakeup-gate/event-gate-synthesis.txt`. Le bridge associe les
attentes aux clés `(wait_kind, wait_key)` et ne possède que les voies de réveil
bornées pour les événements, sémaphores, fins de thread, objets noyau et timers.
La source `src/guest_bridge/lifecycle.hpp` documente explicitement que le
client render-driver est enregistré mais jamais appelé dans le chemin par
défaut; le thread mixer reste donc sur huit événements auto-reset sans
producteur local, le signal étant fourni par le pilote audio console.

Le gate est fermé statiquement: aucun runtime supplémentaire n'est requis pour
établir cette frontière. Le prochain gate doit qualifier la frontière audio/XMA
et le callback render-driver, sans injecter de signal synthétique dans le
chemin de parité.

## Gate fermé — frontière audio/XMA du réveil mixer

Le slicing statique de `artifacts/audio-xma-boundary-gate/static-synthesis.txt`
qualifie l'enregistrement du client render-driver, le callback
`sub_8236DD98` → `sub_82355E58` et le global `0x829DA528`. Les imports XAudio
du bridge ne font qu'enregistrer, compter une soumission-sink et désenregistrer;
les imports XMA bornent la création/libération de contextes. Aucun de ces
chemins ne publie les huit événements auto-reset.

Le callback reste désactivé par défaut tant que
`AC6_DEMO_EXPERIMENTAL_XAUDIO_DRIVE` n'est pas demandé et que le global audio
n'est pas non nul. La frontière est donc fermée statiquement sur le producteur
console audio/XMA absent du chemin natif par défaut; aucun runtime ni signal
synthétique n'est requis. Toute suite doit rester derrière une frontière
audio/XMA explicitement nommée.

## Gate fermé — contrat externe XAudio/XMA

La recherche officielle Microsoft Learn est regroupée dans
`artifacts/audio-xma-external-contract-gate/official-source-check.txt`.
Les pages publiques établissent seulement le lien statique XAudio2 Xbox 360 et
son thread audio périodique; elles ne décrivent pas les exports privés
`XAudioRegisterRenderDriverClient`/`XAudioSubmitRenderDriverFrame` ni le contrat
XMA kernel. Le SDK local Rexglue corrobore un descripteur `(callback,
callback_arg)` et un worker qui appelle le callback quand la file est basse,
mais reste une source secondaire d'émulation.

La décision compacte est dans
`artifacts/audio-xma-external-contract-gate/static-boundary-decision.txt` et
`gate.status`: aucun producteur local des huit événements du mixer n'est
qualifié; la frontière restante est le pilote audio/XMA console. Aucun
runtime, signal synthétique ou activation de
`AC6_DEMO_EXPERIMENTAL_XAUDIO_DRIVE` n'est justifié. La suite doit fournir un
oracle externe borné ou un contrat XDK/xboxkrnl qualifié.

## Gate fermé — producteur guest de l'état XAudio

Le slicing qualifie la chaîne `0x8234F600 → 0x8234F058 → 0x82356410 →
0x82355F70 → 0x82356070`. Le constructeur `0x82355F70` installe les tables
audio, initialise l'objet et publie son adresse dans `0x829DA528`. Les atlas
neutral/start bornés exécutent chacun ces fonctions une fois au tick 106; une
arête indirecte vers `0x8234F058` est observée sur le thread 1. Le producteur
guest n'est donc pas manquant ni inatteignable.

Le callback `0x8236DD98` et le dispatcher `0x82355E58` ne sont pas observés
jusqu'au tick 253. Le gate ferme seulement la construction/publication du
global; la frontière suivante reste l'activation du producteur audio/XMA et
la publication des événements du mixer. Preuves compactes :
`artifacts/audio-xma-client-state-producer-gate/static-chain-decision.txt`,
`runtime-reachability-summary.txt` et `gate.status`.

## Gate fermé — activation bornée du callback XAudio

Une sonde native isolée, explicitement expérimentale et limitée à 140 ticks,
lit `0x829DA528 = 0x1005CEBC` aux ticks 62–67 et entre effectivement dans
`0x8236DD98` (arête `lr=0`, 212 appels). Elle s'arrête au tick 67 sur une
lecture guest non mappée à l'adresse `0x4`, avant qu'un producteur des huit
événements du mixer soit qualifié. La première tentative avec `--atlas` a été
refusée par le CLI avant lancement; elle n'est pas une observation guest.

Le gate d'activation est donc fermé : le callback n'est pas absent, il atteint
un enfant qui reste à qualifier. La frontière suivante est l'accès
`0x82355E58`/`0x4` et la publication éventuelle vers les handles mixer; le
chemin par défaut reste inchangé. Preuves compactes :
`artifacts/audio-xma-callback-activation-gate/runtime-summary.txt` et
`gate.status`.

## Gate fermé — cause du trap dans l'enfant du callback XAudio

Le slicing qualifie `0x8236DD98` comme un simple chargement de
`0x829DA528` dans `r3` suivi d'un tail-call vers `0x82355E58`; `r4` reste donc
hérité du bridge. Le dispatcher lit l'objet `r3+0x40`, son vtable, puis
appelle le slot `vtable+0x44`, observé vers `0x82278160`. Cette fonction passe
`r3+0x1c` à `0x821EDF40` sans remplacer `r4`. Or la première instruction de
`0x821EDF40` lit `r4+4`. Le trap runtime `address=0x4`, `r4=0`,
`lr=0x82278184`, tick 67, correspond exactement à cette lecture.

`0x82355E58` contient aussi un `KeSetEvent` conditionnel (callsite
`0x82355EA8`, garde `object+0x130`), mais aucune publication runtime de la
capture bornée ne lui est attribuée; aucun lien avec les huit handles mixer
n'est qualifié. Le gate ferme donc la cause immédiate sans modifier le
bridge. La frontière suivante est le producteur/layout du descripteur `r4`
et la correspondance des événements. Preuve :
`artifacts/audio-xma-callback-child-gate/static-child-decision.txt` et
`gate.status`.

## Gate fermé — séparation du descripteur d'enregistrement et du `r4` enfant

Le wrapper `0x8234D0E8` est qualifié : son `r4` entrant est `0x8236DD98`,
qu'il place en `descriptor[0]`; `descriptor[1]` vient de `object+0x0c`, et
le second argument ABI est le pointeur de handle. Ce descripteur
d'enregistrement n'est donc pas le `r4` du callback enfant. `0x8236DD98` ne
fait qu'alimenter `r3` avec `0x829DA528` puis tail-caller `0x82355E58`; aucun
producteur guest qualifié ne remplit le `r4` ultérieur. La capture bornée
confirme `r4=0`, trap `0x4`, `lr=0x82278184`.

Le gate ferme la recherche statique du producteur guest et conserve la
frontière XDK/XMA du contexte enfant ainsi que l'attribution de `0x82355EA8`
aux handles mixer. Aucune modification du bridge ni signal synthétique
n'est justifié. Preuve :
`artifacts/audio-xma-r4-descriptor-gate/static-r4-producer-decision.txt` et
`gate.status`.

## Gate fermé — objet d'événement interne XAudio

Le généré PPC qualifié établit que `0x82356070` construit les objets statiques
`0x829DA518` et `0x829DA508`, et initialise le sémaphore `0x829DA4F4` avec
une limite de 6. `0x82355818` publie `KeSetEvent(0x829DA518)` après le test du
compteur de clients; `0x82355CE0` attend cet objet, libère le sémaphore puis
signale `0x829DA508`. `0x82355E58` réutilise `0x829DA518` avant son attente
multiple. Les références Ghidra qualifiées de `0x829DA518` sont limitées à
ces fonctions et au constructeur, sans alias vers l'enregistrement ou les
huit handles mixer. `0x829DA4E4` est séparé et lié à la notification de
terminaison. Le pont natif sait déjà attendre et signaler un objet événement
guest via `object+4`; aucun patch n'est justifié.

Le gate ferme donc l'attribution de cet événement à un worker XAudio interne.
La frontière active reste le contexte enfant privé XDK/XMA passé en `r4`; le
chemin expérimental demeure désactivé par défaut. Preuves :
`artifacts/audio-xma-event-object-gate/static-event-object-decision.txt`,
`gate.status` et `static-ref-owner-map.txt`.

## Gate fermé — contexte enfant `r4` XDK/XMA non qualifié

Le code Xenia local qualifie seulement son oracle : `RegisterClient` stocke
`descriptor[0]`/`descriptor[1]`, puis le worker invoque le callback avec un
unique argument guest. Il ne fournit donc pas de second `r4`. Dans AC6,
`0x8236DD98` ne remplit pas `r4`, `0x82355E58` le transmet implicitement,
et `0x821EDF40` lit `r4+4`; la capture observe `r4=0` et le trap attendu à
`0x4`. Le contexte de registration `object+0x0c` est nul, et le couple CPU4
`+0x2c/+0x30` n'est pas relié statiquement à cet enfant.

Le gate ferme donc la recherche guest et laisse explicitement ouverte la
frontière privée XDK/XMA. Aucun patch du bridge, binding heuristique ou signal
synthétique n'est justifié. Preuves :
`artifacts/next-xdk-xma-r4-context-gate/static-external-context-decision.txt`,
`hypothesis-criteria.txt`, `gate.status` et `validation.txt`.

## Gate fermé — table et writers du slot XMA

Les fenêtres PPC qualifient la table construite par `0x82356528` : compteur
à `table+0x00`, flags à `table+0x04`, base des entrées à `table+0x08`, stride
`0x60`. Chaque entrée réserve son slot de contexte à `entry+0x40` et son index
de lane à `entry+0x50`. `0x82357240` appelle `XMACreateContext` uniquement si
ce slot est nul, puis calcule l'index physique et atteint le premier store
MMIO inconnu à `0x823572D8`.

L'allocation appelle `0x821A4B70` au callsite `0x82356610`; son `dcbzl` efface
l'allocation avant les écritures de table. La libération `0x823567E0` appelle
`XMAReleaseContext` puis efface `entry+0x40`. Le gate ferme donc la propriété
guest du slot et ses writers connus, mais pas l'ABI privé XMA ni l'effet MMIO.
Preuves : `artifacts/xma-output-slot-frontier-gate/static-slot-table-decision.txt`,
`hypothesis-criteria.txt`, `gate.status` et `validation.txt`.

## Gate fermé — réutilisation de la free-list XMA

Le header SDK local `sdk/xdk-xenon-6132.6/XDK/include/xbox/xmadecoder.h`
qualifie `XMACreateContext(PXMACONTEXT*)` comme une création par out-pointer
HRESULT et `XMAReleaseContext(PXMACONTEXT)` comme une fonction `VOID` qui remet
le contexte sur une free-list. Le type `IXMAContext` reste opaque : aucune
taille ou layout privé n'est déduit de cette source.

Le bridge conservait pourtant seulement un index haut-water monotone; un slot
libéré ne pouvait donc jamais être attribué de nouveau. `allocate_xma_context`
réutilise maintenant le premier slot inactif du préfixe déjà alloué, puis
n'étend ce préfixe seulement lorsqu'il est plein. Les validations, le zeroing
et la borne 320 restent inchangés. Le test isolé confirme la réutilisation après
trois releases, et la suite complète passe 27/27. Ce gate ne modifie ni l'ABI
XMA, ni le layout opaque, ni le producteur audio.

Preuves : `artifacts/xma-context-free-list-gate/static-sdk-decision.txt`,
`hypothesis-criteria.txt`, `gate.status` et `validation.txt`.

## Gate fermé — couche privée XMA identifiée, effet MMIO toujours ouvert

Les deux PDB locaux du noyau de debug XDK nomment `_XMA_CONTEXT_DATA`,
`_XMA_REGISTERS`, `CXMADecoder::CreateContext/ReleaseContext/Enable`,
`m_pHWContexts`, `m_pRegisters`, `m_contextsInUse` et la chaîne
`No free client contexts`. Cela confirme une couche noyau privée pour les
contextes matériels, les registres et la free-list, mais ne fournit aucun
offset de champ, aucune adresse de registre ni sémantique du store PPC
`0x823572D8`. Le bridge conserve donc ses mappings MMIO expérimentaux sans
leur attribuer d'effet device-facing; aucun patch ni signal audio synthétique
n'est justifié.

Preuves : `artifacts/xma-private-mmio-gate/rank1-pdb-summary.txt`,
`private-layer-decision.txt`, `gate.status` et `validation.txt`.

## Gate fermé — registres XMA indexés et rôles Enable/Disable qualifiés

Le désassemblage PPC borné du noyau de debug officiellement apparié, puis le
scan statique de toute sa section `.text`, qualifie la formule privée : le
contexte est indexé par blocs de 64 octets, `group = index >> 5`, et la valeur
écrite est le bit `1 << (index & 31)`. Les adresses indexées sont
`0x7FEA1A40 + 4*group`, `0x7FEA1A80 + 4*group`, `0x7FEA1940 + 4*group` et la
lecture `0x7FEA1840 + 4*group`.

`CXMADecoder::Enable` écrit `1A80` puis `1940`; `CXMADecoder::ReleaseContext`
écrit `1A40` puis `1A80`; `XMADisableContext` lit `1840`, teste le bit puis
écrit `1A40`. Le scan complet ne trouve que ces cinq stores indexés. Le
cross-match Xenia confirme la même arithmétique et les rôles Enable/Disable,
mais reste secondaire; la contribution officielle de `XMAInitializeContext`
ne contient pas le store indexé `1A80`.

Le gate ferme donc l'arithmétique et les rôles indexés, mais laisse ouverts le
layout privé `_XMA_REGISTERS`, le sens des registres directs, l'attribution de
`1A80` à l'initialisation publique et l'effet device-facing du store guest
`0x823572D8`. Aucun mapping expérimental ni signal audio n'est activé.

Preuves : `artifacts/xma-private-mmio-gate/xma-kernel-register-addresses.txt`,
`xma-kernel-methods-full.txt`, `variant-pairing.txt` et `gate.status`.

## Gate fermé — layout XMA privé statique épuisé

La recherche de rang 1 est maintenant close : le SDK officiel garde
`IXMAContext` opaque, `xmahardwareabstraction.h` n'expose aucun registre, et
le TPI du PDB officiel ne fournit aucun enregistrement de type exploitable.
Le noyau officiel qualifie les familles indexées et leurs rôles opérationnels,
mais pas les offsets privés ni les registres directs. Xenia/ReXGlue ajoutent une
table générique (`1A40`/`1A80`/`1940`) et un contexte de 64 octets, conservée
comme corroboration de rang inférieur uniquement; aucun nom générique n'est
promu au PAL AC6.

Le store `0x823572D8` reste donc une frontière calculée/observée dont l'effet
device-facing est ouvert. Aucun mapping fonctionnel, producteur PCM ou patch
du bridge n'est justifié. La prochaine gate est une capture bornée des octets
du contexte et des consommateurs immédiats, avec critères H1/H2 pré-enregistrés.

Preuves : `artifacts/xma-register-layout-gate/static-decision.txt`,
`third-party-decoder-extract.txt`, `third-party-register-table.txt`,
`hypothesis-criteria.txt`, `gate.status` et `validation.txt`.

## Gate fermé — état guest après les trois premiers stores `0x1A80`

Un observateur lecture seule, activé uniquement par
`AC6_DEMO_WATCH_XMA_CONTEXT`, a capturé la fenêtre bornée `buttons16` autour
de `0x823572D8`. Trois stores one-hot acceptés à tick 916 (indices 0, 1, 2)
pointent respectivement vers `0x2E7FF000`, `0x2E7FF040` et `0x2E7FF080`; les
16 mots big-endian de chaque contexte sont tous nuls. Aucun appel enfant ou
consommateur immédiat n'apparaît dans la sortie bornée, ce qui concorde avec
la décompilation de `0x82357240` qui retourne après la barrière I/O.

H1 (transition de contenu du contexte) est donc réfutée pour ce chemin observé.
H2 est étayée comme contrôle one-hot sans producteur PCM observé, mais reste
indécidable quant à l'effet device-facing: la fenêtre n'a pas atteint un
consommateur matériel qualifié. Aucun mapping MMIO fonctionnel ni signal audio
n'est ajouté.

Preuves : `artifacts/xma-runtime-state-gate/capture-criteria.md`,
`capture-analysis.txt`, `context-watch-capture/buttons16.stderr.log`,
`ghidra-xma-functions-noanalysis.log` et `validation.txt`.

## Gate fermé — aucun consommateur direct `1A80` dans le PAL qualifié

Le scan Ghidra read-only du projet `ace-combat-6-demo` ne trouve aucune
matérialisation fixe de `0x7FEA1A80`, `0x7FEA1940` ou `0x7FEA1A40`; seul
`0x7FEA1840` est matérialisé directement dans `0x82357050`. Les 33 références
de l'aperture `0x1800..0x1AA4` se concentrent dans `0x82356510`, `0x82357050`,
`0x82357390` et `0x82357458`. `0x82356528` construit les entrées de 0x60
octets puis appelle `0x82357240`; cette dernière retourne après le store
one-hot et `eieio`, sans enfant post-store.

Le premier consommateur device-facing de `1A80` n'est donc pas fermé par le
PAL statique. La prochaine frontière est la chaîne de cycle de vie ultérieure
(`0x82357310`/`0x823575A8`) ou un consommateur noyau officiellement qualifié;
aucun mapping MMIO n'est activé.

Preuves : `artifacts/xma-runtime-state-gate/post-store-static-decision.txt`,
`post-store-consumer-ghidra.log`, `post-store-consumer-raw-materialization.log`,
`post-store-callers-ghidra.log`, `xma-create-caller-decomp.log`, `gate.status`
et `validation.txt`.

## Gate fermé — chaîne XMA ultérieure et état guest distinct

La décompilation PAL ferme la chaîne de cycle de vie sans lui attribuer une
sémantique MMIO. `0x82357458` compare l’index de chaque entrée au registre
direct assemblé `0x7FEA1818..0x7FEA181B`, puis recopie et neutralise des lignes
de cache avant de poser le flag `0x20000`. `0x823575A8` flushe les blocs,
recopie le payload de l’entrée vers `entry+0x40`, émet ensuite le one-hot à
`0x7FEA1940` et efface les flags `0x10000/0x20000`. `0x82357390` ajoute le
store direct `0x7FEA1804=0x03000000` dans le chemin de récupération.

Cet état guest est distinct du store immédiat `0x7FEA1A80`, mais le PAL ne
nomme ni le registre direct, ni un format PCM, ni un effet audible. Aucun
mapping matériel n’est activé. La frontière suivante est le consommateur
noyau officiellement qualifié ou une observation locale du nouvel état
matériel; le trap connu du cinquième bit ne doit pas être répété.

Preuves : `artifacts/xma-runtime-state-gate/later-chain-static-decision.txt`,
`later-chain-extract.txt`, `post-store-callers-ghidra.log` et `gate.status`.

## Gate fermé — consommateur noyau ISR XMA

Le PDB XDK officiel nomme `CXMADecoder::InterruptServiceRoutine` à `0007:0xACE68`.
Dans le binaire apparié, l'ISR lit `0x7FEA1808` avec `lwbrx`, teste trois bits,
puis écrit les commandes `0x100`, `0x200` ou `0x400` vers `0x7FEA1A08` avec
`eieio` avant l'épilogue commun. Cela qualifie le consommateur noyau de l'état
direct, sans donner de nom aux champs ni d'effet PCM/audible. Les headers
publics et le TPI PDB restent opaques; aucun mapping matériel n'est activé.

Preuves : `artifacts/xma-kernel-consumer-gate/decision.txt`,
`isr-static-scan-corrected.txt`, `isr-prologue-body.txt` et `gate.status`.

La recherche suivante n'a trouvé aucun layout privé qualifié dans les headers
XDK, le TPI PDB (zéro record de type) ou les sources Xenia. Le prochain test,
s'il devient nécessaire, est borné aux accès `0x1808`/`0x1A08` autour des trois
premiers stores `1A80`; ses critères H1/H2 sont dans
`artifacts/xma-kernel-consumer-gate/next-observation.md`.

## Gate fermé — ISR XMA non instrumentable dans la route native

Les deux sorties générées ne contiennent que les imports XMA de création et de
libération de contexte; aucune routine générée ni aucun littéral direct
`0x7FEA1808`/`0x7FEA1A08` n'est présent. Le bridge ne mappe pas ces apertures,
et une lecture non mappée est piégée avant l'observateur tardif. La relation ISR
reste donc une preuve noyau statique, non une observation native disponible.
H1/H2 sont indécidables dynamiquement à cette frontière; aucun mapping ou
émulation MMIO n'est ajouté.

Preuves : `artifacts/xma-kernel-consumer-gate/native-isr-decision.md`,
`native-isr-instrumentability.txt`, `native-isr-validation.txt` et
`gate.status`.

## Gate fermé — ABI guest des imports XMA create/release

Le header XDK officiel rend `PXMACONTEXT` opaque et déclare
`XMACreateContext(PXMACONTEXT*)` avec un retour `HRESULT`; le PPC à
`0x82357298` passe le slot en `r3`, teste le statut puis relit le contexte
avant `MmGetPhysicalAddress`. Les seuls imports XMA effectivement appelés
sont `XMACreateContext` et `XMAReleaseContext`; le bridge existant respecte ces
formes sans attribuer de sémantique PCM ou privée.

Preuves : `artifacts/xma-import-abi-gate/decision.md`, `static-extract.txt`,
`validation.txt` et `gate.status`.

## Gate historique supersédé — ancienne lecture du slot XMA

Le bloc suivant conserve l'observation historique, mais son interprétation est
désormais invalidée par le producteur statique qualifié ci-dessous.

La réconciliation statique ne trouve aucune canonicalisation entre le calcul
PAL de `sub_82357240` et `PPC_MM_STORE_U32` : l’adresse brute atteint
`GuestMemory`, dont les MMIO sont exacts. `MmGetPhysicalAddress` renvoie
l’adresse virtuelle inchangée, tandis que l’allocateur natif commence à
`0x10000000` et ses adresses sont alignées sur 0x1000. Avec l’identité
physique actuelle, les 12 bits bas du contexte sont donc nuls; le décalage
depuis `0x829DA52C` ne peut pas produire l’index requis pour le store exact
`0x7FEA1A80`. Si le tableau est la première allocation, l’exemple est
`0x896B` → `0x7FEA2BAC`; le bridge ne mappe que `0x7FEA1A80`.

Le mismatch est confirmé; aucune sémantique de registre ni correction de
traduction virtuelle→physique n’est ajoutée. La prochaine frontière doit
qualifier le producteur de l’adresse physique attendue avant toute modification
de `MmGetPhysicalAddress` ou du mapping.

Cette conclusion traitait à tort `0x829DA52C` comme une adresse fixe. Elle est
supersédée par la qualification de son producteur ci-dessous et ne doit plus
guider une modification de `MmGetPhysicalAddress` ou du mapping.

## Gate fermé — producteur statique de la base XMA

`sub_82356510` lit `0x7FEA1800` avec `lwbrx` et écrit la valeur dans le slot
global `0x829DA52C`. `sub_82357240` recharge ce slot après
`MmGetPhysicalAddress`, le soustrait au pointeur retourné, puis dérive l'index
du `stwbrx` XMA. Le bridge publie précisément ce même tableau à la lecture
`0x7FEA1800` et conserve l'identité de l'adresse physique allouée.

La capture bornée existante confirme le flux `0x2E800000` → global →
`MmGetPhysicalAddress` → tentative `0x7FEA1A80`; l'ancien « mismatch » est donc
réfuté. Aucun code natif n'est modifié.

Preuves : `artifacts/xma-physical-producer-gate/producer-summary.md`,
`validation.txt`, `checkpoint.txt` et `gate.status`.

## Gate fermé — consommateur privé tardif XMA (structure guest)

La chaîne PAL après le store initial est maintenant qualifiée statiquement au
niveau du contrôle guest : entrées de 96 octets, index u16 à `entry+80`,
groupes `index>>5`, one-hot vers `0x7FEA1A40` dans `0x82357310`, copie/cache
vers `entry+0x40` puis one-hot vers `0x7FEA1940` dans `0x823575A8`, et accès
directs `0x7FEA1804`/`0x7FEA1818` dans les routines voisines. Les barrières
`eieio`, les bits de gestion `0x10000`/`0x20000` et les constantes d'adresse
sont vérifiés localement.

Cette gate ferme le consommateur privé et les mutations d'état guest, pas la
sémantique hardware/PCM. Les registres directs et le layout privé restent
opaques; aucun mapping MMIO ni patch natif n'est ajouté. La prochaine preuve
doit obtenir un consommateur device-facing qualifié ou ouvrir une frontière PAL
indépendante avec un discriminant nouveau; le trap connu ne doit pas être
répété.

Preuves : `artifacts/xma-private-consumer-gate/decision.md`,
`formula-validation.txt`, `source-anchors.txt`, `validation.txt` et
`gate.status`.

## Gate fermé — formats de fetch Xenos atteints

Le snapshot statique du draw atteint qualifie `format=57` pour la position
(trois composantes, full fetch vers `r1`) et `format=38` pour la couleur
(quatre composantes, mini fetch vers `r0`). Les tables Xenos locales concordent
avec ces deux correspondances. Les exports `position=max(r1,r1)` et
`interpolator0=max(r0,r0)` sont joints, mais les mots de microcode et les octets
du vertex buffer ne sont pas publiés; la sortie PS reste non jointe.

Cette gate ferme uniquement la cartographie statique des formats. Le traducteur
natif reste fail-closed et aucun pixel n'est synthétisé. La prochaine preuve
doit joindre le producteur du vertex buffer et qualifier séparément la sortie
du pixel shader.

Preuves : `artifacts/shader-static-frontier-gate/decision.md`,
`source-anchors.txt`, `validation.txt` et `gate.status`.

## Gate fermé — jointure producteur du vertex buffer / fetch atteint

La plage guest chargée par le chemin natif est `[0x127CA03C, 0x127CA0A8)`.
Son préfixe de 84 octets contient les 21 dwords du draw normal et se joint aux
fetches atteints `format=57` (position, stride 7) et `format=38` (couleur,
offset 3). Le calcul ReXGlue/Xenon envoie `0x127CA03C` au binding 2, offset
local `0x027CA03C`, exactement l'offset d'écriture de `VulkanSharedMemory`.

Cette gate ferme le producteur et l'adressage du vertex buffer, sans runtime,
sans patch et sans conclure sur la sortie PS ou l'effet EDRAM.

Preuves : `artifacts/vertex-buffer-producer-gate/decision.md`,
`arithmetic-validation.txt`, `source-anchors.txt`, `validation.txt`,
`gate.status` et `SESSION_ROTATE`.

## Gate fermé — sortie pixel exacte et chaîne writer PM4

Le pixel shader atteint `[0x82013E80,0x82013EA4)` contient 9 dwords :
`alloc colors`, `exece`, puis `max oC0,r0,r0`. La modification pixel par défaut
ReXGlue ne demande ni interpolateur ni paramètre généré; `r0` et la cible
couleur 0 sont initialisés à `[0,0,0,0]`, et `StoreResult` mappe `oC0` sur cette
cible. La traduction SPIR-V et la validation locale passent. La couverture
native jointe remplit tous les échantillons du draw et résout 230400 pixels
RGBA zéro, ce qui exclut un draw absent.

Le paquet RT0 est écrit par `0x821B55C0` au PC `0x821B5840`, puis le paquet
`RB_COPY` par `0x821B6FD0` au PC `0x821B7C04`. Ces points qualifient la chaîne
guest writer → PM4 et son ordre, pas les échantillons matériels EDRAM. Le
contenu guest-owned avant le copy et le premier frame non noir restent
indéterminés; aucun patch natif n'est ajouté.

Preuves : `artifacts/pixel-output-edram-gate/decision.md`,
`arithmetic-validation.txt`, `observations.md`, `validation.txt` et
`gate.status`.

## Gate fermé — frontière pixel non-bootstrap atteinte

Les inventaires PM4 neutre/START sont exhaustifs pour les routes déjà
qualifiées : `neutral-first` charge seulement le pixel de 9 dwords avant ses
24 points; le main IB et START chargent `27V, 9P, 15V` avant leurs deux
rectangles. Le résumé renderer conserve 5 loads, 26 draws et 1 present, et
les deux rectangles partagent ce même pixel bootstrap.

Le corpus NSXR contient cinq pixels de 60 octets candidats et deux pixels de
36 octets; seuls `0x8264B610`/`0x8264BA10` sont joints au pixel atteint. Aucun
writer statique supplémentaire ne distingue les samples EDRAM physiques.
La gate ferme donc la frontière statique, sans patch ni pixel synthétique.

Preuves : `artifacts/next-nonbootstrap-gate/decision.md`,
`route-shader-census.txt`, `pm4-nsxr-focused.txt`, `source-anchors.txt`,
`validation.txt` et `gate.status`.

## Gate fermé — fenêtre post-START indécidable

Le runner interactif historique a été refusé avant exécution par incompatibilité
de CLI. Un unique `probe` codegen-on a ensuite consommé 3 036 ticks avec START
au tick 3 000 puis relâchement au tick 3 001. Le rapport atteint 2 928
présentations mais laisse `frontend`, `mission` et `terminal` faux : tous les
threads guest démarrés restent bloqués avant le jalon frontend. La trace confirme
l'entrée bornée, mais ne publie aucun événement shader/draw/pixel/EDRAM; le
résumé renderer n'expose que 2 loads et 24 draws sans payload décodable.

Cette gate ferme le choix d'outil et classe la question pixel non-bootstrap
indécidable, sans patch natif et sans recours à Xenia Wine ou Edge. La prochaine
gate doit qualifier statiquement l'attente guest/kernel au point de blocage et
ne préparer une nouvelle fenêtre qu'après preuve qu'elle franchira frontend.

Preuves : `artifacts/post-start-runtime-window/decision.md`,
`validation.txt`, `direct-probe-20260821-a/report-detail.txt`,
`direct-probe-20260821-a/trace-events-summary.txt` et `gate.status`.

## Gate fermé — identité du blocage guest qualifiée

Le rapport borné décrit 23 threads bloqués; le thread primaire ré-entre
`0x822F8848` depuis `0x822E559C`, puis attend `0xE000004C` avec le LR
`0x821A69CC`. La jointure statique PAL identifie `0x821A69C8` comme l'appel
direct à `NtSignalAndWaitForSingleObjectEx` ordinal 251. La cible dynamique est
le slot 4 de `0x8202A488` pour `0x82934280`, taille `0xA8`, rôle encore inconnu.

Le scheduler natif et les wrappers d'événements sont cohérents avec cette paire
signal/wait; la cause du non-réveil frontend et son producteur d'événement restent
ouverts. Aucun patch ni élargissement runtime n'est autorisé.

Preuves : `artifacts/static-guest-block-gate/decision.md`,
`current-waits-compact.txt`, `event-writer-static-summary.txt`,
`event-caller-list.txt`, `validation.txt` et `gate.status`.

## Gate fermé — producteur de la paire d'événements qualifié

La paire est publiée par `sub_822EED70` dans `0x82934748/0x8293474C` via les
stores `0x822EEDA8/0x822EEDB4`, avec les valeurs `0xE0000048/0xE000004C`.
`sub_822EEE10` relit le wait et appelle `NtSetEvent` (`0x821A6AC0`), tandis
que `sub_822E4080` relit signal/wait et appelle
`NtSignalAndWaitForSingleObjectEx` (`0x821A69C8`). La route bornée observe
351 couples SetEvent/réveil, mais le frontier post-START reste bloqué avant
frontend; l'ownership/scheduling du writer à ce frontier est donc la seule
incertitude immédiate. Aucun patch ni runtime élargi.

Preuves : `artifacts/static-event-producer-gate/decision.md`,
`validation.txt`, `producer-reports-compact.txt` et `gate.status`.

## Gate fermé — ownership du writer et contrainte scheduler

Le thread 12 possède le chemin `0x822E3EC0 -> 0x822EEE10` et produit
normalement `NtSetEvent(0xE000004C)`. Au frontier, ce même thread est bloqué
sur `0xE0000040`; les 23 threads sont bloqués et le scheduler fiber mono-host
ne réveille que les couples `(kind,key)` exacts. Le writer ne peut donc pas
progresser par lui-même. La prochaine question est le producteur de
`0xE0000040`; aucun patch ni runtime élargi.

Preuves : `artifacts/static-event-writer-ownership-gate/decision.md`,
`frontier-thread12.txt`, `generated-callchain-extract.txt`, `validation.txt`
et `gate.status`.

## Gate fermé — producteur statique de `0xE0000040`

Le producteur exact est maintenant joint : le thread 2 atteint
`sub_822E5660`, qui fournit l'objet `0x82934708` à `sub_822E3EB8`; ce dernier
charge le champ `+0x58` à `0x82934760` puis appelle le wrapper
`sub_821A6AB0`. Les atlas bornés observent cette chaîne aux ticks 1–252
(`count=252`). La frontière restante est l'enregistrement et l'activation du
callback au LR indirect `0x821C5178`; aucun patch ni runtime élargi.

Preuves : `artifacts/static-event-producer-0xE0000040-gate/decision.md`,
`producer-detail.txt`, `generated-event0040-candidates.txt`,
`validation.txt` et `gate.status`.

## Gate fermé — enregistrement du callback `0x821C5178`

Le thread 1 installe le callback `0x822E5660` via
`0x822F85B8 -> 0x822F85A8 -> 0x822E5670 -> 0x821C5D68`; le dernier store
écrit le slot `+16520`. Le dispatcher `0x821C5090` relit ce slot et exécute
le callback à LR indirect `0x821C5178`; les atlas neutral/start l'observent
sur le thread 2 aux ticks 1–252. L'objet exact et sa correspondance
affinité restent ouverts. Aucun patch ni runtime élargi.

Preuves : `artifacts/static-callback-registration-gate/decision.md`,
`registration-detail.txt`, `generated-callback-registration-extract.txt`,
`validation.txt` et `gate.status`.

## Gate fermé — objet de registration et frontière d’affinité

`sub_822F85B8` construit l’objet publié dans `stack+80` par
`sub_821BB4C8`, initialise son mot `+0x56F8` à `0x0C000001`, puis passe ce
 même pointeur à `sub_822F85A8`. La chaîne
`0x822F85B8 -> 0x822F85A8 -> 0x822E5670 -> 0x821C5D68` installe donc le
callback `0x822E5660` dans le même objet, au slot `+16520`.

La construction ne contient aucune arête vers `sub_821A5390` ou
`KeSetAffinityThread`; ses sept callsites sont hors de cette chaîne. Les
atlas d’affinité ne montrent aucun appel sur le thread 2. L’objet → callback
est fermé, l’objet → affinité/CPU reste indécidable statiquement. Aucun patch,
runtime large ni oracle Wine/Edge n’a été utilisé.

Preuves : `artifacts/static-owner-object-affinity-gate/decision.md`,
`owner-affinity-detail.txt`, `affinity-wrapper-callers.txt`,
`constructor-callers.txt`, `validation.txt` et `gate.status`.

## Gate fermé — dispatch runtime objet/affinité

Le probe natif borné à 3036 ticks observe une arête indirecte unique
`0x821C5178 -> 0x822E5660` sur le thread invité 2, 3035 fois. Le snapshot
final donne `r31=0x10041A00` et `r11=0x822E5660`, ce qui ferme la jonction
objet enregistré → slot callback → cible exécutée. Dix-neuf transitions
d'affinité valides passent par `0x821A53DC` sur les threads 1, 12–17, 20 et
25; aucune ne concerne le thread 2. L'affectation CPU de l'objet reste donc
indéterminée. Aucun patch de scheduler ou de sémantique native.

Preuves : `artifacts/bounded-dispatch-object-affinity-gate/decision.md`,
`runtime-summary.txt`, `validation.txt` et `gate.status`.

## Gate fermé — validation native et O3

La suite CTest qualifiée passe 27/27 avec `SDL_AUDIODRIVER=dummy`. Les six
contrats ciblés passent dans la configuration courante et lors d'une seconde
compilation directe en `-O3`; aucune différence de résultat n'est observée.
Le guest généré n'a pas été modifié et aucune sémantique native n'a été
patchée par cette gate. Les changements du worktree étaient déjà présents au
préflight.

Preuves : `artifacts/native-validation-o3-gate/decision.md`, `results.txt`,
`o3-results.txt`, `validation.txt` et `gate.status`.

## Gate fermé — observation visuelle native

La fenêtre Vulkan native bornée à 3036 ticks se termine avec le code 4. Elle
produit 2928 notifications de présentation, mais aucune présentation
qualifiée, aucun fichier de capture et aucun jalon frontend ou mission. Le
rapport réduit indique que les threads invités restent bloqués avant le
frontend. La gate est donc fermée sans preuve visuelle de gameplay; aucun
oracle Wine/Edge ni patch de rendu n'a été utilisé.

Preuves : `artifacts/visual-native-gameplay-gate/decision.md`,
`runtime-summary.txt`, `validation.txt` et `gate.status`.

## Gate fermé — réveil natif du producteur 0xE0000040

La documentation XDK confirme la sémantique auto-reset utilisée par cet
événement. L'analyse statique suit le producteur
`0x822E5660 -> 0x822E3EB8 -> 0x821A6AB0`, via l'objet `0x82934760+0x58`, vers
`0xE0000040`. Une sonde native bornée à trois ticks retourne 4 : aux ticks 1 et
2, le thread 2 publie 0040, réveille le thread 12, puis celui-ci reprend à
`0x821A8C88`, publie 004C et se remet en attente sur 0040. Frontend et mission
restent faux. Le producteur et le réveil natif sont donc confirmés ; aucun
correctif scheduler/événement n'est retenu.

Preuves : `artifacts/event0040-native-producer-gate/decision.md`,
`runtime-summary.txt`, `static-summary.txt`, `validation.txt` et
`gate.status`.

Prochaine frontière : qualifier statiquement le consommateur après reprise à
`0x821A8C88` et le chemin primaire `0x821A69CC`.

## Gate fermé — boucle guest post-réveil

Le projet Ghidra qualifié du demo (`ace-combat-6-demo` / `Default.xex`) montre
que `0x821A8C50` est le wrapper d'attente et que `0x821A6AF0` lui passe un
timeout nul. Le callback runtime `0x822E3EC0` attend 0040, incrémente son
compteur sous verrou, appelle `0x822EEE10`, signale 004C, puis réattend 0040
tant que son drapeau `state+0x18` reste nul. Les fonctions
`0x822E4018`, `0x822E4080` et `0x822EEE68` réutilisent la même boucle côté
`SignalAndWait`. La séquence observée `resume 0040 -> set 004C -> reblock
0040` est donc une arête guest attendue, pas une perte du réveil natif.

Preuves : `artifacts/postwake-static-gate/decision.md`,
`static-summary.txt`, `validation.txt`, `caller-decompile.raw.txt` et
`signal-caller-decompile.raw.txt`.

Prochaine frontière : identifier statiquement le compteur ou le drapeau
primaire qui doit progresser pour atteindre le frontend.

## Gate fermé — compteur primaire et cible de comparaison

Le projet démo `ace-combat-6-demo` attribue l'objet propriétaire à
`0x822DA9C0 -> 0x822E40E8`, avec base `0x82934708`; l'objet de synchronisation
primaire est `owner+0x40 = 0x82934748`. Ses handles sont `0xE0000048` et
`0xE000004C`, et son payload `+0x10` est écrit par `0x822EEE10`.

Le callback `0x822E3EC0` incrémente `owner+0x08`, publie ce compteur dans le
payload primaire, signale `owner+0x60` (0040), puis réattend tant que
`owner+0x18` vaut zéro. Les boucles `0x822E4018` et `0x822E4080` comparent le
payload primaire à `owner+0x10` avant d'attendre 0048/004C.

La cible `owner+0x10` est maintenant attribuée : `0x821A3CEC` appelle
`0x822DA7F0(...,1)`, qui route l'objet `0x82934680` vers `0x822E52D0`;
celui-ci écrit le dernier argument moins un à `owner+0x98`, soit exactement
`0x82934718 = 0x82934708+0x10`. L'initialisation statique met donc la cible à
zéro. Aucun patch scheduler/événement/rendu ni runtime supplémentaire n'est
requis pour cette gate.

Preuves : `artifacts/primary-counter-static-gate/decision.md`,
`static-summary.txt`, `validation.txt`, `gate.status` et `marker-scan.txt`.

Prochaine frontière : qualifier le chemin guest qui doit quitter ce protocole
primaire pour atteindre le frontend; ne pas retester la provenance déjà fermée.

## Gate fermé — successeur guest et frontière service

La sortie de `0x822DA9C0` est suivie statiquement vers `0x822E5540`, le
`RenderContextDefault` `0x82934700`, puis la vtable `0x8202A488` et
`0x822F85B8`. La première branche locale est le résultat de `0x821BB4C8`
avant la suite d'initialisation du contexte de rendu.

La boucle principale `0x821A4808` réappelle les wrappers d'état, le timer
`0x821DEAC0` et son delta `0x821DECE8`. Le service `0x8238CDA0` est qualifié :
il pointe vers `0x82386CC0`, vtable RTTI `0x82008EF0`, slot `+0x2C` vers
`0x820FF988`, puis slot `+0x24` vers `0x820FF8D8`. Ce dernier dépend du
drapeau BSS `0x826E2374`; les pointeurs `0x82822F08` et `0x828819A4` restent
également non initialisés dans l'image statique.

Conclusion : la première frontière non qualifiable est bornée au résultat du
contexte de rendu et aux services BSS; aucun jalon frontend n'est encore
prouvé. Aucun runtime ou patch n'est requis pour cette gate.

Preuves : `artifacts/frontend-successor-static-gate/decision.md`,
`static-summary.txt`, `validation.txt`, `gate.status` et `marker-scan.txt`.

Prochaine frontière : retrouver statiquement les écrivains des trois champs
BSS; ne lancer une capture ciblée qu'après épuisement de cette slice.

## Gate fermé — écrivains BSS et services statiques

Les écrivains et types des trois dépendances BSS sont fermés dans le projet
`ace-combat-6-demo` / `Default.xex`. `0x826E2374` est écrit par `0x820FFCA0`
avant la vidange de la file du worker et lu par `0x820FF8D8`. `0x82822F08` est
construit par `0x82259FF8` depuis `0x821A3C30`; sa vtable address-point
`0x82013084` est RTTI `CAce6TaskManager@ACE6`, avec les slots appelés
`+0x04 -> 0x82259D10`, `+0x08 -> 0x82259DA8`, `+0x0C -> 0x82259E18`,
`+0x10 -> 0x82259E90` et `+0x18 -> 0x82259F58`. `0x82259D10` itère les
tâches et `0x82259F58` retire un nœud de file.

`0x828819A4` reçoit statiquement `0x823C0D90` à `0x823732DC`; l'objet porte
la vtable `0x82012C04`, RTTI `CLayeredDrawCallBack`, et ses consommateurs
qualifiés sont `0x821A30F0`, `0x821A4690` et `0x82266400`. Aucun de ces
chemins ne prouve encore un jalon frontend ou mission.

Preuves : `artifacts/bss-service-static-gate/decision.md`,
`static-summary.txt`, `validation.txt`, `gate.status`,
`correct-service-slots.raw.txt` et `marker-scan.txt`.

Prochaine frontière : qualifier le premier consommateur frontend après les
services task/layered; aucun runtime n'est justifié avant ce slice.

## Gate fermé — premier consommateur frontend

La transition statique `0x821929A8 -> 0x82190B18` choisit une fabrique de
tâche et transmet son résultat au slot d'insertion `0x82259E18` du gestionnaire
`0x82822F08`. La fabrique `0x82191468` alloue `0x78` octets et appelle
`0x8218A5F0`, qui pose la vtable `0x8200F01C`. Le RTTI de cette vtable est
`CModeTaskTitle`, avec les bases SWG et Ace6 qualifiées.

Le gestionnaire lie l'objet à une liste de vingt entrées, puis
`0x82259D10` le visite; pour le titre, `+0x28` est le poll PPC
`0x8216C940` et `+0x10` atteint `0x8218A7A8`. Le premier consommateur frontend
est donc fermé statiquement. La branche choisie par l'état courant du demo
reste une question distincte; aucun runtime ni patch natif n'est requis ici.

Preuves : `artifacts/frontend-consumer-static-gate/decision.md`,
`static-summary.txt`, `validation.txt`, `gate.status`, `marker-scan.txt` et
les sorties Ghidra ciblées.

Prochaine frontière : qualifier la provenance de la source compacte du clip
titre, selon `NEXT.md`.

## Gate fermé — provenance de la source compacte du clip

Dans `ace-combat-6-demo` / `Default.xex`, la chaîne
`0x82278F78 <- 0x8227A898 <- 0x8227AAC0` est bornée. Ses quatre appelants
amont (`0x82127D40`, `0x8219A060` et `0x821A1170`) fournissent des
descripteurs, tables et buffers génériques par `0x821EE0F8`,
`0x821A6808` et `0x821A6168`; aucun corps qualifié ne relie l’entrée au
constructeur ou à la ressource du clip titre. La provenance du pointeur source
est donc indécidable statiquement, sans nouveau runtime.

Preuves : `artifacts/compact-title-provenance-gate/decision.md`,
`static-summary.txt`, `upstream-slice.txt`, `validation.txt`, `gate.status`
et `marker-scan.txt`.

Prochaine frontière : décoder isolément la fenêtre de flux titre post-START;
ce gate est maintenant fermé par `artifacts/title-stream-parser-gate/`.

## Gate fermé — parser local du flux titre

Le CFG de `0x823246C0` et le parser local expliquent 23 appels `BOX_CASE0` et
13 événements `opcode=7` dans `0x2DCB2438..0x2DCB2680`. Le premier mot hors du
vocabulaire est `0x2DCB2448 = 0x1A`, consommé par le chemin par défaut. Une
ligne agrégée historique annonçait `0x2DCB243C`; la séquence détaillée la
réfute et l’anomalie est bornée dans le rapport de fermeture.

Preuves : `artifacts/title-stream-parser-gate/decision.md`,
`static-summary.txt`, `closure-check.txt`, `validation.txt`, `gate.status` et
`marker-scan.txt`.

Prochaine frontière : qualifier les producteurs de l’état `mode / mission /
level` lu par le film de titre.

## Gate fermé — absence de producteur natif après START

Le slice Ghidra des lecteurs et écrivains du triplet est épuisé. Le lecteur
mission `0x82095B80` et le lecteur niveau `0x820E9290` utilisent le sélecteur
de slot, les offsets `+0x6C4`/`+0x6D0` et le stride `0xAAB8`; aucun écrivain
entier de niveau n’est trouvé et le seul store entier direct de mission reste
`0x82171988`. Les écrivains du champ mode `base+0x78` sont connus mais ne
sont pas couplés à ce chemin dans la fenêtre.

Le probe codegen-on-b, avec START au tick 3000 et stores bornés à
`0x823C0000..0x823C5000`, atteint 3005 ticks. Aux ticks 3000–3004, l’index
reste 0, mission `0x823C2F14=2048` et niveau `0x823C2F20=4096`; aucun store
ne cible ces champs ni `base+0x78`. Le résultat ferme seulement l’absence de
producteur natif dans cette fenêtre ; il ne qualifie pas une source bytecode
ou VM.

Preuves : `artifacts/title-film-state-producers-gate/decision.md`,
`EVIDENCE.md`, `runtime-compact.txt`, `validation.txt`, `gate.status` et
`marker-scan.txt`.

Prochaine frontière : attribuer statiquement le producteur bytecode/VM du
film autour du mot hors vocabulaire `0x2DCB2448=0x1A`.

## Gate fermé — attribution bytecode/VM du film

Le slice statique ferme `0x823246C0` comme interpréteur consommateur : il
fetch/avance un PC dans le buffer, décode les opcodes `0..7` et dispatch vers
les slots de l’`ASContext`. `0x82278F78` remplit ce buffer par dépaquetage sur
un autre thread. Le corps de l’interpréteur ne référence pas `+0x6C4`,
`+0x6D0`, `+0x78`, ni les wrappers/getter/setter de l’état du titre.

Les producteurs qualifiés restent natifs : `0x820EA550`/`0x820EA598` lisent
le singleton via `0x82095B80`/`0x820E9290`, et `0x82171988` écrit la mission.
La VM n’est donc pas attribuée comme producteur du triplet. Détails :
`artifacts/title-bytecode-vm-static-gate/`.

Prochaine frontière : écrivains natifs hors fenêtre START, en particulier le
champ niveau `base + index * 0xAAB8 + 0x6D0`.

## Gate fermé — écrivains explicites du niveau

Le scan PPC de `+0x6D0` borne cinq stores `stfs`. Les cinq corps construisent
leur base d’écriture dans des zones globales distinctes et ne référencent ni
`0x823C27E0`, ni le sélecteur du slot, ni le stride `0xAAB8`. Les trois chemins
qui passent un pointeur `+0x6D0` à `0x821F1500` ne produisent pas un store : le
callee lit seulement des métadonnées du pointeur.

Aucun écrivain explicite du niveau du slot titre n’est donc attribué. Détails :
`artifacts/title-level-writer-static-gate/`.

Prochaine frontière : construction, publication et initialisation groupée du
singleton titre `0x823C27E0` et de son sous-objet `+0x70`.
## 2026-08-21 — dispatch vtable `+0x0C`

Le rattachement statique d'un appel de slot `+0x0C` à l'instance de titre est limité : 289 dispatches partagent cet offset. Aucun runtime n'a été lancé.
## 2026-08-21 — producteur du contexte titre

`Function_820D29E0` transmet son second paramètre inchangé à `FUN_82324188`; ce paramètre provient d'un dispatch virtuel non résolu statiquement. Aucun runtime n'a été lancé.
## 2026-08-21 — frontière XMA écartée

La garde XMA contextualisée ne modifie ni le scheduler bloqué ni le frontend. Le prochain producteur qualifié à examiner est `RB_COPY` vers EDRAM/surface ; aucun runtime supplémentaire n'est requis à ce stade.
## 2026-08-21 — contrat `RB_COPY` fermé

Le draw RT0 guest, `RB_COPY`, le readback Vulkan et la matérialisation EDRAM forment une chaîne qualifiée. Le premier contenu non noir du draw normal manque encore ; l'analyse bascule sur ce writer réel.
## 2026-08-21 — sélection RT0 manquante

Le frontend natif prépare shaders et mémoire partagée, mais ne renseigne jamais `normal_draw_command_`. Le premier writer RT0 est donc inatteignable avant EDRAM ; le prochain changement minimal est sa sélection depuis le lot de commandes qualifié.

## 2026-08-21 — provenance du bootstrap IB indécidable

Le watcher historique d'IB ne couvre pas le heap `0x16AE...`. Le watcher
générique a été appliqué à `[0x16AE0980, 0x16AE0A40)` pendant un tick sur un
build contenant le guest. Aucun store n'est observé, mais aucune soumission
d'IB ne l'est non plus ; le résultat ne confirme ni ne réfute un producteur
guest. Le gate est limité après cinq lots.

## 2026-08-21 — première soumission IB qualifiée

Le hook partagé existant capture `0x16AE0980`, 48 dwords, au tick 0 sur deux
builds guest. Les générations du premier et du dernier mot sont nulles et le
watcher générique ne voit aucun store. La session établit que la zone heap
n'existe pas avant l'entrée guest. L'hypothèse initiale d'une écriture générée
hors couverture est ensuite réfutée par le routage `AC6_PPC_STORE_*` ; le
contenu courant de la capture doit être requalifié. Le gate est limité après
cinq lots.

## 2026-08-21 — routage des stores générés fermé

Les stores PPC générés scalaires et VMX passent déjà par les cinq adaptateurs
`AC6_PPC_STORE_*`, puis par `GuestMemory::store_*`. La fenêtre configurable
est donc active sur leur point partagé. L'hypothèse d'un hook généré séparé à
étendre est réfutée et aucun code n'est ajouté. L'identité des 48 dwords de la
capture courante reste à qualifier.

## 2026-08-21 — mapping du bootstrap IB qualifié

Un runtime GDB borné ordonne `map_zero(0x16ADF000, 8192)` avant la capture de
`0x16AE0980`, 48 dwords, au tick 0. La région est donc engagée dans ce mapping.
Le payload courant reste indécidable : l'appel de capture est inline et GDB ne
fournit pas la valeur de retour de `load_u32` dans ce build optimisé. Le gate
est limité après cinq lots.


## 2026-08-21 — payload du bootstrap IB qualifié

Le premier tick soumet l’IB `0x16AE0980` avec 48 mots exactement composés de
24 paires `C0003600,00010081`. L’opcode `0x36` est
`PM4_DRAW_INDX_2`; l’IB est donc une séquence réelle de 24 draws PointList,
ni un buffer nul ni un payload opaque. L’ancienne attente `0x2D` est réfutée.
# Checkpoint — producteur `0xE0000040` qualifié

Le producteur exact de l'événement attendu par le thread 12 est le callback du
thread 2 `0x822E5660`, via `0x822E3EB8` puis le wrapper `0x821A6AB0`. Le handle
est chargé depuis `0x82934760`. La frontière active est désormais
l'enregistrement et l'ordonnancement de ce callback à `LR=0x821C5178`.

## 2026-08-21 — framebuffer du tick 5800 qualifié

Les 5 692 présentations comptées sont des notifications `VdSwap`, pas des
présentations du renderer. Le rapport contient 24 draws mais aucune soumission
de ring et aucune commande `present` typée. Sans cette commande, le resolve,
le writeback guest et le screencap sont inaccessibles : aucun framebuffer
rendu ne peut encore être qualifié comme noir ou visible.

## 2026-08-21 — contrat producteur de `present` identifié

Le parseur `XE_SWAP` et le consommateur renderer existent déjà. `VdSwap`
construit également le paquet qualifié dans le buffer système guest. La rupture
est entre ce buffer et le point de soumission Xenos : aucune publication ne le
fait traverser au processeur dans le corridor courant.

## 2026-08-21 — raccord `VdSwap` implémenté, runtime indécidable

Le buffer système traverse désormais `apply_xenos_typed_batch` par une API
bornée qui rejette toute consommation partielle. Le test ciblé observe une
commande `present` exacte et passe. La compilation complète passe ; la suite
globale reste à 25/26. Le probe isolé n'a pas produit de résumé exploitable
avant la limite du gate, donc la validation runtime reste ouverte.

## 2026-08-21 — soumission directe de `VdSwap` réfutée et retirée

Le rapport existait : 171 ticks, 88 appels `VdSwap`, 88 commandes `present`,
puis rejet du lot pour présentations multiples. Cela contredit la route
qualifiée antérieure, où de nombreux swaps correspondaient à un seul paquet
Xenos. Le raccord direct et son test ont été retirés ; build et test core
repassent. La frontière correcte est le writer WPTR/IB réel.

## 2026-08-21 — fausse porte WPTR `0x827AD2F0` fermée

`0x821B9BC8` publie le WPTR du bootstrap, mais son chemin post-`KickOff`
n'est pas la publication normale. `0x827AD2F0` et `device+21508` appartiennent
aux compteurs de performance Microsoft ; les forcer serait incorrect. La
publication post-bootstrap doit être cherchée dans les callbacks D3D.

## 2026-08-21 — vérificateur callback D3D limité par provenance

L'image du build courant contient les 33 ancres, 8 fonctions et 5 producteurs
attendus. Le vérificateur ne signale aucune divergence structurelle ; son seul
échec est une garde d'identité interne non utilisable comme garde de révision
ordinaire. La provenance directe de l'image doit être réconciliée.

## 2026-08-21 — provenance de l'image callback D3D fermée

Le `xex-basefile.bin` courant est produit directement depuis le `Default.xex`
de la démo PAL par `build_demo.py`, avec le manifest Ghidra canonique
`ace-combat-6-demo/Default.xex`. Les images plates PAL historiques conservées
sont directement identiques octet par octet à l'image courante. Les 33 ancres,
8 fonctions et 5 producteurs sont donc qualifiés ; la garde d'identité interne
du vérificateur est obsolète et ne constitue plus un blocker.

Le prochain gate suit statiquement `KickOff` jusqu'au callback qui publie le
travail ou réveille le consommateur. Aucun shim ni runtime avant d'avoir
identifié ce contrat.

## 2026-08-21 — callback post-KickOff réfuté comme publisher

Le ring produit des commandes mais son consommateur reste immobile. Les
interruptions `PM4_INTERRUPT` et leurs callbacks se trouvent en aval de
l'avance du ring : elles ne peuvent pas produire le kick qui permet leur propre
livraison. `0x821B9710 → 0x821C5190` consomme l'interruption et ne publie pas le
WPTR. `0x821C4A60 StartWorkerQueue` est également transporté par le ring bloqué.

La frontière active redevient donc le passage du curseur produit par
`0x821C57D0` au store WPTR MMIO de `0x821B9BC8`. Ne pas réintroduire la fausse
porte des compteurs ni une soumission dans `VdSwap`.

## 2026-08-21 — jointure `0x821C57D0 → WPTR` réfutée

Les preuves historiques corrigées montrent que `0x821C57D0` modifie un IB déjà
référencé sans faire croître le ring primaire. La file guest surveillée n'a pas
non plus de consommateur bloqué : `0x820FF710` et `0x820FFCA0` exécutent leur
danse producteur/consommateur chaque tick. L'ancienne métrique n'observait que
l'état remis à zéro en fin de tick.

La panne discriminante est plus en amont : les slots de payload de 96 octets
restent nuls. La prochaine frontière est donc le premier writer non nul de ces
slots, pas un nouveau kick WPTR.

## 2026-08-21 — producteur de payload limité par xrefs dynamiques

Le corps Ghidra qualifié de `0x820FF710` prend seulement le pointeur de file,
écrit le littéral zéro dans `slot[index]+64`, incrémente l'index et signale la
synchronisation. Aucun argument ni champ source ne fournit un payload.

Les exports structurés actuels ne donnent aucune arête entrante ni référence
directe vers cette fonction et ne recensent pas exhaustivement les stores
adressés par `base + index*0x60`. Le prochain test est un slice P-code Ghidra
borné de `base+0x110`, stride `0x60`, champ `+0x40`.

## 2026-08-21 — balayage des déplacements immédiats limité

Le programme Ghidra canonique ne contient aucun accès mémoire PPC D-form avec
le déplacement immédiat `0x110`, alors que `0x820FF710` reconstruit cette
adresse dans le C décompilé. L'adresse est donc formée par registres et le
balayage des seuls déplacements ne peut pas recenser les écrivains du slot.

La prochaine reprise doit normaliser les expressions d'adresse du P-code haut.
Aucun runtime ni payload synthétique n'est justifié.

## 2026-08-21 — première passe P-code indécidable

`FindScaledStoreWriters.java` normalise récursivement les entrées des `STORE`
et impose `0x820FF710` comme témoin. Ghidra démarre le script, mais le premier
lancement se termine autour de 30 secondes avant fermeture du fichier de
résultat. Aucun checkpoint n'a donc été persisté.

Le prochain lancement doit vider le fichier après calibration et pendant le
balayage, puis être attendu comme processus long.

## 2026-08-21 — famille des writers de payload fermée

Le processus Ghidra original a finalement terminé. La passe P-code retrouve
le témoin type `0` puis quatre writers directs qui posent les sélecteurs
`1`, `2`, `3`, `4`. La vtable canonique confirme leurs slots contigus
`+0x10..+0x1C`.

`0x82117410` appelle sans garde locale les writers types `1` et `4`. Le zéro
observé dans la file ne vient donc pas d'un writer incomplet : cette fonction
n'est pas atteinte dans le corridor courant. La prochaine frontière est son
appelant et sa condition d'activation.

## 2026-08-21 — appelant types 1/4 réduit à un site

Ghidra trouve une seule référence vers `0x82117410` : appel direct
`0x8210A1C0:0x8210ADB4`. Le callsite est au fond d'une chaîne de gardes dans le
C décompilé. La fenêtre textuelle ne suffit pas à identifier honnêtement la
condition dominante.

Prochaine passe : CFG et dépendances de contrôle du bloc `0x8210ADB4`, puis
slice du premier champ mémoire testé.

## 2026-08-21 — contrôle `0x8210ADB4` exporté

Le CFG haut de `0x8210A1C0` contient 502 blocs. Dix-huit choix de branche sont
nécessaires pour atteindre le producteur : dispatch initial sur 16 bits, puis
cinq paires de validation lookup/champ `+0x10C`.

La fonction a un seul appelant, `0x82165CC0:0x82165D8C`. Les paramètres fournis
à ce site sont maintenant la frontière statique utile ; le getter global à
33 appelants n'est pas discriminant.

## Gate 2026-08-21 — arguments producteur render queue

- Gate `render-queue-producer-arguments-gate` fermé statiquement.
- `0x82165CC0:0x82165D8C` appelle toujours `0x8210A1C0` avec `uVar2`, résultat du slot vtable `+4`, puis paramètres appelant 5 à 8.
- Dans `0x8210A1C0`, paramètre 4 bas 16 bits sélectionne type. Route vers `0x82117410` exige type `1`.
- Cinq ressources sont ensuite résolues par `0x821080D0`; chaque index `-1` ou champ d’enregistrement `+0x10C == 0` évite appel.
- Aucun contrat natif ajouté: offsets payload et origine des cinq clés restent à qualifier.

## Gate 2026-08-21 — layout payload type 1

- Gate `render-queue-type1-payload-layout-gate` fermé statiquement.
- Payload commence par cinq chaînes consécutives `int16 length; byte[length]`.
- Après cinquième chaîne: groupe optionnel de 3 drapeaux, puis groupe optionnel de 2 drapeaux.
- `0x8231BCB0` est producteur observé du mot ressource `+0x10C`; `0x820CBAB0` possède chemin de remise à zéro byte pour kinds 6/7.
- Aucun changement natif: RHS exact du producteur non nul et ses appelants restent à slicer.

## Correction 2026-08-21 — faux producteur +0x10C

- Gate `resource-10c-producer-slice-gate` limité après cinq batches.
- Hypothèse `0x8231BCB0` producteur du record render `+0x10C` réfutée: fonction parse RIFF/WAVE sans argument et écrit structure globale retournée par `FUN_823270DC`.
- Même déplacement `+0x10C` est collision de layout; aucun alias vers table `DAT_826F6124` prouvé.
- Scan exhaustif brut non exécuté: launcher `analyzeHeadless` absent environnement courant.
- Aucun changement natif.

## Gate 2026-08-21 — inventaire stores +0x10C

- Gate `record-10c-store-enumeration-gate` limité par quota après inventaire canonique.
- Launcher qualifié retrouvé sous `.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless`; projet `ace-combat-6-demo`, programme `Default.xex`.
- Scan exact sur `0x82000000..0x83000000`: 101 instructions store D-form au déplacement `0x010C`.
- Famille forte: `0x8210D950` écrit `param2` à objet `+0x10C`; `0x8210D9C0` et `0x8210DA80` consomment/libèrent/remettent zéro; `0x8210DB10` initialise champ à zéro.
- Alias de cet objet vers records de `DAT_826F6124` reste non prouvé. Aucun changement natif.

## Gate 2026-08-21 — alias record ressource

- Gate `resource-record-alias-gate` limité par quota avec jointure presque complète.
- `0x8210D8A0` ne retourne pas record: il écrit type `.nud/.nut` à `param1+0x110`.
- `0x8210DD70` reçoit manager en `param1`, alloue record de taille `0x428`, l’initialise, le stocke à `manager+(index+2)*4`, puis appelle `0x8210D950`.
- Consumer `0x8210A1C0` utilise même formule `DAT_826F6124+(index+2)*4`.
- Le callsite `0x82108A98` charge `DAT_826F6124`, le passe en `param1`, puis appelle `0x8210DD70`. Aucun changement natif.

## Gate 2026-08-21 — base manager ressource

- Gate `resource-manager-base-identity-gate` fermé par réfutation de la décompilation.
- `0x82327100` est helper de sauvegarde `r26..r31/LR`; il préserve `r3` et ne retourne aucun manager.
- `0x8210DD70` copie le `r3` entrant vers `r31` à `0x8210DD88`.
- `0x82108A6C` charge `DAT_826F6124`; `0x82108A8C` le place en `r3`; `0x82108A98` appelle `0x8210DD70`.
- Ce chemin producteur écrit donc dans la même table que le consumer `0x8210A1C0`. Aucun changement natif.

## Gate 2026-08-21 — contrat natif d’enregistrement ressource

- Gate `native-resource-registration-contract-gate` limité après cinq batches.
- Sources maintenues et build hôte courant ne contiennent pas les adresses PPC ciblées; build courant a codegen désactivé.
- Sorties guest qualifiées existent sous `build-codegen-on/codegen/`, dont objet guest et manifeste.
- Dernière inspection a été interrompue par `nm | head` sous `pipefail` avant rapport compact; couverture des fonctions reste indécidable.
- Aucun code modifié dans reconstruction native; seul état durable mis à jour.

## Reprise limitée 2026-08-21 — couverture guest ressource

- Objet guest qualifié contient symboles pour `0x82108918`, `0x8210DD70`, `0x8210D950` et `0x8210A1C0`.
- Manifeste annonce 13 071 fonctions confirmées, 53 unités compilées, zéro diagnostic et zéro instruction unsupported.
- Parcours relocations depuis quatre racines a désassemblé 223 fonctions: 67, 61, 3 et 175 fonctions visitées respectivement.
- Jointure imports échoue car 238 entrées du manifeste n’utilisent aucune clé d’adresse supposée par analyse (`address`, `thunk_address`, `guest_address`).
- Gate encore limité; aucun runtime et aucun changement natif.

## Gate 2026-08-21 — jointure imports natifs

- Gate `native-import-schema-join-resume-gate` fermé par réfutation utile.
- Le callgraph sauvegardé est une table directe `caller -> [callee]`; la carte qualifiée contient 228 adresses de thunks.
- Les quatre closures visitent respectivement 67, 61, 3 et 175 fonctions, sans atteindre un seul thunk d'import.
- Aucun handler xboxkrnl/XAM/XMA direct n'est donc le contrat manquant de cette chaîne; prochaine frontière: dispatch indirect guest.
- Aucun runtime et aucun changement natif.

## Gate fermé 2026-08-21 — chaîne statique global/vtable

- Chaîne qualifiée: `0x82386C10 -> objet 0x82386C0C -> vtable 0x82008E50`.
- Slot 1 cible `0x820FEED8`; slot 13 cible `0x820FEF70`.
- Les deux cibles figurent exactement une fois dans le manifeste codegen.
- La famille de trois dispatchs de `0x821075A0` est réfutée comme contrat natif manquant. Aucun runtime, aucun changement natif.

## Gate limité 2026-08-21 — prochaine famille indirecte

- `0x82107870` et `0x82107A00` réutilisent la vtable déjà résolue; éliminées.
- Premier receiver nouveau: retour de `0x82220670`, passé à `0x821154C0` slot 20 (`+0x50`).
- Branche sœur `0x82115530`: slot 21 (`+0x54`).
- Décompilation producteur sauvegardée mais résumé final absent; type/vtable encore indécidable. Aucun runtime, aucun changement natif.

## Gate limité 2026-08-21 — dispatch indirect ressource

- Dix-neuf fonctions de la closure utilisent le helper indirect; neuf sont à un niveau de `0x8210A1C0`.
- Premier candidat `0x821075A0`: trois sites, LR `0x821075E4`, `0x821075FC`, `0x82107614`.
- Le helper reçoit la cible depuis le contexte `+0x108`; ces LR ne sont pas dans la table de slots qualifiée mais utilisent encore la résolution générique.
- Objet, slot et ensemble de cibles restent indécidables après cinq batches. Aucun runtime, aucun changement natif.

## Gate limité 2026-08-21 — slice dispatch `0x821075A0`

- Décompilation qualifiée dans `ace-combat-6-demo` / `Default.xex`.
- Objet commun des trois appels: `PTR_PTR_82386C10`.
- LR `0x821075E4` et `0x821075FC`: slot 1 (`+0x04`).
- LR `0x82107614`: slot 13 (`+0x34`).
- Cibles concrètes encore indécidables; prochain slice sur producteurs du global. Aucun runtime, aucun changement natif.

## Gate limité 2026-08-21 — global/vtable `0x82386C10`

- 24 xrefs directs qualifiés, tous en lecture; aucun store direct référencé.
- Le script de slot existant attend un receiver contenu dans l'objet global, mais ici le global est déjà le receiver; son résultat vide n'est pas probant.
- Prochaine preuve: lecture big-endian directe `global -> objet -> vtable -> slots +0x04/+0x34`.
- Aucun runtime et aucun changement natif.
## Gate resource-slot20-producer-resume limité — 2026-08-21

`0x82220670` est un lookup de tableau : il retourne une entrée, remplacée par
son alias `+0x18C` lorsqu'il existe. Ce retour alimente les dispatchs slots 20
et 21 de `0x821154C0`/`0x82115530`. L'export PPC canonique n'a pas été produit :
`Ac6XenonWords.java` refuse les arguments préfixés par `0x`. Reprendre sans
préfixe dans un nouveau gate. Détails :
`artifacts/resource-slot20-producer-resume-gate/BLOCKER.md`.
## Gate resource-slot20-ppc-abi limité — 2026-08-21

L'ABI Xenon est maintenant prouvée : `r3` porte le conteneur global `+0x308`,
`r4` l'identifiant payload éventuellement remappé, et le retour devient le
receiver des dispatchs slots `+0x50/+0x54`. L'ensemble des types d'entrée ou
d'alias `+0x18C` reste à borner par leurs producteurs statiques. Détails :
`artifacts/resource-slot20-ppc-abi-gate/BLOCKER.md`.
## Gate resource-slot20-type-producers limité — 2026-08-21

Deux routes d'alias sont prouvées : `0x8210A1C0` copie des objets
`DAT_826F6124 record+0x10C` dans `entrée+0x18C`; `0x82220550` y écrit le retour
du sélecteur à deux niveaux `0x821E1D80`. Les callgraphs sauvegardés ne couvrent
aucun appelant du setter. Reprendre avec des xrefs Ghidra directs. Détails :
`artifacts/resource-slot20-type-producers-gate/BLOCKER.md`.
## Gate resource-slot20-setter-xrefs limité — 2026-08-21

Le setter `0x82220550` n'a qu'un appel direct : `0x82216614` dans
`0x82216498`. Il reçoit exactement le conteneur global `+0x308` et un indice
actif normalisé, puis `0x82220750` reçoit le même conteneur avec la valeur `1`.
Il reste à résoudre les trois arguments PPC du sélecteur `0x821E1D80` pour
borner les vtables d'alias. Détails :
`artifacts/resource-slot20-setter-xrefs-gate/BLOCKER.md`.

### Gate limité : table de sélection des slots 20/21

Les arguments de `0x821E1D80` sont fermés : table globale à `+0x29698`, indices
issus des octets `+0x57/+0x58`, résultat écrit à `entry+0x18C`. La recherche de
producteurs par constantes est trop large et ne borne pas les vtables. Voir
`artifacts/resource-slot20-alias-selector-args-gate/BLOCKER.md` pour le prochain
test exact `global load + 0x29698 + lwzx/stwx`.

### Gate limité : accès indexés exacts à `+0x29698`

Le classifieur PPC recense 120 accès indexés exacts dans 114 fonctions, dont le
consommateur `0x82220640` et au moins deux écritures candidates. Le décalage seul
ne qualifie pas le bon objet. La prochaine branche doit propager localement le
registre chargé depuis `0x823C27E0` et ne retenir que les `lwzx/stwx` utilisant
cet alias comme base. Voir le blocker du gate.

### Gate limité : provenance CFG de la base globale

Deux stores `+0x29698` sont compris : le constructeur `0x821E0B7C` initialise
le champ de son objet avec l'adresse du sous-objet adjacent; `0x82212E54` écrit
dans le champ du singleton global depuis `param1+0x2E4` ou `param1+0x2E8`.
La tentative de classement linéaire des 120 sites est invalide et ne doit pas
être citée. Une propagation minimale par basic blocks est le prochain test.

### Gate limité : classifieur SSA du champ global

Un classifieur high-p-code minimal a été ajouté pour rechercher conjointement
`0x823C27E0` et `0x29698` dans les adresses LOAD/STORE. Son premier lancement
n'a traité aucune fonction : les adresses sans préfixe ont été données à
`Long.decode`. Le résultat vide est invalide. La prochaine branche commence par
la correction hexadécimale d'une ligne et relance le même test calibré.

### Gate limité : sur-approximation du classifieur SSA

Le classifieur SSA fonctionne et calibre correctement les deux accès globaux
connus tout en excluant le constructeur. Son inventaire de 1119 sites est
cependant trop large à cause des expressions `MULTIEQUAL` transitives. La
prochaine branche doit appliquer l'ensemble exact des 120 PC PPC comme
allowlist avant toute inspection SSA.
## 2026-08-21 — allowlist SSA `+0x29698` limitée

- Observé: 120 PC exacts classifiés; 117 qualifiés (116 loads, un store), deux
  rejetés, un orphelin `0x82210188` sans fonction Ghidra; zéro échec de
  décompilation.
- Observé: `0x82220640` et `0x82212E54` sont qualifiés; `0x821E0B7C` est rejeté.
- Inférence: l'inventaire SSA ne surcompte plus les opérations voisines, mais
  l'orphelin exige une preuve PPC brute bornée.
- Gate non fermé après cinq batches; voir
  `artifacts/resource-slot20-global-base-ssa-allowlist-gate/BLOCKER.md`.
## 2026-08-21 — orphelin `0x82210188` qualifié

- Observé: `0x82210178..0x82210188` construit `0x29698`, charge
  `*(0x823C27E0)`, puis exécute `lwzx r3,r11,r10`.
- Conclusion: `0x82210188` lit exactement
  `*( *(0x823C27E0) + 0x29698 )`.
- Inventaire final: 118 accès qualifiés, deux rejets, zéro inconnu.
- Gate fermé sans runtime ni shim; voir
  `artifacts/resource-slot20-orphan-82210188-gate/RESULT.md`.
## 2026-08-21 — appels du writer `global+0x29698`

- Observé: `0x82212DF0` a 15 appels directs répartis dans 9 fonctions.
- Observé: son premier argument est l'objet d'état courant; le second choisit
  `object+0x2E4` ou `object+0x2E8`.
- Observé: à `0x82216498`, le PPC conserve l'entrée `r3` dans `r30` avant
  l'appel; la signature Ghidra sans paramètre est erronée.
- Inférence: les producteurs utiles sont les stores des deux champs, pas les
  15 consommateurs du setter global.
- Gate limité après cinq batches; voir
  `artifacts/resource-slot20-table-object-producer-gate/BLOCKER.md`.
## 2026-08-21 — producteurs `+0x2E4/+0x2E8` qualifiés

- Observé: 34 stores bruts; trois paires pertinentes dans `0x82095E98`,
  `0x82174A80`, `0x82176B60`, plus l'initialisation zéro `0x82213628`.
- Observé: les paires pointent vers des conteneurs embarqués et tous les
  producteurs les peuplent via `0x820A4F58`.
- Observé: `0x820A4F58` écrit le tableau de pointeurs à `entry+0xD8` et sa
  borne à `entry+0xDC`, contrat consommé par `0x821E1D80`.
- Inférence: le receiver des slots 20/21 est l'objet `piVar16` du populator.
- Gate limité; voir
  `artifacts/resource-slot20-object-field-stores-gate/BLOCKER.md`.
## 2026-08-21 — factory des receivers localisée

- Observé: les objets `piVar16` publiés dans `entry+0xD8` viennent uniquement
  de `container->slot5`, à `vtable+0x14`.
- Observé: `r3` de `0x820A4F58` est le conteneur; ses six variantes embarquées
  convergent vers la vtable finale `0x82000B94`.
- Observé: les constructeurs propriétaires sont `0x82094CD0`, `0x82174888`
  et `0x82176930`; chacun passe par `0x820A3AF0` puis pose la même vtable.
- Gate limité avant lecture de l'entrée `0x82000BA8`; voir
  `artifacts/resource-slot20-pivar16-producer-gate/BLOCKER.md`.
## 2026-08-21 — slots receiver 20/21 résolus

- Observé: `0x82000BA8` cible `0x82093658`, qui retourne le conteneur entrant
  et restaure sa vtable finale `0x82000B94`; les kinds ne sélectionnent pas
  une famille de receivers distincte ici.
- Observé: les slots `+0x50/+0x54` ciblent respectivement `0x8220E428`
  (retour constant `0`) et `0x82211040` (lecture/différence des floats aux
  offsets `+0x70/+0x74`).
- Observé: aucune des deux adresses n'est présente textuellement dans l'arbre
  natif actuel.
- Gate fermé; voir
  `artifacts/resource-slot20-container-factory-vtable-gate/RESULT.md`.
## 2026-08-21 — couverture codegen non encore qualifiée

- Observé: le C++ XenonRecomp spécifique au jeu est un produit build-only;
  son absence de l'arbre maintenu ne prouve pas une lacune.
- Observé: aucune forme textuelle des starts `0x8220E428/0x82211040`
  n'apparaît dans le checkout actuel, produits de build compris.
- Incertitude: le manifeste structuré peut les représenter sans adresse dans
  le nom généré.
- Gate limité après cinq batches; aucun code modifié. Voir
  `artifacts/resource-slot20-codegen-coverage-gate/BLOCKER.md`.
## 2026-08-21 — hypothèse de trou codegen réfutée

- Observé: l'ensemble effectif contient 13 071 starts, dont 4 745 chunks
  qualifiés hors `.pdata`.
- Observé: `0x8220E428` (8 octets) et `0x82211040` (36 octets) sont tous deux
  dans cet ensemble.
- Observé: les deux figurent dans `ppc_func_mapping.cpp`; le C++ généré
  contient aussi des callsites directs vers `sub_8220E428`.
- Conclusion: aucun stub ni changement de configuration n'est requis pour
  ces slots. Gate fermé et hypothèse réfutée; voir
  `artifacts/resource-slot20-codegen-manifest-gate/RESULT.md`.
## 2026-08-21 — frontière courante réalignée sur cycle 1761

- Observé: les routes fraîches atteignent 5 463 PRESENT puis 23 threads
  bloqués/0 runnable, sans frontend/mission/terminal.
- Observé: chacune possède un unique load64 post-reprise, PC `0x82327154`,
  adresse `0x7F0409D8`, valeur zéro.
- Observé: les traces divergent mais les états outcome/milestones/graphics/
  scheduler rapportés sont identiques.
- Réfuté comme premier levier: `E000004C` est déjà joint set→wake→activation
  scheduler sur 351/351 paires par route.
- Gate limité avant extraction de la première divergence persistante; voir
  `artifacts/first-real-native-boundary-gate/BLOCKER.md`.
## 2026-08-21 — aucune divergence guest dans les traces cycle 1761

- Observé: chaque trace contient exactement 22 153 événements alignables.
- Observé: seules deux lignes divergent après tick 1, toutes deux dans le
  domaine `input`; la première est `buttons=0/16` au tick 252.
- Observé: zéro divergence non-input et zéro différence non-input persistante.
- Conclusion: la capsule ne contient aucun store guest, PM4/draw, scheduler,
  renderer ou readback divergent à slicer. Gate fermé; voir
  `artifacts/post-resume-first-divergence-gate/RESULT.md`.
## 2026-08-21 — fenêtre START minimale retrouvée dans les captures existantes

- Observé: la chaîne START publie `0x10` à `0x829D1550`, normalise `0x400` à
  `0x827B37E0`, puis écrit le bit logique `0x10` à `0x82798488` avant remise
  à zéro au tick 253.
- Observé: au tick 268, le store START-only est
  `0x820CDC20 → [0x2E3D3C0C] = 0`.
- Observé: la route neutre possède à la place la séquence
  `0x823255F0/0x82325644 → [0x2E3D44F0] = 0/0xFFFFFFFF`.
- Conclusion: aucun nouveau runtime requis; slicer statiquement ces trois PC.
  Gate fermé; voir
  `artifacts/start-minimal-observation-window-gate/RESULT.md`.
## 2026-08-21 — divergences tick268 reclassées en bruit de pile

- Observé: les valeurs rapportées comme `pc` sont les LR transmis au hook de
  stores, et non les instructions de store.
- Observé: `0x820CDC20` suit l'appel de `0x82321E20` puis force `r3=1`;
  `0x823255F0` suit le helper de sauvegarde de registres `0x8232710C`;
  `0x82325644` suit un appel virtuel et exécute une comparaison.
- Conclusion: les deltas à `0x2E3D3C0C/0x2E3D44F0` sont des différences de
  pile/cadre d'appel, sans consommateur persistant, tâche ou rendu qualifié.
- Gate fermé et hypothèse sémantique réfutée; voir
  `artifacts/tick268-divergent-pc-static-slice-gate/RESULT.md`.
## 2026-08-21 — aucun store START persistant promu

- Observé: `0x829D1550`, `0x827B37E0` et `0x82798488` forment une chaîne
  transitoire; le dernier bit logique est remis à zéro au tick 253.
- Observé: l'A/B tick268 ne couvre que `0x2E3C0000..0x2E3F0000` et ne peut
  pas qualifier les écritures dérivées de ces globals.
- Conclusion: aucun store persistant hors pile n'est démontré; il faut résoudre
  statiquement tous les lecteurs de `0x82798488`.
- Gate limité; voir
  `artifacts/start-first-persistent-store-static-gate/BLOCKER.md`.
## 2026-08-21 — effet durable START résolu mais propriétaire non atteint

- Observé: les consommateurs typés de `0x82798488` sont `0x82170FCC`
  (demo) et `0x82185210` (mission/title).
- Observé: le premier effet demo est `0x82171128`, qui écrit `1` à
  `[0x827435F8]+0x18`.
- Observé: les fonctions propriétaires `0x82170F58` et `0x82185198` ne
  sont atteintes dans aucune des deux routes bornées.
- Conclusion: le verrou est la construction/publication/dispatch de la tâche,
  pas le shim d'entrée. Gate fermé; voir
  `artifacts/start-logical-bit-reader-xrefs-gate/RESULT.md`.
## 2026-08-21 — consommateurs START qualifiés hors phase bootstrap

- Observé: les vtables consommateurs sont `0x8200C904`
  (`CModeTaskDemoBase`) et `0x8200E5C4` (`CModeTaskMissionTitle`).
- Observé: `0x82259D10` ne dispatche que startup-demo, loading et
  mode-manager; aucune mutation de liste n'apparaît après le tick 221.
- Observé: `CTaskLoading` atteint plus tard l'état 1, sans transition
  sortante dans sa propre update; l'état 2 de requête de mode n'est pas vu.
- Conclusion: le verrou est le contrat de terminaison/retrait de la tâche
  loading, pas une factory menu manquante. Gate fermé; voir
  `artifacts/start-consumer-owner-factory-gate/RESULT.md`.
## 2026-08-21 — état 1 de CTaskLoading sans transition interne

- Observé: la branche état 1 `0x8217E42C..0x8217E440` appelle seulement le
  slot `+0x20` du sous-objet `this+0x1C`, puis quitte sans écrire l'état.
- Observé: seule la branche état 2 décrémente `this+0x44` et peut écrire la
  requête `1` à `[0x827435F8]+0x18`.
- Incertitude: le contrat exact entre le thunk `0x8218CE20`, le dispatcher
  et les mutations de liste reste à extraire du listing déjà capturé.
- Gate limité; voir
  `artifacts/loading-task-retirement-contract-gate/BLOCKER.md`.
## 2026-08-21 — retrait par retour de CTaskLoading réfuté

- Observé: `0x82259D58` teste seulement le retour du slot `+0x28` pour
  décider d'appeler l'update.
- Observé: après le slot 4 à `0x82259D70`, `0x82259D74` avance au nœud
  suivant sans consommer `r3`.
- Observé: `0x82259E18` est une insertion/activation et `0x82259FF8` un
  initialiseur; aucune fonction ne retire une tâche.
- Conclusion: l'état 1 attend un writer externe/asynchrone ou un autre
  producteur de transition. Gate fermé; voir
  `artifacts/loading-task-dispatcher-cfg-gate/RESULT.md`.

## Loading vtable identity gate — CLOSED

- Observé : `CTaskLoading` utilise la vtable `0x8200F388`, installée par le
  constructeur `0x8218BF18` au store `0x8218BF30`.
- Observé : le constructeur initialise `this+0x0A=1` et `this+0x0C=0`.
- Observé : le vrai slot 4 est `0x8218CE20`; il teste `this+0x0A`, puis appelle
  `0x8218CCD0` seulement lorsque ce byte vaut zéro.
- Correction : `0x8217E3E0` n'est pas l'update propre de `CTaskLoading`; cette
  fonction partagée appelle `0x8218CCD0` depuis `0x8217E458`.
- Artefact : `artifacts/loading-vtable-identity-gate/RESULT.md`.

## CTaskLoading field-transition contracts — CLOSED

- Observé : `0x8218CBD8` arme `CTaskLoading` avec le triplet
  `(+0x09,+0x0A,+0x0C)=(1,0,1)` après création réussie d'une ressource.
- Observé : `0x8218CCD0` ne modifie pas l'état tant que `0x8219F5D0` renvoie
  zéro; sur statut non nul, il écrit `(0,1,0)` et propage le signe du statut.
- Observé : `0x8218CD78` remet également le triplet à `(0,1,0)` sur son chemin
  terminal.
- Réfuté : aucun callback externe n'est requis pour écrire directement
  `CTaskLoading+0x0C`; les writers sont internes à la classe.
- Nouvelle frontière : provider asynchrone `0x8219EE40 → 0x8219F5D0`.
- Artefact : `artifacts/loading-field-transition-contracts-gate/RESULT.md`.

## CTaskLoading async-status provider — CLOSED

- Observé : `0x8219F5D0` draine une file de travaux à `manager+0x20` et appelle
  le slot virtuel `+0x14` de chaque travail.
- Observé : slot `0` = pending, `-1` = erreur, autre = terminé; les travaux
  terminés sont publiés/libérés puis retirés. Le drain est borné à 64 travaux.
- Observé : `0x8219EE40` recherche récursivement la ressource dans l'arbre de
  gestionnaires à `manager+0x1C`.
- Prouvé absent : la route `ac6-native` ne contient ni ce provider, ni un
  contrat de file ternaire équivalent, ni du PPC généré câblé au build.
- Artefact : `artifacts/loading-async-status-provider-gate/RESULT.md`.

## Native loading contract integration gate — closed

Observed: native asset and retail-session loading is synchronous. The retail constructor auto-advances its frontend controller through the product states to Mission, and the render loop consumes only mission state. The guest asynchronous loading provider and any environment shim are off this executed path.

## Visible native frontend contract gate — closed

The PAL frontend controller and seven-pack resource closure exist, but native code has no visual producer for pre-mission states. `RetailFrontendResources` validates FHM/NFH payloads and discards their bytes; no decoder, glyph metrics/atlas representation, state draw list, or frontend compositor reaches the presentation boundary.

## PAL NFH glyph producer contract gate — closed at first unresolved field

Official XUI outputs and PAL FHM/NFH closure are qualified, but NFH record semantics are not. The first unresolved field is the big-endian word at `NFH+4`; native code only proves the range 1..4096. `NFH+8` is only proved nonzero. No parser or runtime implementation is justified until their PPC consumers establish cardinality, stride, metrics and texture association.

## Canonical PPC NFH consumer slice — branch stopped at batch limit

Canonical Ghidra evidence reduces NFH recognition to one image constant at `0x82028F5C` and one direct read at `0x822E2870` inside `FUN_822E2858`. The consumer semantics remain open because the final decompiler invocation used the wrong script argument order. See the gate blocker; no dynamic evidence or parser implementation was introduced.

## Unique NFH reader classification — branch stopped

`FUN_822E2858` is a four-byte `NFH\0` recognizer. Its sole caller `Function_822CC9F0` initializes a view and sets its data pointer to `leaf+0x450`. The next consumer is indirect through the data-table entry at `0x82027B0C`; exact vtable start/class/slot remain unqualified after the five-batch limit.

## NFH view table identity — CLOSED

The exact target scan finds `0x822CC9F0` in `.rdata` at `0x82027B0C` and a non-RTTI copy in `.pdata`. The real function table starts at `0x82027A64`; the target is slot `+0xA8`. Constructors `0x822CC118`, `0x822CC168` and `0x822CC200` install that table. Slot `+0xA8` recognizes `NFH\0` at `this+0x08` and writes `leaf+0x450` to `this+0x10`. Slot `+0xB0`, `0x822CC378`, is the first reader: it checks the view data and bound, then returns `this+0x10 + index*0x20`. The next boundary is the record layout.

## NFH view record consumer gate — limited

The 24 numeric `+0xB0` dispatch candidates do not statically resolve to the
NFH vtable `0x82027A64`. Their owner decompilations contain no materialized
table identity or explicit post-dispatch read of `this+0x10`; the slot offset
is shared by unrelated classes. Constructor xrefs are qualified, but receiver
provenance to a candidate is not. The gate is limited after five batches; see
`artifacts/nfh-view-record-layout-gate/BLOCKER.md`.

## NFH receiver provenance gate — limited

Les six appelants de constructeurs ont été qualifiés. Quatre chemins
installent une table dérivée puis terminent par `0x822CC118`, donc la table
NFH `0x82027A64` est vivante pour ces sous-objets; les deux autres restent sur
des tables dérivées. Les fenêtres dérivées ne contiennent pas le lecteur
`0x822CC378`, et aucun receiver n'est encore relié à un dispatch `+0xB0`.
Le dernier traceur a été invoqué avec un seul argument alors que son contrat
est `START END`; le résultat de déplacement large est non discriminant. Gate
limité après cinq lots; voir
`artifacts/nfh-receiver-provenance-gate/BLOCKER.md`.

## NFH direct `+0xB0` consumer gate — CLOSED BY REFUTATION

Le traceur corrigé (`START=0x82000000`, `END=0x8233ffff`) retrouve 24
dispatchs. Aucun retour `r3` n'est utilisé comme pointeur dans la fenêtre
bornée après `bctrl`; les quelques lectures de `r3` sont scalaires ou des
copies sans lecture de record. Les propriétaires ne matérialisent pas la
vtable `0x82027A64` et n'appellent pas directement ses constructeurs. La
branche « consommateur direct parmi ces 24 slots » est réfutée; le layout du
record reste ouvert et le prochain bord est un appel indirect plus long ou un
autre slot. Voir
`artifacts/nfh-receiver-provenance-gate/RESULT.md`.

## IB bootstrap producer static gate — CLOSED BY STATIC BOUNDARY

The generated object retains the `0x821B55C0` and `0x821B9BC8` entry points,
but their guest memory addresses are computed from `PPCContext`; no known
bootstrap address occurs in the object. The native ring captures an address
already published by the guest and contains no bootstrap allocator/producer.
The direct address-based static producer hypothesis is therefore refuted,
without proving absence of a dynamic guest writer. See
`artifacts/ib-producer-static-resume-gate/RESULT.md` and `BLOCKER.md`.

## Generated-store hook gate — CLOSED BY RUNTIME ATTRIBUTION

The bounded address watcher already exposed the needed guest PC/LR/function
metadata, but `AC6_PPC_STORE_U128` bypassed it. The U128 path now reports the
16-byte store while preserving its existing guest-byte ordering and memory
write. The codegen-enabled runtime and core test build pass, and the core CTest
passes 1/1. The existing one-tick capture attributes all 48 writes of the
bootstrap reservation to `0x821B20A0` (U32 stores, LR `0x821B212C`) and shows
the 24 PointList pairs. The remaining boundary is writer → publication →
renderer/scheduler, not writer discovery. See
`artifacts/generated-store-hook-gate/RESULT.md` and `BLOCKER.md`.

## Record writers / consumer gate — CLOSED BY STATIC P-CODE SLICE

La passe `FindScaledStoreWriters.java` du projet démo a parcouru 11 253
fonctions et retrouvé le témoin `0x820FF710` ainsi que quatre writers de
records non nuls : `0x820FF788` (type 1), `0x820FF7F8` (type 2), `0x820FFA88`
(type 3) et `0x820FFB50` (type 4). Le worker `0x820FFCA0` copie les slots et
`0x820FEFA8` dispatch explicitement ces quatre types. Le zéro du chemin type 0
n’est donc pas l’unique contrat de la file. La question restante est le type
atteint au boot et son raccord à la publication renderer/scheduler. Voir
`artifacts/ib-writer-publication-static-gate/RESULT.md` et `BLOCKER.md`.

## Record type reachability checkpoint — STATIC CORRIDOR QUALIFIED

Le slice CFG borné de `0x8210A1C0` vers `0x8210ADB4` qualifie 18 gardes et
confirme que `0x82117410` est atteignable statiquement. Le corridor exige un
état d'objet non nul, un `param_4` dans la famille `0x1102..0x1210` avec les
exclusions du P-code, puis des lookups non nuls via `0x821080D0` et
`0x826F6124`. Cela ferme la crainte d'un callsite mort, mais ne prouve pas
qu'un record type 1–4 est produit dans le plateau natif. La prochaine frontière
est une capture bornée callsite → type de slot → publication, avec observables
pré-déclarés dans `artifacts/record-type-reachability-gate/BLOCKER.md`.

## Record type route capture — OBSERVED ROUTE REFUTED; STATIC CORRIDOR UNACTIVATED

Le watcher opt-in du hook d'entrée générique a été compilé sans modifier le
code généré. Le slice statique montre que `0x820FEFA8` commute sur
`*(r3+0x40)`, pas sur `r4`. La capture START bornée (ticks 2990–3021) n'entre
ni dans `0x8210A1C0` ni dans `0x82117410`; elle observe `0x820FEFA8` 31 fois
avec `record_type=0`. La route worker observée ne publie donc aucun type 1–4
dans cette fenêtre. 3021 ticks/2913 PRESENT ne franchissent pas le frontend.
Le corridor statique reste non réfuté mais son activation est hors fenêtre.
Voir `artifacts/record-type-reachability-gate/RESULT.md` et `BLOCKER.md`.

## Paramètre du corridor record — PRODUCTEUR STATIQUE QUALIFIÉ

Le seul appel direct `0x82165CC0:0x82165D8C` transmet à `0x8210A1C0` son
huitième argument : `r8` est sauvegardé dans `r26`, puis restauré en `r6` au
site d'appel. `0x8210A1C0` calcule ensuite `param_4 = r6 & 0xFFFF` à
`0x8210A20C`. Le producteur statique est donc l'argument d'une arête
indirecte/vtable, pas une constante ou un service natif. La fenêtre
d'activation reste à trouver; aucun shim n'est justifié.
Voir `artifacts/record-type-reachability-gate/param4-producer-static-run5-summary.txt`.

## AVIObjectDemo vtable — TABLE QUALIFIÉE, RÉCEPTEUR AJUSTÉ OUVERT

`0x8200B6FC` est la vtable RTTI `AVIObjectDemo`; son slot `+4` est
`0x82165CC0`. `0x82166550` et `0x821674A8` écrivent cette vtable à `objet+0xC`.
La dispatch `0x821710FC` observée sur l'objet retourné lit au contraire la
vtable primaire `0x8200B844`; les consommateurs du pointeur global ne
montrent pas de `global+0xC` explicite. La table/slot est donc qualifiée, mais
le récepteur ajusté exact et l'activation du corridor restent ouverts. Aucun
shim env ou contrat natif n'est justifié. Référence compacte :
`artifacts/record-type-reachability-gate/avi-vtable-static-run3-summary.txt`.

## AVIObjectDemo — témoin runtime borné

Le hook d'entrée dédié est compilé et les tests core/trace passent. La fenêtre
START `2990..3021` (inputs aux ticks `3000/3001`, store frais, audio dummy)
termine à 3021 ticks / 2913 PRESENT, sans frontend, mission ou terminal. Aucun
appel n'entre dans `0x82165CC0`, `0x8210A1C0` ou `0x82117410`; l'objet généré
contient pourtant bien les hooks vérifiés par désassemblage. L'activation est
donc indécidable dans cette fenêtre, non réfutée globalement. Ne pas répéter ce
runtime; reprendre par la source statique de l'arête virtuelle et du récepteur
ajusté. Voir `artifacts/record-type-reachability-gate/avi-receiver-run1-summary.txt`.

## AVI virtual edge — candidats slot +4

Le scan Ghidra en lecture seule trouve 510 dispatchs virtuels `+4` dans le
binaire, dont 11 dans `0x82160000..0x82172000`. `0x82165D6C` est l'appel
virtuel interne de `0x82165CC0`; le `bl` direct vers `0x8210A1C0` suit à
`0x82165D8C`. Les dix autres sites locaux restent des candidats sans vtable
résolue. Les flux globaux `0x82731A30/+0xC` et `0x823CD6E0/(+0xC,+0x84)` ne
produisent aucun dispatch qualifié. Reprendre par la provenance statique de
la vtable, sans runtime répété. Voir
`artifacts/avi-virtual-edge-static-gate/RESULT.md` et `BLOCKER.md`.

## AVI virtual edge — passe constructeurs/récepteurs statiques

La passe finale confirme que `0x8200B6FC` n'est pas un pointeur de données
statique : seuls `0x82166550` et `0x821674A8` le matérialisent, à `objet+0xC`.
`0x82373090` construit `0x82731A30`; le consommateur connu
`0x821710FC` lit sa vtable primaire `0x8200B844`, pas la sous-vtable cible.
Les dix dispatchs locaux restent donc indéterminés individuellement, mais les
flux globaux/constructeurs connus ne cachent plus d'arête qualifiée vers
`0x82165CC0`. Le gate reste ouvert sur cette frontière statique; aucun shim
env, contrat natif ou runtime répété n'est justifié. Voir
`artifacts/avi-virtual-edge-static-gate/constructor-flow-final.txt`.

## AVI virtual edge — census des callers, gate limité

La seconde passe statique confirme `0x821600C8` comme receiver générique
`param_1` et ne trouve aucune fonction contenante pour `0x8216B3B4`. Les xrefs
directs vers les owners `0x82166AE0`, `0x8216E218`, `0x8216F640`,
`0x82169B38` et les deux annotations `bctrl` vers `0x821600C8` sont consignés,
mais leur vtable n'est pas propagée. Le gate est arrêté à la limite de lots,
sans runtime répété; reprendre par ces callers. Voir
`artifacts/avi-virtual-edge-static-gate/candidate-callers-run1.log` et
`BLOCKER.md`.

## AVI virtual edge — un candidat réfuté statiquement

La disassembly bornée montre que `0x821A4454` passe `0x82390574` à
`0x82169B38`; le premier mot de cette structure est `0x8200BAEC`, donc le
dispatch `0x82169B7C` ne peut pas utiliser la vtable cible `0x8200B6FC`.
Les deux annotations `bctrl` précédemment attribuées à `0x821600C8` sont des
appels via `PTR_PTR_82390034[+8]`. Les receivers state/manager restants sont
encore ouverts; aucun runtime ou shim n'est justifié. Voir
`artifacts/avi-virtual-edge-static-gate/caller-disassembly-run3.log`.

## AVI virtual edge — limite de provenance des managers

La passe ciblée révèle les receivers restants : `0x82165230` via
`FUN_82327108()+0xB5BC`, `0x82166AE0` via `FUN_82327104()+0xD274`,
`0x8216E218`/`0x8216F640` via `FUN_82327100()+0x7C`, `0x8216F7F8` via
`FUN_8232710C()` et sous-objet `+0x68`, `0x82170CD0` via
`PTR_DAT_8238FEF4`, et `0x82170F58` via `FUN_82327108()`/la route globale
primaire. Ghidra modélise `FUN_82327100`/`FUN_8232710C` comme stubs `void`
vides : la valeur de retour et sa matérialisation restent indéterminées, sans
preuve de pointeur nul. Aucun receiver n'est qualifié avec `0x8200B6FC`;
aucun shim env ni runtime répété n'est justifié. Voir
`artifacts/avi-virtual-edge-static-gate/manager-factory-summary-run5.txt`.

## AVI virtual edge — correction ABI save-helper

La passe précédente attribuait à tort `FUN_82327100/04/08/0C` le rôle
d'accessors de managers. La disassembly canonique montre des entrées contiguës
du helper ABI : `0x82327100` sauvegarde `r26`, `0x82327108` `r28`, et
`0x8232710C` `r29`; l'évidence ABI qualifiée établit que le helper préserve le
`r3` entrant. Les valeurs `iVar = FUN_8232710X()` produites par Ghidra sont
donc des artefacts de retour pour ce `r3`, pas des managers. Les receivers de
`0x82165230`, `0x82166AE0`, `0x8216E218`, `0x8216F640`, `0x8216F7F8` et
`0x82170F58` doivent être propagés depuis leurs callers. `PTR_DAT_8238FEF4`
vaut `0x82731150`; son stockage initial est zéro, donc `0x82170CD0` reste
indéterminé. Voir
`artifacts/avi-virtual-edge-static-gate/helper-boundary-correction-run8.txt`.

## AVI virtual edge — indirect table refuted as qualified vtable

La table `0x8200C5C0..0x8200C664` contient des pointeurs exécutables contigus,
dont `0x8200C624 = 0x8216F7F8`, puis des constantes non-code. Aucun alignement
testé ne possède un locator RTTI MSVC valide; les matérialisations PPC brutes
de `0x8200C5C0`, `0x8200C600` et `0x8200C624` donnent zéro hit, comme la
recherche U32 de ces adresses. `0x823270F8` est bien `std r24,-0x48(r1)`, une
entrée du helper ABI qui préserve `r3`. La table reste donc une table de
dispatch sans propriétaire objet statiquement prouvé, et ne relie aucun
receiver à `0x8200B6FC`. Le gate AVI est fermé par réfutation/indécidabilité;
il faut revenir au premier producteur de réveil observé, sans runtime répété.
Références : `artifacts/avi-virtual-edge-static-gate/indirect-vtable-provenance-run10.log`,
`indirect-vtable-owner-run10.log`, `indirect-vtable-rtti-run10.log` et
`indirect-vtable-raw-materialization-run10.log`.

## AVI virtual edge — caller propagation checkpoint

Le census direct qualifié trouve `0x82165744 → 0x82165230`,
`0x82167378 → 0x82166AE0`, `0x8216EAC4/0x8216EEC4 → 0x8216E218` et
`0x8216F920 → 0x8216F640`; aucun appel direct vers `0x8216F7F8` ou
`0x82170F58`. `0x82167320` passe son paramètre `r3` directement à
`0x82166AE0`. `0x8216EA20`, `0x8216ECE0` et `0x8216F7F8` utilisent
`0x8232710C` comme entrée ABI et conservent donc leur `r3` entrant; le dernier
passe ce receiver à `0x8216F640`. Le seul caller direct de `0x82165490` est
`0x821662CC`; cette fonction obtient le receiver de `FUN_823270F8()`, dont
l'identité ABI reste à qualifier. `0x8216F7F8` a une référence de données à
`0x8200C624`, mais sa vtable n'est pas encore reconstruite. Aucun receiver ne
porte encore une preuve `0x8200B6FC`; le prochain gate est la vtable indirecte,
sans runtime.
Références : `artifacts/avi-virtual-edge-static-gate/caller-direct-census-run9.log`,
`caller-propagation-run9.log`, `parent-caller-census-run9.log`.

## Primary counter contract checkpoint

Le gate statique du compteur primaire est fermé. L'état callback `0x82934708`
est initialisé avec `state+0x18=0` et `state+0x10=0`. Le callback live
`0x822E3EC0` incrémente `state+0x08`, puis `0x822EEE10(state+0x40,count)` écrit
le compteur publié `state+0x50` et signale `0xE000004C`. Le worker
`0x822E40E8` consomme cette valeur via les attentes `>= state+0x10` et
`> state+0x10` (`0x822E4018`/`0x822E4080`). La route d'initialisation
`0x822E52D0` réécrit aussi `state+0x10` avec `last_argument-1`; l'argument
qualifié vaut `1`, donc le seuil reste zéro. `state+0x18` ne passe à un que par
la fermeture `0x822E3E48`.

Conclusion : la chaîne wake/counter est complète statiquement; elle ne fournit
pas le producteur natif manquant. Le prochain verrou est la production du draw
normal/frontend, sans relancer le runtime à ce stade. Preuve :
`artifacts/primary-counter-static-gate/gate.status` et `RESULT.md`.

## Frontend visual producer — static gate fermé

La reconstruction native possède un `FrontendController` qui qualifie la
transition `Title -> NewGame -> Briefing -> Hangar -> Loading -> Mission`, mais
ce contrôleur ne produit ni draw-list, ni texture, ni framebuffer. De même,
`RetailFrontendResources::open` vérifie la fermeture FHM/NFH PAL et conserve
seulement des résumés de fonts; le payload NFH, les métriques et les pixels
d'atlas restent opaques. `run_play_impl` ne transmet aucun état frontend aux
renderers : il rend uniquement la scène mission (Vulkan ou CPU capture) puis
présente la cible. Aucun producteur visuel frontend n'est donc présent dans la
source non-test.

Le gate est fermé par preuve statique, sans runtime. Le verrou causal restant
est : `FHM/NFH -> décodage glyph/atlas -> draw-list Title -> present`.
Prochaine preuve discriminante : qualifier le layout d'une feuille NFH PAL et
son contrat de draw Title sur un fixture réel, sans texte synthétique ni
placeholder. Voir `artifacts/frontend-visual-producer-static-gate/RESULT.md`.

## CSwgListener `+0x20` — gate statique fermé

Le sweep RTTI du programme `Default.xex` du projet Ghidra `ace-combat-6-demo`
résout 89 dérivés de `CSwgListener`; 40 ont un handler non-stub à `+0x20`.
Cependant, le snapshot post-START de `SendMsgI("M102")` ne contient que
`CSelectMessageDlgManager` et `CModeTaskTitleDemoOffline`, dont le slot reste
`0x820AC748`. Le canal n'est donc pas mort globalement, mais aucun
implémenteur caché n'explique le plateau actif. Suite : layout NFH PAL réel et
producteur de draw Title. Preuve :
`artifacts/listener-slot20-sweep-gate/RESULT.md`.

## NFH PAL — borne structurelle fermée

Les 28 leaves NFH extraits vérifient `size = 0x450 + count*0x20`, et le
contrat PPC relie `leaf+0x450` au lecteur `base + index*0x20`. Le mot `NFH+4`
reste sémantiquement indéterminé et aucun champ n'est relié à un atlas/draw;
aucun parser natif n'est donc justifié. Reprise : l'arête indirecte du reader
de record. Preuve : `artifacts/nfh-layout-static-gate/RESULT.md`.
## NFH consumer provenance — gate fermé

- Le slice statique de reprise a réfuté que l'un des 24 dispatchs indirects au
  déplacement `+0xB0` consomme directement le record NFH `0x20` renvoyé par
  `0x822CC378`.
- La table de base qualifie `+0xB0 -> 0x822CC378`; `0x82027BE8` qualifie le
  slot homologue `+0xB0 -> 0x822CE210`, sans lien receiver→record démontré.
- Le prochain bord utile est une arête indirecte plus longue vers le lecteur
  de record/atlas ; pas de parser NFH, shim env ni runtime à ce stade.
## NFH canonical PPC accessor — gate fermé

Le slice Ghidra canonique qualifie `0x822E2858` comme reconnaisseur `NFH\0`,
son unique xref `0x822CC9F0`, l'initialisation `this+0x10 = leaf+0x450` et le
reader `0x822CC378` au stride `0x20`. Les getters `0x822CC3B0` et
`0x822CC3B8` exposent `leaf+4` comme `u16` et comme `float`. Cette preuve est
structurelle seulement : elle ne relie pas encore un record à un atlas ou à un draw
Title. Aucun shim env ni parser n'est ajouté. Voir
`artifacts/canonical-ppc-nfh-consumer-slice-gate/RESULT.md`.
## Title resource manager — gate statique fermé par réfutation

Le chemin `CModeTaskTitle` est réel, mais `0x8219EB20` ne fait que construire
une clé et `0x8219F7F8` gère un `CResourceManager` (`0x82012E1C`). Son slot
`+0x40` (`0x8219DC50`) initialise seulement deux flags; `0x822CC2A8` écrit
seulement `this+0x18`. Aucun de ces helpers ne décode NFH/FHM ni ne produit une
commande de draw. Le prochain bord est donc le child resource/view qui relie
le payload aux lecteurs `0x822CC9F0/0x822CC378`. Voir
`artifacts/frontend-compact-title-next/RESULT.md`.
## Child resource/view → NFH metrics — gate fermé

Le chemin child/view est maintenant qualifié dans le projet Ghidra canonique.
Les wrappers `0x822CC7A8`, `0x822CC8E8` et `0x822CC988` valident le child puis
appellent les slots de métriques de la classe NFH; `0x822CCD98` reste dans le
même contrat scalaire. `0x822CCCF8` ne fait qu'une copie de 0x20 octets via
`0x82327D90`, qui est un memcpy PPC optimisé. `0x821A00E8` ne fait qu'enfiler
des enregistrements de ressource utilisés par le titre.

Conclusion : cette branche atteint des métriques et des copies de records,
mais aucun atlas, draw-list, PM4, framebuffer ou present. Elle est donc
réfutée comme producteur visuel manquant. Preuve :
`artifacts/child-resource-view-next/RESULT.md`.

## First non-bootstrap RT0 writer — gate statique fermé

Le premier writer de couleur réellement qualifié est la chaîne indirecte
`0x822F84E0 -> 0x821B6708 -> 0x821B6078 -> 0x821B5B10 -> 0x821B58B0 ->
0x821B55C0`. Le champ `this+0x24` (mot couleur) traverse l'ABI via `r8`, la
pile `+0x5c`, puis `r10` du builder; le record flottant en `r6` alimente les
vertices. `0x821B55C0` émet un vrai `DRAW_INDX_2`/`0xc0003601`, raccordé au
resolve/copy RT0 déjà qualifié.

La provenance ABI est donc fermée statiquement. Il reste à démontrer que le
runtime natif appelle ce slot vtable et que la cible devient non noire; aucun
shim env ni optimisation n'est justifié avant cette vérification ciblée.
Preuve : `artifacts/rt0-writer-callers-next/RESULT.md`.

## RT0 writer — invocation runtime non atteinte, verrou amont identifié

La sonde native bornée n'observe aucun appel du slot `0x822F84E0` ni draw
rectangle/resolve : elle ne produit que les 24 PointList bootstrap à
`tick=0`, avec `surface=0`, `color_mask=0`, et expire avant d'écrire son
rapport. Le rapport natif qualifié à 3021 ticks donne le verrou causal :
`render_queue.producer=5647`, `consumer=0`, `packet_count=0`, `draw_count=24`,
`present_count=0`, alors que `vd_swap.calls=2928`.

Conclusion : l'invocation du writer n'est pas encore testable dans le chemin
actuel; le premier contrat manquant est le consommateur de la render queue,
pas un shim d'environnement ni le décodage NFH. Preuves compactes :
`artifacts/global-progress/report-index-compact.txt`,
`artifacts/native-writer-runtime/compact.txt`.

## Erratum — render queue consumer présent, payload manquant

La conclusion précédente « consommateur absent » est supersédée. Les preuves
exactes montrent que `0x820FF710` publie l'index producteur et que
`0x820FFCA0` publie puis réinitialise l'index consommateur à chaque tick. Le
snapshot `consumer=0` était pris après ce reset.

Le worker calcule `queue + index*0x60 + 0xD0`, copie le record de 96 octets et
appelle `0x820FEFA8`. Le probe RR corrigé reçoit un objet pile non nul, mais
`+0x40..+0x58` sont nuls et aucune branche de type 1–4 ne s'exécute. Les
writers statiques `0x820FF788` et `0x820FF7F8` savent publier des records de
type 1/2, mais ne sont pas atteints dans la route active. Le verrou est donc
la production du payload/menu, pas le consumer ni l'environnement.

Preuve compacte : `artifacts/render-queue-consumer-gate/RESULT.md`.

## Checkpoint — corridor payload qualifié, route active sans payload

La vtable `0x82008EF0` et le P-code qualifient les writers type 1–4; le
writer type 1 (`0x820FF788`) est atteint par l'appel indirect de
`0x82117410`, dont l'unique appelant direct est `0x8210A1C0`. Cette route est
protégée par le sélecteur `param_4` et des gardes de tables.

La capture bornée existante n'entre pas dans ce corridor : `0x820FEFA8` reçoit
31 records de type 0 et aucun type 1–4. Le contrôle de queue reste donc fermé,
mais son producteur utile n'est pas encore identifié. Reprendre par le
producteur/récepteur statique de `param_4`; ne pas ajouter de shim env, d'appel
direct writer, de parser NFH ou d'optimisation.

Preuve : `artifacts/render-queue-consumer-gate/RESULT.md` et
`artifacts/record-type-reachability-gate/RESULT.md`.

## Checkpoint 2026-08-22 — producteur du réveil `state+0x56F8`

L'entrée indirecte unique vers `0x822F85B8` est jointe à la construction
`0x822E52D0 -> 0x822F23F0 -> vtable 0x8202A488`. Son seul appel direct à
`0x821BB4C8` passe `r6=1`, ce qui initialise `state+0x56F8` à `0x0C000001`.
Le seul writer ultérieur de ce champ ne peut modifier que le bit 0; aucun
writer généré ne positionne le bit `0x4` testé par `0x821C57D0`.
`0x821C64E8`, appelé après l'initialisation, ne touche pas ce champ. Le gate
statique est donc fermé : `static_bit4_producer_absent_in_recompilation`.

Pas de patch du sélecteur, du délai, du state object, de shim env ou de `-O3`.
Le prochain bord est une qualification Ghidra binaire de l'éventuelle voie
indirecte/importée qui pourrait fournir une autre valeur de `r6`. Preuve :
`artifacts/wake-producer-gate/RESULT.md`.

## Checkpoint — table constructeur reclassée comme `.pdata`

L'occurrence `0x821BB4C8` à `0x8207DF10` est dans le bloc Ghidra `.pdata`
read-only (`0x82077200..0x82087637`). Les paires voisines sont des
`BeginAddress`/métadonnées de longueur de fonction, non une table de dispatch;
les scans de références et de branches ne trouvent aucun consommateur de ces
mots. Le faux chemin constructeur est donc fermé
(`pdata_false_alternate_constructor`). Le prochain bord reste un producteur
importé/omis hors de cette construction. Preuve :
`artifacts/wake-producer-gate/constructor-table-classification.md`.

## Checkpoint — chaîne CX360UnitManager vers le ring qualifiée

La chaîne renderer est maintenant fermée statiquement : le slot `+0x14` de la
vtable RTTI `CX360UnitManager` (`0x820A45E0`) est le seul producteur de
`(17,6)` (`0x820A4778`), `0x821ADAB8` est le seul écrivain de
`device+0x5460`, et `0x821C57D0` lit ce champ avant toute soumission normale.
Le scan frais ne trouve aucun appel direct à `0x820A45E0` ou `0x821ADAB8`;
les sept sites de construction connus de l'instance restent hors du parcours
natif. Le verrou causal est donc l'activation/construction du gestionnaire
d'unités, pas un shim env, XMA ou une optimisation.

Reprendre par la garde mission qui doit atteindre l'un des sept constructeurs,
puis par un harness borné `(17,6) → +0x5460 → soumission`. Preuve compacte :
`artifacts/cx360-unit-manager-gate/RESULT.md`.

## Checkpoint — activation reclassée derrière le provider de readiness

`0x8217C4D8` est le callback enregistré par le slot `+0x0C` des
`CModeTaskGame*`; son bras `-3` sélectionne ensuite les deux gestionnaires de
mission qui construisent `CX360UnitManager`. La construction n'est donc pas le
premier contrat manquant. Le chemin de chargement atteint par la route forcée
reste dans `0x8219DF00`/`0x8219F5D0`, en attente du provider
`0x8219AF20`/`0x82195B50`; le flag lu par `0x8217E258` n'est pas publié sur la
route observée. Le prochain travail est ce producteur de readiness, sans shim
env, appel direct renderer ni optimisation. Preuve :
`artifacts/cx360-construction-activation-gate/RESULT.md`.

## Checkpoint — le provider de readiness n'est pas le premier verrou

La capture runtime bornée du chemin recompilé a observé l'objet de chargement
`0x2E3B0040` aux ticks `4116..4123`. Après un premier état `0`, il reste en
état `1`; `0x8219AF20` est atteint à chaque poll, `0x82195B50` ne l'est
jamais, et le retour du provider passe de `0` à `1` au tick `4123`.
Le plateau demeure pourtant à `5600` ticks (`5492 PRESENT`, `23 blocked`,
`frontend=false`, `mission=false`).

Le gate « `0x8219AF20` ne termine jamais » est donc réfuté pour cette route.
Le provider est atteint et signale une fin, tandis que le chemin
`0x82195B50` est inactif. Le prochain bord est la transition post-provider et
le premier payload de rendu non-bootstrap, pas un shim d'environnement ni la
construction CX360. Preuve :
`artifacts/readiness-provider-gate/RESULT.md`.

## Correctif — retour observé limité au callback virtuel

La capture suivante qualifie précisément l'objet de travail `0x2E3B0040` :
`0x8219AF20` est atteint, `0x82195B50` reste inactif, puis le callback virtuel
`0x8219DF00` fait `+0x0C: 1→2→0` et retourne `1` au tick 4123. Le champ
`result` du watcher est donc le retour de `0x8219DF00` (LR `0x8219F64C`), pas
une lecture directe du retour de `0x8219AF20` ni de l'agrégateur `0x8219F5D0`.
La transition de l'agrégateur et de `CTaskLoading` reste à qualifier; le gate
post-provider demeure ouvert. Preuve :
`artifacts/post-provider-transition-gate/RESULT.md`.

## Checkpoint — transition aval du task qualifiée (2026-08-22)

À `tick=4123`, `sub_8218A4A0` appelle le slot virtuel 15 `0x8216CB40` de
`0x2E3D0080` (LR `0x8218A55C`), puis écrit `task+0x0C=1` au LR `0x8218A564`;
`sub_820CDF88` publie aussi `0x2E3D00E8` dans `0x826DF804`. L'hypothèse d'une
transition task absente est réfutée. Le runtime reste à `4140/4032 PRESENT`,
`23 blocked/0 runnable`, sans frontend ni mission. Le prochain bord est le
listener/callback et la reprise autour de `0xE000004C` / `0x821A8C88`, pas le
provider, le manager, un shim env ou `-O3`. Preuve :
`artifacts/readiness-consumer-gate/runtime-consumer/`.

## Checkpoint — manager de readiness identifié et drainé (2026-08-22)

`sub_8219EE40` est confirmé comme résolveur de liste. Aux ticks `4115..4123`,
la clé `0x5908E2C8` résout depuis `0x827745F0` vers le manager `0x18BB0100`.
Son layout runtime confirme `+0x20=0x18BB0120` (tête de liste) et
`+0x24=0x18BB0124` (statut); la liste est drainée puis `+0x20` devient nul et
`+0x24` passe `1→0` au LR `0x8219F6B4`. Le gate agrégateur est fermé, mais la
fin n'entraîne aucun réveil : `4140` ticks, `4032 PRESENT`, `23 blocked`,
`frontend=false`, `mission=false`. Le prochain verrou est l'aval
publication/FSM, sans shim env ni `-O3`. Preuve :
`artifacts/post-provider-transition-gate/RESULT.md`.

## Current checkpoint — transition aval du task qualifiée (2026-08-22)

À `tick=4123`, `sub_8218A4A0` appelle le slot virtuel 15 `0x8216CB40` de
`0x2E3D0080` (LR `0x8218A55C`), puis écrit `task+0x0C=1` au LR `0x8218A564`;
`sub_820CDF88` publie `0x2E3D00E8` dans `0x826DF804`. Le gate de transition
aval est fermé. Le runtime reste à `4140/4032 PRESENT`, `23 blocked/0 runnable`,
sans frontend ni mission. Prochain bord : listener/callback puis reprise de
`0xE000004C` / `0x821A8C88`, sans shim env ni `-O3`.

## Checkpoint 2026-08-22 — listener et attente primaire séparés

Le slice statique qualifié réfute la chaîne causale unique proposée. `0x826DF804`
est le second slot du tableau indexé `0x826DF800`; `sub_820E9838` consomme les
listeners via le callback `+0x20`, déjà qualifié no-op, et `sub_820EA4A8` via
le callback `+0x54`, piloté par le script. La boucle `E000004C` appartient au
worker signal/wait (`0x821A69CC` / `0x821A8C88`) et sa reprise est déjà
fonctionnelle jusqu'au premier accès guest `0x82327154`.

Aucun contrat kernel/import manquant n'est établi sur cette arête. Reprendre
statiquement par le sélecteur `param_4` de `0x8210A1C0 → 0x82117410 →
0x820FF788` et le premier record type 1–4 non-bootstrap.

## Checkpoint 2026-08-22 — gate r8/record utile bloqué

Les slices parallèles ont réfuté l’identification de `0x821710FC` comme
dispatch AVI : ce site utilise le sous-objet `CDemoDataManager`/manager
primaire, tandis que `0x8200B6FC/+4 → 0x82165CC0` reste sans owner ni
définition qualifiée de `r8`. Le worker `0x820FEFA8` lit les records 1–4 mais
aucune jonction vers la chaîne RT0/Xenos n’est établie.

Le probe codegen-ON borné à 8 000 ticks a produit 7 892 PRESENT, sans
frontend/mission, sans soumission ring (`0` paquets décodés); il ne discrimine
pas l’arête `r8` et ne justifie aucun patch. Reprise : owner/dispatcher AVI
statique, puis première émission pixel `IM_LOAD_IMMEDIATE`/`DRAW_INDX_2`.
Preuves : `artifacts/first-useful-record-r8-gate/`.

## Checkpoint — seam causal Xenia Edge exécuté (2026-08-22)

Le checkout Edge qualifié est compilé avec les hooks DTRACE/FTRACE. Le seam
opt-in observe les entrées PPC `0x82386C58`, `0x82165CC0`, `0x8210A1C0`,
`0x82117410`, puis les stores I32 bornés; le flux PM4 existant reste le seul
observateur GPU. Le test ciblé Edge passe `79/79` cas (`3723` assertions).
Trois fenêtres Edge de 90 s sur `Default.xex` PAL, avec cache neuf pour les
deux runs instrumentés, n'atteignent aucun PC nommé et n'émettent aucun PM4.
Le natif de comparaison reste à 192 PRESENT, zéro ring/PM4 et zéro owner
ciblé. Le contrat causal reste ouvert : revenir à la qualification statique
du corridor owner/r8/record avant tout correctif renderer.
Preuve : `artifacts/xenia-edge-causal-gate/RESULT.md`.

## Checkpoint — writer de provider qualifié statiquement (2026-08-22)

Le slice Ghidra démo canonique ferme le sous-gate d'activation du provider :
`0x82165490` construit la configuration, puis `0x82114F58` appelle
`0x82114798`. Ce dernier obtient le count depuis la ressource d'index 1,
alloue chaque entrée, écrit le slot `+0x04` depuis le tableau d'index 0,
installe les ressources via `0x82108918`, puis exécute la séquence
`0x8210E548`/`0x8210E5E0` et `0x82108D58`. Ce n'est donc ni une table vide ni
un état synthétique. La jonction vers l'owner AVI/r8 et vers le premier
record type 1 reste ouverte; aucun correctif runtime ou renderer n'est
justifié. Preuve :
`artifacts/static-owner-shared-gate/provider-entry-contract-decomp.log`.

## Checkpoint — validation codegen et package (2026-08-22)

Le build codegen-OFF est propre et CTest passe `27/27`; le build codegen-ON,
la cible `ac6-demo-recomp` et les 7 tests Xenos/Vulkan ciblés passent aussi.
Les quatre runners autonomes de différentielle EDRAM/pixels/padding,
certificat writeback, oracle EDRAM et screencap passent avec leurs variantes
sanitizer quand applicable. L'installation CMake vers la racine termine avec
`bin/bin` absent et le package installé ne contient aucun XEX, PAC/TBL, base
Ghidra ou fichier C++ généré. Cela ferme uniquement la lane de validation
locale; le runtime guest reste bloqué avant tout payload non-bootstrap.
Preuves : `artifacts/build-validation/`.

## Checkpoint — slices statiques owner/records fermés par blockers précis (2026-08-22)

Les passes parallèles ont qualifié trois contrats sans modifier le runtime :
`0x82165490 → 0x82114F58 → 0x82114798` peuple bien les sous-entrées et leurs
clés; `0x820FEFA8` ne rejoint pas directement PM4/Xenos et laisse ouvertes les
callbacks `[entry+0x20]`/`[entry+0x1c]`; l'owner portant `0x8200B6FC` et le
producteur de `r8` avant `0x82165CC0` restent indirects. Aucun correctif
renderer, shim ou état synthétique n'est autorisé. Prochaine passe : producteur
de la liste amont `state+0xB5B4/B5B8`, puis ces callbacks si nécessaire.
Preuves : `artifacts/goal-playable/{avi-owner,record-population,record-pm4}/RESULT.md`.

## Validation longue — audit de complexité hors lane guest (2026-08-22)

Le CTest complet borné a exécuté 88 tests : 87 succès, 4 skips attendus, et
`ac6-cpp-complexity` échoue uniquement parce que des sources d'analyse déjà
présentes sous `artifacts/` dépassent son budget. Aucun test Xenos/Vulkan,
runtime ou renderer n'a régressé; ce résultat ne justifie aucune modification
du C++ généré ni de l'audit. Preuve : `artifacts/goal-playable/ctest-refresh/`.

## Checkpoint — producteur B5B4/B5B8 qualifié (2026-08-22)

Le slice Ghidra a identifié `0x82165E68` comme writer partagé : il initialise
`DAT_826F6188`, remet `state+0xB5B4/B5B8` à zéro, puis remplit ces deux champs
depuis les clés `param_2+0x12/+0x16` via `0x8219EE40` et
`0x821EE130/0x821EE0F8`, avant de publier `DAT_826F6188+0x3B88=1`. Le
producteur d'adresse est donc fermé; les valeurs de configuration et la
publication de l'instance secondaire `0x8200B6FC`/`r8` restent ouvertes.
Preuve : `artifacts/goal-playable/state-list-producer/RESULT.md`.

## Checkpoint — owner AVI et callbacks records épuisés statiquement (2026-08-22)

Le suivi AVI confirme que seuls `0x82166550` et `0x821674A8` publient
`0x8200B6FC` à `object+0xC`; les deux dispatchs trompeurs
`0x82169B7C`/`0x821710FC` sont réfutés et les huit autres candidats restent
sans owner/vtable qualifié. Aucun `r8` ne peut donc être attribué à
`0x82165CC0`. Le suivi records borne les six callbacks data-driven
`entry+0x20`/`entry+0x1c`; leurs wrappers sont CPU-only et aucun target PM4/Xenos
ne se matérialise statiquement. Le gate reste bloqué avant tout payload guest;
aucun renderer, shim ou état synthétique n'est ajouté.
Preuves : `artifacts/goal-playable/avi-owner/RESULT-followup.md` et
`artifacts/goal-playable/record-pm4/RESULT-followup.md`.

Le groupe prioritaire `0x82160104/0x82165340/0x82167020/0x8216E310` est
également épuisé : receivers arbitraires ou offsets `+0xB5BC/+0xD274/+0x7C`,
sans arête `owner+0xC` ni définition de `r8`. Preuve :
`artifacts/goal-playable/avi-owner/RESULT-candidate-followup.md`.

Le dernier census de configuration n'a trouvé aucun appel direct qualifié vers
`0x82165E68` ou `0x82165CC0`, aucune écriture des champs entrants
`param_2+0x12/+0x16`, et aucune vtable primaire résolue parmi les 11 dispatchs
locaux. Le contrat amont restant est donc l'objet `param_2` et son producteur
de clés; le writer et le renderer restent inchangés. Preuve :
`artifacts/goal-playable/record-population/RESULT-config-followup.md`.

## Checkpoint — import window refuted (2026-08-22)

Une fenêtre native bornée a ciblé la boucle apparente de section critique
`0x8219AF48/0x8219B0E4` sur `0x82392EB0`. Les 11,025 appels importés n'ont
produit aucun import non géré; le code invité `0x8219AF20` est un lookup borné
qui se termine, déjà observé complet par le gate de polling de ressources.
Il n'y a donc pas de correctif scheduler/lock/XAM justifié. Le blocage reste
la provenance guest de `param_2+0x12/+0x16`, de `base+0xC → 0x8200B6FC` et de
`r8` vers `0x82165CC0`. Preuve :
`artifacts/goal-playable/runtime-frontier/import-window.md`.

## Checkpoint — queue caller/selector census (2026-08-22)

Le census Ghidra canonique ne trouve aucun `bl` direct vers `0x820FF710` ou
`0x820FFCA0`; les deux sont néanmoins qualifiés comme slots indirects de la
file, respectivement producteur type 0 et worker consumer. Le corridor
`0x82165CC0 → 0x8210A1C0 → 0x82117410` et ses writers type 1/4 sont fermés
statiquement. La seule arête restante est l'owner AVI secondaire
`0x8200B6FC/+4 → 0x82165CC0` et son `r8` (dont le low16 doit sélectionner 1),
avec les cinq ressources non nulles. Preuve :
`artifacts/goal-playable/record-population/RESULT-followup-next.md`.

## Checkpoint — census AVI final (2026-08-22)

Le runner Ghidra final confirme que les deux constructeurs sont les seuls
stores qualifiés de `0x8200B6FC` à `object+0xC`, qu'aucun appel direct vers
`0x82165CC0` n'existe dans le corpus, et que les dispatchs génériques ne
résolvent pas l'owner ajusté. Le `r8` sélectionnant `param_4=1` reste donc
guest/opaque; aucun patch partagé n'est autorisé. Preuve :
`artifacts/goal-playable/avi-owner/RESULT-final.md`.

## Checkpoint — MAIN_THREAD_PROMPT exécuté et payload guest borné (2026-08-22)

La voie Xenia Edge demandée a été instrumentée et construite dans le checkout
Edge local, avec le test ciblé passé (`79` cas, `3723` assertions). Trois
exécutions PAL qualifiées, sous `SDL_AUDIODRIVER=dummy`, ont atteint le
chargement normal mais ont expiré avant les quatre PCs nommés; aucun fichier
causal ni PM4 exploitable n'a donc été produit. Le seam reste une observation
seulement et n'est pas une dépendance native. Preuve :
`artifacts/xenia-edge-causal-gate/RESULT.md`.

Le probe natif codegen-ON borné (`--until frontend --max-ticks 3036`) a expiré
à 120 s au tick guest 1220. La file publie et consomme, mais ses 2048 slots de
96 octets restent nuls et aucun record type 1–4, owner AVI ou PM4 n'est atteint.
Cela ferme le contrat queue/import/renderer comme cause immédiate et laisse la
provenance guest de la valeur vers `0x820FF710`, l'owner `0x8200B6FC/+4` et
`r8→param_4` comme frontière active. Aucun code runtime n'a été modifié.
Preuves : `artifacts/goal-playable/runtime-frontier/RESULT-current-run.md` et
`artifacts/goal-playable/native-contract/RESULT.md`.

## Checkpoint — table indirecte séparée de l'AVI (2026-08-22)

Le slice statique de `0x8200C5C0..0x8200C664` qualifie une vtable distincte
(`0x8200C614`, constructeurs `0x8216F4A8/0x8216F500`), sans arête vers
`0x8200B6FC → 0x82165CC0`. La correction de dispatch place le `bctrl` à
`0x82165D70`; l'AVI reçoit son receiver depuis le `r4` entrant et son
sélecteur via `r8 → r26 → r6` à `0x82165D80`, puis `low16(r6)` dans
`0x8210A1C0`. Le producteur indirect de ces deux valeurs reste non qualifié;
aucune donnée ne justifie un patch. Preuve :
`artifacts/goal-playable/avi-owner/RESULT-dispatch-table.md`.

## Checkpoint — cible titre persistante P1/P2/P0 (2026-08-22)

Le renderer Vulkan conserve désormais la cible titre 1280×720/1× entre les
draws qualifiés et utilise un render pass `LOAD` dédié pour les couches
suivantes. Le replay naturel rend les trois slots exacts du pool vertex
`P1 → P2 → P0`, puis continue cette rotation jusqu'au tick 330 sans trap.
Chaque draw touche 921600 pixels et chaque copy/writeback reste exactement
conforme au différentiel CPU/Vulkan. Les captures non noires évoluent avec la
couleur guest du fade, mais restent des aplats : les IB observés conservent le
même fetch BC3 64×64 à `0x0DF22000` et ne soumettent pas encore le logo.
Les sept tests renderer ciblés passent. `supported=false` reste inchangé.
Preuves :
`artifacts/goal-playable/title-array-writeback-runtime-20260822/` et
`artifacts/goal-playable/title-texture-mutation-frontier-static-20260822/RESULT.md`.

## Checkpoint — payload BANDAI NAMCO qualifié, draw 512 encore sans fragments (2026-08-23)

La soumission naturelle 512×512 atteint désormais le chemin renderer avec un
payload guest non nul ; son dump BC3 décodé montre le mot-symbole BANDAI NAMCO.
La capture native reste néanmoins un aplat bleu : le draw 64×64 compte
921600 fragments, le draw 512×512 en compte zéro. Une A/B `LOAD`/clear et une
A/B du mapping sampler clamp ne changent pas ce résultat. Les builds
codegen-OFF/ON et les probes bornés passent sans trap ni injection. Le
prochain seam est donc le contrat statique fetch → shader BC3 ; aucun pixel
guest n'est encore revendiqué comme visible.

Preuves : `artifacts/goal-playable/brandlogo-texture-dump-runtime-20260823/`,
`artifacts/goal-playable/brandlogo-clamp-runtime-20260823/`.

## Checkpoint — Q writer et index fetch fermés, capture native noire (2026-08-23)

La qualification SDK confirme que `xe_gpu_vertex_fetch_t` fait deux dwords :
`0x4800 + 2*94 = 0x48B8` est bien le slot 94, et son adresse `0x104A4893`
alimente Q1 `0x104A4890`. Le slot n'est donc pas mal indexé. Une trace bornée
des writers titre montre que le record naturel `type=1` de `0x82118D18`
écrit les 208 octets dans P1/P2/P0 seulement; les compteurs Q restent à zéro
et aucun store n'intersecte Q. Le draw qualifié lit alors Q1 nul, produit zéro
fragment et la capture native est noire.

Le dump guest BC3 conserve le mot-symbole BANDAI NAMCO, mais il n'est pas une
capture native. Aucun fallback P→Q, appel forcé ou pixel fabriqué n'est
autorisé. La frontière suivante est le producteur naturel des records type
4/5/6 et leur owner/publication, avec la preuve handle `0x59`/record #2 encore
à fermer.

Preuves : `artifacts/goal-playable/title-vfetch-register-index-static-20260823/RESULT.md`,
`artifacts/goal-playable/brandlogo-q-writer-trace-runtime-20260823/RESULT.md`,
`artifacts/goal-playable/brandlogo-slotfix-runtime-20260823/RESULT.md`.

## Checkpoint — owner logo réinitialisé par le guest (2026-08-23)

La trace bornée `brandlogo-owner-store2-runtime-20260823` suit directement
`0x2E3CED10` entre les ticks 225 et 240. Le chemin naturel
`0x820E5124 → sub_82322D28` remet périodiquement le curseur `owner+0xD8` à
zéro, puis `sub_82322D04` réarme `owner+0xD5`; `sub_82323BB8` republie ensuite
les états transitoires. Cette réinitialisation guest explique pourquoi
l'owner à deux frames reste bloqué à l'index zéro. Aucun changement renderer
ou pixel synthétique n'est justifié. La prochaine fermeture statique porte sur
le résultat de `sub_820D5268` et le producteur du second record logo.

Preuve : `artifacts/goal-playable/brandlogo-owner-store2-runtime-20260823/RESULT.md`.

## Checkpoint — retour resource-index qualifié (2026-08-23)

Le probe `brandlogo-resource-index-runtime2-20260823` ferme le sous-contrat
`0x820D5268`: le slot 6 renvoie `8`, le slot 21 renvoie un descripteur de type
`15`, et la branche statique choisit son champ `+4`. L'index naturel transmis à
l'owner est `0`, puis le chemin guest réinitialise de nouveau `+0xD8`; aucun
record #2/Q n'apparaît. Ne pas modifier Xenos/Vulkan. La prochaine gate est le
producteur statique du record type 4/5/6 et sa publication vers Q.

Preuve : `artifacts/goal-playable/brandlogo-resource-index-runtime2-20260823/RESULT.md`.

## Checkpoint — producteur liste/handle 0x59 non joint (2026-08-23)

La gate statique ferme la ressource `003_NTXR` et le contrat consumer, mais
pas leur publication dans le titre. `0x82323808 → 0x820D18C8 → 0x820D16A8`
est la factory/insertion SWG qualifiée ; aucun xref ne rattache encore
`0x1AA48` à l'owner `0x2E3CED10`. La bascule naturelle exige deux frames,
`+213!=0`, `+214==0`, puis une liste de trois records avec le second en type
2 et handle `0x0E000059`.

Le callback Q `0x82119048` est validé pour types 4/5/6, mais le chemin titre
connu n'en produit pas. Le renderer reste donc correctement fail-closed ; la
prochaine recherche porte sur l'énumérateur/producteur de liste, pas sur
Xenos/Vulkan.

Preuve : `artifacts/goal-playable/brandlogo-frame-owner-static-20260823/RESULT.md`.

## Checkpoint — retour factory SWG borné, payload toujours absent (2026-08-23)

La sonde bornée `swg-factory-return-runtime-20260823` confirme les retours
naturels de `0x820D18C8` à T222/T225 puis T402 : les owners enfants existent et
partagent `0x2E3C3ADC`, mais aucune liste de trois records ni handle `0x59` ne
se matérialise. La borne atteint 500 ticks et 189 presents sans frontend.
Tous les screencaps d'audit restent noirs (`rgb_nonzero=0`), donc le renderer
reste inchangé et fail-closed. Les sondes temporaires ont été retirées après
la capture ; la compilation codegen-ON et les six tests Xenos/Vulkan ciblés
passent.

Preuve : `artifacts/goal-playable/swg-factory-return-runtime-20260823/RESULT.md`.

## Checkpoint — interpréteur SWG et writer Q toujours non joints (2026-08-23)

Les deux recherches statiques parallèles ferment les faux propriétaires :
`0x820E50D8/0x820E5140` ne font que résoudre un index et modifier
`owner+213/+214/+216`, tandis que `0x82119048` reste seulement un candidat
de writer Q1. Aucun chemin qualifié ne relie `0x1AA48` à la factory, au
record #2/handle `0x59`, ni au remplissage naturel de
`[0x104A4890,0x104A4960)`. Le screencap natif reste noir ; renderer et bridge
restent fail-closed.

Preuves : `artifacts/goal-playable/swg-interpreter-static-followup-20260823/RESULT.md`;
`artifacts/goal-playable/title-vertex-writer-followup-20260823/RESULT.md`.

## Checkpoint — route record précoce négative (2026-08-23)

La trace native bornée jusqu'au tick 300 observe 32 appels du worker
`0x820FEFA8`, toujours avec `record_type=0` et `record+0x10c=0`. Aucun appel
à `0x8210A1C0`, `0x82117410` ou `0x820FFCA0` n'apparaît dans la fenêtre
190–430. Le worker titre sonde donc un record vide ; il n'existe toujours pas
de source guest qualifiée qui puisse remplir Q1. Le screencap reste noir et le
renderer ne doit pas être modifié.

Preuve : `artifacts/goal-playable/record-route-early-runtime-20260823/RESULT.md`.

## Checkpoint — matérialisation SWG toujours ouverte (2026-08-23)

La passe statique complémentaire qualifie `0x820E50D8/0x820E5140 →
0x82322D20` comme producteur du curseur/frame actif seulement. `0x82323BB8`
publie et consomme les frames existantes ; il ne crée ni liste, ni record, ni
handle. `0x823246C0` ne fait que dispatcher les opcodes vers les slots virtuels
ASContext. Le record #2/handle `0x0E000059` reste donc sans producteur qualifié
et le renderer reste inchangé.

Preuve : `artifacts/goal-playable/swg-materialization-followup-20260823/RESULT.md`.

## Checkpoint — récepteur SWG qualifié, producteur toujours ouvert (2026-08-23)

La passe statique `swg-slot4-receiver-20260823` qualifie la cellule
`0x820064E8 -> 0x820D18C8` et ses deux callsites naturels
`0x8232342C`/`0x82323594`. Ils transmettent un nouveau nœud et
`param_3`, mais n'écrivent ni `owner+0x20`, ni compteur/liste, ni record #2 ou
handle `0x0E000059`. Le producteur de `global_swg_context+0x10` et la forme de
`param_3` restent donc le seam causal. Aucun changement runtime/renderer n'est
justifié.

Preuve : `artifacts/goal-playable/swg-slot4-receiver-20260823/RESULT.md`.

## Checkpoint — slot 4 SWG exécuté, join titre toujours absent (2026-08-23)

La trace bornée `swg-slot4-trace-runtime-20260823` confirme trois passages
naturels par `0x820D18C8`, puis `0x820D0DB8 → 0x82323808`, avec descripteurs
`0x0B` et `0x01`. Le récepteur est donc vivant, mais aucun type 4/5/6, record
#2, handle `0x0E000059` ou écriture Q1 n'atteint le propriétaire titre. Le
probe finit à 300 ticks (`frontend=false`) ; renderer et screencap restent
inchangés et fail-closed.

Preuve : `artifacts/goal-playable/swg-slot4-trace-runtime-20260823/RESULT.md`.

## Checkpoint — writer du contexte SWG qualifié, join record toujours absent (2026-08-23)

La passe statique `swg-context-writer-static-20260823` ferme les seuls writers
qualifiés de `context+0x10` : initialisation via `0x820CF4E8` et teardown via
`0x820D0A00`. Le lien owner→contexte via `0x820E8B58 → 0x82321E18` est prouvé,
mais aucun raccord vers le descripteur titre, record #2, handle `0x59` ou Q1
n'est établi. Le renderer reste inchangé et le screencap reste noir.

Preuve : `artifacts/goal-playable/swg-context-writer-static-20260823/RESULT.md`.

## Checkpoint — producteur descripteur/blob SWG toujours manquant (2026-08-23)

La passe `swg-descriptor-producer-static-20260823` réfute le pseudo-hit
`0x821846A0/0x821846FC` pour `0x1AA48` : le registre est écrasé avant le
chargement, donc aucun producteur qualifié du descripteur n'est établi.
`0x82326B80` est confirmé comme constructeur de `owner+0x20`, mais le writer
du blob/list (`count`, liens, `record+0x0C`) reste inconnu. Aucun changement
renderer ou guest n'est justifié.

Preuve : `artifacts/goal-playable/swg-descriptor-producer-static-20260823/RESULT.md`.

## Checkpoint — fetch titre qualifié, couverture toujours nulle (2026-08-23)

Le probe Vulkan borné à 430 ticks accepte les profils naturels 64x64 et
512x512 et reçoit bien leurs payloads texture. Le draw titre atteint toutefois
la chaîne Vulkan/RT0 avec quatre vertices Q nuls : `passed_samples=0`,
`rgb_nonzero=0`, 921600 pixels noirs. Aucun nouveau profil renderer n'est
justifié ; la causalité reste le matérialiseur SWG/Q1 guest.

Preuve : `artifacts/goal-playable/title-fetch-trace-runtime-20260823/RESULT.md`.

## Checkpoint — writers provider absents jusqu'au tick 430 (2026-08-23)

La sonde bornée `provider-population-runtime-20260823` appelle explicitement
les writers qualifiés `0x82114350/0x82114798/0x82114A58` et leurs sélecteurs :
aucun événement n'apparaît entre les ticks 190 et 430. Le worker
`0x820FEFA8` reste à `record_type=0` et `record+0x10c=0`; aucun corridor
`0x8210A1C0/0x82117410/0x820FFCA0` n'est atteint. Le readback reste noir
(`rgb_nonzero=0`, `passed_samples=0`) malgré `texture_nonzero=86074`. Le
blocage précède donc la population provider/Q1, sans nouvelle sémantique
Vulkan/Xenos autorisée.

Preuve : `artifacts/goal-playable/provider-population-runtime-20260823/RESULT.md`.

## Checkpoint — writer B5B4/B5B8 isolé, caller non qualifié (2026-08-23)

La passe statique ciblée confirme que `0x82165E68` est le seul writer direct
de `state+0xB5B4/+0xB5B8`, après les prédicats `0x821EE130` et les lectures
`0x821EE0F8` des clés `param_2+0x12/+0x16`. `0x82165490` ne fait que les
consommer sous `state+0x119E4 == 0`; aucun caller virtuel qualifié de
`0x82165E68` ni instance concrète de `param_2` n'est établi. La frontière
guest est donc nommée précisément; aucune modification renderer/kernel n'est
justifiée.

Preuve : `artifacts/goal-playable/provider-state-fields-static-20260823/RESULT.md`.

## Checkpoint — caller qualifié, contrat `param_2` encore manquant (2026-08-23)

`0x82216498` charge le slot `+4` de la vtable primaire `0x8200B63C` et peut
donc atteindre `0x82165E68`. Il transmet pourtant `bVar6` comme second
argument, tandis que `0x82165E68` lit `param_2+0x12/+0x16`; les producteurs et
la représentation binaire de ces champs ne sont pas qualifiés. Une sonde
bornée de cette entrée est en cours; le renderer reste inchangé.

Preuve statique : `artifacts/goal-playable/caller-param2-static-20260823/RESULT.md`.

## Checkpoint — caller `param_2` non atteint dans la fenêtre frontend (2026-08-23)

La trace bornée de `0x82165E68` jusqu'au tick 440 n'observe aucune entrée
naturelle ni valeur pour `param_2+0x12/+0x16`. Elle produit toutefois le même
draw titre avec texture non nulle et Q1 nul : `rgb_nonzero=0`,
`black_pixels=921600`, `passed_samples=0`. Le wrapper a été interrompu après
la fin du processus enfant sans marqueur de statut ; cette passe est
observationnelle et ne ferme aucun gate. Le renderer et les valeurs guest
restent inchangés.

Preuve : `artifacts/goal-playable/caller-param2-runtime-20260823/RESULT.md`.
# État autoritaire courant — cycle 1796 (24 août 2026)

Le gate statique qualifie `0x82323468 -> 0x820D18C8 -> 0x82323808 ->
0x82326B80 -> 0x82323BB8` jusqu'à la sélection déterministe de la frame 0 du
nouvel enfant et au drain éventuel de `MovieMemory` vers `0x82325288`.

L'attribution de `D=0x2DD7936C` à un descripteur constant BrandLogo est
réfutée : la sélection passe par la table relocalisée du storage Title `B`.
Les mots runtime requis n'ont pas été capturés. La frontière exacte est donc
la lecture bornée de `B`, de la commande source, de `D`, de frame 0 et des
offsets VM au tick 3001.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.
# Cycle 1815 — producteur couleur P1 : frontière de dispatch indirecte

Le gate statique parti de `0x82118D18` n'a pas atteint son `done_when` après
cinq lots. `PROUVÉ` : ce chunk type 1 lit le record `r4`, copie `r4+0x18` vers
les quatre couleurs P et consomme aussi `r4+0x48/+0x4C/+0x54`. Aucun autre HIR
ne référence son adresse et l'atlas canonique donne `direct_calls=[]` : la
remontée par appel direct est réfutée, une cellule/table de dispatch indirecte
reste la frontière exacte. Adresse de cellule, index/type, store vers `+0x18`
et provenance amont de `BFFF0000` restent `INCONNU`. Aucun runtime ni changement
renderer. Preuve :
`artifacts/goal-playable/title-p1-color-producer-static-20260824/BLOCKER.md`.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.
# Cycle 1816 — slots exacts du dispatch P1

`PROUVÉ` sur le `xex-basefile.bin` relié explicitement au XEX demo PAL : deux
descripteurs de `0x14` octets contiennent `0x82118D18` à `entry+0x0C`, aux
entrées `0x82009E8C` et `0x82009EB4`. Leurs tuples bruts sont respectivement
`(0000000D,09000001,826F61C0,82118D18,821187A8)` et
`(0000000D,09000003,826F61C0,82118D18,821187A8)`. Aucune branche PPC directe
ne cible le writer et aucune instruction ne construit les cellules exactes :
le consommateur charge une base et indexe la table. Callsite, règle d'index,
type de `r4`, store `r4+0x18` et provenance de `BFFF0000` restent ouverts.
Preuve :
`artifacts/goal-playable/title-p1-dispatch-xref-static-20260824/BLOCKER.md`.
Aucun runtime ni changement renderer. `frontend=false`, `mission=false`,
`terminal=false`, `supported=false`.
# Gate retail US — échec pré-codegen SSSE3 (26 août 2026)

Le manifeste US exact est préparé. L'unique tentative lourde s'arrête avec
Clang 21 dans `rexcore/memory.cpp` : `_mm_shuffle_epi8` requiert SSSE3, absent
des options du target. Aucun codegen, binaire ou run gameplay n'a suivi. La
surcouche contient le correctif CMake ciblé `-mssse3`, non rebâti dans ce gate.
PAL reste bloqué.

Preuve : `reports/retail-us-build-ssse3-boundary.md`.

# Gate retail US — Vulkan valide, synchronisation de route absente (26 août 2026)

Le correctif SSSE3 a permis l'unique génération et le build Clang 21/C++23.
Les 16 tests ciblés passent, ReXGlue Vulkan et SDL dummy sont liés, D3D12 et
`bin/bin` sont absents. La session corrigée présente le dialogue de données de
jeu sans trap fatale mais expire à 4/96 étapes : les marqueurs `type28`,
`selector44`, `state40` et campagne de la route scellée ne sont pas publiés par
le host US stock. PAL reste bloqué ; le gate courant est le cross-match Ghidra
US des quatre fonctions de synchronisation candidates. Aucun nouveau runtime
ou rebuild avant cette qualification.

Preuve : `reports/retail-us-vulkan-route-sync-boundary.md`.

# Gate retail US — observables qualifiés, route échouée à 10/96 (26 août 2026)

Le projet canonique `ghidra-projects/ac6-us` qualifie les quatre fonctions de
synchronisation et leurs étendues `.pdata` depuis le XEX US exact. Les wrappers
read-only, la carte de hooks et le relink sans codegen passent les 16 tests
statiques ; le binaire installé est `44e75813…95d30c`.

La session unique observe `type28=30`, puis `37`, mais l'état suivant est `36`
et non le `35` exigé. Résultat : 10/96 étapes, zéro capture, aucune trap/audio
fatale, arrêt non propre après l'échec de prédicat. Le gate US est fermé en
échec et PAL reste bloqué. Aucun second run, A/B ou rebuild n'est autorisé.

Preuve : `reports/retail-us-route-type35-failed-closed.md`.

# Gate retail US — frontière RT/resolve du monde noir (26 août 2026)

La route Mission 01 termine proprement avec 24 captures et sans panneau. La
cinématique rend le monde, puis le HUD apparaît sur fond noir. Au premier HUD
noir, CModeTaskGame et les phases objet/caméra/radio restent actives et le
backend reçoit ~1 200 draws et 85–87 resolves. La chute ultérieure des draws
suit l'entrée Escape/Start et n'est pas la cause initiale. Le défaut est borné
à RT/resolve/frontbuffer. Une journalisation détaillée, read-only côté invité,
est préparée mais non rebuildée. PAL reste bloqué.

Preuve : `reports/retail-us-black-world-resolve-boundary-20260826.md`.

# Gate retail US — observable resolve rebuildé (26 août 2026)

Le relink unique réutilise le codegen existant et termine avec statut 0. Le
binaire `98bf39a9…37e4e3` est validé Vulkan/SDL dummy, sans D3D12 ni `bin/bin`,
et contient les détails RT/resolve/frontbuffer requis. Aucun runtime n'est
lancé dans cette session. PAL reste bloqué jusqu'à la prochaine corrélation.

Preuve : `reports/retail-us-resolve-observable-rebuild-20260826.md`.

## Retail US — panneau diagnostics et validation bornée de `37 -> 35` (26 août 2026)

Le diagnostic 15/15 atteint `type28=37` avec `NO`, applique `Left`, capture
`YES`, valide puis atteint `type28=35`; arrêt propre et zéro fatal. Le panneau
hôte reste toutefois visible, car sa fermeture avait précédé sa création
ImGui. La fermeture a été resynchronisée au premier point de capture, mais la
tentative corrective s'est arrêtée avant tout `type28` après 1 725 `PRESENT`
et 4/15 étapes. Aucun PNG sans panneau n'a été obtenu. PAL reste bloqué.

Preuve : `artifacts/retail-us-type37-clean-20260826/RESULT.json` et
`artifacts/retail-us-type37-clean-v2-20260826/RESULT.json`.

# Gate retail US — captures `30/37/36`, route révisée non atteinte (26 août 2026)

L'extension autorisée capture les trois écrans et prouve que `37` propose la
création avec `NO` sélectionné ; `space` menait donc correctement à
l'avertissement `36`. Le `Left` inutile sur l'écran `30/OK` est déplacé après
`37`, et la route reste à 96 étapes.

La validation complète révisée ne dépasse pas le démarrage : 841 `PRESENT`,
puis arrêt des présentations, zéro marqueur `type28`, 4/96 étapes en 913 s,
aucun fatal/audio. La correction de sélection n'est donc pas encore validée en
route complète et PAL reste bloqué.

Preuve : `reports/retail-us-route-type35-failed-closed.md`.
# Gate retail US — frontière intermédiaire Vulkan préparée (26 août 2026)

Le runtime unique produit 24 captures sans panneau : cinématique 3D visible,
puis HUD sur monde noir. Les deux états conservent les mêmes resolves principal
et final et le même frontbuffer ; la cause est désormais bornée à une passe ou
ressource intermédiaire Vulkan. Le processus a dépassé la fenêtre de nettoyage
après `Execution complete` et le reçu reste donc en échec. Une sonde hôte
opt-in, agrégée et échantillonnée est prête avec 22 tests légers, sans rebuild
dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-intermediate-resolve-boundary-20260826.md.
# Gate retail US — observable frontière Vulkan validé (26 août 2026)

Le rebuild unique sans codegen termine sous cgroup avec statut 0 et installe
`b854c984…b4de0`. La validation statique passe : Vulkan complet, SDL dummy,
zéro D3D12, pas de `bin/bin`, et les trois marqueurs hôte sont liés. La sonde
et le panneau diagnostics restent désactivés par défaut. Aucun runtime n'a été
lancé dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-vulkan-frontier-observable-rebuild-20260826.md.
# Gate retail US — sortie fragment D5B4 bornée (26 août 2026)

Le runtime unique termine proprement avec 24 captures sans panneau, zéro fatal
et le HUD noir reproduit. La transition conserve 378 draws monde D5B4, leur RT
et 65 resolves ; disparition du pass et perte globale de composition sont
réfutées. Un override blanc limité à la sortie finale D5B4 est préparé, false
par défaut, avec 23 tests légers ; il n'est pas rebuildé dans cette session.
PAL reste bloqué.

Preuve : reports/retail-us-vulkan-frontier-runtime-20260826.md.
# Gate retail US — diagnostic final-white D5B4 validé (26 août 2026)

Le rebuild unique réutilise le codegen et termine sous cgroup avec statut 0.
Le binaire installé `0f208aa3…5c4c` lie la cvar et le marqueur final-white ; la
validation confirme Vulkan complet, SDL dummy, zéro D3D12 et aucun `bin/bin`.
L'override et le panneau restent désactivés par défaut. Aucun runtime n'a été
lancé dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-final-output-rebuild-20260826.md.
# Gate retail US — final-white négatif, depth/stencil borné (26 août 2026)

Le runtime final-white unique termine proprement avec 24 captures sans panneau,
zéro fatal et marqueur confirmé. Les trois HUD restent sur monde noir et le
centre vaut 0.0 : la valeur finale D5B4 n'est pas la cause suffisante. Un bypass
depth/stencil limité à D5B4 est préparé, false par défaut, avec 24 tests légers,
sans rebuild dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-final-white-runtime-20260826.md.
# Gate retail US — bypass depth/stencil D5B4 validé (26 août 2026)

Le rebuild unique réutilise le codegen et termine sous cgroup avec statut 0.
Le binaire installé `d00b9022…eb1c` lie la cvar et le marqueur depth/stencil ;
la validation confirme Vulkan complet, SDL dummy, zéro D3D12 et aucun
`bin/bin`. Tous les diagnostics et le panneau restent false par défaut. Aucun
runtime n'a été lancé dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-depth-stencil-rebuild-20260826.md.
# Gate retail US — depth/stencil négatif, culling borné (26 août 2026)

Le runtime depth/stencil unique termine proprement avec 24 captures sans
panneau, zéro fatal et les deux marqueurs confirmés. Le monde reste noir avec
un centre à 0.0 : le rejet Z/stencil n'est pas la cause suffisante. Un bypass
culling limité à D5B4 est préparé, false par défaut, avec 25 tests légers, sans
rebuild dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-depth-stencil-runtime-20260826.md.
# Gate retail US — bypass culling D5B4 validé (26 août 2026)

Le rebuild unique réutilise le codegen et termine sous cgroup avec statut 0.
Le binaire installé `8600824e…97d1` lie la cvar et le marqueur culling ; la
validation confirme Vulkan complet, SDL dummy, zéro D3D12 et aucun `bin/bin`.
Tous les diagnostics et le panneau restent false par défaut. Aucun runtime n'a
été lancé dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-cull-rebuild-20260826.md.
# Gate retail US — culling négatif, fenêtre raster bornée (26 août 2026)

Le runtime culling unique termine proprement avec 24 captures sans panneau,
zéro fatal et trois marqueurs confirmés. Le monde reste noir avec un centre à
0.0 : le culling n'est pas la cause suffisante. Le catalogue read-only est
enrichi avec viewport/scissor par passe, avec 25 tests légers, sans rebuild
dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-cull-runtime-20260826.md.
# Gate retail US — observable fenêtre raster validé (26 août 2026)

Le rebuild unique réutilise le codegen et termine sous cgroup avec statut 0.
Le binaire installé `4b4a7140…41af` lie viewport/scissor dans le catalogue de
passes ; la validation confirme Vulkan complet, SDL dummy, zéro D3D12 et aucun
`bin/bin`. L'observable est read-only, panneau et overrides false par défaut.
Aucun runtime n'a été lancé dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-raster-window-rebuild-20260826.md.
# Gate retail US — runtime fenêtre raster fermé au hangar (26 août 2026)

Le runtime unique termine techniquement proprement avec 24 captures et sans
panneau vert, mais les captures 70 à 88 sont strictement identiques au premier
hangar. La route n'a atteint ni carte, ni cinématique, ni HUD. Le filtre lourd
n'a émis aucune passe D5B4. Le harness rejette désormais ce faux positif et le
catalogue read-only inclut explicitement les frames D5B4. 26 tests légers
passent ; aucun rebuild n'a suivi le runtime. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-raster-window-runtime-20260826.md.
# Gate retail US — catalogue D5B4 inclusif validé (26 août 2026)

Le rebuild/relink unique réutilise le codegen et termine avec statut 0. Le
binaire installé `bec7412e…c75ac` inclut toute frame D5B4 dans le catalogue
viewport/scissor ; la validation confirme Vulkan complet, SDL dummy, zéro
D3D12 et aucun `bin/bin`. Le panneau et les overrides restent false par défaut.
Aucun runtime n'a été lancé dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-inclusive-catalog-rebuild-20260826.md.
# Gate retail US — startup arrêté pendant le brand movie (26 août 2026)

Le runtime unique échoue fermé à l'opération 5, sans fatal/trap ni capture de
route. Une observation bornée montre le générique 3D « PRODUCED BY NAMCO
BANDAI », sans panneau vert. Les 180 PRESENT surviennent vers 8 secondes et ne
prouvent pas le menu prêt ; l'entrée arrête la présentation avant `type28=30`.
Le harness attend désormais 45 secondes avant toute entrée. 26 tests légers
passent ; aucun rebuild n'a suivi. PAL reste bloqué.

Preuve : reports/retail-us-brand-movie-startup-stall-20260826.md.
# Gate retail US — settle 45 s manque le titre (26 août 2026)

Le runtime unique ne relève aucun fatal/trap mais émet 1 830 PRESENT puis se
fige sur une transition noire, sans `type28=30`, capture de route ou D5B4. La
fenêtre est fermée après l'observation causale et le jeu retourne 0. Le settle
après 180 PRESENT place l'entrée vers 53 s, hors de la fenêtre historique. Le
harness emploie désormais Escape à 35 s, A à 42 s, puis une attente passive.
26 tests légers passent ; aucun rebuild. PAL reste bloqué.

Preuve : reports/retail-us-brand-movie-settle-runtime-20260826.md.
# Gate retail US — fenêtre titre seule expire (26 août 2026)

Le runtime unique continue à présenter 3 499 frames mais expire après 70 s et
cinq opérations, sans `type28=30`, capture de route, fatal/trap ou D5B4. La
paire 35/42 s seule est insuffisante sur ce lancement. Le harness conserve cette
fenêtre puis applique au plus 60 s de retries conditionnels, arrêtés par l'état
invité exact ; un échec produit désormais une capture automatique. 27 tests
légers passent ; aucun rebuild. PAL reste bloqué.

Preuve : reports/retail-us-title-window-runtime-20260826.md.
# Gate retail US — D5B4 valide, composition bornée (26 août 2026)

Le runtime unique franchit startup et hangar, exécute 88/88 opérations, produit
24 captures et s'arrête proprement sans fatal/trap ni panneau. La cinématique
3D est visible, puis le HUD repose sur un monde noir. 44 échantillons D5B4 ont
des fenêtres non vides couvrant la cible pitch 640. Le resolve final vers
`0x1AB60000` persiste ; `0x1C191000` et `0x1B9C0000` passent de 7→1 et 7→4.
La prochaine frontière est statique, dans la chaîne de composition. PAL reste
bloqué.

Preuve : reports/retail-us-d5b4-window-valid-compose-boundary-20260826.md.
# Gate retail US — propriétaires Resolve qualifiés (26 août 2026)

La statique US qualifie `0x82337C68`/LR `0x82337CC4` comme premier propriétaire,
`0x8234D550`/LR `0x8234D5F4` comme dernier, et `0x821E2BB8` comme ABI Resolve.
Les records intermédiaires détaillés étaient volatils. Un catalogue hôte
read-only persiste désormais ordinal, LR, arguments et shadow state sur les
mêmes samples bornés que Vulkan. 28 tests passent ; aucun job lourd n'a été
lancé. PAL reste bloqué.

Preuve : reports/retail-us-compose-resolve-static-qualification-20260826.md.
# Gate retail US — catalogue Resolve aligné validé (26 août 2026)

Le rebuild/relink unique réutilise le codegen et termine avec statut 0. Le
binaire installé `f77554a7…572ea` lie le catalogue sample/ordinal/LR/arguments/
shadow state et passe la validation complète. 28 tests légers, Vulkan, SDL
dummy, zéro D3D12 et aucun `bin/bin` sont confirmés. Aucun runtime n'a été lancé
dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-compose-resolve-catalog-rebuild-20260826.md.

# Gate retail US — divergence Resolve nommée (26 août 2026)

Le runtime unique exécute 88/88 opérations et produit 24 captures, mais échoue
fermé au teardown (`game_status=-2`). La cinématique 3D devient un HUD sur monde
noir. `0x1C191000` passe 7→1 et `0x1B9C0000` 7→4 tandis que le frontbuffer final
persiste. La couture invitée est LR `0x8234D5F4` dans `0x8234D550` : ses objets
640×360/1280×720 disparaissent au profit de 208×144/320×360. Le panneau hôte de
l'ancien artefact type37 est absent du run courant. PAL reste bloqué.

La statique précise que `0x8234D550` résout `*(r3+0)+28` sans sélectionner
l'objet. Les sélecteurs directs sont `0x8234EF60` (liste `manager+36`) et
`0x8234F558` (entrée de 304 octets indexée par `state+20`).

Preuve : reports/retail-us-compose-resolve-runtime-20260826.md.

# Gate retail US — relink owner parent échoué fermé (26 août 2026)

La statique réduit la provenance à LR parent `0x8234F0E8` (liste
`0x8234EF60`) ou `0x8234F598` (entrée indexée `0x8234F558`). Le catalogue
read-only `owner_lr` est prêt. L'unique relink échoue avec statut 1 car la
synchronisation avait retiré de la copie build le hunk legacy
`GetFrameCaptureSummary()` ; ce hunk est restauré après l'échec. Aucun second
job lourd n'est lancé, le binaire installé validé reste intact et PAL bloqué.

Preuve : reports/retail-us-compose-owner-rebuild-failed-20260826.md.

# Gate retail US — catalogue owner parent validé (26 août 2026)

Le relink unique corrigé termine avec statut 0, sans codegen. Le binaire
build/install `345dca09…e4f61`, 37 686 960 octets, passe la validation complète
et contient `owner_lr`. Le catalogue distingue désormais `0x8234F0E8` de
`0x8234F598` sans écriture invitée. Aucun runtime n'est lancé dans cette
session ; PAL reste bloqué.

Preuve : reports/retail-us-compose-owner-relink-20260826.md.

# Gate retail US — runtime owner arrêté au film de marque (26 août 2026)

Le runtime unique échoue fermé après 117,9 s et 5/88 opérations, sans
`type28=30`, fatal ou trap. La capture montre le film Namco Bandai 3D, panneau
absent ; aucun sample Mission 01 `owner_lr` n'est collecté. Les reçus réussis
antérieurs montrent que `type28=30` précède l'appui A fixe. Le harness pulse
maintenant uniquement Escape après 20 s, conditionnellement pendant 90 s, puis
réserve A à l'état invité exact. PAL reste bloqué.

Preuve : reports/retail-us-compose-owner-startup-failure-20260826.md.

# Gate retail US — runtime owner corrélé proprement (26 août 2026)

Le runtime borné sur `345dca09…e4f61` termine avec `85/85` opérations,
24 captures, `game_status=0`, `xvfb_status=0` et `clean_shutdown=true`, sans
fatal/trap. `step-77` conserve une cinématique 3D; `step-79/82/85` montrent le
HUD invité sur monde noir. Le panneau hôte est absent.

Le catalogue read-only attribue les surfaces intermédiaires à
`owner_lr=0x8234F0E8` (`0x8234EF60`) et le resolve final à
`owner_lr=0x8234F598` (`0x8234F558`), vers `0x1AB60000`. D5B4 reste soumis après
la transition noire; la frontière active est `ResolveInfo`/ownership/dump.
PAL reste bloqué.

Preuve : reports/retail-us-compose-owner-runtime-20260826.md.

# Gate retail US — `ResolveInfo` statique préparé (26 août 2026)

La chaîne active est maintenant bornée à `GetResolveInfo` →
`DumpRenderTargets` → copie EDRAM/shared memory → `MarkRangeAsResolved` →
`RequestSwapTexture`. Une instrumentation read-only
`ac6_log_resolve_info` consigne les champs source/destination et la plage
EDRAM (8 premiers resolves puis 1/256). Elle est présente dans les copies
upstream et build mais n'a pas encore été exécutée. Un seul rebuild/runtime
reste autorisé dans une session fraîche; PAL reste bloqué.

Preuve : reports/retail-us-resolve-info-static-20260826.md.

# Gate retail US — runtime `ResolveInfo`/ownership (26 août 2026)

Le runtime instrumenté termine proprement avec 85/85 opérations et 24
captures. Chaque span d'ownership échantillonné sélectionne un rectangle de
dump (`rectangles=1`), tandis que D5B4 persiste après la première capture noire.
`world_center_mean=0`; le reçu gameplay US n'est pas atteint. La couture active
est désormais `RequestSwapTexture` → `LoadTextureData`, instrumentée dans les
copies source/build par `[ac6-swap-texture]`. PAL reste bloqué.

Preuve : reports/retail-us-resolve-info-runtime-20260826.md.

# Gate retail US — texture de swap fermée, transition manager restante (26 août 2026)

Le rebuild ciblé et la validation passent avec le binaire
`bf80b852…cca56a0a`. La route render-summary atteint 85/85 opérations, 24
captures, le vol HUD, et s'arrête proprement sans fatal/trap ni panneau hôte.
Les marqueurs `[ac6-swap-texture]` restent `ok` et
`[ac6-texture-load]` montre `prepared → commit-ok` pour
`0x1AB60000+0x398000`, avec `outdated=0x1` avant chargement et `0x0` après.

La couture texture/présentation est donc réfutée comme cause suffisante. Au
sample 2460/frame 12683, le manager passe à `flags8=0x4` et HSM
`0x822EB1B0`, mais les frames conservent encore 1 600–1 850 draws et 82–87
resolves ; le passage ultérieur à `0x822E71A0` et les deltas nuls sont
postérieurs au premier noir. La frontière revient au contenu des surfaces
intermédiaires `0x1C191000`/`0x1B9C0000` avant `0x1AB60000`. PAL reste bloqué.

Preuve : reports/retail-us-texture-load-runtime-20260826.md.

# Gate retail US — sonde D5B4 reportée par faux rejet statique (26 août 2026)

La route `retail-us-d5b4-texture-runtime-20260826` s'est terminée proprement,
mais le filtre D3D12 de `validate.py` a rejeté à tort l'adresse hexadécimale
`0x00d3d120`; l'installation est donc restée sur l'ancien binaire et le
marqueur D5B4 n'a pas été consommé. Le filtre est corrigé et testé (31 tests),
le nouveau binaire `dce792bd…7691` est installé avec le marqueur. PAL reste
bloqué ; un seul runtime frais est requis, sans rebuild.

Preuve : reports/retail-us-d5b4-texture-runtime-20260826.md.

# Gate retail US — retry sonde D5B4 déplacé au point consommateur (27 août 2026)

- PROUVÉ : le runtime frais avec `dce792bd…7691` a terminé `85/85`, 24
  captures, statut 0, arrêt propre, zéro fatal/trap et panneau hôte absent.
- PROUVÉ : les passes `[ac6-frontier-pass]` sont présentes, mais ni le hash
  pixel shader D5B4 (`D5B4F4A878949938`) ni la sonde placée après
  `RequestTextures` n'apparaissent dans le log.
- CORRIGÉ STATIQUEMENT : le marqueur read-only est maintenant placé juste
  après la construction `Ac6FrontierPass` pour le pixel shader D5B4, dans les
  copies upstream et build.
- PROCHAIN : un seul rebuild/relink sous cgroup, puis une seule route fraîche
  pour consommer la sonde. PAL reste bloqué par le reçu US de gameplay.

Preuve : reports/retail-us-d5b4-texture-runtime-retry-20260827.md.

# Gate retail US — rebuild sonde D5B4 relocalisée (27 août 2026)

- PROUVÉ : rebuild/relink ciblé sous cgroup, statut 0, sans nouvelle
  génération C++.
- PROUVÉ : validation statique pass, Vulkan/ReXGlue, SDL dummy, zéro D3D12,
  `bin/bin` absent ; binaire installé `6db8d006…0768`.
- PROUVÉ : le binaire contient `[ac6-d5b4-texture]` et le shader
  `D5B4F4A878949938`; 32 tests ciblés passent.
- PROCHAIN : une seule route `render-summary` fraîche dans une session lourde
  séparée pour établir l'atteinte du draw D5B4. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-texture-rebuild-20260827.md.

# Gate retail US — sonde texture D5B4 consommée (27 août 2026)

- PROUVÉ : route fraîche avec `6db8d006…0768`, `85/85`, 24 captures, statut
  0, arrêt propre, zéro fatal/trap et panneau hôte absent.
- PROUVÉ : `[ac6-d5b4-texture]` atteint 3 472 fois ; les fetchs sont des
  textures tuilées format 20 (`k_DXT4_5`), `used_mask=0x1`, bases guest
  variables autour de `0x06A40000`–`0x07601000`.
- NON RÉSOLU : `world_center_mean=0` persiste après la cinématique ; ce n'est
  pas encore le reçu gameplay US.
- PRÉPARÉ : le marqueur read-only consigne maintenant masque couleur brut et
  normalisé, rasterisation et mode EDRAM pour qualifier la cible D5B4.
- PROCHAIN : un seul rebuild/relink puis une seule route fraîche. PAL reste
  bloqué.

Preuve : reports/retail-us-d5b4-texture-runtime-20260827.md.

# Gate retail US — rebuild observables masque/cible D5B4 (27 août 2026)

- PROUVÉ : rebuild/relink ciblé sous cgroup, statut 0, sans génération C++.
- PROUVÉ : validation statique pass ; Vulkan/ReXGlue, SDL dummy, zéro D3D12,
  `bin/bin` absent et 32 tests ciblés.
- PROUVÉ : binaire installé `24b8d603…9856`, avec le marqueur D5B4 étendu aux
  masques couleur brut/normalisé, rasterisation et mode EDRAM.
- PROCHAIN : une seule route fraîche en session lourde séparée ; PAL reste
  bloqué jusqu'au reçu gameplay US.

Preuve : reports/retail-us-d5b4-target-mask-rebuild-20260827.md.

# Gate retail US — masque logique D5B4 fermé (27 août 2026)

- PROUVÉ : route `24b8d603…9856`, `85/85`, 24 captures, arrêt propre,
  `game_status=0`, zéro fatal/trap et panneau hôte absent.
- PROUVÉ : 3 406 échantillons D5B4 ont tous `RB_COLOR_MASK=0xF`,
  `normalized_color_mask=0xF`, `raster=1`, `edram=4`.
- RÉFUTÉ : masque couleur logique et rasterisation comme causes suffisantes du
  monde noir.
- PRÉPARÉ : observables read-only du chemin Vulkan, de la clé render pass et
  des bits d’attachement.
- PROCHAIN : un seul rebuild/relink puis une seule route fraîche ; PAL reste
  bloqué.

Preuve : reports/retail-us-d5b4-target-mask-runtime-20260827.md.

# Gate retail US — rebuild attachement hôte D5B4 (27 août 2026)

- PROUVÉ : rebuild/relink ciblé sous cgroup, statut 0, sans génération C++.
- PROUVÉ : validation statique pass ; Vulkan/ReXGlue, SDL dummy, zéro D3D12,
  `bin/bin` absent ; ctest 16/16 et suite Python 32/32.
- PROUVÉ : binaire installé
  `b6497489…45eaa` (37 713 112 octets), avec chemin Vulkan, clé render pass
  et bits d’attachement ajoutés au marqueur D5B4.
- PROCHAIN : une seule route `render-summary` fraîche dans une session lourde
  séparée ; le reçu gameplay US manque encore et PAL reste bloqué.

Preuve : reports/retail-us-d5b4-host-target-rebuild-20260827.md.

# Gate retail US — échec startup avant D5B4 (27 août 2026)

- ÉCHEC FERMÉ : la route `render-summary` avec
  `b6497489…45eaa` s’est arrêtée à l’opération 2 (`type28=30` non atteint),
  après 119,7 s, avec `game_status=-9`, `clean_shutdown=false` et zéro
  fatal/trap.
- PROUVÉ : seuls 165 `PRESENT` du logo Bandai Namco ont été émis ; aucun
  `type28`, `selector44`, `state40`, frontier ou D5B4 n’a été atteint.
- PROUVÉ : la capture d’échec ne montre aucun panneau de diagnostic vert.
- DÉCISION : aucune conclusion renderer ni rebuild n’est autorisé sur cette
  session ; un seul retry startup avec le même binaire reste à consommer dans
  une session lourde distincte.

Preuve : reports/retail-us-d5b4-host-target-runtime-startup-failure-20260827.md.

# Gate retail US — retry host-target D5B4 (27 août 2026)

- PROUVÉ : le retry frais avec `b6497489…45eaa` a exécuté `85/85` opérations,
  produit 24 captures, terminé avec statut 0, arrêt propre et zéro
  fatal/trap.
- PROUVÉ : D5B4 est consommé 3424 fois ; toutes les lignes ont
  `render_path=0`, `render_pass=0x8D`, `attachments=0x3`,
  `RB_COLOR_MASK=0xF`, `normalized_color_mask=0xF`, `raster=1`, `edram=4`.
- RÉFUTÉ : cible/attachement hôte manquant et masque couleur comme causes
  suffisantes du monde noir.
- NON RÉSOLU : la cinématique (`step-77`) montre la 3D, mais les captures
  post-cinématique 79/82/85 restent HUD/radar sur monde noir
  (`world_center_mean=0`).
- PROCHAIN : qualification statique de la couture composition/world-content,
  puis au plus un rebuild/relink et un runtime frais si une divergence causale
  est prouvée. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-host-target-runtime-retry-20260827.md.

# Gate retail US — qualification composition/world-content (27 août 2026)

- QUALIFIÉ STATIQUEMENT : `0x82337C68`/`0x82337CC4` et
  `0x8234D550`/`0x8234D5F4` conservent les destinations de composition
  canoniques ; aucun override de cible n'est présent.
- QUALIFIÉ STATIQUEMENT : la chaîne Vulkan est
  `GetResolveInfo` → `DumpRenderTargets` → compute resolve →
  `MarkRangeAsResolved`, sous `Path::kHostRenderTargets`.
- PRÉPARÉ : le marqueur D5B4 read-only couvre maintenant bases/formats
  couleur-profondeur, pitch, MSAA, viewport et scissor, dans upstream et la
  copie build ignorée.
- PROCHAIN : un unique relink/rebuild ciblé dans une session lourde neuve,
  validation statique, puis un runtime frais dans une session distincte.
  PAL reste bloqué.

Preuve : reports/retail-us-compose-world-content-static-qualification-20260827.md.

# Gate retail US — relink sonde composition/world-content (27 août 2026)

- PROUVÉ : relink/rebuild ciblé sous cgroup, statut 0, avec réutilisation de
  `generated/sources.cmake` et sans nouvelle génération C++.
- PROUVÉ : validation statique pass, ctest 16/16, Vulkan/ReXGlue, SDL dummy,
  zéro D3D12 et `bin/bin` absent.
- PROUVÉ : binaire installé
  `43a35a00…e313a`, contenant la sonde D5B4 étendue aux bases/formats,
  pitch, MSAA, viewport et scissor.
- PROCHAIN : une seule route runtime fraîche dans une session lourde distincte,
  puis comparaison de la première divergence composition/world-content.
  PAL reste bloqué.

Preuve : reports/retail-us-compose-world-content-probe-relink-20260827.md.

# Gate retail US — runtime sonde composition/world-content (27 août 2026)

- ÉCHEC FERMÉ DU REÇU : la route a exécuté `85/85` et produit 24 captures,
  mais le teardown finit `clean_shutdown=false`, `game_status=-2`, sans erreur
  ni fatal/trap ; ce n'est pas un reçu gameplay.
- PROUVÉ : avant/après le noir, D5B4 conserve
  `color_base=0`, `depth_base=720`, pitch 640, MSAA 2×, render pass `0x8D` et
  attachements `0x3` ; les frames gardent une charge élevée de draws/resolves.
- NON RÉSOLU : les captures 79/82/85 restent HUD/radar sur monde noir après la
  cinématique 3D.
- PRÉPARÉ : cvar read-only `ac6_log_resolve_content` et option
  `--mission-resolve-content` pour échantillonner la plage finale
  `0x1AB60000` après readback borné.
- PROCHAIN : build/relink ciblé de cette sonde dans une session lourde neuve,
  validation statique, puis runtime séparé. PAL reste bloqué.

Preuve : reports/retail-us-compose-world-content-probe-runtime-20260827.md.

# Retail US — build sonde resolve-content (27 août 2026)

- PROUVÉ : build/relink ciblé sous cgroup, statut 0, avec réutilisation de
  `generated/sources.cmake` et sans nouvelle génération C++.
- PROUVÉ : validation statique pass, ctest 16/16, Vulkan/ReXGlue, SDL dummy,
  zéro D3D12 et `bin/bin` absent.
- PROUVÉ : binaire installé
  `4f8142aa…0784a`, intégrant la sonde read-only du readback final
  `0x1AB60000` activée par `--mission-resolve-content`.
- PROCHAIN : une seule route runtime fraîche dans une session lourde séparée ;
  gameplay US non validé et PAL fermé.

Preuve : reports/retail-us-resolve-content-probe-build-20260827.md.

# Retail US — runtime sonde resolve-content (27 août 2026)

- PROUVÉ : runtime séparé terminé proprement (`exit=0`, `85/85`, 24
  captures, `game_status=0`, zéro fatal/trap).
- PROUVÉ : `step-77` conserve la 3D ; les étapes 79/82/85 restent HUD vert
  sur monde noir, sans panneau hôte.
- PROUVÉ : le contenu readback final riche tombe à `3448` octets non nuls et
  `byte_sum=6108` au moment du noir.
- QUALIFIÉ : les destinations 160×90 alternent entre `0x1AB60000` et
  `0x1B9C0000`; le fetch swap courant n'est pas encore corrélé.
- PROCHAIN : trace ciblée swap-source/texture-load/décision swap dans une
  session lourde unique ; gameplay US non validé et PAL fermé.

Preuve : reports/retail-us-resolve-content-probe-runtime-20260827.md.

# Retail US — qualification sonde resolve double-target (27 août 2026)

- QUALIFIÉ : `RequestSwapTexture` et les loads hôte restent sur la clé
  `0x1AB60000`, invalidée/rechargée avec succès ; le contenu de cette clé
  tombe à `3448` octets non nuls au HUD noir.
- PRÉPARÉ : la sonde read-only couvre désormais aussi `0x1B9C0000`, l'autre
  destination 1280×720 observée dans les `ResolveInfo` alternés.
- PROCHAIN : un seul relink/rebuild sous cgroup, validation statique, puis un
  runtime séparé pour comparer les deux buffers. Gameplay US non validé et
  PAL fermé.

Preuve : reports/retail-us-resolve-content-dual-target-static-20260827.md.

# Retail US — build sonde resolve double-target (27 août 2026)

- PROUVÉ : build/relink ciblé sous cgroup, statut 0, avec réutilisation de
  `generated/sources.cmake` et sans nouvelle génération C++.
- PROUVÉ : validation statique pass, ctest 16/16, Vulkan/ReXGlue, SDL dummy,
  zéro D3D12 et `bin/bin` absent.
- PROUVÉ : binaire installé
  `9cf04b15…e06e9`, sonde read-only couvrant `0x1AB60000` et `0x1B9C0000`.
- PROCHAIN : une seule session runtime fraîche pour comparer les deux buffers;
  gameplay US non validé et PAL fermé.

Preuve : reports/retail-us-resolve-content-dual-target-build-20260827.md.

# Retail US — runtime sonde resolve double-target (27 août 2026)

- PROUVÉ : runtime séparé terminé avec `exit=0`, arrêt propre, `85/85`, 24
  captures, `game_status=0`, zéro fatal/trap, binaire
  `9cf04b15…e06e9`.
- PROUVÉ : `step-77` conserve la 3D ; les étapes 79/82/85 restent HUD/radar
  vert sur monde noir, sans panneau hôte.
- QUALIFIÉ : les deux plages `0x1AB60000` et `0x1B9C0000` contiennent des
  données pendant la transition ; le fetch et les loads de présentation
  restent fixés sur `0x1AB60000`.
- PREMIÈRE COUTURE STATIQUE : `kComputeWrite` déclare encore
  `VK_ACCESS_SHADER_READ_BIT` alors que le resolve compute écrit le storage
  buffer. Correction minimale à qualifier avant tout nouveau runtime.
- PROCHAIN : patch source miroir upstream/build, unique rebuild/relink sous
  cgroup, validation statique, puis runtime séparé. PAL reste bloqué.

Preuve : reports/retail-us-resolve-content-dual-target-runtime-20260827.md.

# Retail US — rebuild compute-write barrier (27 août 2026)

- PROUVÉ : rebuild/relink ciblé sous cgroup, statut 0, sans nouvelle
  génération C++ (`generated/sources.cmake` réutilisé).
- PROUVÉ : validation statique pass, ctest 16/16, Vulkan/ReXGlue, SDL dummy,
  zéro D3D12 et `bin/bin` absent.
- PROUVÉ : binaire installé
  `c159cbed…a1496e1`, avec `Usage::kComputeWrite` déclaré en
  `VK_ACCESS_SHADER_WRITE_BIT` dans upstream et copie build.
- PROCHAIN : runtime unique séparé avec la même route et les 24 captures pour
  vérifier la visibilité du monde ; PAL reste bloqué.

Preuve : reports/retail-us-compute-write-barrier-build-20260827.md.

# Retail US — runtime après correction compute-write (27 août 2026)

- PROUVÉ : session runtime unique terminée après `85/85` et 24 captures ;
  `xvfb_status=0`, aucune fatal/trap et journal complet jusqu'à
  `Execution complete`.
- QUALIFIÉ : `game_status=-9` et `clean_shutdown=false` sont la course d'arrêt
  du processus pendant la fermeture, pas un crash invité ; les artefacts sont
  exploitables et aucun retry de teardown n'est autorisé.
- RÉFUTÉ : le passage de `VK_ACCESS_SHADER_READ_BIT` à
  `VK_ACCESS_SHADER_WRITE_BIT` pour `kComputeWrite` ne rétablit pas le monde.
- QUALIFIÉ : `0x1B9C0000` est intermédiaire ; le dernier resolve et le swap
  restent sur `0x1AB60000`. Le propriétaire final est `0x8234F598` avec
  destination `0x828C849C`, tandis que le gestionnaire monde s'exécute.
- NON RÉSOLU : étapes 79/82/85 restent HUD vert sur monde noir ; gameplay US
  non validé et PAL fermé.
- PROCHAIN : qualification statique bornée de `DumpRenderTargets`/
  resolve-copy et de l'état shader monde autour de `0x8234F598`/
  `0x8234D5F4`, avant toute nouvelle session lourde.

Preuve : reports/retail-us-compute-write-barrier-runtime-20260827.md.

# Retail US — runtime sonde ownership MSAA (27 août 2026)

- PROUVÉ : relink/rebuild ciblé sous cgroup sans nouvelle génération C++ ;
  validation statique pass, ctest 16/16, suite Python 33/33, Vulkan/ReXGlue,
  SDL dummy, zéro D3D12 et `bin/bin` absent. Binaire installé
  `a0e9ad82…d5fbd64`.
- PROUVÉ : `PerformTransfersAndResolveClears` a émis 56 marqueurs bornés,
  séquences échantillonnées 1…10240, principalement des transferts couleur
  base 0 `k1X`↔`k4X` (valeurs 0↔2) avant la transition de campagne; aucun
  `k2X`→`k1X` du rendu monde n'a été observé.
- QUALIFIÉ : la session s'est arrêtée contrôlément à 55/85 après la capture
  `campaign-intro`, sans `PRESENT` ni `[ac6-campaign-transition]` ;
  `game_status=0`, `xvfb_status=0`, zéro fatal/trap. Ce n'est pas une
  explosion du runtime et aucune preuve Mission 01 n'est disponible dans
  cette exécution.
- DÉCISION : la sonde ownership ne justifie aucun correctif et ne déplace pas
  la couture monde noir. US gameplay non validé, PAL fermé ; reprendre en
  statique autour de `0x8234F598`/`0x8234D5F4` sans nouveau runtime, A/B,
  codegen ou relink.

Preuve : reports/retail-us-rt-transfer-probe-runtime-20260827.md.

# Retail US — expérience sauvegarde/chargement (27 août 2026)

- PROUVÉ : le scénario séparé `run_save_experiment.py` crée le conteneur
  `SAVE` dans un stockage isolé puis, dans un processus neuf, atteint
  `selector44=3 → type28=6 → type28=8 → type28=10`.
- PROUVÉ : le reçu r5 est `pass`, 18/18 opérations, quatre captures, arrêt
  propre (`game_status=0`, `xvfb_status=0`) et zéro fatal/trap.
- CONSERVÉ : `save.dat` et `not_00000000.dat` sous l'artefact r5 ; le profil
  porte un temps de campagne dans FILE 01 mais pas encore de mission, donc pas
  de checkpoint Mission 01 ni de free-mission.
- DÉCISION : cette preuve valide le mécanisme save/load sans modifier le gate
  Vulkan ; le monde 3D et PAL restent bloqués par le gate graphique.

Preuve : reports/retail-us-save-roundtrip-20260827.md.

# Retail US 2026-08-27 — capture RenderDoc D5B4 fermée sans `.rdc`

- ÉCHEC FERMÉ : la tentative unique `ac6-retail-us-renderdoc-cinematic-d5b4-20260827`
  est restée avant `type28=30`; le worker vidéo a bouclé après les premières
  présentations et le superviseur a retourné `124`.
- PROUVÉ : aucun fatal/trap/SIGSEGV, aucune PNG et zéro `.rdc`; la session ne
  qualifie ni D5B4/F556 ni l'attachement/resolve.
- DÉCISION : ne pas répéter la capture identique. Fermer d'abord
  statiquement la frontière d'injection/attente RenderDoc; gameplay US et PAL
  restent fermés.

Preuve complète : reports/retail-us-renderdoc-cinematic-d5b4-20260827.md.

# Retail US 2026-08-27 — injection RenderDoc différée non supportée sous Linux

- ÉCHEC FERMÉ : la variante de lancement normal puis
  `renderdoccmd inject` a atteint `cinematic-view-1` (72/96) mais l'outil
  retourne `4`, avec le message d'injection dans un processus déjà lancé non
  supportée sous Linux.
- PROUVÉ : arrêt contrôlé, zéro fatal/trap et zéro `.rdc`; la ressource D5B4
  et le point-list F556 restent non qualifiés.
- DÉCISION : restaurer le wrapper de lancement RenderDoc et ne répéter aucun
  des deux mécanismes sans outil Linux différent ou hypothèse causale nouvelle.

Preuve complète : reports/retail-us-renderdoc-cinematic-d5b4-injected-20260827.md.

# Retail US 2026-08-27 — RenderDoc vsync désactivé sans `.rdc`

- ÉCHEC FERMÉ : `renderdoccmd capture --wait-for-exit --opt-disallow-vsync`
  atteint `96/96`, produit 27 PNG et quitte proprement, mais `renderdoc/`
  reste vide.
- PROUVÉ : l'hypothèse d'un blocage `vkQueuePresent`/vsync au démarrage est
  réfutée; D5B4/F556 et leur resolve restent non qualifiés.
- DÉCISION : ne pas répéter le wrapper avec ou sans cette option.

Preuve complète : reports/retail-us-renderdoc-cinematic-d5b4-vsync-20260827.md.

# Retail US 2026-08-27 — touche RenderDoc F12 sans `.rdc`

- ÉCHEC FERMÉ : le lancement RenderDoc avec `F12` envoyé à
  `cinematic-view-2` atteint `96/96`, produit 27 PNG, quitte proprement et
  ne crée aucun `.rdc`.
- PROUVÉ : remplacer le keysym `Print` par la touche portable `F12` ne ferme
  pas la frontière de capture; aucun fatal/trap n'est observé.
- DÉCISION : fermer définitivement RenderDoc pour ce gate; toute reprise
  exige un outil Linux de capture différent ou une nouvelle hypothèse causale
  documentée.

Preuve complète : reports/retail-us-renderdoc-cinematic-d5b4-key-20260827.md.

# Retail US 2026-08-27 — sonde ordonnée post-process prête

- PROUVÉ : correctifs ReXGlue génériques SPIR-V/gamma/alpha/resolve déjà
  présents; aucun port amont justifié.
- PROUVÉ : binaire `9890d13f…f0f` relinké sans codegen, validation 16/16 et
  Python 42/42, Vulkan/SDL dummy, installation plate.
- AUTORISÉ : un seul runtime FSI, route qualifiée, sonde read-only bornée à
  30 frames de gameplay pour identifier la première transition 1B9C→1AB6
  riche vers noire. Aucun A/B, RenderDoc, override, rebuild, PAL ou codegen.

Preuve complète : reports/retail-us-postprocess-order-probe-build-20260827.md.

# Retail US 2026-08-27 — runtime post-process arrêté avant sa fenêtre

- ÉCHEC FERMÉ : 2/85, `type28=30` absent, attract cinématique visible,
  superviseur 2, zéro fatal/trap et zéro marqueur post-process.
- QUALIFIÉ : `--mission-render-summary` substituait une ancienne route 85; la
  sonde autonome conserve maintenant la route qualifiée 96/96, tests 42/42.
- PROCHAIN : après rotation imposée par trois gates lourds, un seul runtime
  `--mission-postprocess-order`; aucun rebuild, A/B, override, PAL ou codegen.

Preuve complète : reports/retail-us-postprocess-order-route-boundary-20260827.md.

# Retail US 2026-08-27 — route 96 post-process complète, frontière `2EF/F59F`

- QUALIFIÉ : route scellée `96/96`, 27 captures, 30 frames de sonde, 1045
  draws et 160 resolves qualifiés avec `9890d13f…84f0f`.
- ÉCHEC GATE : les cinq captures de contrôle conservent HUD/radar sur fond
  noir ; teardown `game_status=-9` après fermeture de fenêtre, zéro fatal/trap.
- PROUVÉ PAR ARTEFACT ANTÉRIEUR : le fallback `3ac747eb…ab2a` montre le monde
  3D dans la source `tf0` de `F59F`, mais perd le HUD ; ce n'est pas un fix.
- FRONTIÈRE : collecter `tf19`, vertex `c100/c106` et pixel `c100` du couple
  `2EF/F59F` avant tout override. PAL, codegen et A/B restent fermés.

Preuve complète : reports/retail-us-postprocess-order-route96-runtime-20260827.md.

# Retail US 2026-08-27 — build sonde exposition `2EF/F59F`

- Rebuild incrémental reçu à 0, uniquement `command_processor.cpp`; aucun
  codegen, `sources.cmake` inchangé (`c604796f…f72`).
- Validation reçue : 16/16 natifs, Python 42/42, Vulkan seul, SDL dummy,
  installation plate.
- Binaire installé : `687394ef…a63c`, 37 771 880 octets.
- AUTORISÉ : une route 96 identique, sonde read-only, pour `tf19`, vertex
  `c100/c106` et pixel `c100`; aucun A/B, override, PAL ou codegen.

Preuve complète : reports/retail-us-final-compose-exposure-probe-build-20260827.md.

# Retail US 2026-08-27 — runtime exposition propre, contenu 1×1 restant

- Reçu propre : `96/96`, 27 PNG, 30 frames, superviseur/jeu/Xvfb à 0, zéro
  fatal/trap avec `687394ef…a63c`.
- ÉCHEC GATE : `step-81-gameplay-hud.png` conserve HUD/radar sur fond noir.
- RÉFUTÉ : PS `c100.z/w=(1,1)` sur 31 draws ; les coefficients `F59F` ne
  suppriment pas la scène.
- BORNÉ : `tf19` est valide, 1×1 `k_32_FLOAT`, base `1C152000`; seule sa
  valeur finie/NaN/Inf reste à qualifier par readback exact.
- Trois gates lourds fermés : rotation avant le prochain build/runtime.

Preuve complète : reports/retail-us-final-compose-exposure-probe-runtime-20260827.md.

# Retail US 2026-08-27 — build readback `1C152000`

- Sonde bornée au resolve 1×1 exact, mot brut/`k8in32` et VS/PS producteur ;
  aucune mutation renderer ou invitée.
- Rebuild/validation à 0, 16/16 natifs, Python 42/42, aucun codegen.
- Binaire installé `43fc9068…a5fbf`, Vulkan/SDL dummy, installation plate.
- AUTORISÉ : une route 96 identique avec `--mission-postprocess-order` seul.

Preuve complète : reports/retail-us-exposure-word-readback-build-20260827.md.

# Retail US 2026-08-27 — exposition finie, `2EF/F59F` innocenté

- Premier lancement arrêté avant l'observable : route `80/96`, superviseur 2,
  zéro marqueur post-process et zéro conclusion renderer.
- Reprise à entrée identique reçue : `96/96`, 27 PNG, arrêt propre, binaire
  `43fc9068…a5fbf`, zéro fatal/trap.
- 31/31 valeurs `k8in32` finies et positives, plage `0.0334736…1.148324`,
  producteur unique `EBCCC312/2662DA78`.
- RÉFUTÉ : exposition nulle/non finie et extinction dans `2EF/F59F`.
- ÉCHEC GATE : HUD/radar reste sur monde noir ; prochain observable statique
  et read-only = contenu RGBA8 décodé par producteur des resolves `1B9C`.

Preuve complète : reports/retail-us-exposure-word-readback-runtime-20260827.md.

# Retail US 2026-08-27 — sonde RGBA8 `1B9C` préparée

- PROUVÉ : `readback_resolve=fast` lit potentiellement le slot précédent ; la
  sonde mémorise désormais le vrai frame/draw/VS/PS pour chacun des deux slots.
- DÉCODAGE : grille 32×18, `GetTiledOffset2D`, format/endian du resolve,
  métriques nonzero/somme/min/max par composant ; aucune mutation renderer.
- STATIQUE : source amont et copie de build patchées, `diff --check` propre,
  37/37 tests `test_run_gate.py`.
- PROCHAIN : rotation après trois gates lourds, puis rebuild incrémental et
  validation ; runtime encore fermé.

Preuve complète : reports/retail-us-1b9c-decoded-content-probe-static-20260827.md.

# Retail US 2026-08-27 — build sonde RGBA8 validé

- Rebuild incremental cgroup à 0 (`167/167`), pic ~7,6 GiB ;
  `generated/sources.cmake` réutilisé, SHA `c604796f…f72`, aucun codegen.
- Validation cgroup à 0 : binaire `f9cd73ff…25ce1`, 37 786 120 octets,
  ReXGlue/Vulkan, SDL dummy, D3D12 absent, installation plate.
- Suite Python complète : 42/42.
- AUTORISÉ : une seule route 96 avec la sonde RGBA8 `1B9C`, producteur associé
  au slot readback `fast`; runtime sans A/B ni override.

Preuve complète : reports/retail-us-1b9c-decoded-content-probe-build-20260827.md.

# Retail US 2026-08-27 — `1B9C` décodé, noir en aval

- Un appel de wrapper pré-créé est rejeté avant lancement (`output exists`,
  superviseur 2) ; aucune session invitée n'est comptée.
- La reprise correcte reçoit `96/96`, 27 captures, arrêt propre et zéro
  fatal/trap avec `f9cd73ff…25ce1`.
- 128 métriques RGBA8 décodées de `1B9C` : les passes `2EF/F59F`, `08DE` et
  `6E59` gardent des RGB non nuls sur 576 échantillons.
- ÉCHEC GATE : `step-81-gameplay-hud.png` reste HUD/radar sur fond noir ; le
  noircissement est postérieur à `1B9C`.
- PROCHAIN : décoder `1AB6` avec son propre couple de slots readback.

Preuve complète : reports/retail-us-1b9c-decoded-content-probe-runtime-20260827.md.

# Retail US 2026-08-27 — sonde finale `1AB6` préparée

- Deux ensembles de métadonnées indépendants sont maintenant attachés aux
  slots `1B9C` et `1AB6` en mode `fast`.
- Même grille 32×18, tiling/endian ReXGlue et métriques par composant ; aucune
  mutation renderer.
- Rebuild/validation requis après rotation ; runtime fermé.

Preuve complète : reports/retail-us-1ab6-decoded-content-probe-static-20260827.md.

# Retail US 2026-08-27 — build sonde décodée `1AB6` validé

- Rebuild incrémental cgroup à 0 (`167/167`), pic observé ~7,3 GiB ;
  `generated/sources.cmake` réutilisé (`c604796f…f72`), aucun codegen.
- Validation à 0 : 16/16 tests natifs, ReXGlue/Vulkan, SDL dummy, D3D12 absent,
  installation plate.
- Suite Python retail `pytest -q tests` : 51/51.
- Binaire installé : `603f99da…7cbf1`, 37 786 368 octets ; le marqueur
  `[ac6-postprocess-decoded]` est présent.
- AUTORISÉ : une seule route 96 `--mission-postprocess-order` pour qualifier
  `1AB6`; aucun A/B, override, PAL, codegen ou optimisation.

Preuve complète : reports/retail-us-1ab6-decoded-content-probe-build-20260827.md.

# Retail US 2026-08-27 — première route `1AB6` arrêtée avant gameplay

- La build `603f99da…7cbf1` démarre correctement sous Vulkan/SDL dummy et
  produit 22 captures jusqu'à `step-78-cinematic-view-5`.
- ÉCHEC GATE : le prédicat guest `cinematic=0 world=1 hud=1 stable=30` n'est
  pas atteint; le runner termine à 2 (`game_status=-9` au teardown), zéro
  fatal/trap et aucune ligne décodée `1B9C`/`1AB6`.
- Les captures montrent un retard d'un écran (sélection avion/arme puis
  briefing) par rapport à la route précédente; ce run ne qualifie donc pas le
  renderer et ne constitue pas un A/B.
- AUTORISÉ : une reprise strictement identique, avec le cache de cette build
  déjà parcouru, puis qualification de `1AB6` si la fenêtre gameplay s'ouvre.

Preuve complète : reports/retail-us-1ab6-decoded-content-probe-runtime-20260827.md.

# Retail US 2026-08-27 — gate Mission 01 fermé

Le binaire retail NTSC-U/J installé `d8b7b7b73b7fa0d2b98ca3eba6abdb42a77474ef8c375ccd1ff19c0d4e14fab8`
est validé sous ReXGlue/Vulkan (16/16 natifs, 51/51 Python, installation
plate). La route qualifiée `mission01-qualified-96.steps` atteint le prédicat
`cinematic=0 world=1 hud=1 stable=30`, exécute 96/96 opérations et produit
27 captures avec arrêt propre. Le reçu gameplay et l'audit v2 sont `pass`.

La correction produit maintenant la scène en remplaçant, sous garde stricte,
le swap sparse `0x1AB60000` par le fetch de composition `0x1B9C0000` du couple
`2EF9631F6325FA91/F59F21F4A1E7843E`. Le cache vertex retiré reste une correction
de résidence générale ; il n'était pas suffisant seul.

Les défauts visuels signalés restent classés ouverts et non masqués : avions
et after-effects blancs, hautes lumières ciel/eau écrêtées, plans de
cinématique pré-mission dégradés. Les ressources BC3/D5B4 sont valides ; la
prochaine correction devra être une preuve ciblée fragment/éclairage/blend ou
post-process. PAL reste une cible distincte et n'est pas promu par ce gate.

Preuve complète : reports/retail-us-gameplay-final-compose-20260827.md.

# Retail US 2026-08-28 — run M01 autorisé, timeout startup seed complet

- AUTORISÉ : un seul run runtime borné après décision explicite, sous cgroup
  `ac6-retail-us-m01-v2-auth-20260828`, avec `SDL_AUDIODRIVER=dummy`, binaire
  `b004ee70…4d70`, route `543a97ce…c5c7f` et seed complet
  `/tmp/ac6-cache-seed-v2-complete`.
- ÉCHEC FERMÉ : `2/96` après `248,723 s`, erreur
  `log predicate not reached: type28=30`, zéro capture, `game_status=-9`,
  `xvfb_status=0`, zéro fatal/trap.
- OBSERVÉ : Vulkan sélectionné, environ 5,45 GiB et 73 tâches au pic ; deux
  phases visuelles seulement (`world=0`, puis `hud=1`, `stable=0`).
- DÉCISION : route et manifeste non promus ; prochaine exécution interdite sans
  nouvelle hypothèse causale et autorisation distincte. Audit XPSO :
  en-tête/version valides, zéro hash invalide ; 193 pipelines chargés depuis le
  seed contre 164 au run v2 positif.

Preuve complète : reports/retail-us-mission01-gameplay-candidate-20260828.md ;
artefact : artifacts/retail-us-mission01-gameplay-v2-authorized-20260828/output/RESULT.json.

# Retail US 2026-08-28 — XPSO-164 atteint gameplay, contrôles nuls

- AUTORISÉ : seed `/tmp/ac6-cache-seed-v2-xpso164`, seule différence = XPSO
  164 descriptions; XSH/GLCache du seed complet inchangés.
- OBSERVÉ : route v2 `96/96`, 27 captures, `cinematic=0 world=1 hud=1
  stable=30`, centre non noir (`0.232472/0.287361/1.0`), arrêt propre,
  `game_status=0`, zéro fatal/trap.
- ÉCHEC GATE : `control_changed_pixels=0` pour pitch/roll/yaw/throttle/frein;
  reçu non promu. L'écart de 29 pipelines XPSO n'est donc pas causal pour
  l'atteinte de la frontière gameplay.

Preuve : `artifacts/retail-us-mission01-gameplay-v2-xpso164-20260828/output/RESULT.json`.

# Retail US 2026-08-28 — reverse entrée Linux fermé

- STATIQUE : `ac6_kbm_input.cpp` protège l'injection custom par `_WIN32` et
  `GetAsyncKeyState`, avec `ac6_kbm_enabled=false`; Linux reste sur le driver
  MnK stock. Bindings vérifiés: W/S pitch, A/D roulis, Q/F épaules, LMB
  accélération, RMB frein.
- CORRIGÉ : route v2 réalignée sur les bindings stock, puis settle 15 s
  post-cinématique; SHA courant
  `44c7cac85dba7f7dad42453f7232a1e5c3f44798c9f74bc5e404626d27dcc8a9`.
- CONTRÔLE : activer `REX_AC6_KBM_ENABLED=true` force `mnk_mode` off sans
  injection Linux et bloque l'étape 2 (`type28=30` absent); cette voie est
  rejetée. 60 tests Python passent.

Preuve : `artifacts/retail-us-mission01-gameplay-v2-kbm-oldseed-20260828/output/RESULT.json` ;
rapport : `reports/retail-us-mission01-gameplay-candidate-20260828.md`.

# Retail US 2026-08-28 — settle post-cinématique et screenshot

- La route courante attend 15 s après `Space` avant `Escape`; SHA
  `44c7cac85dba7f7dad42453f7232a1e5c3f44798c9f74bc5e404626d27dcc8a9`.
- Le test settle-15 + seed complet a expiré au startup à l'étape 2
  (`type28=30` absent, `257,571 s`, zéro capture, zéro fatal/trap):
  `artifacts/retail-us-mission01-gameplay-v2-stock-settle15-20260828/output/RESULT.json`.
- Le run complet précédent a atteint `96/96`, `stable=30`, monde non noir et
  27 captures; contrôles encore identiques (0 pixel):
  `artifacts/retail-us-mission01-gameplay-v2-stock-complete193-20260828/output/RESULT.json`.
- Capture utilisateur: `artifacts/retail-us-mission01-gameplay-v2-stock-route-20260828/output/step-40-language.png`.
- Gate M01 v2, débrief/save et missions 02–15 restent fermés; prochaine
  frontière = premier consommateur du paquet MnK stock pendant `FlightActive`.

# Retail US 2026-08-28 — consommateur MnK stock qualifié statiquement

- IDENTITÉ : projet `ghidra-projects/ac6-us`, `default.xex`, XEX
  `6eefba42…67cbbbc`; generated source du même target NTSC-U/J.
- PROUVÉ : `MnkInputDriver::GetState` écrit boutons, triggers et quatre axes
  dans `X_INPUT_STATE`; `XamInputGetState_entry` transmet le paquet à
  `InputSystem` sans supprimer les champs analogiques.
- PROUVÉ : le guest US `0x8234CEB8` passe `r31+68` à `0x82390CE0` (LR
  `0x8234CEE0`), puis `0x8234CE40` lit boutons/axes et `0x8234CC38` lit les
  triggers. Le premier consommateur guest n'est donc pas le point de perte.
- BORNÉ : le logger read-only positif atteint 96/96 et observe les LR XAM et
  boutons, mais ne sérialise pas les axes; le record longstart n'a pas finalisé
  son fichier; l'attach GDB est refusé par `ptrace_scope`.
- DÉCISION : ne pas patcher guest/renderer. Le prochain runtime unique doit
  corréler événement X11, paquet stock non nul, heartbeat de vol et phase
  visuelle; ensuite seulement débrief/save et missions 02–15.

Preuve complète : `reports/retail-us-mission01-gameplay-candidate-20260828.md`.

# Retail US 2026-08-28 — limite trace GDB

- Une tentative unique a lancé le même binaire comme enfant GDB pour éviter
  `ptrace_scope`; breakpoint `MnkInputDriver::GetState` installé.
- ÉCHEC OUTIL : `SIGSEGV` guest à `rex_sub_821E4378` avant tout hit, route
  `1/96`, environ 10 s, reçu `artifacts/retail-us-mission01-mnk-gdb-start-20260828/output/RESULT.json`.
- DÉCISION : aucun état MnK n'est déduit de ce run; mode GDB perturbateur
  abandonné. La qualification statique `GetState -> XAM -> 0x8234CEB8 ->
  0x8234CE40/CC38` reste la preuve active.

# Retail US 2026-08-28 — trace packet stock bornée non concluante

- Une seule session read-only a utilisé le wrapper XAM qualifié avec le binaire
  `c225f5e6…fc08923`, la route historique `6ef77bef…14dac4b`, le seed positif
  et `SDL_AUDIODRIVER=dummy`, sous le cgroup
  `ac6-retail-us-m01-inputstate-log-20260828`.
- Reçu `artifacts/retail-us-mission01-stock-input-state-log-positive-route-20260828/output/RESULT.json`:
  `status=fail`, `2/96`, `249,431 s`, `type28=30` absent, une capture
  cinématique, `clean_shutdown=false`, `game_status=-9`, zéro fatal/trap.
- Seulement deux sites XAM (`0x8234CFA4`, `0x8234CEE0`) ont été journalisés;
  zéro paquet `xinput user=... state`, axe, trigger ou heartbeat de vol.
  La fenêtre est fermée; aucune répétition identique, correction renderer ou
  promotion gameplay n'est permise.

# Retail US 2026-08-28 — instrumentation host réfutée, baseline propre échoue

- Le probe host-side (binaire `68090b0494f42de96b4b8a5784216ce0c26df3bcd74741f5c74c15c5356919c7`)
  n'a produit aucun état : `InputSystem::GetState` renvoie `0x48F` sans
  périphérique, puis `0x82390CE0` synthétise le paquet en mémoire guest. Reçu :
  `artifacts/retail-us-mission01-stock-input-host-log-positive-route-20260828/output/RESULT.json`;
  `2/96`, `253,028 s`, `PRESENT=1829`, capture noire 1-bit, zéro fatal/trap.
- Patch host retiré; rebuild/validate propre installé
  `a279d55180226496b4d2b70194e13a71e122a678d87842944db7e7cc50c2c95c`
  (37 803 744 octets), `bin/bin` absent.
- La baseline propre avec même route/seed échoue encore :
  `artifacts/retail-us-mission01-baseline-clean-positive-route-20260828/output/RESULT.json`,
  `2/96`, `250,685 s`, `PRESENT=1863`, phase `world=0/hud=1`, capture
  cinématique, `clean_shutdown=false`, `game_status=-9`, zéro fatal/trap.
- Décision : hypothèse « instrumentation cause le timeout » réfutée; ne pas
  répéter la route. Divergence `stock/cache/état -> type28` reste ouverte et
  doit être réduite statiquement avant tout nouveau runtime.

# Retail US 2026-08-28 — cache chaud XPSO-193 progresse, gate visuel fermé

- Une seule session bornée avec baseline propre `a279d551…c95c`, route
  historique `6ef77bef…dac4b`, seed `stock-complete193` et cgroup
  `ac6-retail-us-m01-warmcache193-a279-20260828` a chargé 193 pipelines,
  ajouté un 194e record et exécuté 96/96 pas (`PRESENT=16046`, `type28=94`).
- OBSERVÉ : phases `cinematic=0 world=1 hud=1 stable=30` puis
  `cinematic=0 world=0 hud=1`; captures 70/73 montrent le monde 3D mais
  fortement écrêté/blanc. Les captures de vol 77/79/80/84/87/90/93/96 sont
  identiques; cinq contrôles à 0 pixel.
- ÉCHEC : reçu
  `artifacts/retail-us-mission01-warmcache193-a279-20260828/output/RESULT.json`,
  `status=fail`, `clean_shutdown=false`, `game_status=-11`, sans fatal/trap.
  Le journal finit après `Window closing`, worker audio arrêté,
  `TerminateTitle` et `Execution complete`; le signal est post-teardown,
  non attribué au guest.
- DÉCISION : cache chaud retenu comme précondition de progression, pas comme
  fix. Aucun run identique. Gate M01 v2, débrief/save et missions 02–15 restent
  fermés; prochaines frontières statiques = événements GTK/X11 -> `has_focus_`,
  ordre de destruction post-`Execution complete`, producteur postprocess blanc.

# Retail US 2026-08-28 — parcours original confirme le seam XAM stock

- Une session diagnostique unique a utilisé `mission01-qualified-96.steps`
  (`771a77a8…b6043`), baseline `a279d551…c95c`, cache chaud XPSO-193 et
  `--mission-stock-input-log`, sous cgroup
  `ac6-retail-us-m01-route-original-inputlog-20260828`.
- Reçu :
  `artifacts/retail-us-mission01-route-original-inputlog-20260828/output/RESULT.json`;
  `diagnostic-capture-ready`, `96/96`, `11717 PRESENT`, `94 type28`,
  `cinematic=0 world=1 hud=1 stable=30`, 27 captures, arrêt propre et zéro
  fatal/trap.
- Le wrapper XAM `0x82390CE0` voit `A=0x1000`, `START=0x0010` et
  `LB=0x0100` sur les deux LR qualifiés; la livraison GTK/X11 -> seam n'est
  donc pas totalement perdue. Ce logger ne couvre pas axes/triggers.
- Les frames de vol varient (avion visible en pitch/roll, noir en yaw, gris en
  brake), mais le parcours reste diagnostic et le HUD de référence est noir;
  aucune causalité contrôle/renderer n'est promue. Ne pas répéter cette
  route/seed; garder `FlightActive`, postprocess et teardown `-11` ouverts.

# US full-native Linux — profil cible et reprise 2026-08-29

- NTSC-U/J devient la priorité produit Linux AMD64 ; PAL reste une identité
  séparée et aucune preuve n'est fusionnée.
- Les identités XDVDFS du XEX, de `DATA.TBL`, `DATA00.PAC`, `DATA01.PAC` et des
  packs médias sont qualifiées dans
  `analysis/oracle/ac6-recomp-ab90b-us/content-identity.json` sans octets
  retail committés.
- Le runtime manuscrit expose désormais `RetailTarget::{Pal,NtscUj}`, policies
  de contenu/média dédiées, mappings sélectionnés par cible et cache US séparé.
- Build Linux/Ninja, CTest ciblé, pytest identité/retail et audit de frontière
  passent; l’audit de cache sélectionne aussi l’identité et les ressources M01
  par cible. Le palier retail ReXGlue reste bloqué sur la preuve M01 visible et
  contrôlable ; aucune optimisation ni promotion gameplay.

# Retail US 2026-08-30 — composition host réfutée, postprocess ouvert

- Le cache qualifié 193 permet `type28=30`, campagne `0→1→2` et route 96/96.
- Composer `1B9C + 1AB6` avant gamma donne `mean=0,996537`, `stddev=0,0197882`
  et HUD vert `0,0` : l'hypothèse presenter est réfutée.
- Le fallback expérimental est retiré ; rollback build/validate passe.
- Aucun reçu gameplay/HUD promu. Gate courant : perte postprocess guest entre
  tone-map, HUD/UI et resolve final `1AB6`.
# 2026-08-30 — `1B9C → 0311`, sampler réfuté

Les traces 1AB6 prouvent que le HUD est conservé après `0311`; la scène est
déjà noire avant HUD. Le cache prépare bien les reloads `1B9C`. F59F et 0311
partagent la même ressource ; l’A/B scoped point/linéaire n’améliore pas
l’image et a été retiré. Gate actif : contenu de l’image Vulkan après upload
`1B9C`, juste avant `0311`. Rapport courant :
`reports/retail-us-hud-layer-compose-20260830.md`.

# Gate 2 retail US 2026-09-03 — r225 (documentation seule) : pas de résolveur statique pour la table r224

`scripts/ReferencesTo.java` (déjà présent, interroge `ReferenceManager` par
adresse brute, pas besoin de symbole) montre que 12 des 14 entrées de la
table de repli trouvée par r224 n'ont AUCUNE référence dans ce XEX — ni `bl`
direct, ni indirect résolu statiquement. Seule `0x821f4680` (candidate
`XamShowMarketplaceUI` de r224) a un vrai site d'appel (`82140d10`, `bl`
ordinaire), et une adresse voisine non listée dans la table, `0x821f4678`,
en a deux (`82140c10`, `8215cbc4`). La base de la table elle-même
(`0x821f44d8`) n'a aucune référence : rien ne la charge comme pointeur de
base pour un accès indexé, nulle part que ce passage statique puisse voir.

Conséquence : l'hypothèse « fonction résolveur qui lit la table en boucle »
de r224 ne tient pas — aucune preuve statique ne la soutient. Ce qui existe
réellement est plus simple : quelques-unes de ces adresses sont appelées
DIRECTEMENT par des `bl` fixes ordinaires depuis des sites d'appel précis,
comme n'importe quel autre helper interne statiquement lié de ce XEX — pas
via une indirection pilotée par table. De plus, ni `0x821f4680` ni
`0x821f4678` ne portent de symbole Ghidra : ce sont des adresses `.text`
internes ordinaires du XEX qualifié, déjà recompilées normalement par
XenonRecomp — PAS des gaps de stub d'import offline. Tout ce fil de
recherche est donc hors du périmètre du balayage d'imports offline; retiré
de la liste des pistes actives. `ctest` 10/10, pytest 205/205 (inchangés,
aucune source touchée).

Preuve :
`reports/ac6-retail-native-codegen-gate2-r225-doc-xamshow-table-has-no-static-resolver-callers-go-direct-20260903.md`.

# Gate 2 retail US 2026-09-03 — r226 corrige réellement `XamUserGetName`

2 vrais sites d'appel (`0x82161bb8`, `0x821cfd98`) confirment
indépendamment la signature documentée
`XamUserGetName(DWORD dwUserIndex, LPSTR szUserName, DWORD cchUserName)`
avec `cchUserName=0x10` littéral aux deux sites. `Function_82161B08`
n'a jamais vérifié le statut de retour et utilisait le buffer sans
condition en aval — même classe de risque de lecture non initialisée
que r183 (`RtlImageXexHeaderField`), puisque le stub générique
n'écrivait jamais ce buffer. Le fix écrit un nom ASCII synthétique
explicite (« Player », aucun vrai gamertag n'existe hors ligne),
tronqué/terminé par null à la taille confirmée par l'appelant, et
retourne `STATUS_SUCCESS` sans condition. pytest 206/206 (205+1 skip),
`ctest` 10/10.

Preuve :
`reports/ac6-retail-native-codegen-gate2-r226-real-fix-xamusergetname-writes-a-name-and-succeeds-20260903.md`.

# Gate 2 retail US 2026-09-03 — r227 corrige réellement `XamUserGetSigninInfo`

Escalade de r211 (vérifié sans fix confirmé). Le seul appelant direct de
l'import est un wrapper passthrough (`Function_821F5190`), confirmé par
désassemblage brut (`mfspr`/`stw`/`stwu` puis `bl` immédiat, aucun registre
touché) : il transmet ses propres r3/r4/r5 sans modification. 6 vrais
appelants du wrapper trouvés via `ReferencesTo.java`; 3 tracés
(`0x821cf060`, `0x821b66bc`, `0x821ce6f0`), tous appelant avec la forme
`(dwUserIndex, 0, &buffer_pile_12_octets)` et lisant TOUS le même bit à
l'offset +8 (`>>1 & 1`) qui conditionne l'exécution de leur propre logique
par-joueur réelle — le stub générique ne l'écrivait jamais, donc ce bit de
garde lisait toujours des octets de pile non initialisés. Le fix remplit
XUID=0 (+0..+7) et le bit de garde à 0 (+8) pour l'utilisateur 0, même
convention que `XamUserGetSigninState` (r176, index 0 = signé localement);
les autres index gardent l'échec offline `kOfflineStatus` déjà existant.
Rien au-delà de +8 n'a été lu par un appelant tracé, donc rien au-delà n'est
écrit. pytest 207/207 (206+1 skip), `ctest` 10/10.

Preuve :
`reports/ac6-retail-native-codegen-gate2-r227-real-fix-xamusergetsignininfo-fills-the-gating-bit-for-user-zero-20260903.md`.

# Gate 2 retail US 2026-09-03 — r228 corrige réellement `XamUserGetXUID`

Escalade de r211. Le wrapper (`Function_821F4618`) est cette fois un
VRAI remappeur d'arguments, pas un passthrough pur : désassemblage brut
confirme `or r5,r4,r4` puis `li r4,0x7` avant `bl` — il prend
`(dwUserIndex, pXuid)` et insère `dwFlags=7` littéral pour appeler le
vrai import à 3 arguments `XamUserGetXUID(DWORD, DWORD, PXUID)`, même
motif que `NtSetTimerEx`/`XamShowMessageBoxUIEx`. 6 vrais appelants du
wrapper trouvés via `ReferencesTo.java`; 3 tracés confirment un XUID de
sortie 8 octets — l'un des trois (`Function_821CE9A0`) copie le buffer
dans une struct SANS AUCUNE vérification de statut, même classe de
risque que r226/r227. Le fix remplit XUID=0 (8 octets) pour
l'utilisateur 0, même convention que r227; les autres index gardent
l'échec offline existant. pytest 208/208 (207+1 skip), `ctest` 10/10.

Preuve :
`reports/ac6-retail-native-codegen-gate2-r228-real-fix-xamusergetxuid-fills-a-zero-xuid-for-user-zero-20260903.md`.

# Gate 2 retail US 2026-09-03 — r229 (documentation seule) : cluster mort + `NtSetInformationFile` déjà adéquat

11 imports confirmés SANS AUCUN appelant réel dans ce build :
`XamWriteGamerTile`, tout le cluster
`XamContent{GetDeviceState,GetDeviceData,Close,Delete,SetThumbnail,
CreateEnumerator}`, `NtQueryDirectoryFile`, `NtReadFileScatter`,
`Stfs{Control,Create}Device`, `XamLoaderLaunchTitle` (ses 2 seules
références sont des sauts CONDITIONNELS internes à sa propre fonction,
même motif que le cluster trampoline fermé par r225), `XamContentCreateEx`,
`XamEnumerate`. Aucun fix n'est possible ni nécessaire pour un import
sans appelant réel. `NtSetInformationFile` (9 sites réels, le plus haut
compte tracé ce balayage) : 4 sites tracés, tous vérifient le statut de
retour avant de continuer, même famille que le chantier save/reload de
r202 (`Function_82392040` appelle les mêmes fonctions d'écriture que
`Function_82392878`) — le générique `kOfflineStatus` y est déjà la
réponse honnête correcte, pas un bug de forme de contrat. Aucune source
touchée; pytest 208/208 (207+1 skip), `ctest` 10/10 reproduits.

Preuve :
`reports/ac6-retail-native-codegen-gate2-r229-doc-unreached-cluster-plus-ntsetinformationfile-already-adequate-20260903.md`.

# Gate 2 retail US 2026-09-03 — r230 corrige réellement `XamTaskCloseHandle`

r198 avait différé `XamTaskSchedule`/`XamTaskCloseHandle` ensemble comme
nécessitant un sous-système d'exécution de callback invité. Vrai pour
`XamTaskSchedule`, mais l'unique vrai site d'appel de `XamTaskCloseHandle`
(`0x82391e00`, dans `Function_82391A40`) ignore totalement son retour —
même motif que `KeLockL2`/`IoDismountVolume`/`XamVoiceClose`/
`XMsgCancelIORequest`. `STATUS_SUCCESS` inconditionnel;
`XamTaskSchedule` reste différé (son propre besoin de sous-système de
callback n'est pas résolu par ce fix). `__C_specific_handler` vérifié :
zéro référence dans ce XEX, cohérent avec la fermeture SEH de r202.
pytest 209/209 (208+1 skip), `ctest` 10/10.

Preuve :
`reports/ac6-retail-native-codegen-gate2-r230-real-fix-xamtaskclosehandle-returns-success-20260903.md`.

# Gate 2 retail US 2026-09-03 — r231 (documentation seule) : cluster réseau clos (29/29)

Les 29 imports `NetDll_*` catalogués par r218 comme chantier non touché
sont désormais clos, pas seulement réduits. 26 n'ont AUCUN appelant réel
(ni direct, ni via trampoline) : `WSACleanup`, `XNetCleanup`,
`XNetCreateKey`, `XNetGetTitleXnAddr`, `XNetInAddrToXnAddr`,
`XNetQosListen`, `XNetQosRelease`, `XNetQosServiceLookup`, `XNetRandom`,
`XNetRegisterKey`, `XNetXnAddrToInAddr`, `accept`, `bind`, `closesocket`,
`connect`, `getsockname`, `getsockopt`, `ioctlsocket`, `listen`, `recv`,
`recvfrom`, `select`, `send`, `sendto`, `setsockopt`, `shutdown`,
`socket`. Les 3 restants (`WSAGetLastError` 4 appelants réels,
`___WSAFDIsSet` 3 appelants réels, `XNetQosLookup` 1 appelant réel via
son propre wrapper) sont déjà adéquats : le générique offline négatif
ne correspond jamais aux valeurs précises comparées par leurs appelants
(`0x2733`, `0x2747`, `1`, `0`), ET ces 3 chaînes sont de toute façon
gardées par un champ handle-socket qui reste `-1` en pratique puisque
`socket`/`connect` (les seuls moyens de le rendre valide) sont eux-mêmes
morts. Aucune source touchée; pytest 209/209 (208+1 skip), `ctest` 10/10
reproduits.

Preuve :
`reports/ac6-retail-native-codegen-gate2-r231-doc-networking-cluster-dead-or-already-adequate-20260903.md`.

# Gate 2 retail US 2026-09-03 — r232 (documentation seule) : paire `XamSession*` close

r197 avait flaggé `XamSessionCreateHandle`/`XamSessionRefObjByHandle`
comme « pas assez tracé ». Désormais entièrement tracés : 1 site réel
pour `XamSessionCreateHandle`, 11 pour `XamSessionRefObjByHandle` (tous
décompilés). Motif identique partout : `uVar = func_0x823d08dc(handle,
&obj_out); if (uVar == 0) { ...utilise obj_out... }` ou l'équivalent
`if (uVar != 0) return uVar;` — aucun appelant n'utilise l'objet
référencé sans vérifier d'abord le statut, et tous retournent l'échec
honnêtement sinon. La plupart enchaînent ensuite sur le vrai
`XMsgStartIORequest` (r203) — confirme que cette famille fait du vrai
trafic IPC, mais cela ne se traduit par AUCUN bug de forme de contrat
pour cette paire : le générique offline `kOfflineStatus` y est déjà la
réponse correcte partout. Paire close, aucun fix nécessaire. Aucune
source touchée; pytest 209/209 (208+1 skip), `ctest` 10/10 reproduits.

Preuve :
`reports/ac6-retail-native-codegen-gate2-r232-doc-xamsession-pair-fully-traced-already-adequate-20260903.md`.

# Gate 2 retail US 2026-09-03 — r233 (documentation seule) : derniers candidats re-confirmés

`NtDuplicateObject` (site réel unique déjà géré gracieusement — `if
(iVar1<0) Function_821F75B8(); return iVar1>=0;`), `XamVoiceCreate` (site
réel unique, gate `-1<lVar4` déjà sûr) et `XamVoiceSubmitPacket` (2 sites
réels, tous deux `iVar<0` gérés, dépendent d'un handle que
`XamVoiceCreate` ne produit jamais) re-confirmés déjà adéquats.
`XexCheckExecutablePrivilege` (3 sites réels, tous testent un booléen) :
la tentation d'un fix « défaut = privilège refusé en offline » a été
examinée et REFUSÉE À NOUVEAU — r178 avait déjà pesé exactement ce
compromis (aucun cas de contrôle dans ce XEX ne fixe la bonne réponse
pour ces IDs de privilège précis; passer à « refusé » risquerait
d'introduire un chemin d'échec qui ne s'exécute pas actuellement, plus
risqué que le statu quo). Aucune preuve nouvelle ne renverse cette
décision. Aucune source touchée; pytest 209/209 (208+1 skip), `ctest`
10/10 reproduits.

Preuve :
`reports/ac6-retail-native-codegen-gate2-r233-doc-remaining-voice-and-privilege-imports-already-handled-20260903.md`.

# Gate 2 retail US 2026-09-03 — r234 (documentation seule) : balayage d'imports offline déclaré clos

`sprintf` (7 sites réels, 4 décompilés ce cycle) / `_vsnprintf` (2 sites
réels) : formats hétérogènes dans du code de diagnostic save/reload déjà
reconnaissable (`Function_821E9F50` construit des chemins de répertoire
de sauvegarde) — confirme r192, un vrai moteur printf varargs est
nécessaire, pas un cas particulier borné. **Le balayage d'imports
offline (r148-r233) est déclaré à son point d'arrêt naturel** : sur les
~125 stubs génériques du début de ce balayage, des dizaines ont reçu un
vrai fix dérivé de preuve (r169-r230), et chaque autre candidat restant
a désormais une disposition tracée et nommée : mort (`XamContent*`,
`XamWriteGamerTile`, `NtQueryDirectoryFile`, `NtReadFileScatter`,
`Stfs*Device`, `XamLoaderLaunchTitle`, `XamContentCreateEx`,
`XamEnumerate`, `__C_specific_handler`, 26/29 `NetDll_*`); déjà adéquat
(`NtSetInformationFile`, les 3 `NetDll_*` restants, la paire
`XamSession*`, `NtDuplicateObject`, `XamVoiceCreate`/`SubmitPacket`,
`XexGetModuleHandle`/`GetProcedureAddress`, `XamUserAreUsersFriends`,
`XamGetExecutionId`, `XamUserCreate{Achievement,Stats}Enumerator`);
bloqué par sous-système non construit (`XamTaskSchedule`, écriture
save/reload, moteur printf varargs); bloqué par politique renderer
(`VdGetSystemCommandBuffer`/`VdPersistDisplay`); définitivement hors
périmètre (`XeKeysConsole*`); ou sans cas de contrôle
(`XexCheckExecutablePrivilege`). Aucun candidat restant ne correspond
plus à la méthode de ce balayage. Aucune source touchée; pytest 209/209
(208+1 skip), `ctest` 10/10 reproduits.

Preuve :
`reports/ac6-retail-native-codegen-gate2-r234-doc-offline-import-sweep-at-its-natural-stopping-point-20260903.md`.

# Gate 2 retail US 2026-09-03 — r235 (documentation seule) : `_vsnprintf` réaffirmé hors cycle borné, avec bien plus de preuve

Tentative de réduire davantage le périmètre de `_vsnprintf` : les
spécificateurs des 7 sites `sprintf` sont un petit ensemble fermé
(`%s`/`%d`/`%x`/`%X`, largeurs fixes zéro-paddées, décodés via
`scripts/DumpBytes.java`), mais le vrai consommateur de `_vsnprintf`
(`Function_821EF4E0`/`Function_821EF458`, wrappers qui transmettent
leurs propres varargs vers un pointeur `va_list`-style construit sur
leur propre pile, même convention que r222) a **20 + 8 vrais appelants
réels** trouvés via `ReferencesTo.java`, répartis sur au moins 4
fonctions distinctes, la plupart avec des chaînes de format encore non
décodées. Une seule chaîne (`Function_821EF878`, un dump crash/version
gardé par un code de statut précis, remontant à `Function_821E6AC8` qui
n'a AUCUN appelant) est confirmée morte; les autres non — donc ce n'est
PAS un ensemble fermé de quelques formats connus comme espéré. Une
implémentation partielle couvrant seulement les spécificateurs déjà vus
désynchroniserait silencieusement la lecture des varargs sur tout
spécificateur non couvert dans les chemins non encore décodés — pire
que le no-op honnête actuel (le buffer n'est jamais écrit). Réaffirme
r234, avec beaucoup plus de preuve concrète que l'estimation initiale.
Aucune source touchée; pytest 209/209 (208+1 skip), `ctest` 10/10
reproduits.

Preuve :
`reports/ac6-retail-native-codegen-gate2-r235-doc-vsnprintf-helper-is-pervasive-not-bounded-20260903.md`.

# Gate 2 retail US 2026-09-03 — r236 corrige réellement `sprintf`/`_vsnprintf`

Poursuite de r235 : plutôt que s'arrêter à « beaucoup d'appelants
réels », décodage EXHAUSTIF (`scripts/DumpBytes.java`) de CHAQUE chaîne
de format atteignant CHAQUE vrai appelant des deux imports — y compris
une table dynamique de 11 entrées littérales à `0x82691074`. L'ensemble
complet de spécificateurs observés dans tout ce XEX est fermé :
littéraux, `%s` (largeur décimale optionnelle, ex. `%25s`), `%d`,
`%x`/`%X` (largeur zéro-paddée optionnelle). Le pointeur `va_list` réel
de `_vsnprintf` est résolu par désassemblage brut de
`Function_821EF4E0` : `std r5,0x20(r1)` … `std r10,0x48(r1)` (slots de 8
octets séquentiels), valeur aux 4 octets bas (big-endian). Parseur
partagé `guest_vprintf()` ajouté au HEADER, spécificateur non reconnu
copié tel quel (jamais deviné) — évite la désynchronisation silencieuse
des lectures varargs. `sprintf` lit jusqu'à 6 varargs depuis
r5-r10 (aucun site tracé n'en utilise plus de 2), cap défensif de 0x2000
octets. `_vsnprintf` honore son vrai paramètre `size` (r4) comme borne.

Vérifié au-delà de `ctest`/pytest : le corps de `guest_vprintf` extrait
et compilé/exécuté seul contre une mémoire invité simulée, avec CHAQUE
chaîne de format réelle décodée de ce XEX (7 cas incluant padding de
largeur et hex majuscule) plus 2 cas défensifs (troncature,
spécificateur inconnu) — 8/8 passent. pytest 211/211 (210+1 skip),
`ctest` 10/10.

Preuve :
`reports/ac6-retail-native-codegen-gate2-r236-real-fix-sprintf-and-vsnprintf-implement-the-exhaustively-verified-specifier-set-20260903.md`.
