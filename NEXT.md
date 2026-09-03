# AC6 retail NTSC-U/J — Gate 2 runtime natif

## Résultat requis

Atteindre visiblement le début du gameplay Mission 01 avec le runtime natif,
sans substitution de frontbuffer, état synthétique, compteur injecté ni
fallback ReXGlue.

## Identité scellée

- cible : retail NTSC-U/J, `default.xex`, SHA-256
  `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`;
- ISO : `204c5e645d79da8776699c12f17bd069f869fbdb10ae79015d1a0ef2b743c98c`;
- projet Ghidra : `ghidra-projects/ac6-us.gpr` +
  `ghidra-projects/ac6-us.rep/`;
- programme Ghidra : `default.xex`, `PowerPC:BE:64:Xenon`;
- AC6_recomp : `09144bb092ad871584808aeead69c395edbd5200`;
- renderer oracle : ReXGlue, hors produit et hors installation seulement.

## État courant

- La famille de configuration plateforme Vd/X ouverte par r168 est
  entièrement fermée depuis r175 (détail dans `STATE.md` r169-r175).
- r176/r177 ont corrigé `XamUserGetSigninState`/`XamGetSystemVersion`.
- r178 (documentation seule) : `XexCheckExecutablePrivilege` vérifié, non
  corrigé (précédent r164, sémantique de privilège non déterminable
  localement).
