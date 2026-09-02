# AC6 retail NTSC-U/J — reprise Gate 2 runtime natif

Reprendre depuis `recompilation/ace-combat-6-retail`.

Lire d'abord :

- `reports/handoff/CURRENT.json`;
- le report r224 cité comme `source_report`;
- `NEXT.md`;
- `STATE.md` et `EVIDENCE.md` seulement pour une question historique nommée.

## Frontière active

La famille de configuration plateforme Vd/X ouverte par r168 est
entièrement fermée depuis r175. r176/r177 ont corrigé
`XamUserGetSigninState`/`XamGetSystemVersion`. r178 (doc seule) a vérifié
`XexCheckExecutablePrivilege` sans trouver de fix sûr (précédent r164).
r179 a corrigé `KeQuerySystemTime`. r180 (autorisé explicitement par
l'utilisateur : « utilise SDL2 pour le backend d'entrée ») a implémenté le
backend d'entrée manette natif via SDL2 (`NativeGuestInputService`,
`native/include/ac6/native_guest_input.h`) —
`XamInputGetState`/`SetState`/`GetCapabilities` fonctionnels, preuve réelle
du contrat de transfert (code d'erreur `0x48F`, offsets
`XINPUT_CAPABILITIES`). Aucune manette physique dans ce bac à sable — le
symptôme « contrôles nuls » reste à confirmer par une future observation
runtime avec un vrai périphérique. r181 a fermé le dernier import de la
famille `XamInput*` (`XamInputGetKeystrokeEx`, renvoie `ERROR_EMPTY`).
r182 a corrigé `XamUserCheckPrivilege` (`ERROR_SUCCESS` + bool `TRUE`).
r183 a corrigé `RtlImageXexHeaderField` (renvoie `0`/absent — la valeur de
retour EST le pointeur de champ ici; un site d'appel le déréférence
directement, donc `kOfflineStatus` était un vrai risque de crash). r184 a
corrigé `XeCryptSha` (VRAI SHA-1 via OpenSSL EVP, déjà lié). r185 a corrigé
`RtlTimeToTimeFields`/`RtlTimeFieldsToTime` (C++20 `<chrono>`) — complète
r179, resté incomplet seul. r186 a corrigé
`RtlFillMemoryUlong`/`RtlCompareMemoryUlong` (algorithme RTL standard
fixe). r187 a corrigé `RtlUnicodeToMultiByteN` (convertit et renvoie
`STATUS_SUCCESS`). r188 a corrigé
`RtlUnicodeStringToAnsiString`/`RtlFreeAnsiString` (allocation réelle via
`allocate_guest`). r189 a corrigé `NtQueryFullAttributesFile` (ajoute
`NativeGuestMediaService::file_size()`). r190 a corrigé
`NtQueryVolumeInformationFile` (FileFsSizeInformation, unité
d'allocation FATX 16 Kio). r191 a corrigé les primitives spinlock/IRQL
(`KfAcquireSpinLock`/`KfReleaseSpinLock`,
`KeAcquireSpinLockAtRaisedIrql`/`KeReleaseSpinLockFromRaisedIrql`,
`KeRaiseIrqlToDpcLevel`/`KfLowerIrql`) — vraie exclusion mutuelle,
88-110 sites d'appel réels par fonction, même risque de concurrence
réelle que r116 (`spin_lock_for` non récursif comme un vrai spinlock;
`g_dpc_level_mutex` récursif car l'IRQL réel est un état par thread, pas
une identité d'objet). r192 a fermé le reste de cette famille :
`KeTryToAcquireSpinLockAtRaisedIrql` (variante non bloquante) et
`KeInitializeSemaphore`/`KeReleaseSemaphore` (un vrai `KSEMAPHORE` jamais
relâché — tout `KeWaitForSingleObject` dessus expirait toujours). Vérifié
aussi sans corriger : `NtQueryInformationFile`/`NtSetInformationFile`
(séquence de finalisation de fichier, bloquée par le média en lecture
seule) et `sprintf`/`_vsnprintf` (moteur printf varargs, hors scope d'un
cycle borné). r193 a corrigé `KeBugCheck`/`KeBugCheckEx` (ne retournent
jamais sur vrai matériel; maintenant `std::abort()` avec diagnostic réel
au lieu du no-op offline qui laissait l'exécution continuer après un
point jamais prévu comme atteignable). r194 a corrigé
`KeDelayExecutionThread` (retournait instantanément au lieu d'attendre —
maintenant un vrai `std::this_thread::sleep_for` sur l'intervalle
relatif réel lu depuis la mémoire invitée). r195 a corrigé
`XamAlloc`/`XamFree` (statut Win32 signé, pas un NTSTATUS — le no-op
offline échouait systématiquement aux 3 sites d'appel réels
`XamAlloc`; utilise maintenant `allocate_guest`). r196 a corrigé
`ObCreateSymbolicLink`/`ObDeleteSymbolicLink` (boucle réelle de montage
de périphérique au boot — le no-op offline échouait systématiquement —
retourne maintenant `STATUS_SUCCESS` sans condition). r197 a corrigé
`KeLockL2`/`KeUnlockL2`/`KiApcNormalRoutineNop` (retour ignoré par tous
les appelants réels). Vérifié aussi, non corrigé :
`XamSessionCreateHandle`/`XamSessionRefObjByHandle` (11+1 sites réels,
famille de wrappers télémétrie, pas assez tracé) et `NtDuplicateObject`
(signature ambiguë, aucun handle de sortie capturé). r198 a corrigé
`XamTaskShouldExit` (défaut « continue le travail »). Vérifié aussi, non
corrigé : `XamTaskSchedule`/`XamTaskCloseHandle` (sous-système de
callback invité, hors scope d'un cycle borné) et
`VdGetSystemCommandBuffer` (hors politique du renderer natif). r199 a
corrigé `NtFlushBuffersFile` (média en lecture seule — `STATUS_SUCCESS`
sans condition). r200 a corrigé `XNotifyGetNext`/`XNotifyPositionUI`
(le no-op offline signalait une notification à chaque appel, lisant un
id depuis la pile non initialisée — retourne maintenant « aucune
notification »). r201 a corrigé `NtOpenFile` (9 sites d'appel réels —
rejoint le chemin média déjà correct de `NtCreateFile`, même contrat de
registres r3/r5/r6). r202 (documentation seule, aucun changement de
code) a tracé `NtWriteFile`/`NtDeviceIoControlFile` comme un vrai
écriveur de sauvegarde FATX (`NtOpenFile`→IOCTL géométrie→boucle
`NtWriteFile` à décalage croissant dans `Function_82392878`/la fonction
à `0x82392978`) — la forme binaire réelle de la frontière save/reload
de Gate 2, non implémentée. Vérifié aussi, non corrigé : SEH
(`RtlRaiseException`/`RtlUnwind`/`RtlCaptureContext`) et
`XeKeysConsolePrivateKeySign`/`Verification` (clé matérielle console,
hors de portée permanente). r203 corrige r197 : `0x823cfe4c` est le vrai
import `XMsgStartIORequest` (17 sites réels, transport IPC), pas une
fonction interne de télémétrie comme r197 l'affirmait — corrigé aussi :
`XAudioGetVoiceCategoryVolumeChangeMask`/`XAudioGetVoiceCategoryVolume`.
r204 a corrigé `IoDismountVolume`/`IoDismountVolumeByFileHandle`
(retour ignoré par tous les appelants réels). r205 a corrigé
`XamNotifyCreateListener` (retourne un HANDLE, pas un NTSTATUS — alloue
maintenant un vrai handle via `g_next_handle`). r206 a corrigé la
famille `XAudioRegisterRenderDriverClient`/`Unregister`/
`SubmitRenderDriverFrame` (Unregister bloquait systématiquement l'init
audio, vérifié en signé). Vérifié aussi, non corrigé :
`XamGetExecutionId`, `XamShowMessageBoxUIEx`, 6 dialogues `XamShow*`.
r207 a corrigé `XamVoiceHeadsetPresent` (booléen, pas NTSTATUS — signalait
« casque présent » à tort). Vérifié aussi, non corrigé : `XamVoiceCreate`
(échec actuel déjà honnête). r208 a corrigé `XamVoiceClose` (retour
ignoré aux 3 sites d'appel réels). r209 a corrigé
`XamLoaderTerminateTitle` (ne retourne jamais — `std::exit(0)`).
Addenda : `XamGetExecutionId` (r206) garde en réalité au moins 4 sites
d'appel réels de `XamUserReadProfileSettings` — portée plus large,
toujours non corrigé. r210 a corrigé `XMACreateContext`/
`XMAReleaseContext` (Create bloquait l'init audio XMA, vérifié en
signé). Vérifié aussi, non corrigé : `XamVoiceSubmitPacket` (fixer
seul serait inerte sans `XamVoiceCreate`). r211 (documentation seule)
escalade `XamGetExecutionId` — garde en réalité au moins 7 sites
d'appel réels; vraie signature = pointeur-vers-pointeur, pas
remplissage de struct. `XamUserAreUsersFriends` vérifié déjà adéquat.
`XamUserGetXUID`/`GetSigninInfo` vérifiés, non corrigés. r212
(documentation seule) confirme que l'échec de `XexGetModuleHandle`/
`XexGetProcedureAddress` EST le bon chemin de repli statique, aucun fix
nécessaire. r213 a corrigé `ExTerminateThread` (ne retourne jamais,
termine seulement son propre thread via `GuestThreadTerminated`,
rattrapé par `ExCreateThread` et la sonde d'entrée principale — édite
de vrais fichiers `native/`, `prepare.py` relancé) et
`ExRegisterTitleTerminateNotification` (retour ignoré partout). r214 a
corrigé `HalReturnToFirmware` (ne retourne jamais, arrêt niveau console
— `std::exit(0)`). r215 a corrigé `XMsgCancelIORequest` (retour ignoré
aux 3 sites d'appel réels). r216 a corrigé
`NtSetTimerEx`/`NtCancelTimer`/`NtCreateTimer` (signature 8 arguments
résolue via son wrapper; minuteur réel via `std::thread`, enregistré
dans `g_events` — `NtCreateTimer` ne l'enregistrait jamais avant, même
classe que r145). r217 a corrigé `VdSetDisplayMode` (retour ignoré —
`STATUS_SUCCESS` sans condition). Vérifié aussi, non corrigé :
`VdPersistDisplay` (territoire renderer natif fail-closed). Le reste de
la liste générique (~87 imports) se concentre désormais dans une
poignée de gros chantiers déjà documentés (réseau, SEH, écriture de
sauvegarde, XMsg, printf, identité/profil, trampolines UI) — les
petites victoires isolées se raréfient. r218 (documentation seule) est
un bilan complet du balayage r169-r217 (125→87 imports restants),
catégorisé par gros chantier — le lire avant de reprendre le balayage.
r219 (documentation seule) corrige r209/r211 : la porte
`XamGetExecutionId` (5 appelants réels tracés jusqu'à leurs propres
appelants) est TOUJOURS contournée (contrôle littéral 0 partout).
`XamUserCreateStatsEnumerator`/`XamUserCreateAchievementEnumerator`
confirmés déjà adéquats; `XamUserReadProfileSettings` reste différé
pour son propre contrat d'achèvement asynchrone (même famille que
`XamShowMessageBoxUIEx`). r220 (documentation seule) a partiellement
tracé le protocole d'achèvement overlapped (layout
Internal@0/InternalHigh@4 confirmé) mais NON implémenté — adresse pile
réelle de `pOverlapped` ambiguë entre plusieurs candidates, risque réel
d'écriture au mauvais offset. r221 (documentation seule) a résolu la
signature réelle à 9 paramètres de `XamShowMessageBoxUIEx`
(`pOverlapped` = 9e argument pile, résultat à `pOverlapped+0x14`) mais
reste non implémenté : convention `ctx.r1.u32 + 0x54` pour un argument
pile depuis un stub natif non confirmée (recherché, résultat négatif). r222 a corrigé `XamShowMessageBoxUIEx`
(la réserve de r221 ne s'appliquait pas — `ctx.r1.u32` EST le `r1` de
l'appelant par construction; écrit `pMessageBoxResult`/
`pOverlapped+0x14` à 0, retourne `STATUS_SUCCESS` jamais 997). r223 a corrigé
`XamUserReadProfileSettings` (les deux cibles de branchement de
l'appelant sont des retours propres — retourne `STATUS_SUCCESS`;
struct-fill non fait, signature au-delà de 4 paramètres pas assez
confirmée). r224 (documentation seule) a trouvé que les trampolines
`XamShow*` sont les cibles de repli d'une table de résolution
dynamique (≥14 entrées) IDENTIQUE au mécanisme de r212 pour
`XexGetModuleHandle` — confirmées atteignables, toujours non
implémenté (résolveur de la table à tracer). r225 (documentation
seule) corrige r224 : `scripts/ReferencesTo.java` (déjà présent,
interroge par adresse brute sans besoin de symbole) montre qu'il
n'existe AUCUNE preuve statique d'un résolveur qui lirait cette table
— 12/14 entrées et la base de la table elle-même n'ont aucune
référence. Les 2 adresses voisines qui ont de vrais appelants
directs (`0x821f4680`, `0x821f4678`) ne portent aucun symbole Ghidra
et sont du `.text` interne déjà recompilé, pas des gaps de stub
d'import — ce fil est clos, hors périmètre du balayage. r226 a corrigé
`XamUserGetName` (2 vrais appelants confirment `cchUserName=0x10`;
l'un ignorait le statut de retour et utilisait un buffer jamais écrit
— écrit maintenant un nom ASCII synthétique et retourne
`STATUS_SUCCESS`). r227 a corrigé `XamUserGetSigninInfo` (wrapper
passthrough confirmé par désassemblage brut; 3 vrais appelants tracés
lisent tous un seul bit de garde à +8 qui conditionne la vraie logique
par-joueur — remplit XUID=0 et le bit à 0 pour l'utilisateur 0, même
convention que r176). r228 a corrigé `XamUserGetXUID` (wrapper à
remappage d'arguments confirmé par désassemblage brut, insère
`dwFlags=7`; 3 vrais appelants tracés confirment un XUID 8 octets —
remplit XUID=0 pour l'utilisateur 0, même convention que r227). r229
(documentation seule) a confirmé 11 imports morts (zéro appelant réel :
`XamWriteGamerTile`, tout le cluster `XamContent*`,
`NtQueryDirectoryFile`, `NtReadFileScatter`, `Stfs{Control,Create}Device`,
`XamLoaderLaunchTitle`, `XamContentCreateEx`, `XamEnumerate`) et
`NtSetInformationFile` déjà adéquat (9 sites réels, tous vérifient le
statut, même chantier save/reload que r202). r230 a corrigé
`XamTaskCloseHandle` (r198 l'avait différé avec `XamTaskSchedule`, mais
son unique appelant réel ignore le retour — `STATUS_SUCCESS`
inconditionnel; `XamTaskSchedule` reste différé). `__C_specific_handler`
vérifié mort (zéro référence). r231 (documentation seule) a clos les 29
imports `NetDll_*` : 26 morts, 3 déjà adéquats (gardés par un handle
socket toujours `-1` puisque `socket`/`connect` sont morts). r232
(documentation seule) a clos `XamSessionCreateHandle`/
`XamSessionRefObjByHandle` (1+11 sites réels tracés, tous vérifient le
statut — déjà adéquats). r233 (documentation seule) re-confirme
`NtDuplicateObject`/`XamVoiceCreate`/`XamVoiceSubmitPacket` déjà
adéquats et refuse À NOUVEAU un fix pour `XexCheckExecutablePrivilege`
(pas de cas de contrôle, décision de r178 tenue). r234 (documentation
seule) scope `sprintf`/`_vsnprintf` (7+2 sites réels, moteur printf
varargs complet nécessaire, hors cycle borné) et **déclare le balayage
des imports offline (r148-r233) à son point d'arrêt naturel** : chaque
candidat restant a une disposition tracée et nommée, aucun ne
correspond plus à la méthode « fix borné à un import ».

Ce fil de travail spécifique (balayage des stubs d'import offline) n'a
plus de candidat borné. Les pistes restantes (confirmation runtime du
backend d'entrée r180, décision de périmètre pour construire
save/reload ou le moteur printf varargs) nécessitent soit une ressource
externe (périphérique physique), soit une décision explicite qui n'est
pas la mienne à prendre seul — voir NEXT.md « Prochaine décision » pour
le détail avant toute reprise.

Ne pas supposer qu'un import est un remplissage de structure sans lire ses
sites d'appel réels (r170 a infirmé cette hypothèse pour `VdQueryVideoFlags`),
ni qu'une valeur parmi plusieurs candidates plausibles est arbitraire sans
lire comment CE XEX la consomme (r172, r175).

## Environnement de session

Si `.tools/xenonrecomp-source`, `.tools/ghidra_12.1.2_PUBLIC` ou les
extensions Ghidra sous `ghidra-user/.ghidra/.ghidra_12.1.2_PUBLIC/Extensions/`
sont absents (sandbox réinitialisé), les restaurer avant toute analyse :
`XenonRecomp` depuis le commit épinglé dans
`recompilation/ace-combat-6-retail/config/xbox360-toolchain.lock.json`;
Ghidra 12.1.2 depuis sa release publique officielle
(`NationalSecurityAgency/ghidra`, tag `Ghidra_12.1.2_build`); les extensions
`GhidraXenon`/`XEXLoaderWV` en les copiant depuis les copies déjà trackées
dans ce dépôt (`ghidra-user/.ghidra/.ghidra_12.1.2_PUBLIC/Extensions/`) vers
`<install Ghidra>/Ghidra/Extensions/` — ne jamais installer une version non
épinglée de ces extensions. Invoquer `analyzeHeadless` avec
`JAVA_TOOL_OPTIONS="-Duser.home=$PWD/ghidra-user"` (voir
`analysis/microexec/README.md`).

## Modifier `native/` lui-même (pas seulement les stubs générés)

`build.py` compile depuis `build/<target>/native/native-source/`, une
COPIE de `native/` faite une fois par `tools/prepare.py --profile native`
(`shutil.copytree`) — elle ne se resynchronise PAS automatiquement à
chaque build. r148-r179 n'avaient touché que
`tools/materialize_native_import_stubs.py` (régénéré à chaque build par
`build.py` lui-même), jamais `native/`, d'où ce piège resté invisible
jusqu'à r180. Après toute modification de `native/` (nouveaux
fichiers, `CMakeLists.txt`, etc.), relancer d'abord :

```sh
python3 recompilation/ace-combat-6-retail/tools/prepare.py \
  --target ntsc-uj --profile native --xex <default.xex qualifié> --iso <iso qualifiée>
```

Pour le profil `native`, ceci ne fait que resynchroniser `native/` vers
`native-source/` et vérifier les hachages XEX/ISO fournis (pas de copie
d'assets retail ni de clone `AC6_recomp`) — rapide et sûr à relancer.
`game-files/default.xex` n'est PAS le XEX US retail qualifié (c'est le XEX
de la démo PAL, hachage différent) : extraire le vrai depuis l'ISO
qualifiée avec `tools/extract_xdvdfs_file.py --target ntsc-uj --path
default.xex --max-bytes 8000000` si besoin d'un `--xex` frais.

## Validation minimale

Après un changement natif (aux stubs générés OU à `native/` lui-même,
après avoir relancé `prepare.py` si nécessaire) :

```sh
python3 recompilation/ace-combat-6-retail/tools/build.py \
  --target ntsc-uj --profile native
python3 recompilation/ace-combat-6-retail/tools/validate.py \
  --target ntsc-uj --runtime native
```

Le build courant enregistre CTest 10/10. `release_ready=false` reste attendu.
Ne pas lancer d'oracle, d'A/B ou de trace globale sans ambiguïté causale
nommée. PAL reste bloqué jusqu'au gameplay Mission 01 US visible.

Le catalogue d'architecture local et `docs/native-recompilation-tools.md`
n'existent pas. Utiliser `docs/STATIC_EVIDENCE_TOOLING.md`, le README produit
et les outils déjà présents; ne pas inventer leur contenu manquant.
