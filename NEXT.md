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
3. Contradiction ouverte depuis r82 (porte de `sub_821E65B0` exige des
   champs non-nuls, r80 les a lus à zéro plus profondément dans la même
   pile) : r83 a réfuté une deuxième hypothèse (l'escalade `sub_821E6A08`
   est de la pure télémétrie, ne touche aucun champ `object`) et établi que
   `object=0x1a0010` est hors image XEX ET hors plage
   `NtAllocateVirtualMemory` — probablement un pointeur du tas propre au
   jeu, sans plus de précision.
   **Prochaine étape immédiate, non spéculative cette fois** : construire
   la variante de sonde à thread unique proposée depuis r82 (no-op
   temporaire et NON COMMITÉ du spawn `ExCreateThread`), rebuild, relire la
   porte de `sub_821E65B0` (`0x821e65c4`) au point exact du check sans
   contention multi-thread, puis révoquer le changement temporaire. Arrêter
   de proposer de nouvelles hypothèses statiques une à une (rendements
   décroissants, cf. r79). Ne pas écrire de valeur non-nulle synthétique
   avant résolution. Voir
   `reports/ac6-retail-native-codegen-gate2-r83-escalation-is-telemetry-20260831.md`,
   et en arrière-plan r77/r78/r79/r80/r81/r82 pour la chaîne complète.
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
