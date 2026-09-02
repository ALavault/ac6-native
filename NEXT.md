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
- r179 a corrigé `KeQuerySystemTime` (remplissage de FILETIME via pointeur,
  utilise l'horloge murale réelle de l'hôte) — les 4 sites d'appel réels
  exigeaient tous une valeur changeante (date calendaire, delta de temps,
  graine de session).
- **Bloqué sur une décision utilisateur** : le backend d'entrée manette
  natif (candidat le plus prometteur pour « contrôles nuls ») nécessite un
  choix de dépendance et un nouveau sous-système — ne pas commencer sans
  confirmation explicite.
- Suite pytest 161/161, `ctest` 9/9.
- La chaîne DATA.TBL est tracée et close à son niveau actuel. La traduction
  `IM_LOAD_IMMEDIATE` vers SPIR-V reste bloquée par politique de preuve.
- PAL, M02–M15, save/reload et release restent bloqués par Gate 2.

## Prochaine décision

1. **Bloqué sur une décision utilisateur** : backend d'entrée manette natif
   — go/no-go et choix de dépendance hôte (ex. SDL2) avant toute
   implémentation. Ne pas commencer sans confirmation explicite.
2. En l'absence de cette décision, continuer le balayage des imports
   offline restants (mêmes outils que r148-r179, méthode r90/r93/r164)
   pour des candidats ne nécessitant pas de nouvelle infrastructure.
3. Ne pas supposer qu'un import est un remplissage de structure sans lire ses
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
- `reports/ac6-retail-native-codegen-gate2-r179-real-fix-kequerysystemtime-fills-a-real-changing-filetime-20260902.md`;
- `reports/ac6-retail-native-codegen-gate2-r178-xexcheckexecutableprivilege-checked-no-safe-fix-identified-input-backend-needs-scoping-20260902.md`;
- `STATE.md` et `EVIDENCE.md` pour l'historique (r169-r179).

Le catalogue d'architecture local manque; aucune assertion générique ne doit
en être dérivée. N2 sous `reconstruction/` reste historique et hors cible.
