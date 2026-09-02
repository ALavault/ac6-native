# AC6 retail NTSC-U/J — reprise Gate 2 runtime natif

Reprendre depuis `recompilation/ace-combat-6-retail`.

Lire d'abord :

- `reports/handoff/CURRENT.json`;
- le report r204 cité comme `source_report`;
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
(retour ignoré par tous les appelants réels).

Continuer le balayage des imports offline restants (mêmes outils que
r148-r204, méthode r90/r93/r164) pour d'autres candidats.

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
