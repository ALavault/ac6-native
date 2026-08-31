# AC6 retail NTSC-U/J — codegen direct et liaison guest Gate 2

Date: 2026-08-31.

Le checkout XenonRecomp qualifié (`ddd128bcca99fe8bfbb99bea583c972351fa6ace`)
a été compilé séparément en Clang 21. Une copie de build ignorée contient les
correctifs minimaux d'émission pour `dcbst`, `lhbrx`, `mulhdu`, `frsqrte`,
`vandc`, `vpkswss` et `vsel128`, ainsi que l'extension bornée des propriétaires
de tables de saut. La configuration US contient les 70 intervalles oracle
non chevauchants et les cibles `82254248/8225424C`.

Receipt `build/ntsc-uj/native/codegen-20260831-patched-final-r11`:

- XenonAnalyse: pass;
- XenonRecomp: pass;
- 81 fichiers générés, 62 629 029 octets, diagnostics: **0**;
- `unrecognized_instructions=[]`, erreurs switch: 0.

Le codegen mapfix `codegen-20260831-mapfix-96838` ajoute la frontière
qualifiée `0x82096838..0x82096858` requise par un callback indirect; receipt
pass, 81 fichiers, 62 629 837 octets, zéro diagnostic.

Le guest généré avec `XenonUtils/ppc_context.h` compile en 79 objets et se lie
avec `ppc_func_mapping.cpp` et 229 définitions d'imports offline dans une
archive statique. Le build natif produit ensuite `ac6_native_guest_link_test`;
CTest est **9/9** (incluant extraction XDVDFS, parsing XEX et espace d'adresses guest) et
`tools/validate.py --runtime native` passe.
Le probe force une référence `noinline` au guest, vérifie le premier mapping et
parcourt son sentinelle; `nm` expose `sub_82090000`, `PPCFuncMappings` et les
imports C++ HLE. Il ne lance volontairement aucune fonction guest.

Cette preuve ferme le sous-gate codegen/liaison seulement. Les définitions
d'imports restent des bornes HLE offline génériques; elles ne constituent pas
encore le runtime SDK complet, le renderer présentable ni un boot retail M01.

## Bootstrap CLI r12

Le profil natif expose maintenant `ac6recomp <ISO|assets/>` et
`ac6recomp --self-test`. Les builds `artifacts/retail-us-native-build-gate2-r12-cli`
et `artifacts/retail-us-native-build-gate2-r13-linkprobe` compilent et lient
l'exécutable; CTest est **9/9**. Le self-test est exécuté par
`validate.py --runtime native`; une installation CMake temporaire place le
binaire sous `bin/ac6recomp` sans `bin/bin`.
L'audit `artifacts/retail-us-native-install-probe-r13/audit.json` passe avec
un seul fichier installé et zéro contenu ReXGlue/Xenia/XenonRecomp/XenosRecomp.

Cette tranche ne lance pas encore l'entry point retail: elle valide le parseur
média, le cycle boot/shutdown natif et la surface CLI; le smoke leaf généré
reste neutre. Aucun claim de boot retail, gameplay, audio, campagne ou release
n'est ajouté.
Le probe CLI sur l'ISO US qualifiée sort avec 0 et crée uniquement le répertoire
XDG mutable; aucun octet ISO n'est copié dans le produit.

## Espace d'adresses guest

`GuestAddressSpace` réserve 4 GiB virtuels avec `MAP_NORESERVE`, vérifie les
bornes 32 bits et expose une vue bornée pour les pointeurs XenonRecomp. Le test
écrit à `0x82000000` et rejette un dépassement; la classe est désormais
branchée au boot et reçoit l'image XEX décodée, sans appel de code retail.
Le rebuild r17 ajoute un test de taille `size_t` maximale pour empêcher tout
wrap de borne; le build r18 installe automatiquement son préfixe natif ignoré
et lance l'audit fail-closed. Le rebuild r19 branche cette réservation au
cycle `NativeRuntime::boot()`. CTest et la validation native restent verts.
Le binaire r19 démarre aussi avec le répertoire d'assets US qualifié et sort
avec 0; cela reste un boot de cycle de vie, pas une exécution du jeu.
Le rebuild r20 vérifie en plus l'alignement 32-byte requis par
`PPC_FUNC_PROLOGUE`; CTest 9/9 et l'audit d'installation restent verts.
La commande `validate.py --runtime native --require-release` reste refusée avec
`native full release gate is still open`, preuve que campagne/save/runtime ne
sont pas promus par erreur, y compris avec `SDL_AUDIODRIVER=dummy`.

## XDVDFS ISO

Le build r21 ajoute un lecteur XDVDFS natif borné: descriptor, arbre de
répertoires, chemins case-insensitive, cycles, tailles et offsets sont validés
avant lecture. `NativeRuntime::boot()` extrait `default.xex` en mémoire puis
réutilise le parseur XEX2. Le probe CLI sur l'ISO US qualifiée sort avec 0; le
payload n'est jamais copié dans le produit ni écrit sur disque.

## Image XEX mappée

Le build r27 ajoute le décodage XEX2 autonome (OpenSSL `libcrypto` host,
sans SDK Xbox): clé de session AES-CBC retail,
compression basic (blocs données/zéros), validation PE Xenon et bornes de taille.
Les probes CLI sur assets et ISO US affichent
`mapped XEX image 0xa98000 bytes at 0x82000000` et sortent avec 0. Le rebuild r32 confirme cette chaîne avec CTest
9/9. L'image reste en mémoire guest virtuelle; compression normal/LZX et
exécution du code généré restent ouverts.
Le rebuild r35 lie `ac6recomp` au guest r11, résout l'entry `0x821f5ed0` vers
`_xstart`, exécute seulement le leaf ABI neutre `sub_8209C0B4`, et sort avec 0
sur assets et ISO.
Le rebuild r36 peuple ensuite la table `PPC_LOOKUP_FUNC` avec 19 831 pointeurs
host dans l'espace guest; l'entrypoint reste volontairement non appelé tant que
les bindings SDK ne sont pas fermés.
Avec la frontière callback `0x82096838` du mapfix, le binaire final r45
peuple 19 832 mappings; la valeur 19 831 ci-dessus correspond au build r36
antérieur.

## Probe d'entrée borné

Une trace unique opt-in (`AC6_NATIVE_ALLOW_ENTRY_PROBE=1`, `SDL_AUDIODRIVER=dummy`,
12 s, cgroup 12 GiB, core désactivé) a d'abord révélé un callback TLS nul,
puis une faute FPU due au FPSCR non initialisé. Après corrections TLS,
fréquence, handle thread, événements et timeout offline, la trace atteint
`sub_821F9E10` puis timeoute sans crash. Ce résultat ferme une ambiguïté de
dispatch mais ne constitue pas une qualification gameplay; le chemin normal ne
lance jamais ce probe.
Une fenêtre étendue unique de 60 s (`r46`, cgroup 12 GiB) termine également
avec `timeout`/124 sans retour. La prochaine tranche doit donc fermer scheduler,
état initial et imports SDK; aucune répétition de ce probe n'est justifiée avant
ces bindings.

## Bindings provisoires observés

Le générateur de frontières build-only fournit désormais des contrats minimaux
pour TLS thread-local, fréquence Xenon 50 MHz, création de handles
`ExCreateThread`/objets kernel et attente `STATUS_TIMEOUT`. Ils servent
uniquement à franchir des prérequis de trace; ils ne remplacent pas
scheduler/kernel et restent exclus de toute qualification release tant que les
229 ABI SDK ne sont pas migrées.

## Tranches runtime r47–r51 (2026-08-31)

- r47 : `NtAllocateVirtualMemory` alloue dans la réservation guest complète,
  arrondit à 64 KiB, zero-remplit et échoue fermé hors plage; pool/chaînes BE
  sont ensuite couverts par r48. Tests ciblés du générateur : **9/9**, build
  natif/CTest : **9/9**.
- r49 : le probe entry initialise un PCR/thread guest déterministe (`r13`,
  pointeur thread, CPU 0, bornes de pile), sans modifier le XEX; pytest total
  **122/122**, CTest **9/9**.
- r50 : les imports Vd de retrain/HSIO et cycle d’initialisation renvoient un
  succès immédiat borné, ce qui dépasse l’attente EDRAM sans fallback renderer.
- r51 : `ExCreateThread` valide le shim et la routine dans le mapping généré,
  clone le contexte Xenon (r13 conservé), assigne une pile guest déterministe
  et lance le callback dans un worker host détaché; GDB observe 16 workers.
  Le thread principal reste dans `sub_821E6AC8 → sub_821E61A8 →
  sub_821E64A8`, attente queue/ring non encore signalée par un backend Vd réel.

Les artefacts bornés sont dans `artifacts/retail-us-native-build-gate2-r47-vm-bootstrap`,
`r48-pool-strings`, `r49-pcr`, `r50-vd` et `r51-thread`. Ces sondes restent
diagnostiques; elles ne qualifient ni boot visible, ni gameplay M01, ni audio.

r52 synchronise explicitement `native/` vers la copie de build ignorée
`native-source`; GDB vérifie les champs PCR au vrai `_xstart` (`r1=0x8ff00000`,
`r13=0x0f000000`). r53 implémente `MmAllocatePhysicalMemoryEx` dans la plage
guest; l'inspection Vd observe le bloc de readback `0x164e0000` et son adresse
`0x164e003c`, avec readback 0 et write pointer 5. Le prochain travail doit
faire consommer le ring par `VdBridge`/PM4 et publier le pointeur matériel réel;
écrire directement 5 serait un état synthétique interdit.

Après r53 : `tools/validate.py --target ntsc-uj --runtime native` passe,
installation native auditée (1 fichier, aucun forbidden, pas de `bin/bin`) et
les tests Python retail passent **125/125**. `--require-release` reste refusé
car campagne, save et runtime naturel ne sont pas qualifiés.

## Vd primaire et IB natifs r56–r75 (2026-08-31)

La lecture statique qualifie `object+10952` comme WPTR du ring primaire;
`object+10908` reste le curseur du flux secondaire. Le service natif publie
les indices dwords via `MmioBus`, conserve le readback exact `state+60` et
résout les indirect buffers dans l'espace guest big-endian. Une sonde bornée a
confirmé l'objet à `0x10001a00`, le ring `0x162d0000` et le readback
`0x164e003c`.

Le décodage natif accepte maintenant le bootstrap observé :
`PM4_ME_INIT` (`0x48`, 18 payloads), `REG_RMW`, `INVALIDATE_STATE`, binning,
`EVENT_WRITE_SHD`, `NOP` et `IM_LOAD_IMMEDIATE` borné. La sonde r75 montre
successivement `consumed_dwords=19`, puis `consumed_dwords=12` pour les quatre
descripteurs IB. Aucun octet retail n'est versionné ou installé; le microcode
Xenos n'est pas exécuté côté hôte et sa traduction SPIR-V reste ouverte.

Validation r75 : build et CTest **9/9**, pytest retail **126/126**, audit
d'installation sans fichier interdit et `validate.py --target ntsc-uj
--runtime native` passent. `--require-release` sort 2 avec campagne/save/runtime
non qualifiés. La sonde `_xstart` SDL dummy expire après le lot IB (exit 124),
sans retour guest, frame présentable ou gameplay M01; la prochaine frontière
est le scheduler/kernel et les événements SDK.

r76 ajoute une table de handles d'événements dans les stubs offline
build-only : états signaled/manual-reset, set/clear/pulse, attente bornée et
`NtSignalAndWaitForSingleObjectEx` cohérent avec `STATUS_TIMEOUT`. La
validation ciblée et le rebuild restent verts (**9/9**, **126/126**); la
sonde reste à l'expiration post-IB et cette tranche n'est pas une preuve de
scheduler Xenon complet.
