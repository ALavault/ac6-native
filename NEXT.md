# AC6 retail NTSC-U/J — Gate 2 runtime natif

## Résultat requis

Brancher le renderer natif derrière les imports Vd du runtime CPU Xenon généré
depuis le XEX US qualifié. Atteindre boot, titre, menus, cinématique puis début
visible de gameplay M01, sans substitution de frontbuffer, état synthétique,
compteur injecté ou fallback ReXGlue.

## Identité scellée

- cible: retail NTSC-U/J (`default.xex`), XEX
  `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`;
- ISO: `204c5e645d79da8776699c12f17bd069f869fbdb10ae79015d1a0ef2b743c98c`;
- Ghidra: `ghidra-projects/ac6-us/default.xex`,
  `PowerPC:BE:64:Xenon`;
- AC6_recomp: `09144bb092ad871584808aeead69c395edbd5200`;
- route: `recompilation/ace-combat-6-retail/routes/mission01-qualified-96.steps`,
  SHA-256 `771a77a8ff50eda30c5fb24309d8828bb339f91a49471368f117b65c9dbb6043`;
- renderer oracle: ReXGlue, uniquement hors produit et hors installation.

## Travail courant

1. Codegen direct r11 : receipt `pass`, 81 fichiers, zéro diagnostic, 229
   imports et 19 832 mappings. Le profil `native` compile et installe
   `ac6recomp`; CTest **9/9**, pytest **126/126**, audit d’installation et
   validator ordinaire passent.
2. Le renderer Vd natif consomme le WPTR primaire qualifié `object+10952`
   (index dwords), le readback exact `state+60` et les IB depuis la mémoire
   guest big-endian. La sonde r75 accepte `PM4_ME_INIT` (19 dwords) puis le lot
   IB bootstrap (12 dwords); elle expire encore après cette étape, sans retour
   guest ni gameplay visible.
3. **CORRECTION r85** : tout le fil r77-r84 ("objet bloqué = 0x1a0010")
   tracait un objet dérivé par une technique GDB non fiable
   (`frame 1` + `$rbp` pendant un arrêt dans une frame plus profonde, sans
   info de debug). Une méthode fiable (breakpoint à l'entrée brute de
   `sub_821E64A8`, lecture directe de `ctx.r3`) donne
   **`object = 0x10001a00`** — l'objet Vd/PM4 déjà établi bien avant ce fil
   (`reports/ac6-retail-native-codegen-gate2-r11-20260831.md`, tranche
   r53 : "l'objet à 0x10001a00, le ring 0x162d0000 et le readback"). Avec
   cet objet correct, `+0x2a90` déréférence exactement le readback connu
   `0x164e0000`, `+0x30` est un curseur proche de l'anneau connu
   `0x162d0000`, `+0x2a9c=7` — les trois STABLES sur 200 itérations. Les
   récits "objet null"/"contradiction" de r77-r84 sont rétractés; les
   faits de flot de contrôle des fonctions restent valables.
   **Prochaine étape immédiate** : ceci reconnecte à l'item déjà ouvert r53
   ("le consommateur PM4/Vd natif n'est pas encore relié") — vérifier ce
   lien contre `native/src/native_guest_vd.cpp` (déterminer précisément ce
   qui devrait faire avancer le readback `0x164e0000` au-delà du curseur
   `0x162e017c`) avant d'ouvrir une nouvelle piste spéculative. Ne pas
   écrire de valeur non-nulle synthétique avant cette vérification. Voir
   `reports/ac6-retail-native-codegen-gate2-r85-wrong-object-corrected-20260831.md`.
4. Une fois ce décrément fermé, reprendre la migration plus large du
   scheduler/kernel, événements et VFS/XAM par familles ABI avec une sonde
   bornée et des tests ciblés; conserver le poll limité au champ WPTR, jamais
   au contenu non publié du ring.
5. La traduction `IM_LOAD_IMMEDIATE` Xenos→SPIR-V reste ouverte. Aucun rendu
   présentable, titre, M01, campagne, save/replay ou mode offline n’est promu.
   Ne pas optimiser avant le début visible de gameplay.

## Frontières

Le N2 `reconstruction/ace-combat-6` est abandonné pour cette feuille de route;
sa preuve reste historique et aucune de ses sources n'est fusionnée. Ne pas
qualifier PAL, M02–M15, save/reload ou release avant fermeture Gate 2. Le
catalogue d'architecture local manque; aucune assertion générique n'en est
dérivée. ReXGlue reste oracle hors installation et le checkout XenonRecomp
reste ignoré/verrouillé.
