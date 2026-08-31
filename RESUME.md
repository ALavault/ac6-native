# AC6 retail NTSC-U/J — reprise Gate 2 runtime natif

Reprendre depuis `recompilation/ace-combat-6-retail`.

Lire d'abord:

- `reports/handoff/CURRENT.json`;
- `reports/ac6-retail-native-codegen-gate2-r93-threading-avenue-exhausted-76-site-pass-started-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r92-event-callers-settled-dbgprint-added-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r91-wait-event-blocks-aggregate-impact-open-20260831.md`;
- `reports/ac6-retail-native-codegen-gate2-r90-mutant-release-busy-spin-20260831.md`;
- `reports/ac6-retail-native-codegen-gate2-r89-latch-confirms-r87-bounded-negative-20260831.md`;
- `reports/ac6-retail-native-codegen-gate2-r88-nested-callback-is-null-by-design-20260831.md`;
- `reports/ac6-retail-native-codegen-gate2-r86-vd-pipeline-works-generic-wait-is-separate-20260831.md`;
- `reports/ac6-retail-native-codegen-gate2-r85-wrong-object-corrected-20260831.md`
  (rétracte l'identité d'objet `0x1a0010` utilisée par r77-r84 — leurs
  récits sur cet objet sont superseded, seuls leurs faits de flot de
  contrôle des fonctions restent valables);
- `reports/ac6-retail-native-codegen-gate2-r11-20260831.md`;
- `recompilation/ace-combat-6-retail/build/ntsc-uj/native/codegen-20260831-patched-final-r11/codegen-receipt.json`;
- `recompilation/ace-combat-6-retail/build/ntsc-uj/native/build-receipt.json`;
- `recompilation/ace-combat-6-retail/config/xbox360-toolchain.lock.json`;
- `recompilation/ace-combat-6-retail/native/README.md`;
- `recompilation/ace-combat-6-retail/targets/ntsc-uj.json`;
- `recompilation/ace-combat-6-retail/routes/mission01-qualified-96.steps`;
- `docs/native-recompilation-tools.md` (racine portfolio).

Identités déjà scellées: XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`, ISO US
`204c5e645d79da8776699c12f17bd069f869fbdb10ae79015d1a0ef2b743c98c`, Ghidra
`ghidra-projects/ac6-us`, AC6_recomp
`09144bb092ad871584808aeead69c395edbd5200`, route SHA-256
`771a77a8ff50eda30c5fb24309d8828bb339f91a49471368f117b65c9dbb6043`.

Sous-gate codegen/liaison fermé: receipt r11 `pass`, 81 fichiers générés,
62 629 029 octets, zéro diagnostic et aucune instruction non reconnue. Le
guest se lie à `ppc_func_mapping.cpp` et 229 imports offline; `ac6recomp`
peuple 19 832 mappings. Le profil natif passe CTest 9/9, pytest 130/130 (r92),
l'audit d'installation et `validate.py --target ntsc-uj --runtime native`.

Gate actif: runtime natif encore ouvert. La sonde r75 atteint le renderer Vd
sans ReXGlue installé. Elle établit l'objet `0x10001a00`, son WPTR primaire
`+10952`, le readback exact `state+60`, et la mémoire IB big-endian; le renderer
accepte `PM4_ME_INIT` (19 dwords) puis le lot IB bootstrap (12 dwords). Le guest
reste ensuite sans retour et aucun gameplay visible n'est qualifié.

CORRECTION r85 : l'objet réel est `0x10001a00` (confirmé 4x
indépendamment, dernière fois r89). r86 : le pipeline PM4/Vd natif
fonctionne; l'attente bloquée est un sous-allocateur adjacent. r87 avait
proposé le stub no-op `VdSetGraphicsInterruptCallback` comme cause; r88
a réfuté ce mécanisme précis. **r89 a confirmé r87/r88 par preuve mémoire
vivante** — `sub_821E60A8` porte son propre verrou de complétion à usage
unique (`object+0x2abd` bit 0x2), et ce verrou est prouvé jamais posé pour
cet objet. **Négatif borné accepté pour ce sous-fil** (cinq cycles,
r85-r89); le prochain cycle reprend la liste scheduler/kernel/VFS/XAM plus
large de NEXT.md plutôt qu'une sixième hypothèse ponctuelle. Le passage
statique sur les 76 sites d'appel de `sub_821E60A8` reste ouvert mais hors
de portée pour l'instant. **r90** a ouvert la migration scheduler/kernel plus
large (busy-spin `NtReleaseMutant` mesuré et corrigé). **r91** a mesuré
1 547 456 `futex`/15s sur le mutex de la famille `create/set/clear/wait_event`
et rendu `wait_event()` réellement bloquant (condition_variable, vérifié en
direct), sans établir l'impact agrégat. **r92** a réglé la question ouverte
de r91 (négatif, statique) : `NtClearEvent` a un seul appelant direct
(`sub_821F4210`, lui-même appelé depuis neuf sites ordinaires distincts);
`NtSetEvent` n'a aucun appelant direct (dispatch de callbacks indirect
uniquement). Aucun spin invité trouvé — le volume s'explique par l'absence
de régulateur de cadence dans la sonde offline. r92 a aussi ajouté un
gestionnaire `DbgPrint` sûr (sans substitution varargs) mais zéro ligne
produite sur 25s — négatif honnête, gardé pour une sonde future plus longue.
**r93** a réglé (négatif) que la voie threading hôte est épuisée comme piste
vers le jalon (sondes 60s/40s : rien de nouveau; `VdSwap` 0 hit/30s, attendu).
r93 a aussi DÉMARRÉ (pas fermé) le passage différé sur les 76 sites d'appel
de `sub_821E60A8` : 10/76 vérifiés. Trouvé : `sub_821E64A8` est en réalité un
écrivain de paquets d'anneau (écrit 2 dwords au curseur, l'avance, puis
attend). Un filet de sécurité de dépassement (6/10 sites vérifiés) n'est pas
actuellement déclenché (curseur ~65 Ko sous la limite, valeurs vivantes
confirmées) — mécanisme écarté sans rouvrir r88/r89. Prochain : les 66 sites
restants (par lot) ou vérifier si `sub_821E65B0` est conditionné par quelque
chose que ce runtime pourrait faire avancer. Les stubs restent build-only;
`IM_LOAD_IMMEDIATE` est borné mais sa traduction Xenos→SPIR-V n'est pas
fermée. Ne pas revendiquer titre, M01, campagne, save/replay ou release.

Le patch XenonRecomp utilisé pour r11 reste dans une copie de build ignorée;
le checkout verrouillé et les sources générées ne doivent pas entrer dans le
produit installé. ReXGlue reste oracle read-only seulement. N2 de
`reconstruction/ace-combat-6` reste historique, hors cible active.
La primitive `GuestAddressSpace` réserve l'espace virtuel Xenon 32-bit complet
avec bornes testées et est exigée par `NativeRuntime::boot()`; bindings imports
et appel guest restent ouverts.
