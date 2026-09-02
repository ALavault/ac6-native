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
- Suite pytest 176/176, `ctest` 10/10.
- La chaîne DATA.TBL est tracée et close à son niveau actuel. La traduction
  `IM_LOAD_IMMEDIATE` vers SPIR-V reste bloquée par politique de preuve.
- PAL, M02–M15, save/reload et release restent bloqués par Gate 2.

## Prochaine décision

1. Confirmer par une observation runtime (avec un vrai périphérique quand
   disponible) que le backend d'entrée r180 résout effectivement le
   symptôme historique « contrôles nuls ».
2. `NtQueryVolumeInformationFile` (3 sites d'appel réels) : candidat
   plausible maintenant que `file_size()` existe.
3. Continuer le balayage des imports offline restants (mêmes outils que
   r148-r189, méthode r90/r93/r164) pour d'autres candidats.
4. Ne pas supposer qu'un import est un remplissage de structure sans lire ses
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
- `reports/ac6-retail-native-codegen-gate2-r189-real-fix-ntqueryfullattributesfile-fills-the-real-struct-20260902.md`;
- `reports/ac6-retail-native-codegen-gate2-r188-real-fixes-rtlunicodestringtoansistring-rtlfreeansistring-20260902.md`;
- `STATE.md` et `EVIDENCE.md` pour l'historique (r169-r189).

Le catalogue d'architecture local manque; aucune assertion générique ne doit
en être dérivée. N2 sous `reconstruction/` reste historique et hors cible.
