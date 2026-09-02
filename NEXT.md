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

- r173 a corrigé `XGetAVPack` : renvoie `0u`, hors de l'ensemble
  `{0x3,0x6,0x8,0x4}` que le seul site d'appel réel traite spécialement
  (saut de configuration); aucune preuve ne distingue les autres valeurs.
- r172 a corrigé `XGetGameRegion` : renvoie `0x101` désormais — corroboré
  par 2 des 3 sites d'appel réels comme code privilégié à correspondance
  exacte (pas un choix arbitraire de convention de nommage).
- r171 a corrigé `XGetVideoMode` : struct+0x14 (refresh-rate float,
  offset identique à r169) était lu comme DIVISEUR réel
  (`fdivs f1,f31,f0`) sans être rempli — corrigé à 60.0f. Seul cet offset
  est implémenté (aucune preuve pour les autres à ce site).
- r169 a corrigé `VdQueryVideoMode` (remplissage de struct, `+0x00`/`+0x04`/
  `+0x08`/`+0x14`). r170 a fixé les trois imports Vd voisins nommés
  par r168/r169 :
  - `VdQueryVideoFlags` n'était PAS un remplissage de struct (hypothèse de
    r168/r169 infirmée) — simple valeur de retour bitmask, corrigée (`0u`,
    plus `kOfflineStatus` dont le bit 0 forçait une branche par coïncidence).
  - `VdGetCurrentDisplayGamma` : 2 sorties par pointeur remplies (`type=0`,
    `gamma=2.2`), aucun risque de crash identifié (cache aval non
    initialisé au premier appel).
  - `VdGetCurrentDisplayInformation` : remplissage de struct confirmé aux 3
    sites d'appel réels, `+0x48`/`+0x4a`/`+0x56` implémentés (validation
    croisée directe avec r169). `+0x05` (champ booléen réel, confirmé à 2
    sites) reste non implémenté — sa cible de comparaison n'est pas encore
    tracée.
  - Suite pytest 154/154, `ctest` 9/9.
- Aucun autre import Vd n'est actuellement nommé comme trou non vérifié;
  identifier le prochain nécessite un nouveau balayage des offline-imports.
- La chaîne DATA.TBL est tracée et close à son niveau actuel. La traduction
  `IM_LOAD_IMMEDIATE` vers SPIR-V reste bloquée par politique de preuve.
- PAL, M02–M15, save/reload et release restent bloqués par Gate 2.

## Prochaine décision

1. `XGetLanguage` (`0x821f5d9c`, 1 site d'appel réel) reste le dernier
   import non vérifié de cette famille — son résultat est relu et
   borné-vérifié (contrairement à `XGetAVPack`), donc la valeur exacte
   compte : dériver et implémenter dans un cycle dédié.
2. `VdGetCurrentDisplayInformation` struct+0x05 : tracer la logique aval qui
   consomme ce champ pour déterminer sa vraie valeur, ou documenter qu'aucune
   preuve statique supplémentaire n'est atteignable.
3. Balayer plus largement les imports offline restants (mêmes outils que
   r148-r172) pour identifier le prochain trou une fois ces deux fermés.
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
- `reports/ac6-retail-native-codegen-gate2-r173-real-fix-xgetavpack-avoids-the-four-skip-setup-values-20260902.md`;
- `reports/ac6-retail-native-codegen-gate2-r172-real-fix-xgetgameregion-returns-the-privileged-exact-match-region-code-20260902.md`;
- `reports/ac6-retail-native-codegen-gate2-r171-real-fix-xgetvideomode-fills-the-refresh-rate-field-used-as-a-division-divisor-20260902.md`;
- `STATE.md` et `EVIDENCE.md` pour l'historique.

Le catalogue d'architecture local manque; aucune assertion générique ne doit
en être dérivée. N2 sous `reconstruction/` reste historique et hors cible.