- r179 a corrigé `KeQuerySystemTime` (FILETIME réel via l'horloge de
  l'hôte).
- r180 (autorisé explicitement par l'utilisateur) : backend d'entrée
  manette natif via SDL2 (`NativeGuestInputService`) —
  `XamInputGetState`/`SetState`/`GetCapabilities` implémentés avec preuve
  réelle du contrat de transfert (code d'erreur `0x48F`, offsets
  `XINPUT_CAPABILITIES`). Découverte importante : modifier `native/`
  nécessite `tools/prepare.py --profile native` (resynchronise
  `native-source/`), pas seulement `build.py` — voir RESUME.md. Aucune
  manette physique disponible ici; le symptôme « contrôles nuls » reste à
  confirmer par une future observation runtime.
- r181 a corrigé `XamInputGetKeystrokeEx` (dernier import de la famille
  `XamInput*`) : renvoie `ERROR_EMPTY` (0x4306) sans condition — forme
  réelle valide, aucune file de keystrokes n'existe encore.
- r182 a corrigé `XamUserCheckPrivilege` : `ERROR_SUCCESS` + bool de
  sortie `TRUE` (accordé) — identifié comme le thunk `0x823cfe8c` que r176
  avait laissé anonyme.
- r183 a corrigé `RtlImageXexHeaderField` : renvoie `0` (absent) — la
  valeur de retour EST le pointeur de champ ici (pas un statut); les 2
  sites d'appel réels le déréférencent quand non nul, donc
  `kOfflineStatus` était un vrai risque de crash, pas un trou cosmétique.
- r184 a corrigé `XeCryptSha` : calcule un VRAI condensé SHA-1 (OpenSSL
  EVP) sur les octets invités réels — le condensé alimente une
  comparaison réelle en aval, donc un condensé absent échouait toujours.
- r185 a corrigé `RtlTimeToTimeFields`/`RtlTimeFieldsToTime` (via C++20
  `<chrono>`) — complète le fix r179 (`KeQuerySystemTime`), resté
  incomplet seul puisque ces 2 imports étaient encore des no-op.
- r186 a corrigé `RtlFillMemoryUlong`/`RtlCompareMemoryUlong` (algorithme
  RTL standard fixe, aucune ambiguïté).
- r187 a corrigé `RtlUnicodeToMultiByteN` : convertit et renvoie
  `STATUS_SUCCESS` — le seul site d'appel réel prenait TOUJOURS la branche
  d'échec avec `kOfflineStatus` (NTSTATUS négatif).
- r188 a corrigé `RtlUnicodeStringToAnsiString`/`RtlFreeAnsiString`
  (allocation réelle via `allocate_guest`, conversion, libération
  cohérente avec le précédent `ExFreePool`).
- r189 a corrigé `NtQueryFullAttributesFile` (réutilise la forme
  ObjectAttributes de `NtCreateFile`; ajoute
  `NativeGuestMediaService::file_size()`).
- r190 a corrigé `NtQueryVolumeInformationFile` (FileFsSizeInformation,
  unité d'allocation FATX 16 Kio, 8 Gio libre/total — un 2e site de
  validation non entièrement retracé, nommé honnêtement).
- r191 a corrigé les primitives spinlock/IRQL (`KfAcquireSpinLock`/
  `KfReleaseSpinLock`, `KeAcquireSpinLockAtRaisedIrql`/
  `KeReleaseSpinLockFromRaisedIrql`, `KeRaiseIrqlToDpcLevel`/`KfLowerIrql`)
  — vraie exclusion mutuelle, même risque de concurrence réelle que r116
  mais pour une famille bien plus répandue (88-110 sites d'appel réels par
  fonction).
- r192 a corrigé le reste de cette famille : `KeTryToAcquireSpinLockAtRaisedIrql`
  (variante non bloquante) et `KeInitializeSemaphore`/`KeReleaseSemaphore`
  (un vrai `KSEMAPHORE` jamais relâché — tout wait expirait toujours).
  Vérifié aussi, non corrigé : `NtQueryInformationFile` (bloqué par le
  média en lecture seule, même famille que r178); `sprintf`/`_vsnprintf`
  (moteur printf varargs, hors scope d'un cycle borné).
- r193 a corrigé `KeBugCheck`/`KeBugCheckEx` (ne retournent jamais sur
  vrai matériel; le no-op offline retournait normalement — vrai risque
  d'exécution après un point jamais prévu comme atteignable — maintenant
  `std::abort()` avec diagnostic réel).
- r194 a corrigé `KeDelayExecutionThread` (le no-op offline retournait
  instantanément au lieu d'attendre — maintenant un vrai
  `std::this_thread::sleep_for` sur l'intervalle relatif réel).
- r195 a corrigé `XamAlloc`/`XamFree` (statut Win32 signé — le no-op
  offline échouait systématiquement aux 3 sites d'appel réels — utilise
  maintenant `allocate_guest`).
- r196 a corrigé `ObCreateSymbolicLink`/`ObDeleteSymbolicLink` (boucle
  réelle de montage de périphérique — le no-op offline échouait
  systématiquement, un vrai blocage de boot — retourne maintenant
  `STATUS_SUCCESS` sans condition).
- r197 a corrigé `KeLockL2`/`KeUnlockL2`/`KiApcNormalRoutineNop` (retour
  ignoré par tous les appelants réels — `STATUS_SUCCESS` sans condition).
  Vérifié aussi, non corrigé : `XamSessionCreateHandle`/
  `XamSessionRefObjByHandle` (pas assez tracé) et `NtDuplicateObject`
  (signature ambiguë).
- r198 a corrigé `XamTaskShouldExit` (défaut « continue le travail » au
  lieu d'abandonner immédiatement). Vérifié aussi, non corrigé :
  `XamTaskSchedule`/`XamTaskCloseHandle` (nécessiterait un sous-système
  d'exécution de callback invité) et `VdGetSystemCommandBuffer` (hors
  politique du renderer natif).
- r199 a corrigé `NtFlushBuffersFile` (média en lecture seule, jamais
  d'écriture en attente — `STATUS_SUCCESS` sans condition).
- r200 a corrigé `XNotifyGetNext`/`XNotifyPositionUI` (le no-op offline
  signalait une notification à chaque appel, lisant un id depuis la
  pile non initialisée — retourne maintenant « aucune notification »).
- r201 a corrigé `NtOpenFile` (9 sites d'appel réels — le plus haut
  compte du balayage — rejoint le chemin média déjà correct de
  `NtCreateFile`, même contrat de registres r3/r5/r6).
- r202 (documentation seule) : `NtWriteFile`/`NtDeviceIoControlFile`
  tracés comme un vrai écriveur de sauvegarde (forme FATX classique) —
  c'est la frontière « save/reload » de Gate 2 au niveau binaire, non
  implémentée (nécessite un vrai support d'écriture). SEH
  (`RtlRaiseException`/`RtlUnwind`/`RtlCaptureContext`) et clé console
  (`XeKeysConsolePrivateKeySign`/`Verification`, hors de portée
  permanente) vérifiés sans fix sûr.
- r203 corrige r197 : `0x823cfe4c` est le vrai import
  `XMsgStartIORequest` (17 sites réels), pas une fonction interne de
  télémétrie — la famille `XamSession*` de r197 fait du vrai trafic
  IPC, effort de fix plus large que pensé, toujours différé. Corrigé
  aussi : `XAudioGetVoiceCategoryVolumeChangeMask`/
  `XAudioGetVoiceCategoryVolume` (masque « rien n'a changé », volume
  plein par défaut).
- r204 a corrigé `IoDismountVolume`/`IoDismountVolumeByFileHandle`
  (retour ignoré par tous les appelants réels — `STATUS_SUCCESS` sans
  condition).
- r205 a corrigé `XamNotifyCreateListener` (retourne un HANDLE, pas un
  NTSTATUS — alloue maintenant un vrai handle via `g_next_handle`).
- r206 a corrigé la famille `XAudioRegisterRenderDriverClient`/
  `Unregister`/`SubmitRenderDriverFrame` (Unregister bloquait
  systématiquement l'init audio — vérifié en signé, kOfflineStatus
  négatif). Vérifié aussi, non corrigé : `XamGetExecutionId`
  (champ de struct non confirmé), `XamShowMessageBoxUIEx` (attente
  overlapped non implémentée), 6 dialogues `XamShow*` (trampolines non
  tracés).
- r207 a corrigé `XamVoiceHeadsetPresent` (booléen, pas NTSTATUS —
  signalait « casque présent » à tort). Vérifié aussi, non corrigé :
  `XamVoiceCreate` (échec actuel déjà honnête, pas de fix forcé).
- r208 a corrigé `XamVoiceClose` (retour ignoré aux 3 sites d'appel
  réels — `STATUS_SUCCESS` sans condition).
- r209 a corrigé `XamLoaderTerminateTitle` (ne retourne jamais — aucun
  épilogue après son 2e site d'appel réel — `std::exit(0)`). Addenda :
  `XamGetExecutionId` (r206) garde en réalité au moins 4 sites d'appel
  réels de `XamUserReadProfileSettings`, portée plus large que scopée —
  toujours non corrigé.
- r210 a corrigé `XMACreateContext`/`XMAReleaseContext` (Create bloquait
  systématiquement l'init audio XMA, vérifié en signé). Vérifié aussi,
  non corrigé : `XamVoiceSubmitPacket` (dépend d'un handle que
  `XamVoiceCreate` ne produit jamais — fixer seul serait inerte).
- r211 (documentation seule) : escalade de `XamGetExecutionId` — garde
  en réalité au moins 7 sites d'appel réels (`XamUserReadProfileSettings`
  ×4, `XamUserCreateStatsEnumerator` ×2,
  `XamUserCreateAchievementEnumerator` ×1); vraie signature = pointeur-
  vers-pointeur, pas remplissage de struct. `XamUserAreUsersFriends`
  vérifié adéquat sans fix. `XamUserGetXUID`/`GetSigninInfo` vérifiés,
  non corrigés (motif de masquage de retour non confirmé).
- r212 (documentation seule) : `XexGetModuleHandle`/
  `XexGetProcedureAddress` — l'échec actuel EST le bon chemin de repli
  statique (motif de compatibilité ascendante Xbox 360), confirmé
  adéquat, aucun fix nécessaire. `XamContentCreateEx` vérifié, même
  territoire save/reload que r202.
- r213 a corrigé `ExTerminateThread` (ne retourne jamais, termine
  seulement son propre thread — `GuestThreadTerminated`, rattrapé par
  `ExCreateThread` et la sonde d'entrée principale) et
  `ExRegisterTitleTerminateNotification` (retour ignoré partout).
  Édite de vrais fichiers `native/` — `prepare.py` relancé.
- r214 a corrigé `HalReturnToFirmware` (ne retourne jamais, arrêt
  niveau console — `std::exit(0)`).
- r215 a corrigé `XMsgCancelIORequest` (retour ignoré aux 3 sites
  d'appel réels — `STATUS_SUCCESS` sans condition).
- r216 a corrigé `NtSetTimerEx`/`NtCancelTimer`/`NtCreateTimer` (signature
  8 arguments résolue via wrapper; minuteur réel via `std::thread`,
  enregistré dans `g_events` — `NtCreateTimer` ne l'enregistrait jamais
  avant).
- r217 a corrigé `VdSetDisplayMode` (retour ignoré — `STATUS_SUCCESS`
  sans condition). Vérifié aussi, non corrigé : `VdPersistDisplay`
  (territoire renderer natif fail-closed).
- r218 (documentation seule) : bilan complet du balayage r169-r217
  (125→87 imports restants), catégorisé par gros chantier. Voir ce
  rapport avant de reprendre le balayage — il évite de redécouvrir la
  carte des chantiers restants.
- r219 (documentation seule) : corrige r209/r211 — la porte
  `XamGetExecutionId` (5 appelants réels tracés jusqu'à LEURS propres
  appelants) est TOUJOURS contournée (valeur de contrôle littérale 0
  partout), jamais un vrai blocage. `XamUserCreateStatsEnumerator`/
  `XamUserCreateAchievementEnumerator` confirmés déjà adéquats;
  `XamUserReadProfileSettings` reste différé mais pour son propre
  contrat d'achèvement asynchrone (même famille que
  `XamShowMessageBoxUIEx`), pas la porte.
- r220 (documentation seule) : protocole d'achèvement overlapped
  partiellement tracé (layout Internal@0/InternalHigh@4 confirmé via le
  helper d'attente `Function_821F50F8`) mais NON implémenté — l'adresse
  pile réelle de `pOverlapped` reste ambiguë entre plusieurs candidates,
  risque réel d'écriture au mauvais offset.
- r221 (documentation seule) : signature réelle à 9 paramètres de
  `XamShowMessageBoxUIEx` entièrement résolue (`pOverlapped` = 9e
  argument pile, résultat final à `pOverlapped+0x14`). Toujours non
  implémenté : convention d'accès `ctx.r1.u32 + 0x54` pour un argument
  pile depuis un stub natif non confirmée par un exemple existant dans
  ce projet (recherché, résultat négatif).
- r222 a corrigé `XamShowMessageBoxUIEx` (réexamen : la réserve de r221
  ne s'appliquait pas — `ctx.r1.u32` EST le `r1` de l'appelant par
  construction, pas une convention à confirmer séparément. Écrit
  `pMessageBoxResult`/`pOverlapped+0x14` à 0, retourne `STATUS_SUCCESS`
  jamais 997, saute le helper d'attente asynchrone).
- r223 a corrigé `XamUserReadProfileSettings` (les deux cibles de
  branchement « code non concordant » de l'appelant sont des retours
  propres, pas des erreurs — retourne `STATUS_SUCCESS`; struct-fill non
  fait, signature au-delà de 4 paramètres pas assez confirmée).
- r224 (documentation seule) : les trampolines `XamShow*` sont en
  réalité les cibles de repli d'une table de résolution dynamique (≥14
  entrées) IDENTIQUE au mécanisme déjà confirmé actif par r212 pour
  `XexGetModuleHandle`/`XexGetProcedureAddress` — confirmées
  atteignables (pas du code mort), mais toujours non implémenté (la
  fonction résolveur qui lit cette table reste à tracer).
- r225 (documentation seule) corrige r224 : `scripts/ReferencesTo.java`
  (déjà présent, interroge par adresse brute) montre que 12 des 14
  entrées de la table n'ont AUCUNE référence dans ce XEX, et que la base
  de la table elle-même n'en a aucune non plus — il n'existe aucune
  preuve statique d'une fonction résolveur qui lirait cette table en
  boucle. Seules 2 adresses voisines (`0x821f4680`, `0x821f4678`, ni
  l'une ni l'autre porteuse d'un symbole Ghidra) ont de vrais appelants
  directs — mais ce sont des adresses `.text` internes ordinaires déjà
  recompilées par XenonRecomp, PAS des gaps de stub d'import : ce fil de
  recherche est clos et hors périmètre du balayage.
- r226 a corrigé `XamUserGetName` (2 vrais sites d'appel confirment
  indépendamment `cchUserName=0x10`; l'un des deux appelants n'a jamais
  vérifié le statut de retour et utilisait le buffer sans condition —
  même classe de risque que r183. Écrit un nom ASCII synthétique
  explicite (« Player »), tronqué/terminé à la taille confirmée,
  retourne `STATUS_SUCCESS`).
- r227 a corrigé `XamUserGetSigninInfo` (escalade de r211 : le wrapper
  passthrough confirmé par désassemblage brut, 6 vrais appelants, 3
  tracés — tous lisent un seul bit à l'offset +8 qui conditionne
  l'exécution de la vraie logique par-joueur. Remplit XUID=0 et le bit
  de garde à 0 pour l'utilisateur 0 (même convention que
  `XamUserGetSigninState`, r176); les autres index gardent l'échec
  offline existant).
- r228 a corrigé `XamUserGetXUID` (escalade de r211 : wrapper à
  remappage d'arguments confirmé par désassemblage brut — insère
  `dwFlags=7` littéral, même motif que `NtSetTimerEx`/
  `XamShowMessageBoxUIEx`. 6 vrais appelants, 3 tracés confirment un
  XUID de sortie 8 octets; l'un des trois utilise le buffer sans
  aucune vérification de statut. Remplit XUID=0 pour l'utilisateur 0,
  même convention que r227; les autres index gardent l'échec offline).
- r229 (documentation seule) : 11 imports confirmés MORTS dans ce build
  (zéro appelant réel) — `XamWriteGamerTile`, tout le cluster
  `XamContent{GetDeviceState,GetDeviceData,Close,Delete,SetThumbnail,
  CreateEnumerator}`, `NtQueryDirectoryFile`, `NtReadFileScatter`,
  `StfsControlDevice`, `StfsCreateDevice`, `XamLoaderLaunchTitle`,
  `XamContentCreateEx`, `XamEnumerate`. `NtSetInformationFile` (9 sites
  réels, le plus haut compte tracé) confirmé déjà adéquat : les 4 sites
  tracés vérifient tous le statut avant de continuer, même famille que
  le chantier save/reload de r202, pas un bug de forme de contrat.
- r230 a corrigé `XamTaskCloseHandle` (r198 avait différé ce couple avec
  `XamTaskSchedule` ensemble, mais son unique vrai site d'appel ignore
  totalement le retour — même motif que `KeLockL2`/`IoDismountVolume`/
  `XamVoiceClose`/`XMsgCancelIORequest`. `XamTaskSchedule` lui-même reste
  différé). `__C_specific_handler` vérifié : zéro référence dans ce XEX,
  cohérent avec r202 (SEH jamais réellement invoqué dans ce build).
- r231 (documentation seule) : les 29 imports `NetDll_*` sont clos —
  26 sans aucun appelant réel, et les 3 restants
  (`WSAGetLastError`/`___WSAFDIsSet`/`XNetQosLookup`) déjà adéquats : le
  générique offline ne correspond jamais aux valeurs comparées par leurs
  appelants, et ces 3 chaînes sont de toute façon gardées par un champ
  handle-socket qui reste toujours `-1` puisque `socket`/`connect` sont
  eux-mêmes morts. Bucket fermé, pas seulement réduit.
- r232 (documentation seule) : `XamSessionCreateHandle`/
  `XamSessionRefObjByHandle` (r197 : « pas assez tracé ») entièrement
  tracés — 1 + 11 sites réels, TOUS vérifient le statut avant d'utiliser
  l'objet référencé ou retournent l'échec honnêtement sinon. Le
  générique offline est déjà correct partout; paire close, aucun fix
  nécessaire.
- r233 (documentation seule) : `NtDuplicateObject`/`XamVoiceCreate`/
  `XamVoiceSubmitPacket` re-confirmés déjà adéquats (tous les sites
  réels vérifient le statut). `XexCheckExecutablePrivilege` : la
  tentation d'un fix (« défaut = privilège refusé, offline ») a été
  examinée et EXPLICITEMENT refusée à nouveau — r178 avait déjà pesé
  exactement ce compromis sans contrôle disponible pour trancher; aucune
  preuve nouvelle ne renverse cette décision.
- r234 (documentation seule) : `sprintf`/`_vsnprintf` scopés (7+2 sites
  réels, formats hétérogènes, chemins de diagnostic save/reload) —
  confirme r192 : moteur printf varargs complet nécessaire, hors d'un
  cycle borné. **Le balayage des imports offline (r148-r233) est déclaré
  à son point d'arrêt naturel** : chaque candidat restant du catalogue
  r218 a désormais une disposition tracée et nommée (mort, déjà adéquat,
  bloqué par sous-système/politique/périmètre, ou sans cas de contrôle);
  aucun ne correspond plus à la méthode de ce balayage (fix borné à un
  seul import, dérivé de preuve). Rouvrir n'importe lequel exige une
  preuve nouvelle, pas un nouveau passage sur les mêmes sites d'appel.
- r235 (documentation seule) : tentative de réduire encore le périmètre
  de `_vsnprintf` — les spécificateurs des 7 sites `sprintf` sont bien un
  petit ensemble fermé (`%s`/`%d`/`%x`/`%X`), mais le vrai consommateur
  de `_vsnprintf` (`Function_821EF4E0`/`Function_821EF458`, des wrappers
  qui transmettent leurs propres varargs) a 20 + 8 vrais appelants réels
  répartis sur au moins 4 fonctions distinctes, la plupart avec des
  chaînes de format non encore décodées — un seul chemin (dump crash
  `Function_821EF878`) est confirmé mort, les autres non. Réaffirme
  r234 avec bien plus de preuve : une implémentation partielle
  désynchroniserait silencieusement les lectures varargs sur tout
  spécificateur non couvert — pire que le no-op honnête actuel.
- Suite pytest 209/209 (208 + 1 skip, inchangée), `ctest` 10/10.
- La chaîne DATA.TBL est tracée et close à son niveau actuel. La traduction
  `IM_LOAD_IMMEDIATE` vers SPIR-V reste bloquée par politique de preuve.
- PAL, M02–M15, save/reload et release restent bloqués par Gate 2.

## Prochaine décision

Le balayage des imports offline (r148-r234) est clos : plus aucun
candidat borné n'y reste. Les 3 pistes suivantes restent ouvertes, mais
aucune n'est actionnable sans une ressource externe ou une décision de
périmètre explicite — ce ne sont pas des tâches à reprendre seul sans
cette décision :

1. Confirmer par une observation runtime (avec un vrai périphérique quand
   disponible — absent de cet environnement) que le backend d'entrée
   r180 résout effectivement le symptôme historique « contrôles nuls ».
2. Si le 2e site de validation de r190 (comparaison de l'unité
   d'allocation contre une valeur attendue non retracée) échoue en
   pratique, tracer la source de cette valeur avant d'ajuster les
   constantes — conditionné à une observation qui n'a pas eu lieu.
3. Une décision explicite de aller/pas-aller pour construire l'un des
   sous-systèmes nommés par r234 (écriture save/reload, exécution de
   callback invité pour `XamTaskSchedule`, moteur printf varargs) serait
   la prochaine frontière substantielle du balayage d'imports — aucun
   n'est un fix borné à un cycle, et en démarrer un sans décision de
   périmètre explicite violerait la discipline du projet contre
   l'invention de portée. Si le frontier « save/reload » est un jour
   repris : r202 a tracé sa forme binaire réelle
   (`NtOpenFile`→`NtDeviceIoControlFile`→boucle `NtWriteFile` dans
   `Function_82392878`/la fonction à `0x82392978`) — partir de ces
   adresses plutôt que de redécouvrir la forme.
5. Ne pas supposer qu'un import est un remplissage de structure sans lire ses
   sites d'appel réels — r170 a montré que l'hypothèse de r168/r169 pour
   `VdQueryVideoFlags` était fausse. Ne pas supposer non plus qu'une valeur
   parmi plusieurs candidates également plausibles est arbitraire sans lire
   comment CE XEX la consomme — r172 a montré que le contrôle de flux propre
   du binaire tranche entre `0x101` et `0x102`.

`done_when` : layout binaire qualifié, implémentation sans valeur devinée,
tests ciblés verts, CTest 9/9 et validation native fraîche. Si le layout reste
ambigu après valorisation statique, nommer précisément l'ambiguïté avant toute
observation runtime.

## Preuves courantes

- `reports/handoff/CURRENT.json`;
- `reports/ac6-retail-native-codegen-gate2-r235-doc-vsnprintf-helper-is-pervasive-not-bounded-20260903.md`;
- `reports/ac6-retail-native-codegen-gate2-r234-doc-offline-import-sweep-at-its-natural-stopping-point-20260903.md`;
- `reports/ac6-retail-native-codegen-gate2-r233-doc-remaining-voice-and-privilege-imports-already-handled-20260903.md`;
- `reports/ac6-retail-native-codegen-gate2-r232-doc-xamsession-pair-fully-traced-already-adequate-20260903.md`;
- `reports/ac6-retail-native-codegen-gate2-r231-doc-networking-cluster-dead-or-already-adequate-20260903.md`;
- `reports/ac6-retail-native-codegen-gate2-r230-real-fix-xamtaskclosehandle-returns-success-20260903.md`;
- `reports/ac6-retail-native-codegen-gate2-r229-doc-unreached-cluster-plus-ntsetinformationfile-already-adequate-20260903.md`;
- `reports/ac6-retail-native-codegen-gate2-r228-real-fix-xamusergetxuid-fills-a-zero-xuid-for-user-zero-20260903.md`;
- `reports/ac6-retail-native-codegen-gate2-r227-real-fix-xamusergetsignininfo-fills-the-gating-bit-for-user-zero-20260903.md`;
- `reports/ac6-retail-native-codegen-gate2-r226-real-fix-xamusergetname-writes-a-name-and-succeeds-20260903.md`;
- `reports/ac6-retail-native-codegen-gate2-r225-doc-xamshow-table-has-no-static-resolver-callers-go-direct-20260903.md`;
- `reports/ac6-retail-native-codegen-gate2-r224-doc-xamshow-trampolines-are-the-fallback-table-from-r212-20260902.md`;
- `reports/ac6-retail-native-codegen-gate2-r223-real-fix-xamuserreadprofilesettings-returns-success-20260902.md`;
- `reports/ac6-retail-native-codegen-gate2-r218-doc-sweep-status-checkpoint-r169-through-r217-20260902.md`
  (bilan par gros chantier — à lire avant de reprendre le balayage);
- `STATE.md` et `EVIDENCE.md` pour l'historique (r169-r224).

Le catalogue d'architecture local manque; aucune assertion générique ne doit
en être dérivée. N2 sous `reconstruction/` reste historique et hors cible.
