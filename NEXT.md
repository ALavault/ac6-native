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
3. **FERMÉ (r94) : le blocage `sub_821E6AC8`/`0x10001a00` chassé depuis
   r51 est réellement résolu**, pas contourné. r85-r93 avaient établi
   l'identité de l'objet, la santé du pipeline PM4/Vd, réfuté deux
   mécanismes de déblocage candidats (callback d'interruption r87/r88,
   filet de dépassement r93) et accepté un négatif borné. **r94 a trouvé
   et corrigé la cause réelle** : `drain_locked()` faisait passer les
   écritures `EVENT_WRITE_SHD` par `gpu_swap()` PUIS `store_guest_word()`
   — un double échange d'octets qui composait au lieu d'annuler une seule
   transformation voulue, corrompant chaque valeur de fence livrée à
   l'adresse `0x164e0000` déjà tracée depuis r86 (vérifié algébriquement
   contre les octets vivants observés : `05 00 00 00` — jamais expliqué
   en r89/r91/r93 — devient `00 00 00 05`=5 avec le correctif). Corrigé
   via une nouvelle `store_guest_bytes_raw()` (memcpy brut, pas de second
   bswap). **Vérifié en direct par reconstruction A/B isolée** (fichier
   unique stashé/restauré) : sans le correctif, comportement r93 identique;
   avec, le thread principal REVIENT de l'ancienne chaîne d'attente et
   bloque maintenant via une chaîne entièrement nouvelle
   (`sub_821F03B0←sub_8234F558←sub_8233E0A8←sub_8233B5A0`, jamais vue
   avant ce cycle) — progression réelle confirmée, pas une hypothèse.
   Voir
   `reports/ac6-retail-native-codegen-gate2-r94-double-endian-swap-fixed-main-thread-unblocked-20260901.md`.
4. **r95 a corrigé le formatage hex du décodeur et nommé précisément (pas
   corrigé) la plage de registres.** L'alerte r94 sur une adresse IB
   >32 bits était un artefact de formatage; adresse réelle `0x125c0000`
   (valide), header réel `0x00054800` → TYPE0 base=`0x4800`, count=6.
   **r96 a scanné le tampon indirect complet** (2840 dwords, dump GDB +
   parseur Python utilisant la logique de décodage exacte du projet, flux
   propre : 371 TYPE0 + 281 TYPE3, aucune désynchronisation) : registre
   maximum réellement touché `0x5002`. **Corrigé** :
   `XenosState::kRegisterCount` `0x4000`→`0x8000` — borne dérivée du
   FORMAT (`low_register_of` masque 15 bits, `0x8000` est la plage
   complète adressable par ce champ, pas une constante matérielle
   devinée), couvre le besoin observé avec marge. **Vérifié en direct** :
   rejet de plage de registres disparu; publication d'anneau avance de
   31 à 37 dwords (record de progression) avant un NOUVEAU rejet distinct,
   délibéré : `predicated TYPE3 packets are not supported` — sémantique
   d'exécution prédiquée non implémentée, travail de fonctionnalité réel,
   pas un bug. **Prochain cycle : implémenter l'exécution prédiquée des
   paquets TYPE3** (lire le registre/drapeau de prédicat que le décodeur
   suit déjà, déterminer la sémantique saut-vs-exécution correcte). Voir
   `reports/ac6-retail-native-codegen-gate2-r96-register-count-widened-predicate-gap-named-20260901.md`.
   L'identité du nouvel objet bloqué `sub_821E6AC8`/`sub_821F03B0` (r94)
   reste aussi non lue. Le passage sur les 76 sites d'appel de
   `sub_821E60A8` (r79, r93) et la piste `sub_821E65B0` sont hors de
   propos — ils caractérisaient l'ANCIEN blocage, résolu en r94. Ne pas y
   revenir sans raison nouvelle. r90-r92 restent valables (busy-spin
   `NtReleaseMutant` corrigé, `wait_event` bloquant, appelants événements
   réglés, `DbgPrint` disponible) — voir leurs rapports pour
   l'infrastructure de sonde toujours en place (`AC6_NATIVE_IMPORT_TRACE`,
   `AC6_NATIVE_VD_TRACE`). Conserver le poll limité au champ WPTR, jamais
   au contenu non publié du ring; ne jamais écrire de valeur synthétique
   pour faire avancer le
   décodeur.
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
