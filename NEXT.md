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

- La famille de configuration plateforme Vd/X ouverte par r168
  (`VdQueryVideoMode`, `VdQueryVideoFlags`, `VdGetCurrentDisplayGamma`,
  `VdGetCurrentDisplayInformation`, `XGetVideoMode`, `XGetGameRegion`,
  `XGetAVPack`, `XGetLanguage`) est entièrement fermée depuis r175. Aucun
  trou n'y est actuellement nommé; le détail des corrections (offsets,
  preuves par site d'appel, valeurs choisies) est dans `STATE.md` r169-r175
  et les rapports individuels — ne pas le dupliquer ici.
- Suite pytest 158/158, `ctest` 9/9.
- La chaîne DATA.TBL est tracée et close à son niveau actuel. La traduction
  `IM_LOAD_IMMEDIATE` vers SPIR-V reste bloquée par politique de preuve.
- PAL, M02–M15, save/reload et release restent bloqués par Gate 2.

## Prochaine décision

1. Aucun trou n'est actuellement nommé dans la famille de configuration
   Vd/X (r168-r175 tous fermés). Balayer plus largement les imports
   offline restants (mêmes outils que r148-r175, ex. méthode r90/r93/r164)
   pour identifier le prochain candidat.
2. Ne pas supposer qu'un import est un remplissage de structure sans lire ses
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
- `reports/ac6-retail-native-codegen-gate2-r175-real-fix-vdgetcurrentdisplayinformation-struct-plus-0x05-traced-to-a-scaler-choice-20260902.md`;
- `STATE.md` et `EVIDENCE.md` pour l'historique (r169-r175).

Le catalogue d'architecture local manque; aucune assertion générique ne doit
en être dérivée. N2 sous `reconstruction/` reste historique et hors cible.
