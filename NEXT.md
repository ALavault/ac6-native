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
   `ac6recomp`; CTest **9/9**, pytest **130/130** (r92), audit d’installation et
   validator ordinaire passent.
2. Le renderer Vd natif consomme le WPTR primaire qualifié `object+10952`
   (index dwords), le readback exact `state+60` et les IB depuis la mémoire
   guest big-endian. La sonde r75 accepte `PM4_ME_INIT` (19 dwords) puis le lot
   IB bootstrap (12 dwords); elle expire encore après cette étape, sans retour
   guest ni gameplay visible.
3. r85 a corrigé l'identité de l'objet bloqué (`0x10001a00`, confirmé
   indépendamment quatre fois maintenant). r86 a montré que le pipeline
   PM4/Vd natif fonctionne réellement et que l'attente bloquée
   (`sub_821E61A8`, déréférence `object+0x2a90+0x0`) est un sous-allocateur
   adjacent. r87 avait proposé que `VdSetGraphicsInterruptCallback`
   (stub natif no-op, callback réel `0x821E63F0` jamais invoqué) explique
   le blocage. r88 a réfuté ce mécanisme précis : le pointeur de
   sous-callback à `object+0x2a94+0x10` est NUL par conception. **r89 a
   confirmé r87/r88 par preuve mémoire vivante indépendante** :
   `sub_821E60A8` porte son propre verrou de complétion à usage unique
   (`object+0x2abd` bit 0x2, posé en sortie après trois portes), et ce
   verrou est prouvé JAMAIS posé pour cet objet (`0x00` lu en sonde
   mono-thread à l'arrêt confirmé). **Négatif borné accepté pour ce
   sous-fil** (`sub_821E6AC8`/`0x10001a00`) : cinq cycles (r85-r89) ont
   établi ce qui est prouvé (identité, pipeline sain, chemin de déblocage
   identifié mais jamais atteint) et ce qui ne l'est pas (un éventuel
   AUTRE appelant de `sub_821E60A8` pour cet objet; le rôle de
   `object+0x540c` et de `*(0x164e0000)+0x0`, ce dernier délibérément non
   interprété faute d'avoir trouvé son écrivain). Le passage statique sur
   les 76 sites d'appel de `sub_821E60A8` (r79) reste ouvert mais hors de
   portée pour l'instant — ne pas écrire de valeur non-nulle synthétique.
   Voir
   `reports/ac6-retail-native-codegen-gate2-r89-latch-confirms-r87-bounded-negative-20260831.md`.
4. r90 a ouvert la migration scheduler/kernel plus large : diagnostic
   permanent `AC6_NATIVE_IMPORT_TRACE=1` a mesuré et corrigé un busy-spin
   `NtReleaseMutant`. r91 a continué sur les threads worker qu'il a
   révélés : `sub_821F7C80` (dispatch de callbacks, motif ordinaire) n'est
   pas la cause; `strace` a mesuré 1 547 456 `futex`/15s imputables au
   mutex partagé de la famille `create/set/clear/wait_event`.
   `wait_event()` bloque désormais réellement (condition_variable, 2 ms,
   contrat appelant inchangé) au lieu de retourner instantanément — vérifié
   en direct (GDB : vrai `pthread_cond_wait`). **r92 a réglé la question
   ouverte de r91** (négatif) : `FindDirectCallsTo.java` montre
   `NtClearEvent` appelé depuis un seul site (`sub_821F4210`), lui-même
   appelé depuis neuf sites distincts et ordinaires; `NtSetEvent` n'a
   aucun appelant direct (atteint seulement via le dispatch de callbacks
   indirect `sub_821F7C80`). Aucun spin invité trouvé — le volume mesuré
   s'explique par l'absence de régulateur de cadence dans la sonde
   offline. r92 a aussi ajouté un gestionnaire `DbgPrint` (sûr, testé,
   sans substitution varargs) mais zéro ligne produite sur une sonde de 25s.
   **r93 a réglé (négatif) que la voie threading hôte est épuisée** comme
   piste vers le jalon (sondes 60s/40s : rien de nouveau; `VdSwap` 0
   hit/30s, attendu — `poll_once()` publie avant tout `VdSwap`, déjà
   documenté dans le code). **r93 a aussi DÉMARRÉ (pas fermé) le passage
   différé sur les 76 sites d'appel de `sub_821E60A8`** (décliné en r79,
   r88, r89) : 10/76 vérifiés (cluster déjà caractérisé). Trouvé :
   `sub_821E64A8` (déjà dans la chaîne d'attente tracée) est en réalité un
   ÉCRIVAIN de paquets d'anneau — écrit 2 dwords au curseur
   (`object+0x30`), l'avance, puis attend via `sub_821E61A8`. Un filet de
   sécurité de dépassement (`object+0x30 > object+0x38`) partagé par 6 des
   10 sites vérifiés n'est PAS actuellement déclenché (curseur `0x162e017c`,
   limite `0x162eff60`, ~65 Ko d'écart, valeurs vivantes confirmées) —
   deuxième mécanisme indépendant écarté sans rouvrir r88/r89. **Prochain
   cycle : les 66 sites restants** (passage par lot, pas manuel un par un)
   **ou vérifier si la boucle appelante `sub_821E65B0` est conditionnée
   par quelque chose que ce runtime pourrait faire avancer** — piste plus
   étroite que le callback d'interruption déjà réfuté. Voir
   `reports/ac6-retail-native-codegen-gate2-r93-threading-avenue-exhausted-76-site-pass-started-20260901.md`.
   Conserver le poll limité au champ WPTR, jamais au contenu non publié du
   ring.
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
