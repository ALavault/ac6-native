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

- r168 identifie un trou réel : `VdQueryVideoMode` doit remplir une structure
  pointée. Deux appelants réels lisent au moins les offsets `+0x00`, `+0x04`,
  `+0x08` et `+0x14`.
- r169 corrige ce trou : offsets `+0x00`/`+0x04`/`+0x08`/`+0x14` dérivés des
  deux sites d'appel réels de ce XEX (aucun oracle, aucune structure copiée
  depuis une réimplémentation indépendante). CTest 9/9, suite pytest
  151/151 (150/150 + 1 test ciblé), trace d'import live confirmant la sortie
  du fallback offline générique. `+0x0C`/`+0x10` restent non lus par aucun
  site connu et donc non implémentés.
- La chaîne DATA.TBL est tracée et close à son niveau actuel. La traduction
  `IM_LOAD_IMMEDIATE` vers SPIR-V reste bloquée par politique de preuve.
- PAL, M02–M15, save/reload et release restent bloqués par Gate 2.

## Prochaine décision

1. Pour `VdQueryVideoFlags`, `VdGetCurrentDisplayGamma` et
   `VdGetCurrentDisplayInformation` (même famille nommée par r168/r169),
   dans le projet Ghidra US qualifié : fermer les xrefs directs et indirects,
   déterminer si chacun est un remplissage de structure ou une simple forme
   de contrat, puis dériver le layout exact uniquement depuis les lectures
   réelles des appelants de ce XEX.
2. Si un import est bien un remplissage de structure et que les offsets/types
   deviennent univoques, l'implémenter depuis les valeurs déjà suivies par la
   couche Vd native (`native/src/native_guest_vd.cpp`); ajouter un test ciblé
   par import.
3. Si un import s'avère être une simple forme de contrat, ou si aucun site
   d'appel réel n'est trouvé, documenter cela explicitement plutôt que de
   forcer une implémentation.

`done_when` : layout binaire qualifié, implémentation sans valeur devinée,
tests ciblés verts, CTest 9/9 et validation native fraîche. Si le layout reste
ambigu après valorisation statique, nommer précisément l'ambiguïté avant toute
observation runtime.

## Preuves courantes

- `reports/handoff/CURRENT.json`;
- `reports/ac6-retail-native-codegen-gate2-r169-real-fix-vdqueryvideomode-struct-fill-derived-from-this-xexs-own-disassembly-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r168-vdqueryvideomode-is-a-real-gap-struct-fill-not-a-contract-shape-fix-deferred-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r167-real-fix-netdll-xnetstartup-wsastartup-reported-failure-on-an-offline-boundary-20260901.md`;
- `STATE.md` et `EVIDENCE.md` pour l'historique.

Le catalogue d'architecture local manque; aucune assertion générique ne doit
en être dérivée. N2 sous `reconstruction/` reste historique et hors cible.
