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
3. r85 a corrigé l'identité de l'objet bloqué (`0x10001a00`, confirmé
   indépendamment trois fois maintenant). r86 a montré que le pipeline
   PM4/Vd natif fonctionne réellement et que l'attente bloquée
   (`sub_821E61A8`, déréférence `object+0x2a90+0x0`) est un sous-allocateur
   adjacent. r87 avait proposé que `VdSetGraphicsInterruptCallback`
   (stub natif no-op, callback réel `0x821E63F0` jamais invoqué) explique
   le blocage. **r88 a réfuté ce mécanisme précis** : le pointeur de
   sous-callback à `object+0x2a94+0x10` est NUL par conception (bloc
   fraîchement `memset`é, jamais réécrit ailleurs dans l'image) — même en
   corrigeant le stub, le gestionnaire sauterait intentionnellement l'appel
   du sous-callback et n'atteindrait jamais `sub_821E60A8`/`sub_821E5D60`.
   **Après quatre cycles à resserrer puis fermer des mécanismes
   spécifiques sans réponse finale** : la prochaine étape doit reconsidérer
   la portée plutôt que proposer une cinquième hypothèse ponctuelle — soit
   documenter un négatif borné pour ce sous-fil (`sub_821E6AC8` et
   l'attente qui s'y bloque) et revenir à la migration scheduler/kernel
   plus large déjà listée dans ce fichier, soit s'engager dans un passage
   statique substantiellement plus coûteux (les 76 sites d'appel de
   `sub_821E60A8`, r79) seulement si jugé utile. Ne pas écrire de valeur
   non-nulle synthétique. Voir
   `reports/ac6-retail-native-codegen-gate2-r88-nested-callback-is-null-by-design-20260831.md`.
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
