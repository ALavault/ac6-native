# AC6 retail NTSC-U/J — codegen direct et liaison guest Gate 2 (2026-08-31)

- Receipt r11 : XenonAnalyse/XenonRecomp US passent avec 81 fichiers générés,
  62 629 029 octets, zéro diagnostic et `unrecognized_instructions=[]`.
- Guest : 79 objets compilés avec `XenonUtils/ppc_context.h`, liaison à
  `ppc_func_mapping.cpp` et 229 définitions d'imports offline; probe `noinline`
  vérifie symboles et mapping, exit 0.
- Intégration native : build `gate2-codegen-linked`, CTest 9/9 et validator
  `--runtime native` passent. Cette preuve couvre codegen et liaison seulement.
- Bootstrap : `ac6recomp <ISO|assets/>` compile, s'installe sous `bin/`,
  self-test passe; parseur XEX2 AES-CBC/basic, validation PE et audit
  d'installation temporaire sont verts.
- Mémoire : `GuestAddressSpace` réserve virtuellement 4 GiB, teste accès/
  dépassement 32-bit et est exigée au boot `NativeRuntime`; image XEX décodée
  écrite à son adresse de chargement.
- ISO : lecteur XDVDFS natif borné, test chemin case-insensitive et extraction
  en mémoire de `default.xex`; aucune copie retail persistante.
- XEX : AES-CBC XEX2 + compression basic natifs, validation PE Xenon, image
  `0xa98000` mappée à `0x82000000` pour assets et ISO qualifiés; normal/LZX
  rejeté.
- Guest runtime : `ac6recomp` lie le guest r11 et affiche le smoke ABI
  `sub_8209C0B4` après mapping; preuve d'appel généré seulement, pas de boot
  retail/gameplay.
- Dispatch : 19 832 pointeurs `PPC_LOOKUP_FUNC` peuplés en mémoire guest;
  résolution entry `0x821f5ed0` vers `_xstart`, sans exécution de l'entrypoint.
- Probe borné : après bindings provisoires TLS/timebase/handles/waits, une
  trace opt-in atteint `sub_821F9E10` puis timeout 12 s; aucun claim gameplay.
- Fenêtre 60 s unique (`r46`) : timeout/124 sans retour; les sondes r47/r48
  confirment le bootstrap mémoire/pool/chaînes BE sans erreur de chargement.
- r49 : PCR/thread guest déterministe et `r13` initialisé; r50 : retrain
  EDRAM/HSIO Vd immédiat; la chaîne dépasse l’attente EDRAM.
- r51 : `ExCreateThread` lance des shims guest dans des workers bornés (16
  workers observés), mais le thread principal attend encore la queue/ring.
- r52 : la copie `native-source` est synchronisée; GDB confirme au vrai
  `_xstart` `r1=0x8ff00000`, `r13=0x0f000000` et les champs PCR écrits.
- r53 : `MmAllocatePhysicalMemoryEx` fournit le bloc ring; le callback Vd
  reçoit `0x164e003c`, bloc `0x164e0000` valide, readback 0/write 5. Le
  consommateur PM4/Xenos natif reste à relier et aucun pointeur synthétique
  n'est publié.
- r64–r75 : la qualification statique corrige le WPTR primaire à
  `object+10952`; `+10908` est le curseur secondaire. Le service publie les
  indices en dwords, conserve le readback exact `state+60` et résout les IB
  directement dans la mémoire guest big-endian. `PM4_ME_INIT` (19 dwords) puis
  le lot bootstrap IB (12 dwords) sont acceptés par le backend natif.
- r75 : CTest natif 9/9 et pytest retail 126/126; `validate.py --runtime
  native` passe, l'audit d'installation scanne un seul `ac6recomp` sans fichier
  interdit. La sonde `_xstart` SDL dummy expire après le lot IB; son exit 124
  est diagnostique, pas une preuve de gameplay.
- r76 : la génération des imports ajoute une table d'événements offline
  déterministe (handles, set/clear/pulse et waits non bloquants). Build/CTest
  passent 9/9 et pytest reste 126/126; la sonde expire encore après l'IB, donc
  aucun progrès gameplay n'est déduit.
- Bindings provisoires build-only : mémoire virtuelle, pool/chaînes, PCR,
  TLS, timebase 50 MHz, handles/events, Vd et workers; validation unitaire,
  pas encore SDK retail.
- Limite : stubs d'imports génériques, sans SDK Xenon/XAM/Vd/XMA complet;
  scheduler/événements restent la frontière après l'IB et la traduction shader
  `IM_LOAD_IMMEDIATE` vers SPIR-V n'est pas fermée. Aucun boot guest retail,
  gameplay M01, rendu présentable, campagne ou release n'est promu.

Preuve complète : `reports/ac6-retail-native-codegen-gate2-r11-20260831.md`.

# AC6 retail NTSC-U/J — correction Gate 2 et codegen borné (2026-08-30)

- `pytest -q recompilation/ace-combat-6-retail/tests`: 113 passed; CTest natif
  5/5 avec `-UNDEBUG`; validator native pass.
- PM4 capsule `ac6.xenos-capsule.v1`: opcodes hardware Xenos, XE_SWAP `SWAP`,
  TYPE1 à deux registres, prédicat rejeté, 7 dwords validés.
- Census `ac6.retail-native-import-census.v1`: 229 imports uniques dans le
  mapping US; réseau offline et aucun socket, bindings restant à migrer.
- Export Ghidra US qualifié: 8 163 fonctions; sélection des 89 propriétaires
  depuis le log switch r3 et croisement des débuts du mapping précédent.
- Génération `all`: timeout 20 min; sélection propriétaire `r4`: exit 2 avec
  2 881 diagnostics; croisement mapping `r5`: receipt absent/exit 124. Aucun
  output promu.

Preuve complète: `reports/ac6-retail-native-gate2-static-correction-20260830.md`.

# AC6 retail NTSC-U/J — Gate 2 codegen r3 diagnostiqué (2026-08-30)

- Une génération directe XenonAnalyse/XenonRecomp a produit 83 fichiers dans
  `build/` ignoré avec XEX US qualifié.
- Huit helpers ABI US ont été qualifiés par signatures bytes; r3 reste à 1 831
  diagnostics (1 824 switch hors frontières, 7 instructions non reconnues).
- Qualification est sérialisée dans `artifacts/retail-us-native-codegen-gate2/helpers.json`;
  chaque signature est unique et alignée.
- Receipt reste `open-diagnostics`, exit 2, aucune liaison runtime.

Preuve complète: `reports/ac6-retail-native-codegen-gate2-r3-20260830.md`.

# AC6 retail NTSC-U/J — Gate 1 statique fermé (2026-08-30)

- Profil `native` isolé, capsule `ac6.xenos-capsule.v1` et convertisseur
  capture read-only ajoutés; PM4/MMIO/ring/IB/EDRAM/Vulkan/shader boundary
  testés fail-closed. Services offline (réseau, VFS/save, replay, XMA) ont
  leurs contrats natifs et tests; handles, auto-reset event et timebase sont
  aussi couverts. `NativeRuntime` relie boot média, replay, ring, save/load et
  shutdown sans état gameplay synthétique.
- Validation: `pytest` retail `111 passed`, CTest native `5/5 passed`,
  `validate.py --runtime native` pass, `release_ready=false`.
- Audit d'installation natif prêt; l'installation oracle existante échoue
  volontairement sur contenu ReXGlue.
- Capture oracle réelle et replay Vulkan matériel restent ouverts.

Preuve complète: `reports/ac6-retail-native-gate1-20260830.md`.

# AC6 retail NTSC-U/J — Gate 0 état canonique (2026-08-30)

- Identités US qualifiées et scellées: XEX
  `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`, ISO
  `204c5e645d79da8776699c12f17bd069f869fbdb10ae79015d1a0ef2b743c98c`, projet
  Ghidra `ghidra-projects/ac6-us`, AC6_recomp
  `09144bb092ad871584808aeead69c395edbd5200`, route
  `771a77a8ff50eda30c5fb24309d8828bb339f91a49471368f117b65c9dbb6043`.
- XenonRecomp `ddd128bcca99fe8bfbb99bea583c972351fa6ace` et XenosRecomp
  `990d03b28a27b50277ee5d8d942e1c5f873869d1` restaurés proprement dans
  arbres ignorés; jamais liés ni installés.
- N2 reconstruction abandonné pour cette feuille; historique conservé.
- Catalogue architecture local absent, donc aucune assertion générique.
- Validation: `pytest -q recompilation/ace-combat-6-retail/tests` = `94 passed`.

Preuve complète: `reports/ac6-retail-native-gate0-20260830.md`.

# AC6 retail NTSC-U/J — Gate 1 renderer natif statique (2026-08-30)

- `native` build compiles sans AC6_recomp, ReXGlue, Xenia ou média retail.
- PM4/MMIO/ring/EDRAM/present typed boundary passes; malformed packets fail
  before state/output effect, IB and unsupported opcodes fail closed.
- Capsule `ac6.xenos-capsule.v1` validates identity US and rejects unknown
  fields such as retail byte payloads.
- `pytest` retail: 102 passed; CTest native: 1/1 passed; native validator pass
  with `release_ready=false`.
- Oracle runtime capture and real Vulkan replay remain open; no gameplay or
  release claim made.

Preuve complète: `reports/ac6-retail-native-gate1-20260830.md`.

# Native US 2026-08-30 — N1b M01-B formelle reproductible

- Deux runs natifs distincts : 3 600 ticks chacun, exit 0 chacun.
- `cmp=0` : replay, huit captures PPM et reçu v2 complet.
- Reçu commun `90fcddb9…1caf39`; replay commun `6bf324b8…c6fead`.
- Scène : cité 4 226, terrain 65 536, eau 31 191 cellules visibles, F-16
  4 435 sommets/6 468 indices, HUD vert 112 pixels aux huit jalons.
- Validation : 88/88 CTest, test scène US, cache et frontières produit verts ;
  `complete_render_scene=true`, `jv_eligible=true`.

Preuve complète :
`reports/native-us-n1b-m01-b-20260830.md`.

# Native US 2026-08-30 — N1a free-flight reproductible

- Deux runs natifs distincts : 1 800 ticks chacun, exit 0 chacun.
- `cmp=0` : replay, sept captures PPM et manifeste complet.
- Monde : cité 4 226, terrain 65 536, eau 31 191 cellules visibles, F-16
  4 435 sommets/6 468 indices, HUD vert 112 pixels aux sept jalons.
- Contrôles visibles : 121 996 / 193 751 / 235 086 / 279 803 / 283 519
  pixels changés pour pitch / roll / yaw / throttle / frein.
- Validation : 88/88 CTest, complexité et frontières vertes ; scène US
  explicite verte ; `jv_eligible=false` préservé.

Preuve complète :
`reports/native-us-n1a-free-flight-20260830.md`.

# N0 Native US 2026-08-30 — baseline NTSC-U/J qualifiée

- Cache : 926 blobs, 15 payloads et 15 mondes campagne, index
  `d7071928…1a34df5b`; reçu statique US séparé, sans promotion runtime.
- Build : binaire `c1e7ddb5…f65edf`; 88/88 CTest, zéro échec.
- Produit : 260 sources + un ELF verts; paquet 96 entrées vert, SHA-256
  `2d008a25…2a403b`, sans ReXGlue/Xenia/C++ généré/octet retail.
- Frontière : N1a doit encore prouver deux runs identiques de 1 800 ticks,
  cinq contrôles, pose/caméra live et scène/HUD Vulkan.

Preuve complète :
`reports/native-us-n0-baseline-20260830.md`.

# R0 Retail US 2026-08-30 — frontbuffer guest incomplet

- Binaire : `46e3018639c4341e8aaa2ce897fd88e558593a1ccc2f474cfb2ae974741e5a1c`.
- Statique : 16/16 CTest, validation NTSC-U/J `pass`, installation propre.
- Runtime unique : cache 193/193, route `771a77a8…9dbb6043`, phase
  `cinematic=0 world=1 hud=1 stable=30` observée.
- Renderer : resolve `1AB6`, décodage vert, puis `PRESENT`; capture HUD verte
  sur monde noir.
- Terminal : Escape produit `world=0`, predicate post-edge absent, zéro fatal,
  teardown forcé et reçu `fail`.

Preuve complète :
`reports/retail-us-r0-frontbuffer-validation-20260830.md`.

# Retail US 2026-08-30 — LOD 0869 et routage HUD/frontbuffer

- PROUVÉ : `0869` ne noircit plus la scène avec LOD0 limité à `tf2/tf12` mip0;
  la sortie du dernier `50D9` reste riche.
- PROUVÉ : `0311`, `8F1C/4366` et le resolve `1AB6` partagent la chaîne guest
  correcte; aucun mismatch de layers guest.
- PROUVÉ : le presenter actif contournait `1AB6` au profit de `1B9C`, un seul
  `VkImageView`, donc sans HUD post-`0311`.
- IMPLÉMENTÉ : fallback presenter `false` par défaut; `sync-log` avant
  `Escape`; 57 tests ciblés passent.
- NON PROMU : les runs courants sont diagnostiques. Le correctif frontbuffer
  doit encore être rebuildé et produire monde + HUD dans une frame stable
  post-edge appariée.

Preuve complète :
`reports/retail-us-0869-lod-present-frontbuffer-fix-20260830.md`.

# Retail US 2026-08-28 — reachability service et interfaces release fermées

- Chaîne directe : `0x821F5ECC -> 0x821F7B28 -> CRT[0x823F03F4] ->
  0x823CB708 -> 0x82120FD8`, objet `0x829BAF30`, vptr `CNuSound`
  `0x8205D1A4`.
- Slot terminal effectif : `+0x168 -> 0x82124930`, prédicat sur
  `objet+0xCECC`. L'ancien `0x8275CFC4` est corrigé en `0x8205CFC4`.
- Contrat automatisé : gameplay/audit v3, débrief/visuel v2, cinq contrôles
  tous `>5000`, invariants ReXGlue/Vulkan sans claim pixel, replay strict,
  cold cache M01, chaîne save/cache, aggregate campagne v1.
- Validation : `84 passed`; manifeste courant valide, non release :
  `gameplay_pass=0`, `campaign_run=unverified`, `release_ready=false`.

Preuves :
`artifacts/retail-us-campaign-service-owner-reachability-static-20260828/summary.json`
et
`reports/retail-us-mission01-campaign-service-owner-reachability-static-20260828.md`.

# Retail US 2026-08-28 — loader campagne qualifié statiquement

- PROUVÉ : le projet Ghidra US canonique qualifie
  `0x8218F4F0..0x8218F81B`; `state=0->1` dépend de la résolution
  `0x821D3028` puis du retour `>0` de `0x821D28C8`.
- PROUVÉ : la ressource scénario US `DATA.TBL[9]`/FHM enfant 0 est qualifiée;
  l'hypothèse « scénario US absent » est fermée.
- NON PROUVÉ : complétude runtime du graphe, heartbeat `FlightActive`,
  postprocess/resolve et teardown. Le run analogique a expiré avant
  `state=1->2`; ne pas le répéter.

Preuve complète : `reports/retail-us-mission01-campaign-loader-static-20260828.md`.

La chaîne statique `FlightActive`/heartbeat et le layout du paquet MnK sont
qualifiés séparément dans
`reports/retail-us-mission01-flightactive-static-20260828.md`; le heartbeat
runtime reste non observé.
Le propriétaire US initial du service de transition est qualifié :
`0x823F9B28 -> CAce6Sound`, slot `+0x168 -> 0x823A2C30` (`li r3,0; blr`).
La même statique qualifie les sites d'écriture vers `0x829BAF30` et un vptr
candidat `0x8275CFC4`; leur reachability normale et le slot runtime effectif
restent ouverts.
Voir `reports/retail-us-mission01-campaign-service-owner-static-20260828.md`.

# Retail US 2026-08-28 — sonde analogique bornée, frontière non résolue

- PROUVÉ : rebuild/install `job-mtct7k4h-0be226ed` sous binaire
  `e93d9fbe…bd8115`, ReXGlue/Vulkan 1280×720/30, 16/16 tests et `bin/bin`
  absent.
- ÉCHEC BORNÉ : `job-mtctdvii-e9c22860` s’arrête à `2/96` (`type28=30`
  absent), sans ligne analogique, `game_status=-9`, aucun fatal/trap.
- DÉCISION : aucune preuve d’entrée ne peut être promue depuis cette route.
  La route originale qualifiée est admise explicitement au harness diagnostic;
  une corrélation read-only unique est en cours sous
  `job-mtctm90d-3d748f51`.

Preuve complète : `reports/retail-us-mission01-gameplay-candidate-20260828.md`.

# Retail US 2026-08-28 — corrélation analogique originale expirée

- ÉCHEC BORNÉ : `job-mtctm90d-3d748f51` expire à 20 min (`exit 124`) avec le
  binaire `e93d9fbe…bd8115`, route originale `771a77a8…9dbb6043` et seed
  `/tmp/ac6-cache-seed-v2-complete`; aucun `RESULT.json` finalisé.
- PROUVÉ : 50 captures jusqu’à `step-50-language`, `5 587 PRESENT`, `94
  type28` et boutons XAM non nuls; aucun `[ac6-campaign-transition] state=1->2`,
  aucune ligne `xinput ... analog`, aucun `world=1/hud=1`.
- DÉCISION : pas de reçu gameplay et pas d’attribution renderer/input; ne pas
  répéter. La prochaine frontière est statique : transition campagne,
  `FlightActive`, postprocess/resolve, teardown `-11`.

Preuve complète : `reports/retail-us-mission01-gameplay-candidate-20260828.md`.

# Retail US 2026-08-28 — gameplay v2 candidate and adaptive route boundary

- PROUVÉ : binaire candidat `b004ee70…4d70`, route longstart
  `6ef77bef…14dac4b`, Vulkan 1280×720/30 FPS, SDL dummy et observables
  scheduler/objective read-only.
- PROUVÉ : la sonde diagnostique 96/96 atteint
  `cinematic=0 world=1 hud=1 stable=30`; centre `0.332014/0.271874/1.0`,
  contrôles `843291/857818/905833/920970/882008` pixels.
- ÉCHEC BORNÉ : deux gates cache/profil vierges expirent à l'étape 2 sur
  `type28=30`, sans fatal/trap. Un run avec cache Vulkan précréé atteint
  campagne, briefing et cinématique mais expire à l'étape 75 sans `stable=30`.
- DÉCISION : aucun reçu gameplay/audit n'est promu; la prochaine fenêtre est
  une seule route diagnostique adaptative de l'edge `Escape` sur
  `stable=1→stable=30`, sans guest write, trace globale ni A/B. PAL et les
  missions 02–15 restent bloqués.

Preuve complète : `reports/retail-us-mission01-gameplay-candidate-20260828.md`.

# Retail US 2026-08-28 — frontière scheduler US qualifiée

- PROUVÉ : dans `ghidra-projects/ac6-us` / `default.xex` (PowerPC Xenon BE,
  XEX `6eefba42…67cbbbc`), `0x822ED310..0x822ED467` traite le signal `-2`,
  vérifie le contexte et `(object+0x124)&0x3f`, puis appelle `0x82267160`.
- PROUVÉ : le driver `0x82267160..0x8226723B` avance le pas du scénario et
  retourne `1` lorsque le tableau est épuisé ; appels directs `0x82258F8C` et
  `0x822ED408`.
- PROUVÉ : les adresses `0x822ED708`/`0x8226E158` sont des offsets internes,
  pas des entrées Ghidra ; le dump borné US est conservé dans l'artefact.
- NON PROUVÉ : activation native en vol, objectif, débrief, sauvegarde ou
  relecture ; cette preuve ne change pas `debrief_pass=0`.
- DÉCISION : prochaine fenêtre unique, runtime borné et read-only sur `-2`,
  les gardes, le retour du driver et le premier compteur M01 ; aucun signal
  fabriqué, guest write, trace globale ou A/B.

Preuve complète : `reports/retail-us-mission01-scheduler-static-20260828.md`.

# Retail US 2026-08-28 — diagnostic temporisé sans terminal M01

- PROUVÉ : la route candidate (SHA-256
  `425d075a…466a174`) exécute 96/96 opérations, atteint
  `cinematic=0 world=1 hud=1 stable=30`, puis capture à 60/120/180/240/300 s
  sans entrée.
- PROUVÉ : `RESULT.status=diagnostic-capture-ready`, durée `755,014796 s`,
  28 captures, `game_status=0`, arrêt propre, zéro fatal/trap/timeout.
- PROUVÉ : aucun `[ac6-post-mission]`, terminal `0x822E3248` ou
  `[ac6-current-level-set]` après le HUD ; le niveau reste lu `1`.
- NON PROUVÉ : réussite d'objectif, débrief, sauvegarde de fin ou reload niveau
  2 ; le run ne vaut pas reçu gameplay.
- DÉCISION : fermer l'hypothèse d'auto-complétion sans entrée et préparer une
  route de contrôles dérivée statiquement des objectifs/compteurs US ; pas de
  trace globale, A/B, guest write ou correction renderer.

Preuve complète : `reports/retail-us-mission01-roundtrip-timer-diagnostic-20260828.md`.

# Retail US 2026-08-27 — handoff corrigé, phase HUD qualifiée

- PROUVÉ : le candidat `54dd900a…5711f9` atteint briefing/cinématique puis
  `cinematic=0 world=1 hud=1 stable=30` ; route diagnostique `23` captures,
  arrêt propre, Vulkan 1280×720.
- PROUVÉ : centre de `step-86-flight-hud-candidate.png` non noir
  (`mean=0.301670`, `stddev=0.302621`, `nonblack=1.0`).
- PROUVÉ : campagne `selector=1` et transitions `0→1→2` sont observées avant
  le vol ; aucun fatal/trap.
- NON PROUVÉ : contrôles de vol, débrief, sauvegarde et parité shader/lumière ;
  l’avion reste surexposé.
- DÉCISION : réutiliser la recette d’entrée pour le round-trip M01 ; ne pas
  promouvoir ce diagnostic en gameplay v2 ni ajouter d’override global.

Preuve complète : `reports/retail-us-mission01-cinematic-handoff-candidate-20260828-r2.md`.

# Retail US 2026-08-28 — scénario Mission 01 US qualifié

- PROUVÉ : tranche `DATA.TBL[9]` de l’ISO US (`offset=16908288`,
  `length=13234635`) et payload FHM enfant 0 vérifiés sans copier le PAC.
- PROUVÉ : SHA-256 scénario `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`;
  probe natif `230` unités, `434` objets, `4` sous-missions, `232` flag orders,
  `666` lectures, aucune erreur.
- DÉCISION : la ressource US est qualifiée pour le gate M01 ; pas de promotion
  gameplay/debrief et pas d’écriture invitée.

Preuve complète : `reports/retail-us-mission01-scenario-static-qualification-20260828.md`.

# Retail US 2026-08-27 — recette Campaign/New Game qualifiée

- PROUVÉ : settle `8 s` + edges `space=0,6 s` font progresser les écrans
  difficulté, contrôles et langue ; 11 captures sont produites.
- PROUVÉ : le getter publie `selector=1 value=1`, puis le propriétaire de
  transition publie `state=0->1` et `state=1->2`.
- PROUVÉ : `7 483` `PRESENT`, Vulkan 1280×720, aucun fatal/trap ; arrêt borné
  après capture (`status=fail` technique, `game_status=-9`).
- NON PROUVÉ : gameplay, débrief, persistance ou stabilité visuelle monde ; ce
  n’est pas un reçu de release.
- DÉCISION : réutiliser cette recette dans le round-trip M01 et ne pas répéter
  le diagnostic isolé.

Preuve complète : `reports/retail-us-mission01-campaign-confirm-candidate-20260828.md`.

# Retail US 2026-08-27 — handoff titre/campagne borné

- PROUVÉ : le handoff synchronisé franchit le profil et Game Data ; les états
  save `type28=30→37→35→9→5→6→8→10` et 12 captures sont présents.
- PROUVÉ : après la langue, aucune transition `state=1->2` ; l’écran
  `CAMPAIGN / NEW GAME` et `4 610` `PRESENT` restent figés.
- PROUVÉ : captures à 12 s identiques (`62a77bf1…`), arrêt contrôlé `15`, sans
  fatal/trap ; aucun `RESULT.json` de gameplay n'est produit.
- NON PROUVÉ : entrée Mission 01, gameplay, débrief, persistance ou cause
  renderer.
- DÉCISION : conserver le round-trip M01 comme gate et ne pas répéter ce
  handoff sans nouvelle preuve statique.

Preuve complète : `reports/retail-us-mission01-cinematic-handoff-candidate-20260828.md`.

# Retail US 2026-08-27 — route longue d'amorce bornée

- PROUVÉ : avec le candidat `54dd900a…5711f9` et Vulkan, le run a produit
  `349` `PRESENT` en 1280×720 et `141 840` lignes movie worker en `8 min 25 s`.
- PROUVÉ : aucun `[ac6-campaign-transition]`, `[ac6-save-route]`, `type28` ou
  `selector44` ; seulement deux phases `world=0`/`stable=0`.
- PROUVÉ : les captures Xvfb de `23:07:40` et `23:12:21` partagent le SHA-256
  `87a84b42…`; arrêt cgroup contrôlé `exit-status=15`, sans fatal/trap.
- NON PROUVÉ : entrée campagne, gameplay, débrief, persistance ou cause
  renderer ; il n'existe aucun `RESULT.json` de gameplay pour ce run.
- DÉCISION : diagnostiquer l'amorce comme bloquée et conserver le round-trip
  M01 comme gate ; ne pas relancer la même route ni ajouter d'override global.

Preuve complète : `reports/retail-us-mission01-flight-long-candidate-20260828.md`.

# Retail US 2026-08-27 — diagnostic propriétaire monde

- PROUVÉ : le tick US `0x8226CEA0` est appelé avec le manager qualifié
  `0xB0D90000`/vtable `0x820644EC`, état `0->1` puis stable ; la caméra est
  appelée dans 85/93 échantillons.
- PROUVÉ : les frames frontier restent chargées (jusqu'à 1 757 draws et 91
  resolves) et le run s'arrête proprement sans fatal/trap.
- BORNÉ : aucun appel à l'update objet `0x822704A0` n'est observé ; sa seule
  référence directe est `0x82256538` dans `0x82256490`, hors tick.
- NON PROUVÉ : phase visuelle stable 30 frames, HUD cockpit, débrief ou
  persistance ; le reçu est diagnostique uniquement.
- DÉCISION : ne pas ajouter d'override renderer ; reprendre le round-trip
  M01 décrit dans `NEXT.md`.

Preuve complète : `reports/retail-us-mission01-world-owner-diagnostic-20260827.md`.

# Retail US 2026-08-27 — observables de persistance validés statiquement

- PROUVÉ : getter `0x820943B0..0x8209443F` et setter
  `0x82196590..0x8219661F`, feuilles direct-call US de `0x90` octets.
- PROUVÉ : save manager `0x82158D90..0x82159293`, fonction `.pdata` de
  `0x504` octets ; propriétaire progression `0x821A6400..0x821A64FF`.
- PROUVÉ : quatre wrappers forts read-only, zéro store invité, hook-map exact
  `d7d66b01…dc9239a`.
- VALIDATION : build sans codegen statut 0, Python 65/65, natif 16/16, Vulkan,
  SDL dummy, zéro D3D12, `bin/bin` absent.
- IDENTITÉ : candidat runtime `54dd900a…5711f9`, 37 797 240 octets ; reçu
  gameplay antérieur conservé séparément et non promu.
- OUVERT : round-trip naturel M01, opération save, quiescence et relecture
  fraîche du niveau 2 ; `debrief_pass=0`.

Preuve : `reports/retail-us-mission01-persistence-observables-build-20260827.md`.

# Retail US 2026-08-27 — débrief et progression Mission 01 qualifiés

- PROUVÉ : projet `ac6-us`, XEX `6eefba42…67cbbbc`, aucune preuve PAL/démo.
- PROUVÉ : `0x822E3248` convertit l'état monde terminal 4/5 en état mission 14
  et résultat 1, avant `CModeTaskGame -> CModeTaskDebriefing`.
- PROUVÉ : chaîne normale Debriefing -> DemoIntermission -> Unlock ->
  InterMissionSelect -> MissionTitle, factories et vtables RTTI canoniques.
- PROUVÉ : `0x821A6400` est le slot `+0x2C` de
  `CModeTaskInterMissionSelect` et appelle `0x82196590(niveau + 1)` sous 15.
- PROUVÉ : la tâche enregistre et actualise `CSelectSaveLoadManager` par
  `0x82158D00`/`0x82158D90`, dispatcher canonique des opérations 1 à 8.
- OUVERT : opération choisie après M01, fin save propre et relecture fraîche du
  niveau 2 ; `debrief_pass=0` reste fail-closed.
- PROCHAIN : un seul round-trip runtime en deux phases, sans A/B, trace globale
  ni mutation de l'état invité.

Preuve : `reports/retail-us-mission01-debrief-progression-static-20260827.md`.

# Retail US 2026-08-27 — routes campagne statiques fermées

- PROUVÉ : projet `ac6-us`, XEX `6eefba42…67cbbbc`, feuille directe
  `0x821CD168..0x821CD2E3`, appel entrant `0x821D121C`, trois `blr`, prochaine
  entrée `.pdata` `0x821CD2E8`.
- PROUVÉ : `0x821D6248` écrit le format `2`, `0x821D624C` appelle
  `0x821CC288`, qui charge `sim:DATA.TBL` et publie `entry_count` à
  `0x8293B950` ainsi que la base `file+8`.
- PROUVÉ : ISO US `204c5e64…743c98c`, extraction XDVDFS bornée du seul
  `DATA.TBL`, 14 824 octets, 926 entrées, 2 PACs, SHA-256
  `bad3a157…863b2f` ; identité PAL distincte et non utilisée.
- CONTRAT : quinze routes qualifiées, sélecteurs `1..15`, DPL et entrées
  `DATA.TBL` `9..23`; `static_qualified=15`, `gameplay_pass=1`,
  `debrief_pass=0`, `visual_pass=0`, `release_ready=false`.
- VALIDATION : Python 65/65, natif 16/16, `validate.py` pass, Vulkan complet,
  SDL dummy, zéro D3D12, `bin/bin` absent ; `--require-release` échoue comme
  attendu.

Preuve : `reports/retail-us-campaign-manifest-static-20260827.md`.

# Retail US 2026-08-27 — contrat campagne 15 missions

- PROUVÉ : projet `ac6-us`, XEX `6eefba42…67cbbbc`, sélecteur `0x821B6EE8`,
  table `.rdata` `0x820657B0`, mapping missions `1..15` vers DPL `9..23`.
- PROUVÉ : `0x821D1190` garde la voie directe à `<0x39D` et appelle
  `0x821CD168`; `0x821CC288` charge `sim:DATA.TBL`.
- BORNÉ : la copie `.data` `0x826919A4` n'a aucun matérialiseur PPC trouvé et
  n'est pas promue.
- OUVERT : `0x821CD168` n'est pas encore une frontière Ghidra canonique et
  l'identité du `DATA.TBL` US extrait de l'ISO qualifiée manque.
- CONTRAT : manifeste 15 missions valide mais fail-closed ;
  `selector_qualified=15`, `static_qualified=0`, `gameplay_pass=1`,
  `debrief_pass=0`, `release_ready=false`.
- VALIDATION : Python 60/60, natif 16/16, `validate.py` pass, Vulkan complet,
  SDL dummy, zéro D3D12, `bin/bin` absent ; `--require-release` échoue comme
  attendu.

Preuve : `reports/retail-us-campaign-manifest-static-20260827.md`.

# Retail US 2026-08-27 — gate gameplay v2

- PROUVÉ : route qualifiée 96 opérations et 27 captures ; les onze dernières
  sont cinq vues cinématiques, HUD, pitch, roll, yaw, throttle et frein.
- PROUVÉ : reçu `ac6.retail-gameplay-gate.v2`; v1, cinématique, centre noir et
  contrôles à 5 000 pixels ou moins sont rejetés avant audit.
- PROUVÉ : le signal compositor monde est maintenant publié par Vulkan ; le
  marqueur read-only attend 30 frames `cinematic=0 world=1 hud=1`.
- PROUVÉ : tests Python 40/40, build cgroup statut 0, validation statique statut
  0, tests natifs 16/16, Vulkan, SDL dummy, zéro D3D12 et `bin/bin` absent.
- IDENTITÉ : le premier préflight a refusé l'ancien binaire sans marqueur avant
  lancement ; après synchronisation de la copie préparée, binaire installé
  `b869e2b1…61fde55`, marqueur vérifié dans l'image.
- NON RÉSOLU : aucun reçu visuel gameplay ; prochaine preuve autorisée = un
  `.rdc` `cinematic-d5b4` dans une nouvelle session lourde.

Preuve : `reports/retail-us-gameplay-gate-v2-build-20260827.md`.

# Retail US 2026-08-27 — sonde F556/resolve

- PROUVÉ : le premier draw `F556C89634BEBFEF` consomme `tf0` en format 6,
  endian 2, tuilé 256×256; clamp `2,2,2` (edge) et bordure `0` (noire).
- RÉFUTÉ : la bordure blanche comme cause des taches F556.
- PROUVÉ : `0x1C95E000` est une destination de resolve GPU; les zéros observés
  côté CPU ne décrivent pas le contenu GPU échantillonné.
- INCONCLUANT : le probe de contenu resolve est resté dans le hangar avant F556.
- RECLASSÉ : les étapes `84/87/90/93/96` de la route 96 sont la cinématique
  pré-mission malgré leurs noms `flight-*`; `flight-long2/step-87` est le seul
  candidat crédible de début de gameplay, avec HUD/radar sur monde noir.
- GATE FERMÉ : aucun reçu gameplay US, PAL toujours bloqué.

Preuve complète : `reports/retail-us-f556-sampler-resolve-20260827.md`.

# Retail US 2026-08-27 — qualification statique surfaces blanches

- PROUVÉ : D5B4 vise une cible couleur+profondeur valide et son fetch BC3 est
  présent; les probes raster, culling, depth/stencil et dé-swizzle ne suppriment
  pas l'aplat avion.
- PROUVÉ : F556 est une passe point-list `tf0` avec expansion Vulkan et
  coordonnées point clampées dans `[0,1]`; son fetch est format 6/endian 2,
  clamp edge et bordure noire.
- RECLASSÉ : les surfaces blanches observées dans cette preuve appartiennent à
  la cinématique pré-mission, pas à un HUD de vol.

Preuve complète : `reports/retail-us-white-texture-static-boundary-20260827.md`.

# Retail US 2026-08-27 — water-gradient inconcluant

- PROUVÉ : binaire US inchangé, 89/89 opérations, 24 captures, arrêt propre,
  zéro fatal/trap.
- PROUVÉ : les captures 73→89 sont toutes le même hangar (SHA-256
  `95bd914ef3160f4d2969869662471776b2075e0e8df4f4434494e9411f11bfd9`).
- INCONCLUANT : aucune image de vol, donc aucune décision sur
  `ac6_fix_water_line`.

Preuve complète : `reports/retail-us-stock-water-runtime-20260827.md`.

# Retail US 2026-08-27 — surfaces blanches : dé-swizzle/FBO non causaux

- PROUVÉ : `--ac6_fix_deswizzle=false` conserve les aplats blancs dans les
  captures de vol; le dé-swizzle ciblé reste activé.
- PROUVÉ : les fetchs D5B4/BC3 observés sont présents et tuilés; aucune texture
  nulle ou vue absente n'est démontrée.
- BORNÉ : `render_target_path_vulkan=fbo` n'atteint pas `type28=30` en 120 s;
  ce chemin n'est pas qualifié pour la route US.
- GATE FERMÉ : sonde diagnostique seulement (`clean_shutdown=false`), aucun
  reçu US et PAL toujours bloqué.

Preuve complète : `reports/retail-us-stock-deswizzle-runtime-20260827.md`.

# Retail US 2026-08-27 — FSI et sortie post-process bornés

- PROUVÉ : binaire US validé `a150da4022425d0477e20ecfaa06cd73162fa44074f409dac8edaffa36974de9`, sans nouvelle génération.
- PROUVÉ : 93/93 shaders SPIR-V valides et 93/93 réflexions réussies avec le tooling utilisateur.
- PROUVÉ : `VK_LAYER_KHRONOS_validation` 1.4.341 est visible par `vulkaninfo` via `VK_LAYER_PATH`, sans installation système.
- PROUVÉ : sessions longues propres (`89/89` et `85/85`), sans fatal/trap ; cinématique 3D visible puis sortie noire/HUD invité.
- PROUVÉ : les captures sans panneau n'ont pas le diagnostic hôte ; les anciens traits verts
  provenaient de ce panneau, pas d'un HUD de cockpit qualifiant.
- BORNÉ : dernière divergence entre passes post-process plein écran, resolves `0x1B9C0000`/`0x1AB60000` et texture swap.
- NON PROUVÉ : cause d'un shader, d'une barrière ou d'une seule cible ; aucun correctif aveugle retenu.
- GATE FERMÉ : pas de reçu gameplay Mission 01 avec HUD stable et cinq effets de vol ; PAL bloqué.

Complément : `glslc` (shaderc 2026.1-1) est installé sous
`/home/lavaulta/.local/bin` ; le paquet est identifié par le SHA-256
`9741c7e0351654a9b40afd7f679c5e4fac437221597411a4885e1d5df2739115`. Le probe
`reports/retail-us-native-hud-sequence-probe-20260827.md` a atteint `type28=30`
mais n'a pas émis `[ac6-campaign-transition]` et a été interrompu avant la
mission. La route 96/96 reste mécanique : ses captures de vol n'exhibent pas
le HUD de cockpit. Aucun reçu US n'est créé.

Preuves complètes : reports/retail-us-fsi-handoff-logged-runtime-20260827.md,
reports/retail-us-flight-long2-runtime-20260827.md,
reports/ac6-render-tooling-20260827.md et leurs artefacts associés.

# Retail US 2026-08-26 — observable propriétaire validé

- PROUVÉ : rebuild sans codegen, statut 0.
- PROUVÉ : binaire installé `64f34acf…46af5b8`, 37 654 672 octets.
- PROUVÉ : cinq wrappers qualifiés liés comme symboles forts.
- PROUVÉ : cvar et marqueurs mode/tick exigés par `validate.py`.
- PROUVÉ : 16/16 tests, Vulkan complet, SDL dummy, zéro D3D12/`bin/bin`.
- PROCHAIN : une route causale unique corrélant phases et chute des draws.

Preuve complète : reports/retail-us-world-owner-observable-rebuild-20260826.md.

# Retail US 2026-08-26 — propriétaires US qualifiés

- PROUVÉ : projet `ac6-us`, XEX `6eefba42…67cbbbc`, Xenon big-endian.
- RÉFUTÉ : CModeTaskGame US à `0x8219A170`.
- PROUVÉ : CModeTaskGame `0x8219A510`, `.pdata` jusqu'à `0x8219A748`.
- PROUVÉ : tick monde `0x8226CEA0`, objet `0x822704A0`, caméra `0x822638B0`,
  radio `0x82271908`, avec étendues et octets exacts.
- PROUVÉ : tick monde appelle directement caméra/radio et possède 35 appels
  indirects ; observable read-only borné préparé, zéro store invité.

Preuve complète : reports/retail-us-world-owner-static-qualification-20260826.md.

# Retail US 2026-08-26 — soumission du monde effondrée au handoff

- PROUVÉ : hangar franchi, carte/briefing/cinématique/HUD atteints sans panneau.
- PROUVÉ : 88/88 opérations, arrêt propre, aucune fatal ni trap.
- PROUVÉ : cinématique 969–975 draws, 91 resolves, 32 pointlists, 1280x720.
- PROUVÉ : pré-handoff jusqu'à 1 197/84/31 ; HUD stable à 106/1/0.
- PROUVÉ : `guest_swap_texture` et viewport restent 1280x720.
- RÉFUTÉ : absence globale de renderer ou simple rupture PRESENT/viewport aval.
- BORNÉ : activation/dispatch/soumission du monde avant le swap.

Preuve complète : reports/retail-us-flight-world-submission-collapse-20260826.md.

# Retail US 2026-08-26 — divergence bornée au hangar sans panneau

- PROUVÉ : panneau vert absent des 24 captures.
- PROUVÉ : 87/87 opérations, arrêt propre, aucune fatal ni trap.
- PROUVÉ VISUELLEMENT : étapes 61/64/67 correctes, puis hangar identique aux
  étapes 70–87 (zéro pixel différent entre 70 et 87).
- RÉFUTÉ : les libellés 73–87 prouvent carte, briefing, cinématique ou vol.
- BORNÉ : première divergence = A immédiat sur `Deploy`; focus et clavier actifs.
- CORRIGÉ : attente de deux secondes avant l'unique confirmation du hangar.

Preuve complète : reports/retail-us-no-panel-hangar-divergence-20260826.md.

# Retail US 2026-08-26 — rebuild résumé persistant sans panneau

- PROUVÉ : rebuild sans codegen, statut 0.
- PROUVÉ : binaire installé `60c9fc6a…15da94`, 37 644 224 octets.
- PROUVÉ : 16/16 tests, Vulkan complet, SDL dummy, zéro D3D12, zéro `bin/bin`.
- CORRIGÉ : conservation du dernier résumé non vide entre PRESENT vides.
- CORRIGÉ : panneau diagnostic désactivé par défaut, activation explicite seule.
- PROCHAIN : un probe signatures/HUD sans panneau dans une nouvelle session.

Preuve complète : reports/retail-us-last-meaningful-rebuild-20260826.md.

# Retail US 2026-08-26 — résumé de frame non autoritatif

- PROUVÉ : 87 opérations, cinématique 3D puis HUD sur monde noir.
- PROUVÉ : `capture active=yes`, mais 0/0/0 draws/clears/resolves sur les deux.
- RÉFUTÉ : le zéro instantané qualifie l'absence de commandes graphiques.
- PROUVÉ : Vulkan présente `guest_swap_texture` en 1280x720, format 6.
- PROUVÉ STATIQUEMENT : les appels générés US ciblent les wrappers instrumentés.
- CORRIGÉ EN SOURCE : conserver le dernier résumé non vide entre PRESENT vides.
- ÉCHEC FERMÉ : `game_status=-9`, arrêt non propre, aucune fatal ni trap.
- PROCHAIN : rebuild unique avec rétention du résumé et panneau masqué ; pas d'A/B.

Preuve complète : reports/retail-us-black-world-frame-summary-20260826.md.

# Retail US 2026-08-26 — mode vol, monde noir

- PROUVÉ : le seuil 180 PRESENT permet le parcours complet jusqu'au HUD.
- PROUVÉ : cinématique 3D puis HUD visible sur monde noir.
- PROUVÉ : 87 opérations, arrêt propre, aucune trap ni fatal.
- OUVERT : première rupture world draw → RT → resolve → swap.
- PROCHAIN DISCRIMINANT : résumé par-frame borné déjà compilé.

Preuve complète : reports/retail-us-hud-black-world-20260826.md.

# Retail US 2026-08-26 — seuil de démarrage

- PROUVÉ : Escape après le premier PRESENT empêche tout second PRESENT.
- PROUVÉ : aucun type28, capture, fatal, trap ou échec audio.
- RÉFUTÉ : une seule présentation comme seuil de stabilisation suffisant.
- QUALIFIÉ STATIQUEMENT : seuil historique de 180 PRESENT avant entrée.

Preuve complète : reports/retail-us-cinematic-hud-one-present-20260826.md.

# Retail US 2026-08-26 — stall avant probe HUD

- PROUVÉ : 1 630 PRESENT puis arrêt des présentations avant tout type28.
- PROUVÉ : focus et impulsions actifs, aucun fatal, trap ou échec audio.
- NON EXERCÉ : recette cinématique A/Start et capture HUD.
- CORRECTION DE PROBE : attendre le premier PRESENT avant l'Escape initial.

Preuve complète :
reports/retail-us-cinematic-hud-startup-stall-20260826.md.

# Retail US 2026-08-26 — lancement 3D atteint

- PROUVÉ : A depuis la carte tactique lance la cinématique 3D du F-16.
- PROUVÉ : l'avion et le pilote sont visibles à 15 s; la séquence continue à
  35 s et reste sans HUD à 65 s.
- PROUVÉ : le run termine proprement sans fatal ni trap.
- OUVERT : première frame HUD après la recette qualifiée A puis Start.

Preuve complète : reports/retail-us-tactical-map-confirm-20260826.md.

# Retail US 2026-08-26 — carte tactique atteinte

- PROUVÉ : A depuis le premier hangar atteint la carte tactique Mission 01.
- PROUVÉ : la carte reste stable à 15/35/65 s et affiche A OK.
- PROUVÉ : le run termine proprement sans fatal ni trap.
- OUVERT : résultat du seul A confirmé depuis la carte tactique.

Preuve complète : reports/retail-us-first-hangar-confirm-20260826.md.

# Retail US 2026-08-26 — route hangar qualifiée

- PROUVÉ : Space mène de la sélection avion à l'arme puis au premier hangar.
- PROUVÉ : Shift à l'opération 70 renvoie du hangar vers les armes.
- RÉFUTÉ : les probes 0,1/0,6 s comme essais de lancement; leur Space revenait
  seulement des armes au hangar.
- OUVERT : résultat d'un unique A envoyé depuis le premier hangar.
- QUALIFIÉ SOURCE : le panneau vert est ouvert par Show(); le cvar dédié,
  désactivé par défaut, attend encore son rebuild.

Preuve complète : reports/retail-us-hangar-route-divergence-20260826.md.

# Retail US 2026-08-26 — lancement Mission 01

- PROUVÉ : le préfixe US atteint la transition campagne 0→1→2 et le hangar
  Mission 01.
- RÉFUTÉ : le bouton A de lancement tenu 0,1 s comme confirmation suffisante;
  les captures passives à 50/70/100 s restent à A OK / B CANCEL.
- OUVERT : résultat du seul probe à 0,6 s sur ce bouton; aucune autre entrée
  ne doit être injectée.

Preuve complète : reports/retail-us-mission-launch-short-press-20260826.md.

# Cycle 1849 — timeline marque requalifiée

- `PROUVÉ` : le parent `0x2E3CDD10` a 2 220 frames et avance naturellement
  d'une frame toutes les trois ticks; sa fin est autour du tick 6 882.
- `PROUVÉ` : `0x82323BB8` publie déjà les listes parent/enfant et produit
  l'index `owner+0xD8` sous son garde guest normal.
- `RÉFUTÉ` : `child+0xDC=0` à 3 200 ticks comme prédicat ou producteur à
  corriger pour atteindre Title.
- `OUVERT` : relier statiquement la dernière entrée du parent au cue Startup;
  le prochain test observe d'abord Title naturel sans entrée à 7 200 ticks.

Preuve complète : `reports/cycle-1849-demo-brand-movie-timeline-requalified.md`.

# Cycle 1848 — MovieController runtime borné

- `PROUVÉ` : le probe HSIO=1 atteint 3 200 ticks avec le ring actif (`1 182`
  soumissions, `1 089` présentations) et sans frontend, mission ni terminal.
- `PROUVÉ` : `0x82323BB8` atteint le parent `0x2E3CDD10` puis l'enfant
  `0x2E3CED10`; l'enfant a `D5=1`, `D6=0`, `D8=DC=0` dans la fenêtre ciblée.
- `RÉFUTÉ` : `D5` ou `D6` comme membre faux à corriger pour produire EndMode.
- `OBSERVÉ` : aucun appel SWG `0x820EA4A8` avant la borne; le cue parent→enfant
  et son producteur restent ouverts.

Preuve complète : `reports/cycle-1848-demo-moviecontroller-runtime-negative.md`.

# Cycle 1847 — dispatch render post-fence non causal

- `PROUVÉ` : `0x822E559C` est le LR du `bctrl` de `0x822E5540` dans le main
  loop, et son receiver dynamique appelle `0x822F8848` au slot `+0x10`.
- `PROUVÉ` : `0x822F8848` ne contient ni import/event wait ni accès direct à
  StartUp, EndMode, MovieController ou `manager+0x18`.
- `RÉFUTÉ` : le plateau render/context comme producteur à corriger pour Title.
- `OUVERT` : jointure runtime read-only du controller Startup à
  `0x82323BB8`, frame1/list1 et EndMode.

Preuve complète : `reports/cycle-1847-demo-render-dispatch-noncausal.md`.

# Cycle 1846 — prédicat post-fence négatif borné

- `PROUVÉ` : la chaîne aval `EndMode -> listener StartUp -> manager+0x18 ->
  Title` existe dans le guest.
- `PROUVÉ` : `0x82323BB8` garde l'avance MovieController par
  `effective_enabled != 0 && owner+0xD6 == 0`.
- `RÉFUTÉ` : traiter ce garde aval comme le correctif du stall HSIO=1; aucune
  trace ne le montre atteint.
- `OBSERVÉ` : le dernier dispatch guest démontré est
  `0x822E559C -> 0x822F8848`; son receiver/producteur reste ouvert.

Preuve complète : `reports/cycle-1846-demo-post-fence-predicate-bounded-negative.md`.

# Cycle 1845 — fence scratch observé sain

- `PROUVÉ` : le probe HSIO=1 atteint 3 200 ticks avec ring actif, 1 182
  soumissions et 1 089 présentations; `frontend=false`, `mission=false`.
- `PROUVÉ` : 192 entrées source 1 lisent `scratch0=4` et les 192 retours de
  `0x821B9710` lisent `scratch0=0`.
- `RÉFUTÉ` : un producteur zéro manquant, une complétion host absente ou une
  instruction PPC fautive sur le fence `0x16AE2000`.
- `OBSERVÉ, NON CAUSAL` : `pending_wait=4` à la fin de la fenêtre avec zéro
  interruption pending; il ne contredit pas les retraites observées.

Preuve complète : `reports/cycle-1845-demo-scratch-fence-runtime-refuted.md`.

# Cycle 1844 — retraite du fence scratch

- `PROUVÉ` : `SCRATCH_REG0=4` publie le dword attendu par
  `WAIT_REG_MEM(0x16AE2002, 0)` à `0x16AE2000`.
- `PROUVÉ` : `PM4_INTERRUPT(4)` est livré comme interruption graphique
  source 1; `0x821B9710` appelle le callback inscrit puis efface son bit CPU
  actif dans `scratch+0x0`.
- `PROUVÉ` : le callback courant `0x822E4240(0x82935270)` retourne; le zéro
  attendu est donc l'écriture de retraite du dispatcher, pas une complétion
  renderer ni un import HLE.
- `OUVERT` : vérifier dans le run réel l'entrée/sortie de cette retraite et la
  valeur observée par le wait, avant toute correction.

Preuve complète : `reports/cycle-1844-demo-scratch-fence-producer-qualified.md`.

# Cycle 1843 — validation Title négative

- `PROUVÉ` : build/CTest OFF 27/27 et ON 26/26 passent avec le profil texture
  BC3 320×160 existant.
- `PROUVÉ` : HSIO=1 initialise le ring et produit 1 182 soumissions sans
  mismatch à 3 200 ticks.
- `RÉFUTÉ` : `StartUp→Title` dans cette fenêtre; `frontend=false`, aucune
  frame Title à inspecter.
- `OBSERVÉ` : wait mémoire final sur `0x16AE2000`, valeur 4, référence 0;
  lecteur CP qualifié, producteur du retour à zéro inconnu.

Preuve complète : `reports/cycle-1843-demo-title-natural-not-reached.md`.

# Cycle 1842 — sélecteur HSIO et frontière renderer

- `PROUVÉ` : `0x821C64E8 -> 0x827AD310 -> 0x821BA780 ->
  device+0x2ABD.bit1 -> 0x821B9BC8` est la première chaîne divergente; elle
  sélectionne volontairement le ring sous HSIO=1.
- `RÉFUTÉ` : le plateau `0x822E559C -> 0x822F8848` comme cause; les workers et
  le ring progressent encore.
- `OBSERVÉ, NON CAUSAL` : le dernier `WAIT_REG_MEM` à 3 000 ticks porte sur
  `0x16AE2002`, `observed=4`, `reference=0`; son décodage et sa reprise sont
  couverts par le test CP existant.
- `PROUVÉ` : la première trappe de la longue exécution HSIO=1 est le profil
  texture BC3 320×160 au tick 4 911, produit par le draw invité et lu à la
  frontière `qualified_title_texture_profile`.
- `PROUVÉ` : ce profil et son test ciblé sont déjà présents dans l'arbre
  produit courant; aucune seconde correction n'est justifiée.

Preuve complète :
`reports/cycle-1842-demo-first-post-startup-divergence.md`.

# Maintenance post-cycle 1841 — suppression des handoffs contradictoires

## Observé

- avant : `NEXT.md` 3 981 lignes, `RESUME.md` 3 369 lignes;
- après : `NEXT.md` 29 lignes, `RESUME.md` 20 lignes;
- un seul titre principal par fichier, tous les liens courants existent;
- `AGENTS.md` réserve ces fichiers à l'état courant et route l'historique vers
  les ledgers et rapports;
- `git diff --check` et le parse de `reports/handoff/CURRENT.json` passent.

## Conclusion

Les instructions historiques ne peuvent plus se présenter comme gates actifs
dans les deux fichiers de reprise. Aucun build ou runtime n'était requis.

# Cycle 1841 — le correctif threading proposé n'a pas de défaut cible

## Observé

- `publish_guest_event` réserve déjà un waiter auto-reset dans
  `granted_thread`; seul ce waiter peut consommer la réservation.
- `run_runnable_threads` sert le primaire puis chaque thread runnable dans la
  même passe, avec yield toutes les 10 000 opérations mémoire et plafond de
  256 activations.
- Run cycle 1839 : t15 atteint 1 878 dispatches, dernier tick 2 997, pendant
  que t1 atteint 1 038 811 appels dans sa boucle; état final 1 runnable/22
  bloqués.
- Les critères runtime sont ring=1 114 PASS, t1=5 FAIL, Title absente FAIL.
- Validation source courante post-retrait : OFF 27/27, ON 26/26.

## Conclusion

L'auto-reset et la fairness demandés sont déjà effectifs; aucun patch produit
n'est causalement justifié. La branche s'arrête après la borne de batches. Une
suite doit nommer un prédicat invité ou service hôte post-StartUp distinct.

Preuves : `artifacts/goal-playable/threading-contract-review-20260825/`,
`reports/cycle-1841-demo-threading-contract-already-satisfied.md`.

# Cycle 1840 — qualification des producteurs de sémaphores

## Observé

- Projet Ghidra `ace-combat-6-demo`, cible `ac6-demo-xbox360-pal`, module
  `Default.xex` PAL démo qualifié.
- `0xE0000130` : `NtCreateSemaphore`; cinq producteurs appellent le wrapper
  `0x822E1E70`, dont l'import final est `NtReleaseSemaphore` ordinal 243.
- `0xE000005C` : `NtCreateSemaphore`; `0x822EF750` appelle directement
  `NtReleaseSemaphore` à `0x822EF7B4` après l'enqueue.
- Les atlas existants atteignent le premier producteur une fois et le second
  cinq fois avant les attentes observées.

## Conclusion

L'absence de publications dans le census antérieur est une limite de
couverture (`set/pulse` seulement), pas une absence d'exécution. Les deux
origines sont `guest-import`; l'état bloqué ultérieur correspond à des
sémaphores redevenus vides. L'oracle `AC6_recomp` n'est pas requis.

Preuves : `artifacts/goal-playable/thread-producer-static-20260825/RESULT.md`,
`reports/cycle-1840-demo-semaphore-producers-qualified.md`.

# Cycle 1839 — rotation seule réfutée

## Observé

- Variante : builds codegen-OFF/ON PASS ; CTest OFF 27/27, ON 26/26.
- Run unique HSIO=1 : rapport `max_ticks`, 3 000 ticks ; marqueur d'exit
  absent après interruption du contrôleur, donc run non qualifié comme succès.
- Thread 1 : 5 handshakes ; critère `>5` échoué.
- Ring : 1 114 soumissions ; critère `>0` passé.
- `StartUp→Title` : absente ; critère échoué.
- Scheduler : 2 998 épuisements de tranche, dernière tranche à 256 activations.
- Après retrait : builds PASS ; CTest OFF 27/27, ON 26/26.

## Conclusion

La rotation déterministe seule ne satisfait pas le gate et n'est pas
conservée. Les producteurs invités absents des posts `0xE000005C` et
`0xE0000130` redeviennent la frontière causale nommée.

Preuves : `artifacts/goal-playable/scheduler-rotation-20260825/`,
`reports/cycle-1839-demo-scheduler-rotation-refuted.md`.

# Cycle 1822 — stores `MovieController+0x14c/+0x15c`

- `PROUVÉ` — `0x82323BD0: or r30,r4,r4` identifie le second argument de
  `0x82323BB8` comme base des loads du transform.
- `PROUVÉ` — `0x82323CF4..0x82323D14` écrit
  `C14c=P4c*T0c` et `C15c=P5c*T0c+T1c`.
- `PROUVÉ` — `0x82323D3C..0x82323D5C` est la branche de copie de
  `P+0x40..+0x5f` vers `C+0x140..+0x15f` lorsque `C+0x08` et `C+0x04`
  sont nuls.
- `PROUVÉ` — la constante type 4 du cycle 1821 réduit les deux branches à
  `C14c=P4c`, `C15c=P5c`; la quantification alpha reçoit donc `P4c+P5c`.
- Preuve persistante : `artifacts/goal-playable/title-p1-parent-color-row-static-20260824/`.

# Cycle 1821 — `0x8264CDF8[4]` retourne une constante

- `PROUVÉ` — l'entrée type 4 à `0x8264CE08` contient `0x82322EC8`.
- `PROUVÉ` — `0x82322EC8..0x82322ED0` retourne uniquement `0x8264CDA0`.
- `PROUVÉ` — le global contient huit floats big-endian
  `[1,1,1,1,0,0,0,0]`; `T+0x0c=1` et `T+0x1c=0`.
- `PROUVÉ` — les composantes transmises par `0x82326420` se réduisent aux
  coefficients `MovieController+0x140..+0x15c`.
- `RÉFUTÉ` — l'alpha `BF` ne provient pas d'un transform type 4 dynamique.
- Artefact :
  `artifacts/goal-playable/title-p1-type4-transform-static-20260824/RESULT.md`.

# Cycle 1820 — matrice couleur produite par `0x82326420`

- `PROUVÉ` — `0x82326540` passe `r6=r1+0x50`; `0x82325E84` conserve ce bloc
  dans `r30` et `0x82325EB8` le passe en `r5` au slot 7 / `0x820EB200`.
- `PROUVÉ` — les cinq stores sont `0x823264A4`, `0x823264C8`, `0x823264EC`,
  `0x82326510` et `0x82326524`; leurs formules complètes sont conservées dans
  le `BLOCKER.md` du gate.
- `PROUVÉ` — la somme bornée consommée ensuite est
  `T[0x0c]*(C[0x14c]+C[0x15c])+T[0x1c]`.
- `RÉFUTÉ` — le `r5` renderer n'est pas une ressource persistante; c'est un
  bloc de pile construit dans le callback extérieur.
- `INCONNU` — `0x823237B8` tail-dispatche via `0x8264CDF8[type]`; la cible
  type 4 et les floats exacts restent à résoudre.
- Artefact :
  `artifacts/goal-playable/title-p1-matrix-color-producer-static-20260824/BLOCKER.md`.

# Cycle 1819 — `0x821DEED8` synthétise `record+0x18`

- `PROUVÉ` — `0x821DF018: stw r11,0x18(r31)` est l'unique store du constructeur
  vers ce champ.
- `PROUVÉ` — `0x821DEF88..0x821DEF90` teste le masque `0x20` de
  `parameters+0x40`; le chemin faux conserve `r11=0xffffffff`.
- `PROUVÉ` — le chemin vrai quantifie et assemble, du byte fort au byte faible,
  `parameters+0x3c`, `+0x30`, `+0x34`, `+0x38`.
- `PROUVÉ` — `0x820EB200` alimente ces champs depuis `r5+0x40`, `+0x44`,
  `+0x48` et `clamp(r5+0x4c+r5+0x5c,0,1)`; son `ori ...,0x860` force le bit
  `0x20`.
- `RÉFUTÉ` — le champ couleur n'est pas une copie opaque ni une corruption du
  readback. Le record guest porte déjà `BFFF0000` avant le writer P1.
- Artefact :
  `artifacts/goal-playable/title-p1-record-color-pack-static-20260824/RESULT.md`.

# Cycle 1818 — allocation et insertion du record P1

- `RÉFUTÉ` — `0x8211860C` n'écrit pas le nœud source : il copie
  `source+0x24` vers `owner_de_travail+0x18` dans `0x821185A8`, appelée en
  aval par `0x82118FA0`.
- `PROUVÉ` — `0x82095DF0` réserve un record aligné de `0x70` octets et appelle
  `0x821DEED8(owner,new_record,parameters)`.
- `PROUVÉ` — `0x82095E70` lie `tail+0x10=new_record`, `0x82095E78` établit
  `owner+0x20=new_record` pour une liste vide, et `0x82095E7C` publie la
  nouvelle tail en `owner+0x24`.
- `PROUVÉ` — `0x821DEED8` reçoit le record en `r4/r31`, le bloc de paramètres
  en `r5/r30`, et initialise `record+0x10=0` avant publication.
- `FORTEMENT ÉTAYÉ` — le callsite titre `0x820EB3C0..0x820EB3D4` fournit
  l'owner `0x8281EAF0` et le bloc local `r1+0x50`; les reçus existants joignent
  cette liste à `0x8219DB50 -> 0x82119488`.
- `INCONNU` — instruction exacte de `0x821DEED8` qui définit `record+0x18` et
  champ source correspondant dans le bloc local.

# Cycle 1817 — dataflow du record couleur P1

- `PROUVÉ` — `0x82119524` charge un owner depuis le tableau `r31+4+4*i` ;
  `0x8211954C` charge la tête de liste depuis `owner+0x20`.
- `PROUVÉ` — `0x821195B0` avance par `record+0x10`; `0x82119588` passe ce
  record en `r5` à `0x82118650`.
- `PROUVÉ` — le dispatcher charge `*(0x82009E78 + index*0x14 + 0x0C)`, pose
  `r4=record` et appelle le writer. Pour les deux slots déjà qualifiés, il
  appelle `0x82118D18`, qui lit donc le champ `record+0x18`.
- `CANDIDAT` — `0x8211860C` dans `0x821185A8` écrit `fr12` à `r31+0x18` ;
  l'identité de ce `r31` et les bits de `fr12` restent inconnus.
- `RÉFUTÉ` — `r30` n'est pas un tableau de records à stride fixe, et les
  helpers temporaires `0x82118C00/0x82118C98` ne prouvent pas le writer de
  `record+0x18`.
- Aucun runtime, aucune capture nouvelle et aucun changement renderer.

# Cycle 1816 — table de writers P/Q

- `PROUVÉ` : le build codegen-on lie son `xex-basefile.bin` au
  `demo-game-file/extracted/stfs-root/Default.xex` qualifié.
- `PROUVÉ` : `0x82118D18` apparaît exactement à `0x82009E98` et
  `0x82009EC0`, soit `+0x0C` des entrées `0x82009E8C` et `0x82009EB4` de
  taille `0x14`.
- `PROUVÉ` : les deux entrées diffèrent au dword `+0x04`
  (`09000001`/`09000003`) et partagent `+0x00=0D`, `+0x08=826F61C0`,
  `+0x10=821187A8`.
- `PROUVÉ` : zéro branche PPC directe vers `0x82118D18` et zéro construction
  directe des adresses de cellule ; l'indirection part d'une base de table.
- `INCONNU` : consommateur/index, sémantique des discriminants, producteur de
  `r4+0x18`.

Preuves complètes :
`artifacts/goal-playable/title-p1-dispatch-xref-static-20260824/`.

# Cycle 1850 et pivot retail

- `PROUVÉ` : le probe démo qualifié termine en trap après 5811 ticks avec le
  diagnostic `unsupported Xenos draw shape`; `frontend=false`,
  `mission=false`, `terminal=false`.
- `PROUVÉ` : le rapport primaire et les logs complets restent sous
  `artifacts/cycle-1850/`.
- `DÉCISION` : le runtime démo artisanal est archivé et supersédé; ses caches,
  rapports et preuves ne qualifient pas le produit retail.
- `PRODUIT COURANT` : AC6_recomp
  `09144bb092ad871584808aeead69c395edbd5200`, ReXGlue Vulkan complet,
  bootstrap NTSC-U/J avant PAL retail.

Synthèse : `reports/cycle-1850-demo-runtime-closed-superseded.md`.

# Preuve courante — cycle 1814 (24 août 2026)

- `PROUVÉ` : `vf0` lit le fetch constant 95/P1, tuple
  `103FB893 100A9002`; le slot 94/Q1 est seulement adjacent.
- `PROUVÉ` : les quatre mots couleur P1 observés valent `BFFF0000`.
- `PROUVÉ` : `k16in32` transforme le chargement hôte `0x0000FFBF` en
  `0xFFBF0000`; `FMT_8_8_8_8` puis `zyxw` donnent `(191,0,0,255)`.
- `RÉFUTÉ` : le renderer devait lire Q1, ou remapper Q1 vers P1.
- `RÉFUTÉ` : cette couleur naturelle est blanche.
- `PROUVÉ` : `0x82118D18` charge une seule fois `r4+0x18` et copie ce dword
  aux quatre offsets couleur de la primitive.
- `INCONNU` : identité du record `r4` et producteur de son champ `+0x18`.

Preuve :
`artifacts/goal-playable/title-vfetch95-color-semantics-static-20260824/RESULT.md`.

# Preuve précédente — cycle 1813 (24 août 2026)

- `PROUVÉ` : le PS titre exécute `tfetch2D` puis multiplie le sample par
  l'interpolateur couleur vertex avant l'export.
- `PROUVÉ` : la cible titre native est un attachement
  `VK_FORMAT_R8G8B8A8_UNORM`; son readback direct précède les étapes EDRAM et
  présentation.
- `RÉFUTÉ` : `copy_dest_swap`, le swizzle de présentation ou l'écriture PPM
  créent le rouge observé. Le rouge est déjà présent dans la cible du draw.
- `PROUVÉ` : la vue texture titre courante est déjà
  `VK_IMAGE_VIEW_TYPE_2D_ARRAY`; l'ancien défaut de vue 2D n'est plus une cause
  active.
- `PROUVÉ` : la preuve `k16in32` rouge du cycle 1812 décode le slot 94, tandis
  que le VS qualifié consomme `vf0`, joint au slot 95 dans le bridge.
- `INCONNU` : valeur des deux dwords du slot 95 au même draw ; l'identité entre
  P et Q ne doit pas être présupposée.

Preuve :
`artifacts/goal-playable/renderer-color-parallel-20260824/RESULT.md`.

# Preuve précédente — cycle 1812 (24 août 2026)

- `PROUVÉ` : le slot vertex 94 vaut `104A4893 100087F2`; les bits 0:1 du
  premier mot valent 3.
- `PROUVÉ` : dans l'enum Xenos locale, 3 signifie `k16in32`; il échange les
  deux demi-mots de chaque dword.
- `RÉFUTÉ` : le slot 94 emploie `k8in32`, comme l'indiquaient certains reçus
  historiques.
- `PROUVÉ` : `FF FF 00 00` chargé par Vulkan, échangé en `0xFFFF0000`,
  décompacté comme RGBA8 puis swizzlé `zyxw`, donne rouge opaque.
- `PROUVÉ` : le SPIR-V matérialisé exécute exactement ces opérations avant de
  stocker `r2`, que le VS publie dans l'interpolateur couleur.
- `RÉFUTÉ` : fabriquer du blanc dans le décodeur vertex serait une correction
  Xenos valide.
- `INCONNU` : producteur exact des octets couleur à `vertex+0x30`, ou état de
  composition antérieur censé fournir le fond blanc.

Preuve :
`artifacts/goal-playable/title-vfetch-color-semantics-static-20260824/RESULT.md`.

# Preuve précédente — cycle 1811 (24 août 2026)

- `PROUVÉ` : le fetch de présentation porte le swizzle `B,G,R,1`; le resolve
  porte `copy_dest_swap=1`. Le readback natif doit appliquer le premier après
  avoir conservé l'ordre du second en mémoire invitée.
- `PROUVÉ` : le test asymétrique `11,22,33,44` restitue `11,22,33,FF` après
  resolve puis présentation; la construction codegen-on réussit et son test
  ciblé passe.
- `PROUVÉ VISUELLEMENT` : après correction, la capture 1280x720 est rouge,
  avec le logo Bandai Namco partiel (« Games » et trois lobes) plus sombre.
- `RÉFUTÉ` : l'écran bleu était la couleur réellement produite en amont. Le
  passage bleu→rouge est exactement l'effet attendu de la correction R/B.
- `RÉFUTÉ VISUELLEMENT` : le frontend attendu est atteint; les quatre coins
  sont rouges au lieu d'être blancs et seuls les octets R sont non nuls.
- `PROUVÉ` : les draws répétés sont un quad plein écran en couleur vertex
  `0xFFFF0000`, puis un quad 512x512 en `0xBFFF0000`; le pixel shader module la
  texture par cet interpolateur.
- `INCONNU` : interprétation correcte de ces mots `FMT_8_8_8_8` sous les
  réglages fetch/endian invités, ou état de composition manquant qui doit
  produire le fond blanc.

Preuve :
`artifacts/goal-playable/title-present-swizzle-20260824/RESULT.md`.

# Preuve précédente — cycle 1810 (24 août 2026)

- `PROUVÉ` : l'unique appel invité de `VdIsHSIOTrainingSucceeded` inverse son
  résultat dans `0x827AD310`; la valeur non nulle sélectionne ensuite le
  chemin logiciel `0x821BA130`.
- `PROUVÉ` : retourner 1 restaure 502 soumissions, 4405 dwords, 409
  notifications et 407 présentations renderer à 1160 ticks.
- `RÉFUTÉ` : le retour 0 est une émulation correcte du succès HSIO ou une
  réparation valide de Startup→Title.
- `RÉFUTÉ VISUELLEMENT` : « non noir » suffit à valider le frontend. Le logo
  complet est bleu sur bleu; l'oracle exige un fond blanc et R=G=0 partout.
- `INCONNU` : premier défaut exact entre le fetch couleur vertex
  `FMT_8_8_8_8` et l'interpolateur couleur du pixel shader.

Preuve :
`artifacts/goal-playable/hsio-static-gate-20260824/RESULT.md`.

# Preuve précédente — cycle 1809 (24 août 2026)

- `PROUVÉ` : l'ancien build positif effectue 53 appels `NtReadFile` qualifiés
  avant 1160 ticks; les tuples complets sont conservés dans `old.stdout`.
- `PROUVÉ` : il produit 502 soumissions et 407 présentations.
- `RÉFUTÉ VISUELLEMENT` : son readback 1280x720 est entièrement noir.
- `NON QUALIFIÉ` : le run dit courant a ciblé une configuration codegen-off,
  a refusé le probe à tick 0 et ne peut participer à l'A/B.
- `INCONNU` : premier tuple `NtReadFile` divergent du vrai build courant.

Preuve :
`artifacts/goal-playable/ntread-first-divergence-20260824/BLOCKER.md`.

# Preuve précédente — cycle 1808 (24 août 2026)

- `PROUVÉ` : `0x821A61EC` charge `*( *(0x823C2D2C) + 0x10 )` et l'appelle par
  `bctrl`; son LR est `0x821A61F0`.
- `PROUVÉ` : `Function_821A6168` traite `STATUS_PENDING` (`0x103`) et peut
  attendre le handle retourné par `FUN_82327108` avec
  `NtWaitForSingleObjectEx`.
- `PROUVÉ` : le dispatch natif courant `NtReadFile` écrit les bytes et l'IOSB,
  puis appelle immédiatement `publish_guest_event` pour l'événement fourni.
- `PROUVÉ` : l'ancien binaire positif appelle lui aussi le même helper depuis
  sa branche `NtReadFile` (`0x027A3F0B -> 0x0278BAC0`) et implémente le même
  transfert du waiter auto-reset.
- `RÉFUTÉ` : appel direct à `NtSetEvent` au site `0x821A61EC`.
- `RÉFUTÉ` : différence locale « publication présente contre absente » entre
  les deux implémentations `NtReadFile`.
- `INCONNU` : premier état d'entrée `NtReadFile` divergent entre les builds.

Preuve :
`artifacts/goal-playable/event-site-821a6168-static-20260824/RESULT.md`.

# Preuve précédente — cycle 1807 (24 août 2026)

- `PROUVÉ` : le courant publie seize événements supplémentaires
  `0xE0000088…0xE00000C8` au LR `0x821A61F0`; l'ancien ne les publie pas.
- `PROUVÉ` : le courant termine avec 23 threads bloqués et le principal sur
  `0xE000004C`; l'ancien garde un thread runnable.
- `PROUVÉ` : `0x821A61F0` appartient à `Function_821A6168` dans l'atlas démo
  qualifié.
- `INCONNU` : cible et arguments exacts de l'appel à `0x821A61EC`; le
  pseudocode manque à l'atlas persistant.

Preuve :
`artifacts/goal-playable/first-guest-divergence-20260824/BLOCKER.md`.

# Preuve précédente — cycle 1806 (24 août 2026)

- `PROUVÉ` : le binaire codegen-on du 22 août produit 502 soumissions, 4405
  dwords et 407 présentations à 1160 ticks; le courant en produit zéro.
- `RÉFUTÉ` : disparition du hook `AC6_PPC_STORE_U32` pour `0x7FC80714`; le
  test et l'appel MMIO existent dans les deux désassemblages.
- `RÉFUTÉ VISUELLEMENT` : le readback ancien est uniformément noir malgré les
  présentations; il ne valide pas le frontend.
- `INCONNU` : première divergence native en amont qui conduit l'ancien guest à
  409 notifications et le courant à 1052 sans publication ring.

Preuve :
`artifacts/goal-playable/pm4-old-codegen-on-b-1160-20260824/RESULT.md`.

# Preuve précédente — cycle 1805 (24 août 2026)

- `PROUVÉ` : avec le binaire courant et la borne historique de 1160 ticks, le
  ring reste à `RPTR=WPTR=0`, zéro soumission et zéro dword.
- `PROUVÉ` : 1052 `VdSwap` et 24 draws typés existent, mais zéro présentation
  renderer et aucun readback.
- `RÉFUTÉ` : reset tardif après le tick 1160 expliquant les compteurs nuls des
  runs à 3002 ticks.
- `PROUVÉ` : régression native entre le binaire positif du 23 août et le
  binaire courant reconstruit le 24.

Preuve :
`artifacts/goal-playable/pm4-epoch-1160-current-20260824/RESULT.md`.

# Preuve précédente — cycle 1804 (24 août 2026)

- `PROUVÉ` : neutre et START donnent chacun 3002 ticks, 2894 `VdSwap`, 24
  draws typés, zéro soumission ring, zéro dword et zéro présentation renderer.
- `RÉFUTÉ` : START ou le changement de frame SWG cause la disparition PM4.
- `VALIDÉ VISUELLEMENT` : le contrôle positif historique montre un logo Namco
  centré non noir en 1280×720.
- `PROUVÉ` : aucun readback n'est produit par les deux runs courants ; une
  absence de capture n'est pas une validation visuelle.
- `PROUVÉ AU CYCLE 1805` : régression native ring/MMIO entre le run positif du
  23 août et le binaire reconstruit le 24.

Preuve :
`artifacts/goal-playable/post-start-pm4-ab-20260824/RESULT.md`.

# Preuve précédente — cycle 1803 (24 août 2026)

- `PROUVÉ` : type 1 sélectionne le mot `0x821187A8` à `0x82009E9C`.
- `PROUVÉ` : `0x822DA568` place le callback à `node+0x08` et Q à
  `node+0x0C`.
- `PROUVÉ` : `0x822E35E8` appelle ce callback avec Q inchangé ; Q
  `0x8270F598` rejoint donc `0x821187A8`, puis `0x821B4D80`.
- `RÉFUTÉ` : `0x8232710C` ne calcule pas le callback ; c'est
  `__savegprlr_29`.
- `RÉFUTÉ` : `acc302…` qualifie le retail PAL, pas la démo PAL.
- `INCONNU` : premier événement post-`0x821B4D80` présent dans Edge et absent
  du natif avant le kickoff GPU.

Preuve :
`artifacts/goal-playable/q-to-callback-static-20260824/RESULT.md`.

# Preuve précédente — cycle 1802 (24 août 2026)

- `PROUVÉ` : owner `0x8281EAF0`, compte 5, tête `0x827B3A80` au tick 3001.
- `PROUVÉ` : ce record porte la ressource `0x0E000071`, le type 1 et
  `record+0x14=1`.
- `PROUVÉ` : `0x82118FA0(record) -> 0x821185A8(Q=0x8270F598, record) ->
  0x821186B0(Q)`.
- `PROUVÉ` : readiness `[0x826F61B8]=0x2E8B8C20`, donc non nulle.
- `PROUVÉ` : le ring final conserve `RPTR=WPTR=0`, zéro soumission et zéro
  dword malgré 24 draws typés.
- `CANDIDAT NON JOINT` : le worker thread 14 exécute
  `0x821187A8 -> 0x821B4D80`, mais avant l'ingestion du lot courant.
- `VALIDÉ VISUELLEMENT` : 15 PNG 1280×720 strictement noirs ; inspection
  humaine de `capture-014.png`.
- `INCONNU` : champ/token transférant Q `0x8270F598` au record P du callback.

Preuve :
`artifacts/goal-playable/title-record-consumer-runtime-20260824/RESULT.md`.

# Preuve précédente — cycle 1801 (24 août 2026)

- `PROUVÉ` : `draw_index=0x12` lit le handle `0x0E000071` et le descripteur
  `0x2DCB13DC` (`byte+8=0`).
- `PROUVÉ` : les seize records `list13` passent
  `0x820EB200 -> 0x820EA9A0 -> 0x82095DF0 -> 0x821DEED8` au tick 3001.
- `PROUVÉ` : `0x82095DF0` reçoit l'owner `0x8281EAF0` et le payload
  `0x7F040548`; `0x821DEED8` crée un record de `0x70` octets.
- `RÉFUTÉ` : index `0x12` hors table, handle sentinelle ou rejet avant la
  création du record.
- `VALIDÉ VISUELLEMENT` : 21 PNG 1280×720, une couleur, moyenne 0 ; inspection
  humaine de `capture-020.png` : noir uniforme.
- `INCONNU` : première condition entre le consumer du record et l'émission
  PM4/Xenos ; `present_count=0` demeure.

Preuve :
`artifacts/goal-playable/title-draw18-record-runtime-20260824/RESULT.md`.

# Preuve précédente — cycle 1800 (24 août 2026)

- `PROUVÉ` : `table[13]={type=0, offset=0x1998}`, liste
  `0x2DCB2BB8`, compte 16.
- `PROUVÉ` : les records sont type 0, portent `draw_index=0x12` et rejoignent
  `0x82325E70`.
- `PROUVÉ` : l'appel virtuel au LR `0x82325ED4` utilise le slot 7 de la vtable
  `0x82007D0C` et cible `0x820EB200`.
- `PROUVÉ` : 24 draws typés, mais `present_count=0`.
- `INCONNU` : entrée ABI 0x12, handle/queue exact et producteur présentable.
- `NON VALIDÉ VISUELLEMENT` : aucune screencap ni PPM n'a été produit.

Preuve :
`artifacts/goal-playable/title-list13-runtime-20260824/RESULT.md`.

# Preuve précédente — cycle 1798 (24 août 2026)

- `PROUVÉ` : enfant `D=0x2DD7936C`, frame 0 unique type 5, index `0x42`.
- `PROUVÉ` : cet index crée `0x2E3F1350`, `D=0x2DD79358`.
- `PROUVÉ` : frame 0 du sous-enfant unique type 4, `list_index=13`.
- `RÉFUTÉ` : callback VM direct depuis cette branche au tick 3001.
- `INCONNU` : record de liste 13 et premier consumer renderer persistant.

Preuve :
`artifacts/goal-playable/title-grandchild-frame-runtime-20260824/RESULT.md`.

# Preuve précédente — cycle 1797 (24 août 2026)

- `PROUVÉ` : factory enfant `0x2E3F8C50`, parent `0x2E3EDA90`, au tick 3001.
- `PROUVÉ` : les seuls `execute_raw` du tick appartiennent au parent ; aucun à
  l'enfant.
- `PROUVÉ` : construction de `0x2E3F1350`, parenté à `0x2E3F8C50`, dans la
  même séquence.
- `INCONNU` : descripteur/frame initiale du sous-enfant et premier effet hors
  hiérarchie.

Preuve :
`artifacts/goal-playable/title-child-first-effect-runtime-reuse-20260824/RESULT.md`.

# Preuves durables

## Jointure matériau démo → descripteur NSXR → VS/PS (24 août 2026)

`Function_822E0950` enregistre les descripteurs NSXR sous leur mot `+0x00` dans
`0x8296BF80`; `Function_822E8668` interroge ce même registre avec
`material+0x00`. Le corpus donne `0x30000010` à 4 307 matériaux de 169 objets,
et le descripteur correspondant contient exactement `vsCstCT` puis `psCT`.
Le recensement reproductible couvre 170 objets, 4 312 descripteurs, 170 GIDX
et 1 903 descripteurs NSXR.

Contrôles négatifs : aucune des 170 valeurs `NU_HASH` n'est une clé NSXR ;
`0x30000090` est absent de la banque ; l'immédiat `0x821227C0` est un faux
pivot GPU. Reçus : `artifacts/shader-selection-xrefs.txt`.

## Promotion pending et premier tick enfant — cycle 1795 (24 août 2026)

Dans `0x82323BB8`, le début du tick copie `controller+0xE4` vers
`controller+0xF4` et vide la liste pending. Après les handlers de frame, la
fonction parcourt la nouvelle liste `+0xE4` et s'appelle récursivement sur
chaque enfant. L'enfant construit dans le tick reçoit ainsi son premier update
immédiatement ; sa promotion dans le `+0xF4` du parent intervient au tick
parent suivant.

Preuves : `reports/cycle-1795-demo-moviecontroller-promotion-first-tick.md` et
`artifacts/goal-playable/title-child-promotion-static-20260824/RESULT.md`.

## Consumer enfant et publication active — cycle 1794 (24 août 2026)

Le projet Ghidra canonique qualifie `0x82323468..0x823235CF` comme le corps du
callsite enfant. Le résultat construit ou réutilisé est enregistré puis
inséré dans `A+0xE4/A+0xE8`; aucune publication de ce résultat dans `A+0xF4`
n'existe dans le corps. La seule écriture à `A+0xF4` retire un ancien candidat
actif correspondant.

La chaîne structurelle appelle `0x82323BB8(A+0xF4)` pour mettre à jour le
`swg::MovieController`, produire/consommer les offsets de `MovieMemory` et
les remettre à la VM. La prochaine preuve doit donc qualifier la promotion
pending→active, sans confondre ce contrôleur UI scripté avec la vidéo.

Preuves : `reports/cycle-1794-demo-child-controller-consumer.md` et
`artifacts/goal-playable/title-child-consumer-static-20260824/RESULT.md`.

## MovieController enfant post-START — cycle 1793 (23 août 2026)

Une trace observation-only relie, au tick 3001 sous le vrai Title, le
callsite enfant `0x8232356C` à la factory virtuelle `0x820D18C8`, puis à
l'initialiseur `0x82323808`. Le résultat est un `MovieController` de vtable
`0x820304D8`; `A`, `B=A+8`, `D`, `s=-1` et `MovieMemory` concordent avec la
jointure statique. L'hypothèse d'absence de reconstruction post-START est
réfutée.

La portée reste bornée au retour de la factory enfant : aucune publication
racine, transition `EndMode`, notification du listener Title ou écriture
`manager+0x18` n'est prouvée. Le backend headless ne qualifie pas de frame
frontend. `frontend=false`, `mission=false`, `terminal=false` et
`supported=false` restent donc autoritaires.

Preuves :
`reports/cycle-1793-demo-post-start-child-moviecontroller.md`,
`artifacts/goal-playable/swg-acc-caller-join-static-20260823/RESULT.md` et
`artifacts/goal-playable/title-post-start-moviecontroller-join-runtime-20260823/RESULT.md`.

## START — correction SWG, descripteur et consommateur typé (23 août 2026)

Le cadrage antérieur `M102=0 / lookup 0x0B -> EndMode` est réfuté. Le lookup
retourne `NULL` sans effet de scheduling, M102 retourne un entier par des
listeners stubs, et les deux items étaient déjà indépendants dans
`MovieMemory`. Le vrai handler type 6 est `0x82322A80`; après START, la plage
de frames est republiée par le chemin d'initialisation du descripteur et la
frame mono-entrée atteinte est de type 4. Elle ne fait que sélectionner une
liste et une matrice, puis rendre par
`0x82326608 -> 0x82326420 -> 0x82325E70 -> 0x820EB200`. Elle ne contient ni
`MovieMemory::Add`, ni `EndMode`, ni accès au mode manager.

L'analyse exhaustive des douze descripteurs `brandLogo` couvre 2 351 frames
et onze éléments type 6. Aucun payload brut n'est littéralement `0xE04` ; ce
constat ne ferme pas encore une relocation vers l'offset `EndMode` du buffer
heap décompacté. La frontière statique exacte est donc la carte SWG brut vers
heap/descripteur, le producteur des arguments storage/descripteur republiés à
START, et l'état persistant de `CModeTaskTitleDemoOffline` susceptible de
déclencher une transition indépendante.

La taxonomie aval est désormais prouvée : `MovieController+0x0c` désigne un
agrégat SWG dont `+0xe0` est un `CSwgRenderer*`; la vtable globale
`0x82007D0C` place `0x820EB200` au slot 7. `0x820EB200` est donc une méthode
virtuelle d'instance qui transforme matrice et record SWG en record CPU de
layout `0x70`, ensuite sérialisé et mis en file. `0x82325E70` reste un
callback de table globale, pas une méthode prouvée ; la même règle conservatrice
s'applique aux handlers sélectionnés seulement par `0x8264CE6C` et
`0x8264D07C`. Aucun write PM4 direct n'est établi à ce niveau. La taxonomie
machine-readable validée est
`analysis/demo/ac6-demo-structural-types-v1.json` (SHA-256
`00867bfe00c6435244d3a250091e67a719dd500ae805e9dbc8594326cbeba76c`).

Preuves :
`artifacts/goal-playable/title-swg-post-start-static-correction-20260823/RESULT.md`,
`artifacts/goal-playable/title-frame-descriptor-static-20260823/RESULT.md`,
`artifacts/goal-playable/title-parallel-matrix-consumer-static-20260823/RESULT.md`
et `reports/cycle-1788-demo-title-swg-frame-switch-correction.md`.

## Logo naturel et `CTaskModeManager` — gates fermés (23 août 2026)

Deux exécutions froides naturelles produisent la même trace, les mêmes
readbacks et le même logo Namco 1280x720 reconnaissable après la chaîne
`draw_index=2 -> handle 0x0E000059 -> Q1 guest-owned -> fragments -> resolve`.
La preuve reproductible est conservée dans
`artifacts/goal-playable/title-vertex-window-runtime-20260823/` et son replay
`title-vertex-window-runtime-20260823-r2/`.

Le batch read-only du projet Ghidra `ace-combat-6-demo`, qualifié par le XEX
`de917...da8`, corrige ensuite la taxonomie du mode manager. Le constructeur
`0x821908B8` écrit la vtable `0x82011694` dans son receiver puis publie ce même
receiver dans `0x827435F8`; la cellule est donc `CTaskModeManager*`.
`0x82190B18` conserve `r3` comme `this` et, à
`0x82190EC0..0x82190EF0`, range le retour de factory en `this+0x08`, l'insère
par le service global de liste, puis initialise virtuellement le nouvel objet.
Preuve et classification complète :
`artifacts/goal-playable/frontend-mode-manager-canonical-ghidra-20260823/RESULT.md`.

Cette preuve est démo-only pour `CModeTaskGameDemoOffline`. Les prototypes
retail Preview ne sont ni une source positive ni une source négative pour
cette classe. Le prompt visible START appartient à une époque postérieure au
logo. La chaîne `menu_endMode -> Title slot48 -> manager+0x18` est qualifiée
seulement si `menu_endMode` est invoqué ; elle n'est pas une causalité START
naturelle prouvée. L'origine guest effective de la transition reste ouverte.

Les sections anciennes qui parlent encore d'un logo invisible ou d'une
factory non publiée sont conservées comme historique et supersédées.

## BrandLogo — sélecteur de liste borné (23 août 2026)

Sur 32 itérations naturelles de la fenêtre titre, l'owner SWG imbriqué garde
une frame valide d'une entrée, `element+8=0`, `list_count=1`, puis le record
`0x2DF0159C` avec `record+0x0c=0` et le slot `0x57`. Le record atteint bien
`0x820EB200`; aucun record `draw2/0x59` n'est sélectionné. En parallèle,
l'owner parent progresse jusqu'à la frame 31. Le contrat absent est donc le
producteur de la frame/liste imbriquée, pas un état Vulkan ni un flush PM4.
Preuve : `artifacts/goal-playable/brandlogo-draw2-selector-runtime-20260823/`.

## Renderer titre et mapping NTXR naturel (23 août 2026)

La qualification statique de l'état titre donne un blend couleur
source-alpha / one-minus-source-alpha. Son implémentation Vulkan est validée
par le build et six tests ciblés; un replay naturel de 400 ticks atteint 249
soumissions ring, 64 draws typés, 154 presents et 66 captures. Les readbacks
évoluent mais les captures restent uniformes, ce qui exclut le blend comme
cause restante du logo absent.

Une trace bornée et observation-only joint le FHM imbriqué aux handles
`0x57`, `0x58` et `0x59`. Seul `0x59` désigne `003_NTXR`, le wordmark Namco
1280x720. Les draws observés n'ont pas encore sélectionné ce handle. Preuves :
`artifacts/goal-playable/title-blend-runtime-20260822/` et
`artifacts/goal-playable/brandlogo-resource-entry-runtime-20260822/`.

## BrandLogo — worker et constructeur guest atteints (22 août 2026)

Une sonde native unique de 300 ticks observe à partir du tick 206 la ressource
globale non nulle, le record type 1 de `0x8281EAF0`, sa sérialisation, puis le
worker `0x821187A8` et le constructeur de commandes `0x821B4D80`. Le rapport
conserve zéro soumission ring, zéro paquet PM4 et seulement les 24 draws
bootstrap d'un index. Preuves :
`artifacts/goal-playable/brandlogo-consumer-runtime-20260822/RESULT.md`,
`consumer.log`, `consumer.trace` et `consumer.report.json`.

## BrandLogo — publication naturelle des records géométriques (22 août 2026)

Une unique sonde ciblée de 3021 ticks observe 64 passages bornés dans chacun
des quatre points `0x820EB200`, `0x820EA9A0`, `0x82095DF0` et `0x821DEED8`.
Les draws 0 et 1 ont des slots ressource non-sentinel et une géométrie non
nulle; leurs records sont naturellement liés à `0x8281EAF0`. Le rapport
reste à zéro soumission ring et 24 draws bootstrap d'un seul index. Preuves
complètes : `artifacts/goal-playable/acc-resource-runtime-20260822/RESULT.md`,
`guard-runtime.log`, `guard-runtime.trace` et `guard-runtime.report.json`.

## Gate SWG — callback atteint, état logique non armé (22 août 2026)

La sonde native `artifacts/goal-playable/swg-callback-arm-20260822/probe/`
appelle naturellement `0x820CDF30` sur la vtable qualifiée `0x820061FC`;
`this+8=1`, `this+9=0` et `0x826DFC48=0`. Les lectures bornées de
`base+0x23980`, `+0x23988`, `+0x2398C` et `+0x23990` restent nulles; ce sont
des bitsets d'état logique/action, pas une table allouée de ressources. Aucun
draw normal n'est soumis et le frontbuffer reste noir à 3000 ticks. Le VFS
initial (`vfs2/`) ouvre et lit `DATA.TBL` et `DATA00.PAC`, mais ne suffit pas à
qualifier le chargement SWG dans cette fenêtre.

Le bord causal « callback jamais appelé » est donc fermé; les écrivains
qualifiés des bitsets sont `0x821DE990` et `0x821DE6E0`, tandis que l'état
SWG/script ou le consommateur de la garde globale reste ouvert. Aucun patch de
bits, callback, record ou pixel n'est justifié. Preuves complètes :
`artifacts/goal-playable/swg-callback-arm-20260822/RESULT.md` et `BLOCKER.md`.

## Gate entrée de jeu — publication `CModeTaskGameDemoOffline` absente (22 août 2026)

Le gate statique `task-loading-publication` confirme que la terminaison de
`CTaskLoading`, le dispatcher de listes et `CTaskModeManager` ne publient pas
de tâche de jeu : `0x8218CE20`/`0x8218CCD0` pollent seulement, `0x82259D10`
met à jour les nœuds existants, `0x82259E18` insère un objet déjà construit,
et `0x821929A8` ne fait qu'avancer ses champs de requête. Aucun producteur
qualifié de `CModeTaskGameDemoOffline` ni import partagé manquant n'est établi.
La factory amont reste le prochain bord causal. Voir
`artifacts/goal-playable/task-loading-publication/RESULT.md` et `BLOCKER.md`.

Le follow-up owner/r8 ferme la provenance AVI : `0x8200B6FC/+0x04` cible
`0x82165CC0`, mais aucun constructeur ne publie `base+0xC` comme receiver et
aucun producteur de `r8` n'est qualifié. Le writer type 1 reste inaccessible;
aucun record ou draw synthétique n'est permis. Voir
`artifacts/goal-playable/owner-r8-followup/RESULT.md` et `BLOCKER.md`.

La passe statique finale confirme que `0x8217C678` est le slot `+0x0C`
commun aux `CModeTaskGame*` et enregistre le callback
`0x8217C4D8`; elle ne prouve toutefois aucune publication de
`CModeTaskGameDemoOffline`. Le thunk START `0x820D32D0` (`vtable+112`) reste
dépendant de l'objet ActionScript et ne peut pas être remplacé. Provider,
listener et réveil `E000004C` sont déjà qualifiés. Aucun import ou pont
partagé n'est donc corrigeable sans forcer l'état guest. Preuves :
`artifacts/goal-playable/game-entry-static/RESULT.md` et `BLOCKER.md`.

## Sonde courante — aucune production de draw (22 août 2026)

Le run borné `artifacts/goal-playable/runtime-current-20260822/` s'est
terminé sur son timeout à 1622 ticks. Il n'a émis aucun record type 1–4,
aucun événement AVI et aucun PM4; les traces ne montrent que les pointeurs de
contrôle de queue et des slots nuls. Les audits statiques ACC/AVI confirment
qu'aucun constructeur ou provider qualifié ne relie encore `brandLogo` à
`0x8219E580`, ni le receiver AVI à `0x82165CC0`.

## Dernière tranche — jonction logo→ACC toujours absente (22 août 2026)

Le slice final qualifié (`ghidra-projects/ace-combat-6-demo`) ferme
`0x8218BFB0` comme sélecteur de providers et `0x8219BFB0` comme simple load
dans `0x8219BF40`; ni index `170/171`, ni `brandLogo`, ni `texture_id 2` ne
traverse vers `source+0x20`/`0x8219E580`. Le PNG décodé prouve le payload
disque Namco, pas son affichage natif. Voir
`artifacts/goal-playable/acc-guest-join-next/RESULT-final.md`.

## Gate logo Namco — payload disque identifié, jointure ACC ouverte (22 août 2026)

Le premier événement de logo est un cue/film SWG (`0x820E8F90 →
0x820EA4A8 → 0x821728C0`). La passe statique du contenu qualifie maintenant
`DATA.TBL[170]` et `[171]` comme `FHM → FHM → SWG "brandLogo"` de 118244
octets, accompagnés de huit NTXR portant `GIDX=0x08000000`. Le tableau de
textures du SWG donne le mapping exact `texture_id 2 → 003_NTXR.ntxr` : le
payload décodé est le mot-symbole rouge `namco®` sur fond blanc. Le nom est
donc rattaché à des octets démo précis. `sub_8219E580` consomme toutefois un
bloc ACC guest déjà construit et produit seulement des handles dynamiques
`0x0E...`; leur jointure avec ce SWG/NTXR reste ouverte.

La chaîne renderer compatible est déjà présente (`nuTexture` puis SWG draw,
RT0/resolve/writeback), mais aucun draw non-bootstrap n'est qualifié et aucun
readback non noir n'existe. La provenance immédiate est désormais bornée :
`0x821A02C0 → 0x8219E768` alimente la source consommée par `0x8219E580`, et
la branche ressource `0x821A0180 → 0x8219F080/0x8219F1C0 → 0x8219E428` écrit
son bloc ACC. La jointure entre ces objets guest et PAC/FHM/DATA.TBL reste
non démontrée. Décision : pas de patch renderer ni d'asset synthétique.
Prochaine preuve attendue : mapping `handle → DATA.TBL/FHM/NTXR/GIDX` ou
octets movie/SWG jusqu'au premier draw non-bootstrap.
Preuves : `artifacts/goal-playable/namco-logo-route/RESULT.md`,
`artifacts/goal-playable/swg-payload-static/RESULT.md`,
`artifacts/goal-playable/brandlogo-swg-refs/RESULT.md` et
`artifacts/goal-playable/brandlogo-loader-join/RESULT.md`.

La tranche finale du callback de démarrage réfute une jonction ACC implicite :
`0x821728C0` ne fait que déléguer à son owner et écrire l'état `2`. Les
consommateurs ACC sont des voies virtuelles distinctes (`0x8219E580` via
`CResourceLoaderSwg`, `0x82127D40` via `CSelectAircraftSetupManager`) et aucun
producteur statique ne les relie au SWG/NTXR `brandLogo`. Le gate reste donc
bloqué sur le constructeur/registre ACC concret; aucune image native n'est
revendiquée. Voir `artifacts/goal-playable/acc-guest-join-next/RESULT-followup.md`.

La passe DPL qualifie les producteurs des clés `0xCB..0xCF` et leur
normalisation en `DPL::[cb..cf,0]`. Ils alimentent `0x821A02C0 → 0x8219E768`
mais aucun xref ne les joint au bloc `0x8219E428`, à un chargeur
PAC/FHM/DATA.TBL ou aux octets SWG. Cette branche est fermée négativement ;
le prochain `done_when` reste le producteur ACC réellement consommé par
`0x8219E580`, puis le premier record de draw non-bootstrap.
Preuve : `artifacts/goal-playable/logo-key-registry/RESULT.md`.

## Gate courant — owner/r8 et activation des records

La preuve la plus récente est limitée mais causale : les constructeurs ne
publient pas l'AVI receiver `base+0xC`; le type 1 atteint la queue seulement
avec cinq ressources non nulles; et le worker lit `0x82386C58`/slot `+0x08`
sans cible statiquement résolue. La trace native bornée START voit uniquement
`0x820FEFA8` avec `record_type=0`, zéro paquet soumis/décodé et aucun passage
par le selector/AVI corridor. Voir `artifacts/owner-r8-static/RESULT.md`,
`artifacts/owner-r8-static/BLOCKER.md` et
`artifacts/owner-r8-static/runtime-trace/RESULT.md`.

Le slice titre trouve les writers `0x820B3E80` et `0x82321E18` qui écrivent
`+0xE8`, mais leur identité d'objet n'est pas jointe à l'owner
`0x820D29E0`/AVI; cette preuve réduit la recherche sans autoriser de shim.

La passe AVI complémentaire qualifie les deux constructeurs qui écrivent la
vtable `0x8200B6FC` à `objet+0xC`, mais ne trouve aucune publication de
`base+0xC` vers `0x82165CC0`; `0x82374AD0` est classifié comme initialiseur
de la base `0x82731A30`. La trace bornée `owner-window-300` (stores 220:223) reste
négative: 192 PRESENT, zéro ring/PM4, frontend et mission faux.

La passe provider sépare les tables sans les confondre : `0x82165AF8` lit
`DAT_826F6188`; `0x821080D0` transforme cinq clés de payload en IDs qui lisent
`DAT_826F6124`; `FUN_821ee130` fournit seulement `record+0x108`. La population
des entrées de `DAT_826F6188` et l'égalité des clés restent ouvertes, sans
autoriser une activation synthétique. Voir
`artifacts/record-activation-static/agent-writers/RESULT-resources-provider-followup.md`.

Les deux configurations restent saines : codegen-ON `26/26`, codegen-OFF
`27/27` avec le test négatif guest CLI, puis installation sans `bin/bin`.
Cette validation ne transforme pas l'oracle Xenia en dépendance et ne prouve
pas encore un titre/menu visible.

## Gate statique limité — portée de la watch IB

Le code de `GuestMemory` borne `AC6_DEMO_WATCH_IB_WRITERS` à l'IB principal
`0x1274…`; il ne surveille jamais le lot bootstrap `0x16AE…`. L'absence de PC
dans les receipts antérieurs n'est donc pas une preuve d'absence de writer.
Voir `artifacts/ib-bootstrap-producer-static-gate/BLOCKER.md`.

## Gate limité — bootstrap `PointList`

L'inventaire neutre précoce borne 24 draws dans `0x16AE0980`; leur chaîne IB
écrit un masque couleur nul et la sonde ne relève aucun payload vertex. Ils ne
sont pas un writer RT0 qualifiable. L'effet interne Xenos reste indécidable.
Voir `artifacts/rt0-first-nonblack-writer-static-gate/BLOCKER.md`.

## Gate fermé — transport RT0 vers résolution

L'appel normal conserve une lecture RGBA qui est passée à la résolution avec
la commande de copie et le PRESENT. Le test de sélection, le harness EDRAM,
la compilation et les tests configurés passent. Cette preuve porte sur le
transport de buffer, pas sur son contenu. Voir
`artifacts/rt0-readback-contract-gate/decision.md`.

## Gate fermé — conservation des commandes RT0

Le producteur de commandes qualifiées alimente les deux options consommées par
le chemin de draw normal puis copie. Un test de contrat vérifie que les deux
affectations précèdent la déduplication des pipelines, et la vérification
native compile puis exécute tous les tests configurés. Voir
`artifacts/rt0-draw-selection-implementation-gate/decision.md`.

## Gate limité — vtable du slot d'installation

`0x820D29E0` est l'entrée `+0x0C` de `0x820064D8`. Les deux constructeurs
statiques qui écrivent cette vtable n'écrivent pas `owner + 0xE8`; l'appelant
direct de l'un d'eux est encore indirect. Cette chaîne ne peut donc pas
attribuer l'instance owner. Voir
`artifacts/title-swg-owner-static-gate/BLOCKER.md`.

## Gate limité — délégation de l'installation du contexte

Le CFG de `FUN_82324188` écrit le pointeur conditionnel `owner + 0xE8` dans
le champ `+0x0C` de l'interpréteur et appelle son slot `+4`. Son unique
appelant `Function_820D29E0` transmet son propre second argument. Cette chaîne
borne l'origine de la vtable en amont de l'owner. Voir
`artifacts/title-swg-context-vtable-static-gate/BLOCKER.md`.

## Gate fermé — thunks des handlers SWG post-START

Quatre entrées de la table font un tail-dispatch virtuel après avoir chargé
`context + 0x0C`; leurs offsets sont `+0x80`, `+0xC8`, `+0x64` et `+0xA8`.
`0x823248C8` est le seul handler concret : il lit un mot puis appelle
`+0xCC`. Les CFG ne montrent aucun accès direct au sélecteur global. Voir
`artifacts/title-swg-poststart-handlers-static-gate/decision.md`.

## Gate fermé — indexation de la table SWG

Le CFG de `Function_82325160` lit un mot, avance `context + 0x14`, et fait un
appel indirect par `DAT_8264CEA8 + mot * 4`. Les xrefs du sélecteur global et
du setter mission n'incluent pas ce consommateur. Cela attribue les cibles
post-START au flux, non à un producteur direct de sélection. Voir
`artifacts/title-swg-table-consumer-static-gate/decision.md`.

## Gate limité — références statiques des cibles SWG post-START

Les cinq entrées `0x8232xxxx` ont une seule référence, toutes dans la table
`0x8264CED8..0x8264CFB8`; les quatre entrées `0x820Dxxxx` ont deux références
dans des tables de données. Six entrées n'ont pas de fonction reconnue. Cette
preuve borne le dispatch de table, pas sa sémantique. Voir
`artifacts/title-swg-poststart-static-gate/BLOCKER.md`.

## Gate fermé — divergence virtuelle SWG post-START

Une exécution native unique avec START au tick 3000 compare directement les
appels relevés avant et après l'effet. Le site `0x823251B8` acquiert neuf
cibles exclusives au tick 3001, dont `0x82324548`, `0x82324440` et
`0x820D7768`. Les trois sites instrumentés ne relèvent pas `0x8218AB98`
pendant les ticks 3000–3035. Les données et le périmètre sont dans
`artifacts/title-swg-virtual-target-runtime-gate/`.

## Gate fermé — frontière native du dispatcher SWG

Dans `ace-combat-6-demo` / `Default.xex`, le dispatcher `0x820EA238` atteint
la chaîne neutre `0x8217C890 → 0x8218AB98`. Les CFG et xrefs disponibles ne
montrent aucun appel direct vers `0x82171988`. Le choix dynamique demeure dans
le film; les critères de la capture bornée sont dans
`artifacts/title-dispatcher-selection-static-gate/decision.md`.

## Gate limité — collision de déplacement `+0x70`

La chaîne de vtable relie `0x8218AB98` à `CModeTaskTitleDemoOffline`, et son
corps ne montre qu'un store dans son paramètre objet. Les xrefs du setter
mission du projet démo montrent séparément le pointeur global de sélection.
Les deux observations ne fournissent aucune arête de données ou d'appel entre
les objets. Voir `artifacts/title-singleton-constructor-static-gate/`.

## Gate fermé — vocabulaire compact du loader

Le CFG de `0x82278F78` montre un bit-reader avec tables de nœuds de 6 octets,
sortie de littéraux et copie arrière. Il explique le producteur des mots
`0x1A/0x2E/0x10` sans leur attribuer abusivement une sémantique d’opcode.
La source compacte demeure la frontière suivante.

## Gate fermé — décodage borné du flux titre

Le CFG PPC de `0x823246C0` fournit les longueurs des opcodes `0..7`. Le parser
local, alimenté uniquement par les écritures capturées du loader, valide les
23 `AC6_SWG_BOX_CALL` et atteint sa première frontière au mot
`0x2DCB2448 = 0x1A`. Les rapports détaillés sont dans
`artifacts/demo-title-bytecode-parser-gate/`.

## Gate fermé — chargeur de flux relié au contexte titre

La capture bornée confirme le lien dynamique : `0x82278F78` écrit 8 192 octets
sur `0x2DCB2000..0x2DCB3FFF` au tick 2435, puis les 23 PC observés du contexte
titre au tick 3001 restent entre `0x2DCB2440` et `0x2DCB2680`. La destination
du chargeur est donc bien le flux interprété du titre, non un autre contexte.

## Gate fermé — frontière statique du branchement de film

`0x823246C0` a un PC et une borne de flux heap; `0x82278F78` décompacte une
entrée via son objet d'entrée. Les bindings de natives titre sont construits
en C++ et ne référencent pas le flux. Ces faits qualifient le mécanisme et
établissent la limite de l’analyse statique : le compacté n’est pas relié au
contexte titre par une référence binaire statique exploitable.

## Gate fermé — consommateurs statiques du triplet sélectionné

Dans `ace-combat-6-demo` / `Default.xex`, la table statique du titre associe
les commandes `GetCurrentMission` et `GetCurrentLevel` aux natives
`0x820EA550` et `0x820EA598`. Leurs CFG Ghidra appellent respectivement les
getters `0x82095B80` et `0x820E9290`; le niveau subit un remappage 6/7. Les
xrefs PPC directs des getters sont vides, ce qui borne le mécanisme d'appel
sans nier ces consommateurs script qualifiés.

## Gate fermé — écrivain direct de mission au START écarté dans la fenêtre

Le getter/setter démo qualifié indexe la mission depuis la base statique
`0x823C27E0 + 0x70`; pour l'index observé zéro, le slot est `0x823C2F14`.
Deux probes avec START au tick 3000 limitent l'observation aux ticks 3000–3004:
le slot et le niveau voisin restent constants, puis l'instrumentation de stores
sur quatre octets du slot mission ne produit aucun événement. Cette absence
réfute seulement l'atteinte d'un setter direct qui écrirait le slot sélectionné
dans cette fenêtre; faute de store, aucun PC ni argument `r4` n'est observable.

## Gate fermé — producteurs statiques de mission bornés

Les références directes à `0x82171988` bornent neuf appelants. Les CFG
échantillonnés montrent des écritures de mission à `1`, une progression avec
retour à zéro, des valeurs calculées et une valeur de table UI. La recherche
statique ne rattache pas une de ces voies au START ni un setter de niveau : ces
ambiguïtés restent des frontières runtime, non des sémantiques inférées.

## Gate fermé — bornes START dans le XEX démo

Le Ghidra démo en lecture seule confirme les entrées `0x820EA550`,
`0x82095B80`, `0x820E9290` et `0x82171988`. Le getter et setter mission
accèdent tous deux au champ indexé `+0x6C4`; le getter niveau lit `+0x6D0`.
La correspondance est désormais binaire-qualifiée et non dérivée du code
généré seul.

## Gate fermé — entrée du codegen démo qualifiée

Le cache du build codegen-on associe explicitement l'exécutable à
`demo-game-file/extracted/stfs-root/Default.xex` et au manifeste
`analysis/demo/ac6-demo-ghidra-manifest.json`. L'identité locale désigne le
projet Xenon séparé `ace-combat-6-demo`. Aucun fait observé par le runtime
codegen ne peut donc être appliqué au XEX retail.

## Gate fermé — réconciliation du corpus Ghidra canonique

Le manifeste de l'import canonique donne 10 708 fonctions. Les adresses
utilisées pour les handlers générés du demo tombent dans des fonctions PAL aux
bornes différentes et aux corps incompatibles. Le code généré peut donc rester
un indice de contrôle/ABI du demo seulement; il ne fonde aucune sémantique
statique pour le PAL canonique.

## Gate fermé — cible du setter de mission au START réfutée

Le probe codegen-on, avec START injecté au tick 3000 et observation seulement
des ticks 3000–3004, voit `gs=0x82774B00` et `gs+112=0x8201DFDC` à chaque tick.
Cette valeur est une table de méthodes déjà qualifiée statiquement. La plage
de store de l'ancien slot `0x82775234` reste muette : elle ne permet ni de
confirmer ni de nier l'exécution de `0x82171988`, car elle n'est pas le slot
actif. Le résultat est une réfutation de cible, pas une absence de setter.

## Gate fermé — valeurs SWG après START

La fenêtre START donne `GetCurrentMode = 0`, `GetCurrentMission = 0` et
`GetCurrentLevel = 2`. Les sources de ces valeurs sont statiquement les champs
de l'état de jeu et de son sous-objet de slot. Le monde SWG reste identique et
aucun `menu_endMode` ne suit. Les substitutions ciblées déjà observées ne
changent pas ce chemin; ces retours ne sont pas le déclencheur de transition.

## Gate fermé — sélecteur SWG post-START du titre

`0x820CE368` est l'unique écrivain de `DAT_826DFC44`; ses autres références
sont des wrappers ActionScript qui délèguent aux slots du monde SWG, dont
`0x820EA238` au slot `+0x1C`. Le neutre atteint `menu_endMode` puis
`0x8218AB98`; START exécute cinq natives synchrones et s'arrête. Aucun CFG
PPC ne décide entre ces branches : la décision reste dans le film ou ses
valeurs, non dans le renderer.

## Gate fermé — avancement neutre de l'attract title

Dans `ace-combat-6-demo` / `Default.xex`, `0x820EA238` appelle le slot SWG
global `+0x1C`; la fenêtre neutre le relie à `0x8217C890 → 0x8218AB98`.
`0x8218AB98` stocke son argument dans l'état titre `+0x70`; le traitement de
l'état 2 dans `0x8218A7A8` compte vers son slot `+0x4C` puis signale la
transition de tâche. Le chemin START n'atteint pas ce callback dans la fenêtre
existante : il supprime l'avancement neutre, sans preuve de draw ou RT0 nouveau.

## Gate fermé — consommateur statique du compteur de titre

Le scan de références Ghidra de `0x820D3AC8` retourne uniquement la case de
vtable `0x82006B0C`. Cette absence d'appel direct borne la provenance statique
du compteur `+0x0C`; combinée au burst SWG unique et à l'absence observée de
transition persistante, elle réfute d'en faire une progression renderer.

## Gate fermé — récepteur virtuel du START pendant le titre

La capture bornée au tick 3001 donne, pour les deux dispatches, vtable
`0x82006A9C` et slot `+0x70 = 0x820D3AC8`. Le CFG Ghidra de
`0x820D3AC8` appelle `param_2->vtable[+0x5C]` et soustrait son retour à
`param_1+0x0C`. Les observations SWG postérieures sont un burst unique : elles
ne démontrent aucune transition persistante de mode, frontend ou mission.

## Gate fermé — dispatch START pendant le titre

La capture existante atteint `0x820D32D0` un tick après START lorsque le mode
titre est présent; les essais plus tôt visaient le mode startup. Dans le projet
Ghidra `ace-combat-6-demo`, module `Default.xex`, les instructions
`0x820D32D0..0x820D32DC` effectuent `vtable(r3) → slot +0x70 → CTR → bctr`.
Les xrefs statiques ne fournissent qu'une table de thunks, pas sa cible à
l'exécution.

## Gate fermé — producteur PM4 du draw RT0

Dans le projet Ghidra canonique `ace-combat-6-demo`, module `Default.xex`, le
CFG de `0x821B55C0` construit la commande RT0 atteinte. Celui de `0x821B9BC8`
émet les descripteurs dans le ring et avance le WPTR Xenos. Aucun store CPU
vers EDRAM n'apparaît dans cette chaîne; elle ne qualifie donc aucun échantillon
non nul, seulement son unique frontière d'exécution GPU.

## Gate fermé — reflection varying du draw normal

Le geometry shader ne réémet que `SV_Position`. L'analyse qualifiée du vertex
shader atteint établit toutefois `interpolator0` et trois couleurs nulles,
ainsi qu'une entrée `color0` nulle au pixel shader. La perte d'interface est
réelle mais réfutée comme cause de la noirceur du rectangle normal atteint.

## Gate fermé — interface vertex du draw normal

`XenosDrawCommand` ne contient que primitive, indexation, identités shader et
snapshot de registres; son constructeur ne joint pas de mémoire guest. Le
backend lie toutefois en parallèle le descripteur shared-memory, dont la plage
qualifiée contient le fetch normal, avant `vkCmdDraw(3, 1, 0, 0)`. Le record
n'est donc pas l'unique voie d'entrée vertex; seul le sens
shader/format/lane des octets reste ouvert.

## Gate fermé — disposition d'installation native

L'installation CMake dans un préfixe temporaire crée
`bin/ac6-demo-recomp` et `share/ac6-demo-recomp/config`. Le contrôle
`test ! -e bin/bin` passe. Cela valide la surface d'installation, sans
exécution guest ni preuve de rendu visible.

## Gate fermé — intégration locale du bridge natif

Dans `recompilation/ace-combat-6-demo/build`, la compilation CMake est à jour
et CTest passe intégralement avec 27 tests sous `SDL_AUDIODRIVER=dummy`.
Les suites couvrent notamment core, graphiques, XAM, XAudio, Xenos, Vulkan et
le cycle de vie Xvfb. La couverture est locale et ne prouve pas le gameplay.

## Gate en pause — producteur de `VdGlobalDevice+0x4084`

Le CFG Ghidra de `0x821C64E8` appelle `0x821C6400`, écrit son `param_1` dans
l'import `VdGlobalDevice`, puis poursuit l'initialisation. Le CFG de
`0x821C6400` appelle `VdSetGraphicsInterruptCallback(0x821B9710, param_1)`.
Cela qualifie le producteur de l'objet et l'enregistrement, pas l'écrivain du
champ interne `+0x4084` contrôlé par l'ABI Vd externe.

## Gate fermé — interface du callback indirect graphique

Le décompilateur Ghidra du projet `ace-combat-6-demo`, module `Default.xex`,
montre que `0x821C5190` acquiert le spinlock de `VdGlobalDevice+0x4130`,
teste `+0x4084`, construit un record local de six mots et appelle ce pointeur
avec l'adresse du record. Cette preuve borne le contrat de l'indirection, pas
sa cible ni une soumission renderer.

## Gate fermé — contrat CPU du callback XAudio

Dans `recompilation/ace-combat-6-demo/build`, la cible
`ac6-demo-xaudio-callback-cpu-contract-tests` est à jour et le test CTest
ciblé du même nom passe. Cette preuve vérifie uniquement le contrat local de
sélection et de rejet des descripteurs ; elle ne constitue pas une preuve de
soumission XAudio ni de progression guest.

## Gate en pause — écrivain non nul du record render

Le CFG de `0x820FF710` pose zéro dans le slot indexé et incrémente le
producteur. Le CFG de `0x820FFCA0` charge ensuite le record de 96 octets vers
la pile. Dans le projet Ghidra `ace-combat-6-demo`, module `Default.xex`, le
balayage des flux directs vers `0x820FF710` ne trouve aucun appel direct : il
ne permet donc pas de qualifier ses appelants ni un store non nul.

## Gate fermé — `0x820FEFA8` n'est pas le lecteur de slot render

Dans le projet Ghidra `ace-combat-6-demo`, module `Default.xex`, le p-code de
`0x820FFD84` établit `r3 = r1+0x50` avant l'appel à `0x820FEFA8` : cette pile
contient les quatre chargements vectoriels et les huit mots complémentaires du
record de 96 octets. À l'entrée de `0x820FEFA8`, l'appel
`0x82327104` remplace `r3`; les chemins suivants utilisent ce contexte dans
`r31`, sans sauvegarder l'argument d'origine. Le callee est donc réfuté comme
consommateur de payload.

## Frontière statique en pause — lecteur de `fournisseur+0x20`

`0x822FFCC8` copie 16 octets et `0x822FFDB8` 64 octets de `r4` à `r3+0x20`.
Le scan global des accès D-form `+0x20` retourne 1 066 candidats à base
`r3`/`r4`, sans lien statique d'identité avec ces fournisseurs. `0x822F5608`
est seulement un wrapper de test nul vers `0x822F54E0`, et non un lecteur
qualifié. La conclusion est indécidable dans le budget statique ; aucun runtime
n'a été utilisé.

## Gate fermé — corps des slots `+0` de fournisseurs non-bootstrap

- Sources : header SDK local `dxc/WinAdapter.h` pour `E_NOTIMPL`, et p-code/désassemblage du projet Ghidra `ace-combat-6-demo`, module `Default.xex`, sous `artifacts/nonbootstrap-provider-slot-bodies-gate/`.
- `0x822F2F20–0x822F2F28` produit `0x80004001` dans `r3`, puis `blr`. Les boucles `0x822FFCC8–0x822FFCEC` et `0x822FFDB8–0x822FFDDC` copient respectivement 16 octets et 16 mots de `r4` à `r3+0x20`, posent `r3=0`, puis retournent.
- `DecompileMany.java` et `FindDirectCallsTo.java` ne trouvent aucune frontière ni appel direct aux trois entrées; `Ac6PcodeDump.java` borne néanmoins leurs trois retours. Aucun CALL, CALLIND, MMIO ou accès ring ne précède ces retours.

## Gate fermé — vtables des fournisseurs non-bootstrap

- Source : projet Ghidra `ace-combat-6-demo`, module `Default.xex`; logs et p-code sous `artifacts/nonbootstrap-provider-vtables-gate/`.
- `SweepMsvcRtti.java` associe les sept adresses `0x8202ADF8..0x8202AEA0` aux classes `Shader::ShaderParameter` et dérivées. `DumpU32Range.java` donne pour leur premier mot seulement `0x822F2F20`, `0x822FFCC8` et `0x822FFDB8`.
- `FindPpcAddressMaterialization.java` retrouve les installations dans la fenêtre `0x822FB2B8–0x822FB610`; `Ac6PcodeDump.java` montre les stores de vtable à l'offset zéro. `DecompileAt.java` ne trouve aucune fonction contenant `0x822FB2B8`, ce qui borne explicitement l'usage du pseudocode.

## Gate fermé — effet de fournisseur `0x822E0B90`

- Source : projet Ghidra `ace-combat-6-demo`, module `Default.xex`; p-code sous `artifacts/nonbootstrap-provider-effect-gate/822e0b90-pcode.txt`.
- Les instructions `0x822E0B90–0x822E0B9C` font deux loads à l’adresse du fournisseur puis sautent indirectement vers `vtable+0` par `CTR`.
- C’est une dispatch en queue, pas une call suivie d’un traitement local. Aucun chemin PM4 n’existe dans ce CFG de quatre instructions.

## Gate fermé — lecteurs du profil global non-bootstrap `0x829D0800`

- Source : projet Ghidra `ace-combat-6-demo`, module `Default.xex`; scans et p-code sous `artifacts/nonbootstrap-profile-readers-gate/`.
- `FindPpcSplitAddressMaterialization.java` donne `0x822DEBD8` comme seul site indépendant; son p-code montre la trampoline vers `0x822E9BD8` avec `r3 = 0x829D0800`.
- Le p-code de `0x822E9BD8` établit `r29 = r3`, les loads `+0xc/+8/+0x10` et les trois calls `0x822E0B90`. Cette jointure est de niveau registre, sans hypothèse de type du décompilateur.

## Gate fermé — provenance statique de l’enregistrement de profil non-bootstrap

- Source : projet Ghidra `ace-combat-6-demo`, module `Default.xex`; p-code complet sous `artifacts/nonbootstrap-profile-r3-slice-gate/caller-pcode.txt`.
- Le slice PPC `0x822E9968–0x822E9978` produit `r3 = 0x829D0800`, écrit son drapeau à `+0`, puis appelle `0x822E9A18`.
- Le p-code du prologue de `0x822E9A18` montre `r31 = r3` après la call de l’aide de sauvegarde. L’adresse globale est donc établie par le flux de registre, non par l’inférence du décompilateur.

## Gate fermé — consommateur du profil de fournisseurs non-bootstrap

- Source : projet Ghidra `ace-combat-6-demo`, module `Default.xex`; logs complets sous `artifacts/nonbootstrap-profile-consumer-gate/`.
- `FindDirectCallsTo.java 0x8232710C` établit que l’accesseur a de nombreux appelants, dont le cluster `0x822E…`; ce fait réfute son emploi comme clé de provenance du profil.
- `DecompileAt.java 0x8232710C` établit un corps vide, sans adresse, champ ou effet observable. L’absence de consommateur shader/PM4 n’est pas une preuve d’inexistence globale : elle borne exactement ce que cette indirection peut établir.

## Gate fermé — implémentation de la slot de résolution `+0x2c`

- Ghidra, projet `ace-combat-6-demo`, module `Default.xex` : le mot de vtable `0x8202AAE0` cible `0x822F2B88`; RTTI associe la vtable `0x8202AAB4` à `Shader::ShaderContextXenon`.
- `0x822F2B88` cherche dans les deux listes chaînées issues de `FUN_82327108` et délègue chaque essai à `0x822F89D0`. Celui-ci teste l’identifiant contre le champ `provider+8` et retourne le pointeur du fournisseur en sortie seulement en cas de succès.
- Les logs statiques sont sous `artifacts/nonbootstrap-vtable-resolution-gate/`; aucune observation dynamique ne fonde cette conclusion.

## Gate fermé — sélection du pixel shader non-bootstrap

- Ghidra, projet `ace-combat-6-demo`, module `Default.xex` : `0x822E3858` appelle `0x822F0340` puis `0x822E9948`; `0x822E9948` active cinq drapeaux globaux et invoque les cinq initialiseurs de profils.
- Dans le même projet et module, `0x822E0B10` passe par le gestionnaire global `0x8296BF80`, puis appelle la slot virtuelle `+0x2c` de l’objet résolu. Le CFG ne révèle ni implémentation de cette slot, ni clé MATE, ni paquet `IM_LOAD`.
- Logs reproductibles : `artifacts/nonbootstrap-pixel-selector-gate/822E3858.log`, `822E9948.log` et `822E0B10.log`. Ce sont des preuves statiques; aucun runtime n’a été exécuté.

## Gate fermé — origine EDRAM / readback du draw normal démo

- Source native : `src/vulkan_neutral_resolve.cpp` délègue le resolve qualifié puis certifie `normal.resolved_rgba8` contre les octets tuilés avant d’autoriser le writeback. `src/xenos_guest_present_join.hpp` écrit ensuite les octets certifiés et les relit pour comparaison.
- Vérification locale isolée : `tests/reached_edram_copy_oracle_tests.cpp` établit qu’un motif RGBA non nul est matérialisé aux offsets EDRAM attendus et que le canari est conservé. Le certificat a ses cas exact, pixel corrompu et padding corrompu dans `tests/reached_copy_runtime_certificate_tests.cpp`.
- L’artefact `analysis/demo/ac6-demo-edram-source-materialization-v1.json` qualifie le readback du draw normal atteint comme nul avant matérialisation. La conclusion porte seulement sur ce draw démo atteint; elle ne qualifie ni un shader non-bootstrap, ni le rendu final, ni une exécution Xenia/retail.

## 2026-08-20 — Gate fermé : enveloppe de soumission pilote `0x821BFBA8`

Le CFG démo calcule une longueur en unités de `0x200`, puis forme le buffer de canal avec un préfixe de `0x10`, une copie conditionnelle de `0x38` octets depuis `device+0x3584` et une section conditionnelle de `0x600` octets. `0x821A66E0` transmet ensuite le pointeur et cette longueur à `NtWriteFile` sans interpréter les octets; `0x821BA880` ne fait que sauver un curseur et drainer.

Les éléments matériels connus du CFG restent hors de l'enveloppe : aucun champ ne porte un WPTR MMIO ni le Type-3 indirect qualifié. Le layout interne est opaque au pilote à cette frontière; aucun changement natif, test synthétique ou runtime n'est autorisé sur cette base.

## 2026-08-20 — Gate fermé : contrat natif de soumission `0x821BFBA8`

Le CFG démo remet une enveloppe de canal à `0x821A66E0` / `NtWriteFile`; il ne publie pas de WPTR MMIO. À l'inverse, le store guest à `0x7FC80714` est routé par `guest_bridge.cpp` vers `apply_xenos_mmio_write`, qui lit le ring, applique le batch, puis écrit le read pointer après consommation. Ces contrats sont distincts.

Les tests isolés `test_xenos_ring_snapshot`, `test_xenos_ring_capture_wraps` et `test_xenos_unknown_packet_keeps_rptr` couvrent respectivement progression, wrap et rejet sans writeback. Aucun écart concret du publisher Xenos n'est établi; le layout de l'enveloppe pilote reste une frontière statique séparée.

## 2026-08-20 — Gate fermé : premier producteur PM4 non-bootstrap

La comparaison des cinq initialiseurs de `0x822E9948`, achevée par `0x822EA010`, montre un même protocole : construction d’objet par `0x822E6A38`, appel virtuel à la slot `+0x10`, puis trois résolutions par `0x822E0B10`. Ce dernier obtient une interface et appelle sa slot virtuelle `+0x2c`; il ne forme pas de paquet PM4. Aucun initialiseur ne contient d’écriture de ring ni MMIO.

`FUN_8232710c`, employé comme receveur de stores relatifs par ces fonctions, est un stub typé `void` par Ghidra. Cette limite interdit d’attribuer les champs à un objet plus précis, mais ne crée aucun lien vers Xenos. La prochaine frontière est le premier consommateur des handles résolus ; aucun runtime n’est justifié.

## 2026-08-20 — Gate fermé : effets de rendu bootstrap et transition non-bootstrap

Dans le bridge, `IM_LOAD` vérifie stage, offset et taille, produit un `XenosShaderLoadCommand`, et actualise l’identité vertex ou pixel. Un `DRAW_INDX2` ultérieur copie ces deux identités dans `XenosDrawCommand`; `GuestBridge::apply_xenos_typed_batch` conserve à la fois les snapshots et la séquence renderer. La voie bootstrap atteint donc bien le renderer natif, sans supposer d’exécution complète.

Le CFG démo qualifié de `0x822E3858` appelle linéairement `0x822F0340`, puis `0x822E9948`. Le premier initialise le bootstrap; le second pose cinq globaux et appelle cinq initialiseurs distincts de profils NSXR. Aucun branchement, consommation de profil, écriture de buffer PM4 ou soumission ne figure dans cette fonction. La première jointure profil non-bootstrap → PM4 est donc explicitement non résolue, et aucun runtime n’est lancé avant son identification statique.

## 2026-08-20 — Gate fermé : format PM4 du buffer indirect de `0x821B1D58`

Le CFG démo écrit les indices 0 à 73 et retourne 74 DWORDs. Ils forment `INVALIDATE_STATE` Type 3 (`0x3B`, un mot), puis deux `IM_LOAD` Type 3 (`0x2B`, 26 et 11 mots) dont les paires initiales décrivent respectivement 24 mots de shader sommet et 9 mots de shader pixel. Le décodeur natif exige ces formes exactes.

La suite est une série de Type 0, d’indices compris entre `0x0004` et `0x2312`. Le fichier de registres natif contient 0x8000 entrées ; les index atteints ne sont pas opaques avec une valeur non qualifiée. Le scanner de ring conserve Type 0, `0x2B` et `0x3B`. Aucun écart concret ne justifie un test isolé ou du runtime.

## 2026-08-20 — Gate fermé : opérandes indirectes de `0x821B2BC8`

Le Type 3 `0xC0013F00` écrit par `0x821B2BC8` prend ses opérandes dans `device+0x35c4` et `device+0x35c0 & 0x00ffffff`. `0x821B20A0` initialise ces champs depuis une allocation de 0x2000 octets et le retour de `0x821B1D58`. Ce dernier écrit au plus l’index 72 et retourne exactement 73, donc un compte de DWORDs très inférieur au masque `0xFFFFF` du bridge.

L’implémentation locale de `MmAllocatePhysicalMemoryEx` arrondit l’alignement au moins à la page. L’adresse retournée a donc ses deux bits bas nuls et l’encodage de `+0x35c4` les préserve; le `& ~3` de `capture_indirect` est identitaire. Les unités et les deux masques coïncident sans écart statique; aucun test isolé ni runtime ne sont requis.

## 2026-08-20 — Gate fermé : paquets écrits après le drain `0x821BA780`

La fenêtre de `0x821B0D20` matérialise successivement les en-têtes Type 3 `0xC0006000`, `0xC0006200`, `0xC0006100` et `0xC0006300`, chacun avec un unique mot. `XenosCommandProcessor` impose précisément un opérande aux opcodes `0x60–0x63` et met à jour les masques/sélecteurs de bin correspondants.

La fenêtre de `0x821B2BC8` forme `0xC0013F00` avec deux valeurs adresse/longueur. `graphics_ring.hpp` n’accepte `0x3F` qu’à cette longueur, capture ces deux valeurs et décode le flux indirect séparément. Le Type 3 indirect n’est donc pas remis par erreur au processeur de commandes. Aucun écart concret ne justifie un nouveau test ou du runtime.

## 2026-08-20 — Gate fermé : consommateur de la publication de `0x821BA780`

Les fenêtres littérales du code démo montrent un même garde `cursor > limit` avant l’appel à `0x821BA780` dans `0x821B0D20`, `0x821B2BC8` et les deux chemins de `0x821C57D0`. `0x821C64E8` utilise le même drain à l’initialisation et rappelle séparément `0x821BAAD0`. Après chaque appel, le curseur de retour devient la position d’écriture des mots suivants.

La frontière de publication est déjà statiquement établie par `0x821BA780 → 0x821BA058 → 0x821B9BC8`. Le lien est donc un drain de capacité, non une conjecture fondée sur l’ordre d’initialisation. Aucun runtime n’a été utilisé.

## 2026-08-20 — Gate fermé : sémantique native du segment de plage optionnel

Le cross-match littéral du CFG démo `0x821B9DB0` montre deux constructions de onze mots. Chacune contient le Type 0 `0x00000A31`, le Type 0 de deux valeurs `0x00010A2F`, puis `0xC0043C00` et les cinq opérandes du `WAIT_REG_MEM`. Les valeurs de plage diffèrent selon la source, pas les en-têtes ni leurs longueurs.

Dans `src/xenos_command_processor.cpp`, le Type 0 écrit tout index valide ; la liste opaque ne couvre que `0xA02–0xA05`, `0x2290–0x2291` et une tranche `0x230B–0x2314`. Les indices `0xA2F`, `0xA30` et `0xA31` sont donc admis. Le Type 3 `0x3C` exige et gère précisément cinq opérandes, dont la forme cohérente produite ici. Le scanner du ring reconnaît les Type 0 et ce Type 3. Aucun écart concret ne motive un test isolé ou du runtime.

## 2026-08-20 — Gate fermé : entrées de contenu de `0x821B96B8`

`0x821B96B8` effectue un échange exclusif sur `device+0x2e28` : il renvoie les deux moitiés du mot 64-bit précédemment capturé et le remplace par le sentinelle. `0x821C6928` initialise ce champ; `0x821B9648` y applique une mise à jour atomique composante par composante depuis `0x821B8090` et `0x821B86F8`.

Dans `0x821B9DB0`, la moitié basse devient la base alignée de page et la moitié haute contribue à la borne de la plage du segment optionnel construit localement. Cette donnée ne modifie pas le format adresse/longueur remis à `0x821BA058`. Aucun runtime n’a été utilisé.

## 2026-08-20 — Gate fermé : producteur de contenu de `0x821B9DB0`

Le CFG qualifié de `0x821B9DB0` reçoit deux pointeurs de sortie. Il réserve une zone de `11` ou `22` dwords par `0x821B9AE0`, y construit les mots de commande, puis écrit l’adresse matérielle dérivée de cette zone dans la première sortie et le nombre de dwords dans la seconde. `0x821BA780` relit ces deux sorties et les passe comme adresse et longueur à `0x821BA058`.

La seule entrée de contenu non locale est la paire fournie par `0x821B96B8` sur la branche correspondante. Aucun runtime n’a été utilisé.

## 2026-08-20 — Gate fermé : source des descripteurs indirects du publisher

Les exports statiques qualifiés du projet `ace-combat-6-demo` / module `Default.xex` donnent la chaîne directe `0x821BA780 → 0x821BA058 → 0x821B9BC8`. Le cross-match de contrôle/ABI généré qualifié montre que `0x821BA780` reçoit la taille et l’adresse de contenu de `0x821B9DB0`, réserve un en-tête par `0x821B9AE0`, et passe ce descripteur à `0x821BA058`; ce dernier transmet un élément à `0x821B9BC8`.

`0x821B2BC8` appelle `0x821BA780` uniquement sur sa branche de capacité avant de poursuivre l’émission de son template. La provenance du contenu du drain est donc bornée à `0x821B9DB0`; aucune identité directe entre ce contenu et le template de `0x821B2BC8` n’est établie. Aucun runtime n’a été utilisé.

## 2026-08-20 — Gate fermé : contrat natif du publisher WPTR

La routine statique `0x821B9BC8` ne lit que la base, le masque et l’index du ring ; elle publie l’index final dans `CP_RB_WPTR`. Dans `graphics_ring.hpp`, le bridge valide cette écriture, calcule la plage circulaire depuis le dernier WPTR, reconnaît le Type-3 `0x3F`, puis capture l’adresse alignée et le compte borné avant d’appliquer le batch et de publier les pointeurs locaux.

Validation locale réussie :

`cmake --build recompilation/ace-combat-6-demo/build -j16`

Les suites `ac6-demo-core-tests` et `ac6-demo-xenos-command-tests` passent sous Xvfb avec l’audio factice. La correction associée rend l’en-tête de trace graphique visible dans les deux variantes de compilation ; elle ne modifie aucun contrat du publisher. Aucun runtime n’a été utilisé.

## 2026-08-20 — Gate fermé : publication du ring système

Exports lecture seule du projet Ghidra démo `ace-combat-6-demo` / `Default.xex` : `0x821BAAD0` passe l’adresse physique du buffer à `VdInitializeRingBuffer`, puis initialise `state+0x3a18` (base), `state+0x3a1c` (masque) et `state+0x2ac8` (index). `0x821B9BC8` lit ces mêmes champs, écrit les triplets indirects `0xC0013F00`, adresse et longueur avec retour circulaire, publie l’index final à `0x7FC80714`, puis exécute `sync/eieio/sync`.

Les journaux bornés sont `artifacts/ring-system-publication-gate/821b9bc8-headless.log` et `artifacts/ring-system-publication-gate/821baad0-headless.log`. La jointure est entièrement statique ; aucun runtime n’a été utilisé.

## 2026-08-20 — Gate fermé : contrat d’initialisation du ring natif

`0x821BAAD0` transmet à `VdInitializeRingBuffer` la base physique et un logarithme de taille. Le bridge conserve ces deux arguments et calcule sa capacité en DWORDs par `1 << (size_log2 + 1)`, soit exactement la taille en octets divisée par quatre : son parcours circulaire emploie donc le même masque que le guest.

Le même initialiseur passe l’adresse de writeback à `VdEnableRingBufferRPtrWriteBack`; le bridge l’enregistre, puis écrit le RPTR final lors de la complétion. Build incrémental et les suites `ac6-demo-core-tests` et `ac6-demo-xenos-command-tests` passent sous Xvfb avec l’audio factice. Aucun écart ni runtime n’a été observé.

## 2026-08-20 — Gate fermé : consommateur du travail en attente

Dans le projet Ghidra démo `ace-combat-6-demo` / `Default.xex`, `0x821BF720` pose
`(device+0x5494)+0x258:0x08` quand son curseur reboucle. `0x821BF7D8` récupère
le même sous-objet, teste ce bit avant chaque canal et, lorsqu’il est présent,
choisit la borne mémorisée plutôt que la borne de writeback. Il ne l’efface pas
et n’écrit pas directement le ring ou le MMIO ; `0x821A6530` puis `0x82337E38`
forment la frontière statique restante. Les journaux sont
`artifacts/pending-work-consumer-gate/821bf720-headless.log` et
`artifacts/pending-work-consumer-gate/821bf7d8-headless.log`. Aucun runtime n’a été utilisé.

## 2026-08-20 — Gate fermé : appels de drain de canaux

Dans le projet Ghidra démo `ace-combat-6-demo` / `Default.xex`, `0x821A6530`
calcule puis positionne l’offset d’un handle via les slots virtuelles `+0x20`
et `+0x24`. `0x82337E38` appelle `NtQueryInformationFile`, puis deux
`NtSetInformationFile`. Aucun CFG ne contient de ring, MMIO ou API Vd : ces
deux appels réfutent la publication directe et bornent la suite au wrapper
`NtWriteFile` atteint par `0x821BF7D8`. Journaux :
`artifacts/drain-channel-submission-gate/821a6530-headless.log` et
`artifacts/drain-channel-submission-gate/82337e38-headless.log`. Aucun runtime n’a été utilisé.

## 2026-08-20 — Gate fermé : handle du wrapper NtWriteFile

`0x821A66E0` appelle `NtWriteFile` avec le buffer et la longueur reçus, mais
son handle provient de `FUN_8232710C`. La décompilation DEMO de cette dernière
est un `return` vide : aucun producteur de handle ne peut être déduit de cette
frontière statique. Le journal est
`artifacts/ntwritefile-handle-gate/8232710c-headless.log`; aucun runtime n’a été utilisé.

## 2026-08-20 — Gate fermé : buffer du drain NtWriteFile

`0x821BF7D8` passe à `NtWriteFile` un buffer de pile de `0x800` octets.
`0x82327D90` est une copie mémoire et y transfère exactement 56 octets depuis
`device+0x3584`; `0x821C0E28` prépare séparément une table gamma dans l’état
du device. Le balayage D-form de `+0x3584` ne produit aucun accès : ce champ
est formé par adressage composé. Ces faits ne prouvent pas un format PM4.
Journaux : `artifacts/drain-buffer-producer-gate/821c0e28-headless.log`,
`82327d90-headless.log` et `offset-3584-headless.log`. Aucun runtime n’a été utilisé.

## 2026-08-20 — Frontière statique : provenance de l’en-tête de drain

La recherche d’adressage composé trouve cinq `addi` portant `0x3584`.
`0x821BFBA8` copie cette région vers le ring ; `0x821C57D0` la transmet à
`0x821C55F0`. Ce dernier et `0x821C8608` construisent des commandes/scaler,
mais n’écrivent pas le buffer. La provenance reste indécidable dans cette
fenêtre. Journaux : `artifacts/drain-header-provenance-gate/`.

## 2026-08-20 — Gate fermé : writer WPTR du ring système

Faits statiques, projet Ghidra démo `ace-combat-6-demo`, module `Default.xex` :

- `0x821BAAD0` établit `device+0x3a18` (base), `device+0x3a1c` (masque) et
  remet `device+0x2ac8` à zéro avant son appel à `VdInitializeRingBuffer`.
- `0x821B9BC8` lit ces trois champs, écrit les triplets de commandes dans le
  ring, publie le nouvel index `device+0x2ac8` par
  `stw r29,0x714(r11)` après `lis r11,0x7fc8`, avec
  `sync → eieio → sync`.
- Ses appelants directs sont `0x821BA058`, `0x821C3B88` et `0x821C41F8`.

Conclusion : `0x821B9BC8` est le publisher guest qualifié de `CP_RB_WPTR`.
La prochaine question est la fidélité du bridge natif à ce contrat exact.

## 2026-08-20 — Gate fermé : enveloppe pilote de `0x821BFBA8`

Faits statiques, projet Ghidra démo `ace-combat-6-demo`, module `Default.xex` :

- `0x821BFBA8` appelle `0x821BA368`, lequel délègue à `0x821BA130` avec
  `VdGlobalDevice`, le sous-objet de canal et le mode `6`.
- `0x821BA130` effectue une réservation/synchronisation de capacité du device
  global. Il ne définit aucun format d'enveloppe de soumission.
- Juste avant `0x821A66E0` (`NtWriteFile`), `0x821BFBA8` obtient handle et
  buffer par accès indexés au sous-objet de canal ; `r5` est une quantité
  dérivée et arrondie. Les headers locaux qualifient `VdGlobalDevice` comme
  slot d'import du device et le layout public `D3DDevice` comme contrat du
  ring, sans couvrir ces champs de canal.

Conclusion : le buffer de `NtWriteFile` est une enveloppe pilote opaque ; il
ne doit pas être assimilé à un flux PM4 ni adapté au bridge MMIO sans une
jointure statique supplémentaire.

| Claim | Statut | Preuve | Reproduction | Confiance | Réfutation possible |
|---|---|---|---|---|---|
| La cible est exclusivement la démo PAL qualifiée. | observé | `recompilation/ace-combat-6-demo/config/demo-identity.json`; `analysis/demo/ac6-demo-ring-readback-frontier-v1.json` | `sha256sum demo-game-file/extracted/stfs-root/Default.xex` | haute | Un SHA différent dans un artefact runtime canonique. |
| Edge écrit le RPTR uniquement à l’adresse fournie à `VdEnableRingBufferRPtrWriteBack`. | observé | `.tools/xenia-edge-source/src/xenia/kernel/xboxkrnl/xboxkrnl_video.cc`; `.tools/xenia-edge-source/src/xenia/gpu/command_processor.cc`; commit Edge `e4b13738…` | `rg -n 'VdEnableRingBufferRPtrWriteBack|EnableReadPointerWriteBack' .tools/xenia-edge-source/src/xenia` depuis la racine portfolio | haute | Un autre writer Edge qualifié vers `ptr-0x3c`. |
| Le runtime ne touche plus `RPTR-0x3c` et écrit le WPTR consommé au vrai writeback. | observé | `recompilation/ace-combat-6-demo/src/guest_bridge/graphics_ring.hpp`; test core | `cmake --build recompilation/ace-combat-6-demo/build-codegen-on --target ac6-demo-core-tests -j16 && recompilation/ace-combat-6-demo/build-codegen-on/ac6-demo-core-tests` | haute | Test montrant une écriture hôte à l’adresse adjacente ou un RPTR différent du WPTR consommé. |
| Le processeur courant ne reste pas arrêté à un hypothétique mot primaire 7 au boot. | observé | `artifacts/primary-ring-word-7/run.stderr`, `run.report.json` : 22+3 dwords, deux waits satisfaits, `pending_wait=false` | capture ponctuelle retirée après validation ; artefacts conservés | haute | Une capture bornée sur le même build montrant une consommation partielle ou une attente pendante. |
| Le debugger voit un chemin sans MMIO dans le runtime généré après l'init finale. | non qualifié pour Ghidra canonique | `artifacts/wptr-producer-debugger-gate/run.gdb.log` | fenêtre GDB bornée ; arrêt explicite après l'observable local | haute pour le runtime démo lié uniquement | Le runtime est régénéré depuis le même programme que Ghidra canonique. |
| Le codegen lié et Ghidra canonique ont des provenances XEX distinctes. | observé | `analysis/demo/ac6-demo-ghidra-manifest.json`; CMake cache codegen-on; `analysis/ghidra/canonical-import.json`; `artifacts/wptr-remaining-callers-static/cfg-windows.log` | lecture des manifests et `analyzeHeadless` canonique en lecture seule | haute | Un manifeste qualifié reliant les deux chaînes au même programme. |
| Les opcodes atteints `0x61/0x62/0x63` traversent le wrap du ring. | observé | `tests/ac6-demo-core-tests.cpp`; `graphics_ring.hpp` | même test core | haute | Un stream atteint avec une taille/payload différente ou un trap sur le corpus qualifié. |
| `EVENT_WRITE_SHD` accepte les valeurs dynamiques atteintes et conserve l’endian guest. | observé | `src/xenos_command_processor.cpp`; `tests/ac6-demo-xenos-command-tests.cpp` | `cmake --build recompilation/ace-combat-6-demo/build-codegen-on --target ac6-demo-xenos-command-tests -j16 && recompilation/ace-combat-6-demo/build-codegen-on/ac6-demo-xenos-command-tests` | haute | Une adresse/initiator atteinte hors allowlist ou des bytes guest différents. |
| Le cold START final atteint 3 036 ticks, 23 threads et 2 899 `VdSwap` sans trap. | observé | `analysis/demo/ac6-demo-ring-readback-frontier-v1.json` avec hashes report/trace | commande `probe` headless donnée dans `RESUME.md` | haute | Un run process-fresh avec même binaire/input qui trap ou produit des digests différents. |
| Le frontbuffer déclaré est `0x1374A000`, format 6, `1280×720`. | observé | même preuve, section `vulkan_start`; rapports `graphics.vd_swap` | même run `probe`, puis `jq '.graphics.vd_swap' <report>` | haute | Un run qualifié où fetch, adresse ou dimensions divergent. |
| Le ring final `0x126CA000` n’est jamais soumis dans le corridor actuel. | observé | même preuve ; `submissions=0`, `typed_presents=0` | même run, puis `jq '.graphics.ring' <report>` | haute sur 3 036 ticks | Un write MMIO post-seconde-init produisant une soumission non nulle. |
| Le draw normal démo couvre entièrement sa cible mais produit du noir. | observé | `reports/cycle-1755-demo-normal-draw-coverage.md`; `analysis/demo/ac6-demo-normal-draw-coverage-v1.json`; `reports/cycle-1785-demo-real-readback-certificate.md` | reçus Vulkan neutral bornés, sans pixels synthétiques | haute pour ce draw neutral | Une capture qualifiée du même draw avec pixels non noirs ou couverture partielle. |
| Le pixel du draw normal démo est un bootstrap : sans entrée couleur ni fetch de texture, il écrit RGBA zéro. | observé | `analysis/demo/ac6-demo-bootstrap-pixel-zero-v1.json`; diagnostic temporaire `ac6-demo-rexglue-shader-cli` | CLI local sur le slice qualifié, sorties propriétaires sous `TMPDIR` seulement | haute | Un diagnostic sur le même shader montrant une entrée couleur, un fetch ou une sortie non nulle. |
| Les profils NSXR non-bootstrap connus sont initialisés sans garde locale, mais leur sélection pour un batch n'est pas établie. | observé / indécidable | `analysis/demo/ac6-demo-nsxr-shader-inventory-v1.json`; `analysis/demo/ac6-demo-static-references-v1.json`; décompilation locale temporaire du projet `ace-combat-6-demo` : `0x822E3858 -> 0x822E9948`, `0x822F0340 -> 0x822F8078` | `Ac6XenonDump` en lecture seule, sortie sous répertoire temporaire | haute pour l'initialisation ; aucune conclusion dynamique | Une jointure qualifiée profil → commande PM4 → draw. |
| Le prévol GDB NSXR s'est arrêté avant le guest par absence de `run`, pas par un symbole absent. | observé | `/fastdata/lavaulta/tmp/ac6-nsxr-run.qGWgnR/gdb.stderr` : breakpoint résolu, puis « The program is not being run. » | lecture locale de `gdb.stderr` et `gdb.stdout`, sans relance | haute | Une sortie GDB montrant un échec antérieur à la résolution du breakpoint. |
| Un store VFS démo valable requiert le marqueur et les neuf fichiers qualifiés ; il ne peut pas être réduit au seul marqueur. | observé | `src/content.cpp` : `DemoStore::verify` et `import_directory`; `include/ac6demo/content.hpp`; inventaire de `.build/ac6-demo-store-test-3` | lecture source et noms/tailles seulement | haute | Un chemin de montage qui contourne `DemoStore::verify`. |
| L'import natif publie un store VFS démo isolé sans modifier la référence. | observé | `/fastdata/lavaulta/tmp/ac6-nsxr-import.inm9D9/import.stdout`; inventaire du store importé | `ac6-demo-recomp import` vers un enfant d'une racine `mktemp -d` | haute | Échec d'import ou fichier non qualifié dans la destination. |
| Les clés de lookup bootstrap et non-bootstrap sont toutes initialisées au tick 0. | observé | `/fastdata/lavaulta/tmp/ac6-nsxr-profile-run.S7SY8j/gdb.stdout` : 12 lignes `AC6_NSXR_PROFILE_LOOKUP`, clés `-14` à `-9` et LR des wrappers | une fenêtre GDB bornée sur `__imp__sub_822E0B10`, store isolé, 16 hits maximum | haute pour l'initialisation ; aucune conclusion PM4 | Une capture du même point sans une des six clés. |
| Le chemin d'initialisation NSXR remonte statiquement à `0x822DA7F0`, sans intermédiaire de registre à `0x8232710c`. | observé | décompilation temporaire `ace-combat-6-demo` de `0x8232710c` et `0x822E3858`; atlas statique : inverse de `0x822E3858` | Ghidra 12.1 en lecture seule | haute pour le CFG local ; aucune conclusion sur PM4 | Un second appelant direct qualifié de `0x822E3858`. |
| `0x822DA7F0` est une étape d'initialisation globale, pas un consommateur PM4 de profil. | observé | décompilation temporaire démo de `0x822DA7F0` et ses appels directs ; atlas : unique appelant `0x821A3C30` | Ghidra 12.1 en lecture seule | haute pour le CFG local ; aucune conclusion sur l'orchestrateur | Un appel PM4 direct ou un lecteur de slot dans ce CFG. |
| `0x821A3C30` n'établit pas directement le lien NSXR→Xenos. | observé | dump DEMO : appel `0x821a3cec → 0x822DA7F0` avec `(0x500,0x2d0,4,0,1)` ; atlas direct sans `0x821B1D58`, `0x821B6078` ni `0x821B6FD0` | Ghidra 12.1 en lecture seule | haute pour les appels directs ; aucune conclusion indirecte | Un appel direct qualifié de cet orchestrateur vers un de ces producteurs. |
| `0x821B1D58` fabrique le template bootstrap, sans le soumettre. | observé | dump DEMO qualifié : copie `0x82013e80` de taille `0x24` vers `param_2+0x80` ; seuls appels directs `0x823270E8`, `0x82327D90` | Ghidra 12.1 en lecture seule ; atlas statique | haute pour le CFG local ; aucune conclusion pour les consommateurs | Une écriture MMIO ou appel de soumission dans cette fonction. |
| Le constructeur de template bootstrap appartient à l'initialisation du device global. | observé | atlas inverse : `0x821C64E8 → 0x821B20A0`; dump DEMO : `VdGlobalDevice = param_1`, puis appel de `0x821B20A0` après les prérequis | Ghidra 12.1 en lecture seule | haute pour cette chaîne d'appels ; aucune soumission inférée | Un CFG de cette tranche avec écriture MMIO ou appel de soumission de template. |
| Le template bootstrap est publié dans le device aux offsets `+0x35bc/+0x35c0/+0x35c4`. | observé | dump DEMO de `0x821B20A0` : allocation `0x2000`, appel `0x821B1D58`, puis écritures de base/longueur dans ces champs | Ghidra 12.1 en lecture seule | haute pour le stockage ; aucune consommation ultérieure inférée | Une décompilation qualifiée montrant des offsets ou un flux d'adresse différents. |
| `0x821B2BC8` émet le template bootstrap dans le buffer de commandes courant. | observé | recherche de déplacements puis dump DEMO : lit `+0x35c4/+0x35c0`, écrit ces valeurs à `uVar2+8/+0xc`, avance le curseur à `uVar2+0x24`, puis appelle `0x821B1F28` | Ghidra 12.1 en lecture seule | haute pour l'émission locale ; commit final non qualifié | Un flux qualifié montrant que `uVar2` n'est pas un buffer de commandes. |
| Le sous-chemin immédiat après l'émission bootstrap n'effectue pas le commit du ring. | observé | dumps DEMO de `0x821B1F28` et `0x821AE1E8` : bits d'état et écritures au curseur `+0x30`, avec seulement l'extension `0x821BA780` | Ghidra 12.1 en lecture seule | haute pour ces CFG locaux ; aucune conclusion sur leurs appelants | Une publication de curseur ou écriture MMIO dans ces CFG. |
| Le bootstrap est appliqué pendant l'initialisation du device. | observé | atlas inverse de `0x821B2BC8`; dump DEMO de `0x821C5DB8`, appelée depuis `0x821C64E8`, avec appel terminal `0x821B2BC8(iVar6,0,0,0)` | Ghidra 12.1 en lecture seule | haute pour cette initialisation ; aucun lien vers un draw runtime | Un appelant ou des paramètres qualifiés qui contredisent ce CFG. |
| Le chemin distinct `0x821BFBA8` construit un batch mais n'en sonne pas directement le doorbell. | observé | dump DEMO : construction au curseur, appel `0x821BF720`; celui-ci met à jour `+0x254/+0x17c` et pose seulement `+0x258:0x08` au retour canal zéro | Ghidra 12.1 en lecture seule | haute pour ce CFG ; publication ultérieure non qualifiée | Une écriture MMIO ou API Vd de publication dans `0x821BF720`. |
| Le frontend n’est pas visuellement validé. | observé | stdout renderer résumé dans `analysis/demo/ac6-demo-ring-readback-frontier-v1.json` | run Vulkan de `RESUME.md` avec répertoire `AC6_DEMO_AUDIT_SCREENCAP_DIR` existant | haute | Pipeline raster, writeback guest, screenshot et RGB non nul joints au même état guest. |
| Les notifications `VdSwap` seules ne prouvent pas une image. | inféré | 2 899 notifications avec 0 pipeline, 0 writeback et 0 screenshot | comparer `graphics.vd_swap` au résumé renderer du même run | haute | Un chemin démontrant que la notification porte déjà des pixels guest validés. |
| L’asynchronisme du CP Edge peut compter dans le blocage D3D. | spéculatif | source Edge : CP séparé ; aucun résultat causal natif | aucun test accepté à ce checkpoint | faible | Une trace native montrant que l’ordre synchrone produit correctement la première soumission finale. |
| Le dépôt codegen-on passe entièrement. | observé | dernier run `26/26`; commit `aa9b0534` | `SDL_AUDIODRIVER=dummy xvfb-run -a ctest --test-dir recompilation/ace-combat-6-demo/build-codegen-on --output-on-failure` | haute | Un test échoue depuis un checkout propre du checkpoint. |
## Mise à jour statique — pending de canal

| Hypothèse / constat | Statut | Preuve | Méthode | Confiance | Ce qui la réfuterait |
|---|---|---|---|---|---|
| Le bit de canal pending est consommé par `0x821BF7D8`, mais ce CFG ne fait pas de publication directe. | observé | dump DEMO de `0x821BF7D8` : lectures du masque numérique `0x08` ; sélection des paramètres de drain, appels indirects à `0x821A66E0`, puis aucune écriture au champ ni MMIO direct | Ghidra 12.1 en lecture seule | haute pour ce CFG ; frontière indirecte conservée | Une écriture de publication ou un effacement du bit dans ce CFG. |
## Mise à jour statique — interface de publication

| Hypothèse / constat | Statut | Preuve | Méthode | Confiance | Ce qui la réfuterait |
|---|---|---|---|---|---|
| Le drain rejoint une frontière noyau via `NtWriteFile`, avec un buffer de `0x800` octets, mais la nature du handle n'est pas encore établie. | observé / indécidable | dump DEMO de `0x821A66E0` : appel `NtWriteFile`, attente éventuelle `NtWaitForSingleObjectEx`; appelant `0x821BF7D8` : handle du sous-objet, buffer local, longueur `0x800` | Ghidra 12.1 en lecture seule | haute pour l'ABI du wrapper ; indécidable pour la cible du handle | Producteur qualifié du handle indiquant un objet non-graphique, ou son ouverture sur un device graphique. |
## Mise à jour statique — constructeur de canal

| Hypothèse / constat | Statut | Preuve | Méthode | Confiance | Ce qui la réfuterait |
|---|---|---|---|---|---|
| Le constructeur de canal alloue sa mémoire physique mais ne produit pas directement le handle passé à `NtWriteFile`. | observé / indécidable | dump DEMO : `0x821BEFF0` initialise les deux sous-objets et appelle `0x821BEE60`; ce dernier appelle `MmAllocatePhysicalMemoryEx` avec fallback, puis écrit ses sorties aux pointeurs fournis; recherche D-form `+0x14` : aucune écriture associable au sous-objet | Ghidra 12.1 en lecture seule | haute pour le constructeur; indécidable pour l'alias du handle | Un store qualifié sur le même sous-objet, ou une fonction d'ouverture lui transmettant ce champ. |
## Mise à jour statique — limite du slice d'alias

| Hypothèse / constat | Statut | Preuve | Méthode | Confiance | Ce qui la réfuterait |
|---|---|---|---|---|---|
| Le projet Ghidra DEMO et les scripts existants ne permettent pas de rattacher le champ `device+0x54a8` à un producteur. | observé / indécidable | recherche D-form `+0x14` : aucun store qualifié du sous-objet; `ClassifyPpcOffsetUses 0x54a8` : zéro accès indexé | Ghidra 12.1 en lecture seule, cinq batches ciblés | haute pour l'absence dans ces analyses; aucune conclusion sur le binaire complet | Un slice interprocédural qualifié ou une écriture trouvée par un autre mode d'adressage. |
## Mise à jour statique — chaîne de soumission candidate

| Hypothèse / constat | Statut | Preuve | Méthode | Confiance | Ce qui la réfuterait |
|---|---|---|---|---|---|
| `0x821BFBA8` relie localement l'émission PM4 au buffer passé à `NtWriteFile`. | observé | xrefs DEMO de `0x821A66E0`; CFG de `0x821BFBA8` : appel `0x821B2BC8`, construction du buffer de canal, puis appel `0x821A66E0` avec handle/buffer/longueur calculés dans le même sous-objet; `0x821A1170` et `0x822FA5A0` n'ont pas ce contexte | Ghidra 12.1 en lecture seule | haute pour la chaîne locale; la nature exacte du pilote reste à qualifier | Un CFG montrant que le buffer transmis est dissocié de l'émission PM4 précédente. |
## Mise à jour native — contrat non isomorphe

| Hypothèse / constat | Statut | Preuve | Méthode | Confiance | Ce qui la réfuterait |
|---|---|---|---|---|---|
| Le point partagé natif n'implémente pas directement l'enveloppe `NtWriteFile` observée dans le binaire. | observé | `guest_bridge/graphics_ring.hpp` : `apply_xenos_mmio_write` valide `0x7FC80714`, lit le ring, applique le batch puis met à jour WPTR/état pending; CFG DEMO : publication par `NtWriteFile` depuis `0x821BFBA8` | lecture source native et Ghidra DEMO en lecture seule | haute | Un décodeur qualifié établissant que le buffer `NtWriteFile` est exactement le même ring MMIO. |
# Gate fermé — entrées de contenu `0x821B96B8` → `0x821B9DB0`

- Qualification : projet Ghidra `ace-combat-6-demo`, module `Default.xex`, lecture seule.
- `artifacts/b96b8-content-inputs-gate/821b96b8-ghidra.txt` montre le `storeDoubleWordConditionalIndexed` qui remplace `state+0x2e28` par `-1` et extrait les deux moitiés de la valeur antérieure.
- `artifacts/b96b8-content-inputs-gate/821b9db0-ghidra.txt` montre la consommation des deux sorties : base page-alignée issue du mot bas, longueur page-alignée bornée par le mot haut, puis émission de 11 mots lorsque cette plage existe.
- Conclusion : la paire est un transfert atomique de plage consommé localement ; aucune hypothèse runtime n’est requise.
# Revalidation — producteur de contenu `0x821B9DB0`

- Projet Ghidra démo `ace-combat-6-demo` / `Default.xex`, lecture seule.
- `artifacts/b96b8-content-inputs-gate/821b9ae0-ghidra.txt` : `0x821B9AE0` prend le nombre de dwords, calcule `×4`, réserve la zone et met à jour le compteur de bytes.
- `artifacts/b96b8-content-inputs-gate/821ba780-ghidra.txt` : `0x821BA780` reçoit adresse et longueur de `0x821B9DB0`, réserve quatre dwords, puis appelle `0x821BA058` avec ces deux valeurs inchangées.
# Revalidation — descripteur `0x821BA058` → `0x821B9BC8`

- Qualification : Ghidra démo `ace-combat-6-demo` / `Default.xex`, lecture seule.
- `artifacts/descriptor-source-revalidation-gate/ba058-ba11c-b9bc8-ghidra.txt` : `0x821BA058` construit le descripteur local à deux dwords et appelle `0x821B9BC8(..., &descriptor, 1)`.
- Le même export montre que le publisher lit le mot 0 comme longueur masquée sur 24 bits et le mot 1 comme adresse, puis écrit `0xC0013F00`, adresse et longueur dans le ring avant publication du WPTR.
- Conclusion : le descripteur drainé provient du contenu de `0x821B9DB0`; le template de `0x821B2BC8` reste une branche distincte de capacité.
## Gate fermé — contrat public XMA

Source de rang 1 : `sdk/xdk-xenon-6132.6/XDK/include/xbox/xmadecoder.h` déclare `XMACreateContext(PXMACONTEXT*)`, `XMAInitializeContext` et `XMAReleaseContext`; cette dernière est documentée comme remise en liste libre. Source de rang 2 : le CFG PAL de `0x82357240` passe `entry+64` et relit ce slot. Cela ne qualifie aucun layout opaque ni registre XMA.
## Gate fermé — causalité du cycle XMA

La trace `cycle-1748-demo-xma-release-context.md` fournit les trois appels bornés au release-loop après création. Le SDK limite l'effet public à la remise du contexte en liste libre; ni le SDK, ni la trace, ni le CFG n'établissent une écriture RT0/EDRAM. Les constantes de l'allocateur local restent non-PAL.
## Gate fermé — store XMA calculé

Ghidra canonique décompile `0x82357240` en création, `MmGetPhysicalAddress`, mémorisation à `entry+0x50`, store one-hot vers l'aperture calculée depuis `0x7FEA1A80`, puis `enforceInOrderExecutionIO`. La recherche directe de références de plage est vide, cohérente avec cette matérialisation calculée. `xmahardwareabstraction.h` ne fournit aucun layout de registre.
## Gate fermé — synchronisation de l'événement

Ghidra relie `0x822EED70` à `NtCreateEvent` et `0x822EEE10` à `NtSetEvent`. `analysis/demo/ac6-demo-event-post-set-scheduler-join-v1.json` compte 351 handoffs `E000004C` par route, avec delta de tick nul. Cela réfute une perte de signal par le bridge ou le scheduler.
## Gate fermé — chaîne d'entrée START

Les cycles 1774 et 1775 joignent la donnée XInput, le snapshot guest, la normalisation et le mapping logique PAL. Les cycles d'entrée ultérieurs ne produisent pas de frontend. Cette preuve exclut une perte de contrôleur ou un bouton de confirmation manquant, sans promouvoir l'oracle Xenia en preuve native.

## Gate fermé — consommateurs mission/niveau natifs

Les rapports détaillés sont dans `artifacts/mission-consumer-static-gate/`.
La décompilation Ghidra et le corps généré concordent : `0x820EA550` appelle
les getters mission et prédicat film, substitue `16` si nécessaire et écrit
via `r4`; `0x820EA598` appelle le getter niveau, échange `6`/`7` et écrit via
`r4`. La sonde valide le démarrage du guest jusqu'à 120 ticks, mais pas encore
la reachability de ces consommateurs.

## Gate fermé — getters mission/niveau et validity gate natifs

Les rapports détaillés sont dans `artifacts/mission-dispatch-static-gate/`.
La décompilation Ghidra démo et les corps générés concordent pour
`0x82095B80`, `0x820E9290` et `0x820E9300` : sélection par mode, index borné
sur trois entrées, constante niveau `2` en mode `4`, et prédicat de validité
du champ `0x6c8 + index*0xaab8`. Les cinq symboles natifs sont forts et les
26 tests passent. La sonde runtime bornée n'entre dans aucun getter; ce
résultat ne tranche que la reachability.

## Gate fermé — écrivain de validity mission natif

Les preuves détaillées sont dans \`artifacts/mission-validity-callers-static-gate/\`.
Les sept xrefs directs de \`0x820E9300\` sont qualifiés dans le projet démo;
\`0x821714C0\` est le site qui écrit \`state+0x6c8+index*0xaab8\` lorsque la gate
est vraie et publie \`global+0x20\`. Le symbole natif est fort et les 26 tests
passent. Les helpers \`0x8218E088\` et \`0x8218EA88\` restent explicitement la
prochaine frontière.

## Gate fermé — helpers de publication de validity natifs

Les preuves détaillées sont dans \`artifacts/mission-helper-native-gate/\`.
Ghidra et le corps généré donnent pour \`0x8218E088\` un test signé \`r3 < 2\`
et une lecture de la table \`0x82391D34\`; \`0x8218EA88\` ne fait que stocker
\`r4\` à \`r3+0x10\`. Les deux symboles sont désormais natifs et les 26 tests
passent. Les xrefs montrent que le second helper est partagé par de nombreux
appelants, ce qui justifie cette couverture au point d'exécution commun.

## Gate fermé — appelant validity \`sub_8216DB10\` natif

Les preuves détaillées sont dans \`artifacts/mission-8216DB10-static-gate/\`.
Ghidra et le corps généré concordent sur l'appel à \`0x820E9300\`, le choix
\`0/1\`, la table \`0x82391CDC\`, la publication globale et les stores
\`param-0x24=3\`, \`param-0x5c=2\`. Le helper et l'appelant sont natifs; les
26 tests passent.

## Gate fermé — parent validity \`sub_8216D760\` natif

Les preuves détaillées sont dans \`artifacts/mission-8216D760-static-gate/\`.
Le projet Ghidra démo et \`ppc_recomp.13.cpp\` concordent sur l'objet \`r3\`, le
prologue \`__savegprlr_29\`, la publication globale, les appels virtuels
\`vtable[13]\`/\`vtable[14]\`, le choix \`mission+207\`/\`223\`, les deux enfants
récursifs et les stores \`+0xc\`, \`+0x44\`, \`+0x60930\`. Le natif recharge la
vtable avant le second appel et conserve \`r29\`/\`r30\`/\`r31\`/LR. Le
dispatcher qualifie les deux LR exacts. Le symbole est lié dans
\`ac6-demo-recomp\`, l'audit de complexité passe et CTest passe à 26/26.

## Gate fermé — enfants récursifs de \`sub_8216D760\` natifs

Les preuves détaillées sont dans \`artifacts/mission-8219EE40-EC88-static-gate/\`.
Les corps Ghidra et \`ppc_recomp.17.cpp\` concordent sur les listes chaînées,
les offsets \`+0x1c/+0x24\`, les appels virtuels et les retours récursifs de
\`sub_8219EAA0\`, \`sub_8219EC88\` et \`sub_8219EE40\`. Le natif conserve les
registres callee-saved et le LR, recharge les vtables à chaque dispatch et
laisse la mémoire invalide remonter comme dans le chemin PPC. Les LR/slots
\`EAE8/1\`, \`ECBC/3\` et \`EE94/3\` sont les seuls ajouts au dispatcher pour
ce gate; \`EEB0\` est un appel direct natif. Build, audit, CTest (26/26),
\`git diff --check\` et les symboles forts passent.

## Gate fermé — mutateur partagé \`sub_8219ECF8\` natif

Les preuves détaillées sont dans \`artifacts/mission-list-producers-static-gate/\`.
Les xrefs démo donnent 249 appels directs à \`sub_8219EE40\`, 40 à
\`sub_8219EC88\` et deux à \`sub_8219EAA0\`; le slicing des appelants
identifie \`sub_8219ECF8\` comme le mutateur partagé qui appelle \`EAA0\`, recycle
la free-list globale \`0x827745ec\` et écrit \`object+0x1c\`. Le corps généré
qualifie les neuf dispatchs \`ED34/1\`, \`ED64/1\`, \`ED78/5\`, \`ED94/14\`,
\`EDA8/12\`, \`EDC0/14\`, \`EDD4/12\`, \`EE10/11\`, \`EE34/1\`; le corps natif
les recharge par vtable et conserve les registres callee-saved/LR. Build,
audit, CTest (26/26), \`git diff --check\` et les symboles forts passent. Les
wrappers producteurs et les implémentations concrètes des slots mission
restent ouvertes.

## Gate fermé — wrappers producteurs et cibles virtuelles mission natifs

Les artefacts détaillés sont dans `artifacts/mission-wrapper-targets-static-gate/`.
Les décompilations Ghidra et `ppc_recomp.17.cpp` concordent pour les cinq
wrappers producteurs : clés, tailles d'allocation `0x1c/0x44/0x101c/0x9c`,
constructeurs, dispatchs conditionnels, free-list `0x827745e0` et insertion
chaînée. Le contrôle de contrats donne une correspondance exacte des 24
couples LR/offset de slot. Le prologue `0x82327104/08` est le `savegprlr`, donc
le global est l'objet d'entrée; cette correction évite une fausse dépendance.
Les getters de slots `0x82311960` (+4) et `0x820d2c60` (+8) sont natifs. Le
dispatcher contient toutes les qualifications LR/slot; build, audit, CTest
(26/26) et symboles forts passent.

## Gate fermé — appelants directs des wrappers producteurs mission natifs

Les artefacts détaillés sont dans
`artifacts/mission-wrapper-callers-static-gate/`. Le projet Ghidra démo
canonique qualifie 36 xrefs directs et 21 fonctions appelantes. Les corps
Ghidra et générés des helpers `0x8219E428`/`0x8219E768` donnent les mêmes
offsets. Les six petits appelants `0x821A00E8`, `0x821A0180`, `0x821A0240`,
`0x821A02C0`, `0x821A0338` et `0x821A03A8` ont été remplacés au point natif,
avec mappage exact des producteurs, champs, valeurs, scratch et restauration
ABI; les six appels virtuels slot 1 sont qualifiés par LR exact. Les assertions
de contrats (`closure-contract-assertions.txt`), le build, l'audit, CTest
(26/26) et l'installation passent (`validation-*.log`). Cette preuve reste
statique: elle ne conclut pas que Mission 01 est atteinte au runtime.
## Gate fermé — runtime borné: alias free-list avant Mission 01

Les artefacts détaillés sont dans `artifacts/mission-runtime-reachability-gate/`.
L'import de la source démo réussit, puis le probe borné s'arrête au tick 61 sur
`0x8219ECBC` avec une cible nulle. Le watcher hôte des free-lists qualifie une
écriture de `0x18BB0300` dans `0x827745E0` par `ac6_mission_take_node`; le
watcher du mot 0 qualifie ensuite l'écriture de `0x18960180` au même objet par
la même aide. Le snapshot virtuel est cohérent avec un slot 3 mappé mais nul,
et quatre appels valides suivent au même LR. `producer-alias-closure.txt`,
`null-slot3-freelist-host-watch.summary`, `null-slot3-object-host-watch.summary`,
`object-host-callers.txt` et `generated-f080-free-list.txt` contiennent les
preuves compactes. Le contrôle de flux généré et l'aide native concordent; la
cause restante est le producteur/chaînage de la free-list, pas le dispatcher.

## Gate fermé — preuve ciblée du resolver au tick 67

Les artefacts `artifacts/mission-runtime-reachability-gate/resolver-descriptor-static-gate.txt`,
`resolver-chain-codegen2.log` et `resolver-chain-codegen2.report.json` donnent
la chaîne PPC et la capture minimale. Le descripteur à `0x7F040158` indique
`count=2`, `table=0x18BD1014`; l'appel à `0x821EE0F8` reçoit `index=0x30`, donc
la branche `index >= count` renvoie zéro. Le résultat zéro devient l'argument
`r4` de `0x82278160`, puis `0x821EDF40` tente la lecture `r4+4`, ce qui
confirme le trap `0x82278184`, adresse `0x4`. La capture précédente du même
tick montre au contraire le descripteur `count=1`, entrée `0x1000`, qui produit
`0x18BD1000`; l'anomalie est donc le sélecteur hors bornes, pas l'allocateur ni
le dispatcher. L'instrumentation a été retirée après capture.

La validation finale est consignée dans
`artifacts/mission-runtime-reachability-gate/resolver-chain-final-validation.status`:
build, 26/26 tests CTest, installation et layout passent.

## Gate fermé — producteur statique de l'index resolver `0x30`

La preuve compacte est dans
`artifacts/mission-runtime-reachability-gate/resolver-index-producer-static-gate.txt`
et sa forme structurée `.json`. Les corps PPC générés et le projet Ghidra
`ace-combat-6-demo` qualifient `sub_8219E7B0`: `+0x14` est la racine resolver,
`+0x18` le descripteur et `+0x20` l'index consommé par le second lookup.
`sub_821A03A8` stocke `r8` à `+0x20`; son appelant `sub_82191088` place dans
`r8` le retour du slot virtuel d'offset 160 (`0x82191148`), puis appelle
`0x821A03A8` (`0x82191164`). Cela explique le `0x30` jusqu'à la frontière
virtuelle sans inventer un producteur local. La valeur amont reste
indécidable statiquement; une garde de borne ou un A/B serait hors preuve.
## Gate fermé — cible virtuelle de l’index (offset 160)

La preuve compacte est dans
`artifacts/mission-runtime-reachability-gate/virtual-index-producer-final-static.txt`
et `virtual-index-producer-final-validation.status`. Le constructeur
`sub_8223D4D0` installe le vptr `0x820210D0` sur `objet+0x222D60`; la vtable
Ghidra donne les cibles `0x8223D4C0` (offset 156) et `0x8223D4C8` (offset 160).
Le setter écrit `r4` à `+3164`, le getter le relit, et `sub_82191088` fait
`li r4,48` avant la paire virtuelle. `sub_821A03A8` stocke ensuite ce retour
dans `resolver+0x20`. La valeur `0x30` est donc expliquée par du PPC local,
sans frontière de service ni garde à ajouter.
## Gate fermé — contrat du resolver après l’index `0x30`

La preuve compacte est dans
`artifacts/mission-runtime-reachability-gate/resolver-contract-final-static.txt`,
`resolver-contract-final-static.json` et
`resolver-contract-final-validation.status`. Le corps PPC de
`sub_821EE0F8` qualifie `+0=count`, `+4=base`, `+12=table`, le test
`index < count`, l’adresse `table+(index<<2)` et le retour nul hors bornes ou
sur entrée nulle. `sub_821EDF40` produit ces champs; `sub_82278160`
transmet le résultat et retourne zéro s’il est nul. La capture tick 67
`count=2,index=0x30` est donc une conséquence déterministe du contrat, sans
garde à ajouter ni changement natif.

## Gate fermé — setter object+8 et cible virtuelle slot 16

La preuve statique compacte est dans
`artifacts/mission-runtime-reachability-gate/object8-slot16-static-gate.txt`.
`sub_820D2C68` écrit directement `r4` à `object+8`; le code natif ne surcharge
que le getter `sub_820D2C60`. Le résumé runtime
`object8-slot16-qualified-runtime-summary.txt` montre un appel setter au tick
61 sur `object=0x189600C0` avec `r4=0x18BC0100`, puis la relecture non nulle de
ce même objet par `sub_8219E7B0` au tick 67. Le trap est distinct: `object=0x18960100`
est consommé par `sub_8219E580` avec `object+8=0`. Cela ferme la question du
setter/aliasing sans garde spéculative. La validation finale est consignée dans
`object8-slot16-final-validation.status` (build, CTest, installation et layout
passants).

La prochaine preuve doit relier statiquement le vtable `0x82012DC4` et le
constructeur/producteur de `0x18960100` aux écritures de `object+8`.

## Gate fermé — E550, slot 16 et zéro écrit à `+8`

Preuves compactes : `artifacts/object8-null-producer-gate/vtable-rtti-summary.txt`,
`generated-producer-bodies.txt`, `setter-body.txt`, `gate-decision.txt` et
`F42C-zero-context.txt`. Le RTTI donne pour `0x82012DC4` le slot index 4 vers
`0x820D2C68` et l'index 5 vers `0x8219E580`; `sub_820D2C68` exécute seulement
`stw r4,8(r3)`. F300 construit E550 puis passe `out` au slot 16. La trace du
tick 61 observe trois constructions E550 suivies de stores F42C de zéro à
`object+8`, puis E580 relit ce zéro au tick 67. La thèse « setter absent/NULL
légitime » est réfutée; le prochain test porte sur le producteur de `out`.

## Gate fermé — producteur `out` et cadre ABI F300

Preuves compactes : `artifacts/f300-out-producer-gate/sub_821E1CD8-generated.txt`,
`runtime-stage-summary.txt`, `runtime-frame-outcome.txt` et
`final-validation.status`. Le corps généré réserve `-128(r1)` avant
`sub_821E1CD8`; la trace ciblée observe `out` non nul après A0660, slots 8/28/36
et ECF8, puis nul après E1CD8 lorsque le cadre natif manque. Après ajout du
cadre/restauration PPC dans F300, les trois passages gardent `out` non nul à
E1CD8/E550/F42C et aucun trap n'est observé. Le probe s'arrête à 100 ticks sur
`max_ticks`/budget scheduler avant le jalon mission. Build, 27 tests, install et
layout passent.

## Gate fermé — scheduler bloqué après F300

Preuves compactes : `artifacts/mission-post-f300-gate/outcome-summary.txt` et
`scheduler-summary.txt`. À 300 ticks, `milestone_reached=false`, aucun trap
n'est signalé et la frontière runtime est « all started guest threads blocked
before mission milestone ». Les compteurs sont `runnable=0`, `blocked=23`,
`finished=0`, `slice_exhaustions=201`; la correction ABI F300 reste donc
stable, mais ne suffit pas à atteindre le gameplay visible.

## Gate fermé — frontière statique des réveils d'événements mission

Les implémentations de `block_current_guest_thread`, `wake_guest_waiters`,
`wake_one_guest_waiter`, `publish_guest_event` et `update_guest_timers` sont
indexées dans `artifacts/mission-wakeup-gate/event-contract-extract.txt` et
`artifacts/mission-wakeup-gate/bridge-event-helpers.txt`. Elles couvrent les
clés d'événements, sémaphores, événements noyau, fins de threads et timers.

La capture structurée des attentes et publications est résumée dans
`artifacts/mission-wakeup-gate/event-gate-synthesis.txt`; le résumé scheduler
antérieur compte 23 threads bloqués, et la capture détaillée montre les huit
premiers événements auto-reset sans publication correspondante dans le
snapshot. La preuve déterminante est statique: `lifecycle.hpp` indique que le
client render-driver est seulement stocké, jamais appelé par défaut, et que le
mixer attend sur ces huit événements; le pilote audio de la console est le
producteur absent.

Observations prévues: une publication locale vers l'un de ces huit handles
réfuterait la frontière; une voie de réveil déjà couverte par timeout/fin de
thread la confirmerait; l'absence de liaison objet-producteur laisserait le
cas indécidable. Ici la documentation et le code du bridge ferment le gate,
donc aucun runtime n'a été lancé.

## Gate fermé — frontière audio/XMA du réveil mixer

La preuve compacte est dans
`artifacts/audio-xma-boundary-gate/static-synthesis.txt`,
`callback-contract-windows.txt`, `callback-body-slice.txt` et
`static-oracle-check.txt`. Le callsite généré `sub_8234D0E8` enregistre un
descripteur `(callback, context)` et une taille de frame; le callback
`sub_8236DD98` charge `0x829DA528` puis tail-calle `sub_82355E58`.
Le corps callback appelle seulement `KeSetEvent`/`KeWaitForMultipleObjects`
pour son état invité et ne contient aucun import XAudio/XMA ou
`NtSetEvent`/`NtPulseEvent`.

Le bridge confirme que Register/Submit/Unregister ne réveillent aucun waiter;
Submit est un sink borné, et XMACreate/Release gèrent uniquement les contextes.
`lifecycle.hpp` documente que le callback enregistré n'est jamais appelé par
défaut et que le mixer attend huit événements auto-reset dont le producteur est
le pilote audio console. Le gate est donc fermé sur cette frontière externe,
sans runtime supplémentaire ni injection de publication.

## Gate fermé — contrat externe XAudio/XMA

`artifacts/audio-xma-external-contract-gate/official-source-check.txt` et
`static-boundary-decision.txt` regroupent la recherche Microsoft Learn et la
jonction avec le SDK local Rexglue. Les sources officielles publiques ne
documentent que l'architecture XAudio2 générale (liaison statique Xbox 360 et
thread audio périodique), pas les exports privés du render-driver ni le
contrat XMA kernel. Rexglue fournit une corroboration secondaire de l'ABI du
descripteur et du dispatch callback par worker, sans qualifier les handles
événement PAL.

Les observations discriminantes sont fixées dans
`hypothesis-criteria.txt`: un callback plus un `KeSetEvent` sur une clé du
mixer confirmeraient un producteur; l'absence de publication locale le
réfuterait; l'absence de contrat public laisse la correspondance externe
indécidable. Le gate est fermé statiquement: la frontière reste le pilote
audio/XMA console, sans runtime ni signal synthétique.

## Gate fermé — producteur guest de l'état XAudio

`analysis/demo/ac6-demo-static-semantics-v1.json` et les fenêtres PPC
`artifacts/audio-xma-client-state-producer-gate/` établissent la chaîne de
création. `0x82355F70` écrit le pointeur d'objet à `+0` et le publie dans
`0x829DA528` après l'initialisation des tables `0x820653A8`/`0x820653BC`.
Les deux atlas d'exécution bornés observent `0x8234F600`, `0x8234F058`,
`0x82356410`, `0x82355F70` et `0x82356070` une fois au tick 106; l'arête
indirecte vers `0x8234F058` a `lr=0x821CD4B8`.

Cela ferme l'hypothèse d'un constructeur guest absent. Aucun appel de
`0x8236DD98`, `0x82355E58` ou `0x82355EA8` n'est présent jusqu'au tick 253;
la valeur consommée par le callback et la publication des événements restent
donc une frontière audio/XMA séparée. Voir
`artifacts/audio-xma-client-state-producer-gate/gate.status`.

## Gate fermé — activation bornée du callback XAudio

La capture isolée `artifacts/audio-xma-callback-activation-gate/runtime-cpu4b/`
active explicitement le bridge expérimental sur un store copié et un backend
headless. `AC6_XAUDIO_CPU` observe le client `0x1005CEBC` aux ticks 62–67,
donc le global `0x829DA528` est non nul; le rapport de contrôle de flot compte
212 entrées vers `0x8236DD98` (`lr=0`). Le premier trap arrive au tick 67 sur
une lecture guest non mappée à `0x4`, avec `lr=0x82278184`.

Cette sonde confirme l'activation et ouvre seulement la frontière de l'enfant
du callback : elle ne qualifie aucun `KeSetEvent` vers les huit handles mixer.
La tentative initiale avec `--atlas` a été rejetée avant lancement car le CLI
exige un enregistrement/rejeu XAM. Résumé et critères :
`artifacts/audio-xma-callback-activation-gate/runtime-summary.txt`,
`hypothesis-criteria.txt` et `gate.status`.

## Gate fermé — cause du trap dans l'enfant du callback XAudio

Les corps PPC générés, utilisés seulement comme preuve littérale de flot/ABI,
établissent la chaîne `0x8236DD98 → 0x82355E58 → vtable+0x44 → 0x82278160 →
0x821EDF40`. Le premier accès de `0x821EDF40` est `lbz r11,4(r4)`. Le rapport
`runtime-cpu4b` donne `r4=0`, `address=0x4`, `lr=0x82278184` au tick 67 : la
cause du trap est donc un descripteur/contexte `r4` absent, non un client
`r3` nul. Le bridge n'est pas modifié.

Le même dispatcher contient un `KeSetEvent` conditionnel au callsite
`0x82355EA8` (champ `object+0x130`). Les publications observées dans la
capture ne portent pas ce callsite et ne sont pas reliées aux huit handles du
mixer. Le producteur de `r4` et la sémantique des handles restent ouverts.
Références : `artifacts/audio-xma-callback-child-gate/static-child-decision.txt`,
`bridge-dispatch-window.txt` et `gate.status`.

## Gate fermé — producteur du descripteur `r4`

Les fenêtres PPC et l'import runtime établissent que le seul `r4` de
`XAudioRegisterRenderDriverClient` est le pointeur de callback
`0x8236DD98`; le wrapper construit ensuite `[callback, callback_context]` et
transmet séparément le pointeur de handle. Le callback enfant ne réinitialise
pas `r4`, puis `0x821EDF40` lit `r4+4`; la capture CPU4 voit exactement
`r4=0`, `address=0x4`. Les recherches Ghidra de branches, pointeurs et
matérialisations n'ont trouvé aucun producteur guest direct.

Le résultat est une frontière ABI externe, pas une preuve pour le contrat
privé XDK/XMA. L'absence d'attribution du `KeSetEvent` `0x82355EA8` aux huit
handles mixer reste également ouverte. Détails :
`artifacts/audio-xma-r4-descriptor-gate/static-r4-producer-decision.txt`,
`indirect-callers-exact.txt`, `hypothesis-criteria.txt` et `gate.status`.

## Gate fermé — propriétaire de l'événement XAudio statique

Les fonctions `0x82356070`, `0x82355818`, `0x82355CE0` et `0x82355E58`
forment une chaîne cohérente autour de `0x829DA518` : construction,
publication, attente puis réutilisation avant attente multiple. Le même
constructeur initialise le sémaphore `0x829DA4F4` (limite 6) et l'objet pair
`0x829DA508`. Le census des références statiques ne trouve pour `0x829DA518`
que ces quatre propriétaires XAudio; aucune référence de registration ou de
handle mixer n'est qualifiée. `0x829DA4E4` est l'événement distinct de
terminaison utilisé par `0x823550F8`.

Le bridge actuel couvre ce layout manuel : `KeWaitForSingleObject` lit
`object+4`, tandis que `KeSetEvent` y écrit l'état et réveille les waiters.
Le gate ferme donc la question de l'objet interne sans modifier le bridge ni
activer le callback expérimental. Détails :
`artifacts/audio-xma-event-object-gate/static-event-object-decision.txt`,
`address-materialization-census.txt`, `static-ref-owner-map.txt` et
`kernel-dispatch-windows.txt`.

## Gate fermé — frontière externe du contexte enfant `r4`

La source Xenia locale établit une invocation callback à argument guest unique
(`r3`); elle ne constitue pas une preuve du contrat privé Xbox 360. Le binaire
AC6 exige néanmoins un pointeur en `r4` (`lbz r11,4(r4)` dans `0x821EDF40`),
hérité depuis `0x8236DD98 → 0x82355E58 → 0x82278160`, et la capture bornée
confirme `r4=0`, trap `address=0x4`, `lr=0x82278184`. Le contexte enregistré
`object+0x0c` vaut zéro; aucun producteur guest qualifié ni lien CPU4
`+0x2c/+0x30` n'a été trouvé.

La conclusion est une indécidabilité externe XDK/XMA, pas une permission de
fabriquer un second argument. Références détaillées :
`artifacts/next-xdk-xma-r4-context-gate/static-external-context-decision.txt`,
`hypothesis-criteria.txt`, `gate.status` et les fenêtres PPC de
`full-contract-and-target-windows.txt`.

## Gate fermé — writers du slot de contexte XMA

Le contrôle de flux AC6 utilise une table à entrées de `0x60` octets; le slot
`entry+0x40` est testé puis passé en `r3` à `XMACreateContext` par
`0x82357240`. Le constructeur efface toute l'allocation via `dcbzl` dans
`0x821A4B70` avant ces tests, et `0x823567E0` libère puis remet le slot à zéro.
Le champ `entry+0x50` reçoit l'index de lane dérivé de `MmGetPhysicalAddress`.

Ainsi aucun producteur guest de paquets n'est justifié par ce slot. Le pointeur
post-import, les paquets et le registre MMIO `0x823572D8` restent une frontière
XMA privée. Détails :
`artifacts/xma-output-slot-frontier-gate/static-slot-table-decision.txt`,
`hypothesis-criteria.txt`, `gate.status` et `full-static-review.txt`.

## Gate fermé — free-list publique XMA et allocator natif

La source SDK de rang 1 `sdk/xdk-xenon-6132.6/XDK/include/xbox/xmadecoder.h`
déclare `IXMAContext` opaque, `XMACreateContext(PXMACONTEXT*)` avec out-pointer
et `XMAReleaseContext(PXMACONTEXT)` en `VOID`; sa documentation associe la
libération à une free-list. Elle ne qualifie ni la taille ni le layout du
contexte privé.

L'allocator natif réutilise désormais un slot inactif avant d'avancer son
index haut-water. Le test isolé libère trois contextes puis observe une adresse
déjà libérée; les gardes de propriété, d'alignement et de double release restent
actives. Build et ctest passent 27/27. Aucun changement de contrat guest ou de
chemin audio n'est justifié par ce gate.

Détails : `artifacts/xma-context-free-list-gate/static-sdk-decision.txt`,
`hypothesis-criteria.txt`, `gate.status`, `validation.txt` et
`full-build-test.log`.

## Gate fermé — couche privée XMA identifiée par PDB officiel

Les PDB officiels locaux exposent les types privés `_XMA_CONTEXT_DATA` et
`_XMA_REGISTERS`, le cycle de vie `CXMADecoder` et ses pointeurs d'état
matériel (`m_pHWContexts`, `m_pRegisters`). La chaîne `No free client contexts`
confirme une gestion interne des contextes. Cette preuve est de rang 1 pour
la forme de la couche, mais pas pour les offsets, les adresses de registres ou
l'effet du store indexé de `sub_82357240` à `0x823572D8`.

Détails : `artifacts/xma-private-mmio-gate/rank1-pdb-summary.txt`,
`hypothesis-criteria.txt`, `private-layer-decision.txt`, `gate.status` et
`validation.txt`.

Le flux `llvm-pdbutil --publics` fournit en outre des coordonnées
segment:offset pour `XMACreateContext`, `XMAReleaseContext`,
`XMAInitializeContext`, `XMAEnableContext`, `XMADisableContext` et les
membres `m_pHWContexts`/`m_pRegisters`. Ces coordonnées bornent le prochain
désassemblage du noyau officiel, sans encore qualifier leur translation en VA
guest ni les champs privés. Voir `artifacts/xma-private-mmio-gate/`
`pdb-publics-xma-summary.txt` et `pdb-publics-official.txt`.

## Gate fermé — arithmétique des registres XMA indexés

Le couple binaire/PDB officiel est maintenant validé par les prologues PPC et
la translation `.text` section-relative. Le scan borné des méthodes puis de
toute la section exécutable donne une formule unique : index du contexte par
pas de `0x40`, groupe `index >> 5`, masque `1 << (index & 31)`, puis registre
`base + 4*group`. Les bases observées sont `0x7FEA1A40`, `0x7FEA1A80`,
`0x7FEA1940` et la lecture `0x7FEA1840`.

Les méthodes officielles qualifient les rôles : Enable écrit `1A80` et `1940`,
Release écrit `1A40` et `1A80`, Disable lit `1840`, teste le bit puis écrit
`1A40`. Le scan complet ne trouve aucun autre `stwbrx` suivant cette formule.
La source Xenia locale reproduit l'arithmétique et les callsites génériques,
mais la contribution officielle de `XMAInitializeContext` n'émet pas le store
indexé `1A80`; cette attribution reste donc ouverte.

Détails : `artifacts/xma-private-mmio-gate/xma-kernel-register-addresses.txt`,
`xma-kernel-methods-full.txt`, `pdb-publics-official.txt` et `gate.status`.

## Gate fermé — layout privé XMA : sources statiques épuisées

Le SDK officiel ne livre qu'un contexte opaque et aucune table de registres;
le PDB officiel local nomme `_XMA_CONTEXT_DATA`/`_XMA_REGISTERS` mais son TPI
ne contient aucun record de champ. Les implémentations Xenia/ReXGlue locales
fournissent une table générique et un contexte de 64 octets, utile pour
comparer l'arithmétique mais insuffisant pour attribuer les noms `Kick`,
`Lock` ou `Clear` au PAL. Le binaire officiel garde seuls les rôles
Enable/Release/Disable déjà qualifiés au gate précédent.

Ainsi le layout privé, les registres directs, le propriétaire de `1A80` dans
le chemin d'initialisation et l'effet du store `0x823572D8` restent ouverts.
La prochaine observation doit être bornée au contexte pointé, aux accès des
quatre familles MMIO et aux enfants immédiats; aucune sémantique ne doit être
activée entre-temps.

Détails : `artifacts/xma-register-layout-gate/static-decision.txt`,
`indexed-semantic-crossmatch.txt`, `official-layout-search.txt`,
`hypothesis-criteria.txt`, `gate.status` et `validation.txt`.

## Gate fermé — capture bornée du contexte XMA après `0x823572D8`

La variante codegen-on recompilée avec l'observateur lecture seule passe les
27 tests. Une exécution `probe` bornée (route `buttons16`, entrée au tick
252, audio dummy) accepte trois stores à `0x7FEA1A80` au tick 916. Les
contextes `0x2E7FF000`, `0x2E7FF040` et `0x2E7FF080` livrent chacun le même
vecteur de seize mots nuls. Le log ne contient aucun enfant post-store; la
décompilation de `0x82357240` confirme aussi le retour immédiat après la
barrière.

Cette observation réfute une transition de contenu du contexte pour les trois
premiers stores. Elle ne qualifie pas l'effet matériel du registre: aucun
consommateur PCM ou device-facing n'est atteint dans cette fenêtre. Le bridge
reste fail-closed; l'observateur est le seul changement de code.

Détails : `artifacts/xma-runtime-state-gate/capture-analysis.txt`,
`capture-criteria.md`, `context-watch-capture/buttons16.stderr.log`,
`context-watch-trace-summary.txt`, `build-codegen-on-context-watch.log` et
`ctest-context-watch.log`.

## Gate fermé — recherche statique du consommateur post-store

Dans le projet Ghidra PAL qualifié, aucune matérialisation fixe de `1A80`,
`1940` ou `1A40` n'est trouvée; la seule matérialisation directe est `1840`
dans `0x82357050`. Les xrefs de l'aperture se limitent à quatre fonctions de
cycle de vie. La décompilation confirme que `0x82356528` prépare les tables,
appelle `0x82357240`, puis que `0x82357240` ne fait qu'initialiser le contexte,
écrire le one-hot et exécuter la barrière avant son retour.

Le consommateur device-facing de `1A80` reste donc ouvert. Les appels
ultérieurs `0x82357310`/`0x823575A8` sont les prochains candidats statiques;
aucune sémantique de registre n'est ajoutée au bridge.

Détails : `artifacts/xma-runtime-state-gate/post-store-static-decision.txt`,
`post-store-consumer-ghidra.log`, `post-store-consumer-decomp.log`,
`post-store-callers-ghidra.log`, `xma-callgraph-ghidra.log` et `gate.status`.
## Gate fermé — chaîne XMA ultérieure et état guest distinct

Le PAL qualifié fournit un état distinct du create-store `0x7FEA1A80` :
`0x82357458` lit le registre direct assemblé `0x7FEA1818..0x7FEA181B`,
compare l’index courant, puis effectue les copies/cache-clears des entrées;
`0x823575A8` flushe les blocs, recopie vers `entry+0x40`, et émet enfin le
one-hot `0x7FEA1940`. `0x82357390` écrit aussi `0x7FEA1804=0x03000000` dans
son chemin de récupération. Ces observations ferment la chaîne guest, pas la
traduction en registre matériel ni la production PCM.

Détails : `artifacts/xma-runtime-state-gate/later-chain-static-decision.txt`,
`later-chain-extract.txt`, `post-store-callers-ghidra.log` et `gate.status`.

## Gate fermé — consommateur noyau ISR XMA

Le couple binaire/PDB XDK qualifié expose `CXMADecoder::InterruptServiceRoutine`.
La séquence PPC à `0xACE68` lit `0x7FEA1808`, discrimine trois bits, puis
acquitte/commande `0x7FEA1A08` par `stwx` et `eieio`. L'association est donc un
consommateur noyau statique qualifié; les noms de registres, les offsets privés
et l'effet device-facing restent indécidables à ce stade.

Détails : `artifacts/xma-kernel-consumer-gate/decision.txt`,
`isr-static-summary-corrected.txt`, `register-role-inspect.txt`,
`official-register-name-search-absolute.txt` et `gate.status`.

La piste layout est close négativement pour ce cycle : les headers publics sont
opaques, le TPI du PDB apparié ne contient aucun record de champ et la source
Xenia ne nomme pas ces registres directs. Une capture éventuelle doit donc
observer uniquement `0x1808`/`0x1A08` et leur ordre; voir
`artifacts/xma-kernel-consumer-gate/next-observation.md`.

## Gate fermé — ISR XMA non instrumentable dans la route native

Les variantes codegen-on et codegen-on-b n'exposent dans le chemin généré que
`XMACreateContext`/`XMAReleaseContext`; aucun accès direct `0x1808`/`0x1A08`
n'est généré. Le bridge ne mappe pas ces adresses et une lecture absente est
piégée avant la trace tardive. La capture H1/H2 ne peut donc pas être exécutée
sans inventer une implémentation noyau/MMIO; les hypothèses restent
indécidables et aucune modification native n'est faite.

Détails : `artifacts/xma-kernel-consumer-gate/native-isr-decision.md`,
`native-isr-instrumentability.txt`, `native-isr-validation.txt` et
`gate.status`.

## Gate fermé — ABI guest des imports XMA create/release

Le SDK officiel et le cross-match PPC qualifient l'ABI minimale : contexte
opaque, slot out unique en `r3`, retour HRESULT testé à `0x82357298`, puis
relecture du pointeur pour `MmGetPhysicalAddress`; la libération est `VOID` à
un argument. Aucun import XMA d'initialisation/activation n'est appelé dans le
XEX généré, et aucune sémantique PCM ou registre privé n'est ajoutée.

Détails : `artifacts/xma-import-abi-gate/decision.md`, `static-extract.txt`,
`validation.txt` et `gate.status`.

## Gate historique supersédé — réconciliation d’adresse post-création XMA

Les lignes suivantes sont conservées pour la traçabilité; leur interprétation
du slot global est invalidée par la preuve de producteur ci-dessous.

Les deux sorties codegen partagent le calcul `MmGetPhysicalAddress` →
soustraction `0x829DA52C` → index 16 bits → `stwbrx`. `PPC_MM_STORE_U32` est
redéfini vers le bridge sans transformation et `GuestMemory::map_mmio` fait
une recherche exacte. La valeur immédiate exige un index inférieur à `0x20`;
les allocations natives sont page-alignées tandis que `0x829DA52C` finit par
`0x52C`, ce qui rend ce résultat exact impossible avec l’identité actuelle.
Si le tableau XMA est la première allocation (`0x10000000`), l’exemple est
`0x896B` → `0x7FEA2BAC`. Aucun alias ou canonicaliseur n’est présent, et seul
`0x7FEA1A80` est mappé.

Cette preuve ferme l’hypothèse d’une traduction cachée et laisse ouverte la
source de l’adresse physique XMA attendue par le PAL. Aucun code n’a été
modifié.

Détails : `artifacts/xma-post-create-gate/address-reconciliation-summary.md`,
`address-reconciliation.txt`, `validation.txt` et `gate.status`. La conclusion
de mismatch est supersédée : `0x829DA52C` est un slot écrit par
`sub_82356510`, non une base constante.

## Gate fermé — producteur de la base XMA qualifié

Le code PAL de `sub_82356510` effectue `lwbrx` depuis `0x7FEA1800`, puis
`stw` vers `0x829DA52C`. Le consommateur `sub_82357240` recharge ce slot après
`MmGetPhysicalAddress`, calcule `(P - base) >> 6` et forme le `stwbrx` de
`0x7FEA1A80`. Le bridge natif expose le même tableau via `0x7FEA1800` et
préserve son adresse dans `MmGetPhysicalAddress`; la trace bornée observe la
même valeur aux quatre points.

Cette preuve invalide l'interprétation précédente d'un global fixe et ne
justifie aucun patch de traduction ou d'aperture. La sémantique privée du
contexte reste ouverte.

Détails : `artifacts/xma-physical-producer-gate/producer-summary.md`,
`validation.txt`, `checkpoint.txt` et `gate.status`.

## Gate fermé — consommateur privé tardif XMA (structure guest)

Les fonctions PAL `0x82357310`, `0x82357390`, `0x82357458` et `0x823575A8`
forment une chaîne distincte du create-store `0x7FEA1A80`. Le scan qualifie
les entrées stride 96, l'index à `+80`, les copies vers `+64`, les flags de
gestion et les one-hot indexés `0x7FEA1A40`/`0x7FEA1940`; les accès directs
`0x7FEA1804` et `0x7FEA1818` sont également confirmés. Les constantes PPC et
la représentation wire passent la validation locale.

Cette preuve ne nomme aucun registre privé et ne démontre aucun effet PCM ou
audible. Le bridge reste inchangé et fail-closed. Une future observation, si
elle devient nécessaire, doit être bornée aux quatre apertures et aux entrées
concernées, avec critères de confirmation/réfutation préenregistrés.

Détails : `artifacts/xma-private-consumer-gate/decision.md`,
`formula-validation.txt`, `source-anchors.txt`, `validation.txt` et
`gate.status`.

## Gate fermé — formats de fetch Xenos atteints

Le snapshot AC6 atteint joint les fetches position/couleur aux formats Xenos
`57`/`38`, avec leurs registres, offsets, strides et swizzles. Le catalogue
Xenos local confirme respectivement `FMT_32_32_32_FLOAT` et
`FMT_32_32_32_32_FLOAT`. Les exports de position et d'interpolateur sont
observés statiquement, mais ni les mots de microcode ni le vertex buffer ne
sont publiés, et la sortie du pixel shader n'est pas jointe. Le traducteur
natif reste donc refusé sans modification.

Détails : `artifacts/shader-static-frontier-gate/decision.md`,
`source-anchors.txt`, `validation.txt` et `gate.status`.

## Gate fermé — producteur et adressage du vertex buffer atteint

Le chemin Vulkan charge 108 octets depuis `0x127CA03C`; les 84 premiers
octets sont les 21 dwords du draw normal et les 24 derniers commencent au
fetch de resolve `0x127CA090`. Avec `Features(false)`, le traducteur ReXGlue
choisit quatre buffers, 25 bits d'adresse locale dword et le binding 2 pour
`0x127CA03C`. L'offset local en octets `0x027CA03C` concorde avec le masque et
l'écriture native.

La sémantique de la sortie pixel et le writer EDRAM restent ouverts; aucun
effet de rendu n'est revendiqué.

Détails : `artifacts/vertex-buffer-producer-gate/decision.md`,
`arithmetic-validation.txt`, `source-anchors.txt`, `validation.txt`,
`gate.status` et `SESSION_ROTATE`.

## Gate fermé — sortie pixel exacte et chaîne writer PM4

La source pixel PAL atteinte est bornée à 36 octets et se désassemble en
`alloc colors; exece; max oC0,r0,r0`. ReXGlue, avec sa modification pixel par
défaut, initialise le seul registre général et la cible couleur 0 à zéro; le
SPIR-V exporte ensuite ce registre vers la couleur 0. La validation SPIR-V est
passée et la couverture observée remplace le sentinel sur les 921600 samples,
avec 230400 pixels résolus à zéro.

Le draw RT0 et le `RB_COPY` sont reliés à leurs writers guest et à leur ordre
dans l'IB. Ils restent des writers de paquets PM4 : aucune preuve statique ou
native actuelle ne donne les octets des samples EDRAM guest-owned avant le
copy. La projection native de resolve est donc bornée au readback du draw.

Détails : `artifacts/pixel-output-edram-gate/decision.md`,
`source-anchors.txt`, `arithmetic-validation.txt`, `observations.md`,
`validation.txt` et `gate.status`.

## Gate fermé — frontière statique pixel non-bootstrap

Les routes neutre et START inspectées ne chargent aucun pixel différent du
bootstrap de 9 dwords. `neutral-first` a 24 draws point; le main/START a
trois loads (`27V, 9P, 15V`) et deux rectangles. Les cinq containers pixel
NSXR de 60 octets restent des candidats non joints; les deux containers de
36 octets sont les seuls atteints. Le writer RT0/`RB_COPY` reste qualifié au
niveau paquet, pas au niveau samples EDRAM.

Détails : `artifacts/next-nonbootstrap-gate/decision.md`,
`route-shader-census.txt`, `source-anchors.txt`, `validation.txt` et
`gate.status`.

## Gate fermé — fenêtre runtime post-START

Le chemin legacy `ac6-oracle-run.py` n'a pas démarré le jeu : son ancienne
commande a été refusée par le CLI courant. Le `probe` codegen-on de remplacement
est déterministe et borné : START unique au tick 3 000, relâchement au tick
3 001, budget 3 036. Il termine sur `max_ticks`, avec 2 928 présentations mais
aucun jalon frontend/mission/terminal; le diagnostic rapporte tous les threads
guest bloqués avant frontend. Aucun événement graphique ou EDRAM n'est publié
dans la trace, et le rapport ne joint pas de payload pixel.

La preuve est donc indécidable pour le pixel non-bootstrap et n'autorise aucun
runtime plus large. Le prochain discriminant est le point d'attente guest/kernel
identifié par le rapport, à qualifier statiquement avant toute nouvelle capture.

Détails : `artifacts/post-start-runtime-window/decision.md`,
`validation.txt`, `direct-probe-20260821-a/graphics-ring-summary.txt`,
`direct-probe-20260821-a/trace-events-summary.txt` et `gate.status`.

## Gate fermé — identité du blocage guest

Le thread primaire du probe reste bloqué sur `0xE000004C` après la ré-entrée
`0x822E559C -> 0x822F8848`. Le callsite `0x821A69C8` est joint statiquement à
`NtSignalAndWaitForSingleObjectEx` ordinal 251, et les wrappers PAL voisins
`NtSetEvent`/`NtPulseEvent`/`NtClearEvent` sont présents dans le même atlas.
Le body dynamique est RTTI-joint mais sa sémantique applicative demeure
inconnue. La preuve établit la frontière d'attente, pas le producteur qui devrait
réveiller l'événement; aucun patch n'est fait.

Détails : `artifacts/static-guest-block-gate/decision.md`,
`current-waits-compact.txt`, `guest-wait-source.txt`, `event-caller-list.txt`
et `gate.status`.

## Gate fermé — producteur d'événement

La publication initiale de `0xE0000048/0xE000004C`, le `NtSetEvent` du wait et
le `NtSignalAndWaitForSingleObjectEx` du couple sont tous joints à des PCs PAL
exacts. La preuve reste locale à la route bornée : elle ne démontre pas que le
writer est encore planifié au frontier post-START ni que le réveil suffit à
atteindre frontend.

Détails : `artifacts/static-event-producer-gate/decision.md`,
`producer-detail.txt`, `event-handle-values.txt`, `validation.txt` et
`gate.status`.

## Gate fermé — ownership/scheduling du writer

Le writer de `0xE000004C` est exécuté par le thread 12 via
`0x822E3EC0 -> 0x822EEE10`. Au frontier, thread 12 attend `0xE0000040`, tandis
que le scheduler mono-host ignore toute fibre bloquée et n'accepte qu'un wake
sur la paire exacte. L'absence de progression est donc une dépendance guest,
pas une conséquence de Wine ou d'un manque de parallélisme host.

Détails : `artifacts/static-event-writer-ownership-gate/decision.md`,
`ownership-detail.txt`, `source-ownership-extract.txt`, `frontier-thread12.txt`
et `gate.status`.

## Gate fermé — producteur de `0xE0000040`

`sub_822E3F48` initialise le handle dans le champ `+88` à `0x82934760`.
`sub_822E5660` construit `0x82934708` et appelle `sub_822E3EB8`, qui charge
ce champ et appelle `sub_821A6AB0`. Les atlas neutral/start t253 relient la
chaîne au thread 2 via l'arête indirecte LR `0x821C5178`, 252 fois aux ticks
1–252. La présence du callback au frontier post-START n'est pas démontrée.

Détails : `artifacts/static-event-producer-0xE0000040-gate/decision.md`,
`producer-detail.txt`, `generated-event0040-candidates.txt`,
`report-index.json` et `gate.status`.

## Gate fermé — enregistrement du callback producteur

Le chemin d'installation est `0x822F85B8 -> 0x822F85A8 -> 0x822E5670`, puis
`0x821C5D68` écrit `0x822E5660` dans le slot `+16520`. Le dispatcher
`0x821C5090` charge ce slot et le branche indirectement à LR `0x821C5178`;
les atlas qualifient ce dispatch sur le thread 2. Cette preuve ferme
l'enregistrement, mais pas l'identité complète de l'objet ni son affinité
Xenon.

Détails : `artifacts/static-callback-registration-gate/decision.md`,
`registration-detail.txt`, `generated-callback-registration-extract.txt`,
`validation.txt` et `gate.status`.

## Gate fermé — identité de l’objet et affinité non publiée

Le retour de `sub_821BB4C8` est conservé dans `stack+80`; son appel depuis
`sub_822F85B8` fixe `r4=1`, `r6=1` et produit `0x0C000001` dans l’état de
l’objet. Ce pointeur devient l’argument de registration de
`sub_822E5670`, qui écrit `0x822E5660` au slot `+16520`. La jonction objet →
callback est donc statique et exacte.

Le chemin ne joint pas le wrapper d’affinité `sub_821A5390`; les callsites de
ce wrapper sont séparés et l’atlas dynamique n’enregistre aucun appel du
thread 2. Cela réfute une attribution statique de l’affinité à partir de cette
API, sans prouver une valeur CPU implicite. Une observation ciblée du pointeur
`r31` au dispatcher `0x821C5090` est le prochain discriminant minimal.

Détails : `artifacts/static-owner-object-affinity-gate/decision.md`,
`owner-affinity-detail.txt`, `report.json`, `validation.txt` et `gate.status`.

## Gate fermé — observation runtime ciblée du dispatch

Le probe headless natif avec audio SDL factice atteint la borne de 3036 ticks.
L'arête `LR=0x821C5178`, cible `0x822E5660`, s'exécute sur le thread invité 2
avec `r31=0x10041A00` et `r11=0x822E5660`. La trace d'affinité contient 19
transitions valides sur les autres threads; le thread 2 n'en a aucune dans la
fenêtre. La preuve dynamique confirme l'objet et le callback, mais ne publie
pas de CPU pour ce thread.

Détails : `artifacts/bounded-dispatch-object-affinity-gate/runtime-summary.txt`,
`runtime/frontier.report.json`, `runtime/stderr.log`, `validation.txt` et
`gate.status`.

## Gate fermé — validation locale native et O3

Les 27 tests CTest du build existant passent avec l'audio SDL factice. Les
contrats affinité Xenon, callback XAudio, interruption Xenos, payload renderer,
tiling canonique et copie EDRAM passent également dans la configuration de
référence puis avec `-O3`. Les statuts et sorties de test sont invariants;
aucune modification supplémentaire du worktree n'est apparue.

Détails : `artifacts/native-validation-o3-gate/results.txt`,
`o3-results.txt`, `ctest.log`, `validation.txt` et `gate.status`.

## Gate fermé — observation visuelle native

Le probe natif Vulkan a consommé la borne de 3036 ticks sans frontend ni
mission. Les 2928 notifications de présentation ne deviennent pas des
présentations qualifiées : le compteur reste à zéro, aucun fichier de capture
n'est produit et le diagnostic situe le blocage avant le frontend. Cette
observation réfute seulement la disponibilité d'une screencap native dans
cette fenêtre; elle ne transforme pas les notifications en preuve de
gameplay.

Détails : `artifacts/visual-native-gameplay-gate/runtime-summary.txt`,
`validation.txt` et `gate.status`.

## Gate fermé — producteur et réveil 0xE0000040

Faits observés : le chemin statique du producteur aboutit à 0040 ; la sonde
native publie 0040 depuis le thread 2 aux ticks 1 et 2, réveille exactement le
thread 12, puis observe sa reprise à 821A8C88. Le thread 12 publie 004C avant
de se remettre en attente sur 0040. Le comportement auto-reset concorde avec
le SDK et le bridge natif.

Inférence : l'absence du producteur ou une perte du réveil par le scheduler ne
peut pas expliquer le blocage actuel. La reprise guest et son arête de retour
restent non qualifiées avant le frontend.

Détails : `artifacts/event0040-native-producer-gate/decision.md`,
`runtime-summary.txt`, `static-summary.txt` et `validation.txt`.

## Gate fermé — boucle guest post-réveil

Faits : dans le projet demo qualifié, `0x821A8C50` effectue une attente unique
quand son bridge `0x821A6AF0` reçoit le timeout nul. Le callback `0x822E3EC0`
réattend explicitement 0040 après avoir publié 004C, sous le garde
`state+0x18 == 0`. Les synchroniseurs `0x822E4018`, `0x822E4080` et
`0x822EEE68` attendent un compteur puis repassent par
`NtSignalAndWaitForSingleObjectEx` tant que leur condition n'est pas satisfaite.

Conclusion : la ré-attente runtime est expliquée par le guest et ne justifie
aucun changement natif. L'état qui devrait faire sortir la boucle primaire
reste à qualifier.

Détails : `artifacts/postwake-static-gate/decision.md`,
`static-summary.txt`, `validation.txt` et les sorties Ghidra ciblées.

## Gate fermé — attribution du compteur primaire

La jointure statique du projet `ace-combat-6-demo` ferme la structure de
synchronisation : `0x822DA9C0` appelle `0x822E40E8` avec `owner=0x82934708`,
et le primaire est `owner+0x40=0x82934748`. Le payload `primary+0x10` est
écrit par `0x822EEE10`; le callback `0x822E3EC0` lui fournit les compteurs
successifs après son attente sur 0040.

Les consommateurs `0x822E4018` et `0x822E4080` lisent le payload primaire et
le comparent à `owner+0x10`. La cible n'est pas indéterminée : l'unique appel
`0x821A3CEC -> 0x822DA7F0` fournit le dernier argument `1`, puis
`0x822E52D0` écrit `1-1` à `owner+0x98`, adresse égale à `owner+0x10`.
L'initialisation statique de la cible est donc zéro.

Détails : `artifacts/primary-counter-static-gate/decision.md`,
`static-summary.txt`, `validation.txt`, `gate.status` et `marker-scan.txt`.

## Gate fermé — successeur guest et frontière service

Faits : `0x822DA9C0` mène à `0x822E5540`, puis au contexte de rendu dont la
vtable `0x8202A488` résout `0x822F8578` et `0x822F85B8`. Le résultat de
`0x821BB4C8` est testé avant la suite d'initialisation. Dans la boucle
principale, `0x8238CDA0` pointe vers `0x82386CC0`; sa vtable RTTI `0x82008EF0`
résout `+0x2C` vers `0x820FF988`, puis `+0x24` vers `0x820FF8D8`.

La condition de `0x820FF8D8` lit `0x826E2374`, dont la valeur initiale est BSS.
Les services `0x82822F08` et `0x828819A4` sont eux aussi BSS sans valeur
statique. La liaison vers un jalon frontend reste donc indécidable à cette
frontière, sans justifier un runtime ou un changement natif.

Détails : `artifacts/frontend-successor-static-gate/static-summary.txt`,
`decision.md`, `validation.txt`, `gate.status` et `marker-scan.txt`.

## Gate fermé — écrivains BSS et services statiques

`0x826E2374` a un écrivain worker (`0x820FFCA0`) et un lecteur dans
`0x820FF8D8`. `0x82822F08` est construit par `0x82259FF8` depuis
`0x821A3C30`; la vtable `0x82013084` est RTTI `CAce6TaskManager@ACE6` et les
slots effectivement appelés sont `0x82259D10`, `0x82259DA8`, `0x82259E18`,
`0x82259E90` et `0x82259F58`. Les corps de `0x82259D10` et `0x82259F58`
bornent respectivement l'itération et le retrait des tâches.

`0x828819A4` reçoit `0x823C0D90` depuis `0x823732DC`; l'objet est un
`CLayeredDrawCallBack` (`0x82012C04`) consommé par `0x821A30F0`, `0x821A4690`
et `0x82266400`. Les services sont ainsi attribués, mais aucun consommateur
frontend ou jalon mission n'est encore démontré.

Détails : `artifacts/bss-service-static-gate/decision.md`,
`static-summary.txt`, `validation.txt`, `gate.status`,
`correct-service-slots.raw.txt` et `marker-scan.txt`.

## Gate fermé — premier consommateur frontend

Le chemin de transition `0x821929A8 -> 0x82190B18` sélectionne la fabrique
`0x82191468` (table `0x82391EF0`) dans une branche valide, alloue `0x78`
octets et construit l'objet avec la vtable `0x8200F01C`. Le RTTI qualifie
cette vtable comme `CModeTaskTitle`, dérivé de `CSwgModeTaskBase`,
`CModeTaskBase`, `CAce6Task` et `CSwgListener`.

`0x82190B18` passe l'objet au slot `+0x0C` du gestionnaire; `0x82259E18`
l'insère dans la liste et `0x82259D10` visite ses méthodes. Pour le titre,
`+0x28` est le corps PPC `0x8216C940` et `+0x10` est `0x8218A7A8`, ce qui
ferme la première consommation frontend. Les fabriques alternatives et la
sélection effective du demo restent à qualifier séparément.

Détails : `artifacts/frontend-consumer-static-gate/decision.md`,
`static-summary.txt`, `validation.txt`, `gate.status`, `marker-scan.txt` et
les sorties Ghidra ciblées.

## Gate fermé — provenance de la source compacte du clip

Le décompresseur `0x82278F78` est atteint par `0x8227A898` puis
`0x8227AAC0`. Les appelants `0x82127D40`, `0x8219A060` et `0x821A1170`
initialisent le même contexte générique avec des descripteurs, des tables et
des buffers; aucun ne référence statiquement une ressource ou un constructeur
du clip titre. La source concrète reste donc indécidable à la frontière
statique. La liaison dynamique destination → flux titre demeure une preuve
séparée et n’est pas mélangée à ce gate.

Détails : `artifacts/compact-title-provenance-gate/decision.md`,
`static-summary.txt`, `upstream-slice.txt`, `validation.txt`, `gate.status` et
`marker-scan.txt`.

## Gate fermé — parser local du flux titre

La table de longueurs issue du CFG de `0x823246C0` rend la fenêtre
`0x2DCB2438..0x2DCB2680` décodable jusqu’à `0x2DCB2448 = 0x1A`. Le parser
explique les 23 appels de boîte et les 13 `opcode=7`; les mots restants sont
des valeurs compactées consommées par le chemin par défaut, sans sémantique
mission/niveau encore attribuée.

Le rapport conserve une incohérence d’agrégat (`0x243C = 0x15`) mais la
séquence détaillée et l’alignement `23/23` sont cohérents. Détails :
`artifacts/title-stream-parser-gate/decision.md`, `static-summary.txt`,
`closure-check.txt`, `validation.txt`, `gate.status` et `marker-scan.txt`.

## Gate fermé — absence de producteur natif après START

Le projet `ace-combat-6-demo` / `Default.xex` qualifie les lecteurs mission
`0x82095B80` et niveau `0x820E9290` sur le slot sélectionné. Le probe borné
codegen-on-b injecte START (`buttons=16`) au tick 3000, relève 3000–3004 et
termine à 3005. L’index est 0, la mission reste `2048` à `0x823C2F14` et le
niveau `4096` à `0x823C2F20`; aucun store ne vise ces champs ni le champ mode
`base+0x78`.

La conclusion est limitée à cette fenêtre : aucun producteur natif actif du
triplet n’est observé après START. Une affectation fournie par le bytecode ou
la VM reste ouverte.

Détails : `artifacts/title-film-state-producers-gate/decision.md`,
`EVIDENCE.md`, `runtime-compact.txt`, `validation.txt`, `gate.status` et
`marker-scan.txt`.

Prochaine frontière : qualifier statiquement la source bytecode/VM du film.

## Gate fermé — attribution bytecode/VM du film

Le désassemblage de `0x823246C0` établit une boucle PC/buffer, les longueurs
des opcodes `0..7` et les dispatchs vers les slots `ASContext` `+0x38..+0x50`.
Le chargeur `0x82278F78` dépaquette le flux dans le buffer sur un autre thread.
Un scan du corps de l’interpréteur donne zéro mention de `+0x6C4`, `+0x6D0`,
`+0x78` et zéro référence aux wrappers `0x820EA550`, `0x820EA598` ou au
setter `0x82171988`. Les getters et le setter restent dans le chemin natif
du singleton, hors VM.

Preuves : `artifacts/title-bytecode-vm-static-gate/decision.md`,
`static-aggregation.txt`, `EVIDENCE.md`, `gate.status`, `validation.txt` et
`marker-scan.txt`.

Prochaine frontière : qualifier statiquement les écrivains natifs du niveau
`+0x6D0` et les initialiseurs du triplet hors fenêtre START.

## Gate limité — watcher du bootstrap IB

Le build `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp` exécute exactement
un tick et termine par `max_ticks`. L'état final expose 17 threads bloqués.
Dans la fenêtre `[0x16AE0980, 0x16AE0A40)`, le watcher générique ne relève
aucune écriture. La trace ne relève toutefois aucune soumission d'IB ; le
critère préalable de réfutation n'est pas rempli.

Preuves : `artifacts/ib-bootstrap-writer-runtime-gate/decision.md`,
`BLOCKER.md`, `candidate-ac6-demo-atomic-runtime-1.trace` et
`candidate-ac6-demo-atomic-runtime-1-writes.log`.

## Gate limité — première soumission IB

Deux builds guest indépendants capturent sept IB au tick 0. Tous deux
rapportent pour l'IB ciblé `0x16AE0980`, 48 dwords et des générations de bord
nulles ; aucun store n'apparaît dans la fenêtre de 192 octets. Le démarrage de
session mappe uniquement les pages bootstrap fixes et l'image XEX avant
`run_entry`, ce qui exclut une initialisation pré-guest de la zone heap.

Ce gate seul ne localisait pas le producteur ; le gate suivant établit que les
stores générés directs sont bien couverts. Preuves :
`artifacts/ib-first-submit-observable-gate/decision.md`, `BLOCKER.md`,
`runtime-summary.txt` et `runtime-codegen-1741-summary.txt`.

## Gate fermé — stores PPC générés

Le binaire codegen 1741 expose `AC6_PPC_STORE_U8/U16/U32/U64/U128`. Les cinq
définitions convergent vers `GuestMemory::store_*`; le store 128 bits couvre
les écritures VMX. Ces méthodes appellent déjà le watcher de fenêtre
configurable. Il n'existe donc pas de frontière d'instrumentation distincte à
corriger entre le codegen et `GuestMemory`.

Preuve : `artifacts/generated-store-window-gate/decision.md` et ses résumés
statiques. Aucun code ni fichier généré n'a été modifié.

## Gate limité — mapping et payload du bootstrap IB

Le breakpoint sur `GuestMemory::map_zero` observe une région de 8192 octets à
partir de `0x16ADF000`, qui couvre `0x16AE0980..0x16AE0A40`. La capture ciblée
survient ensuite au tick 0 avec 48 dwords. Le payload n'est pas récupéré : les
retours de `load_u32` sont indisponibles dans ce binaire optimisé.

Preuves : `artifacts/ib-payload-mapping-gate/decision.md`, `BLOCKER.md`,
`observations.log` et `final-classification.txt`.

## Gate fermé — écrivains explicites du niveau

Le scan PPC de `+0x6D0` produit le getter et cinq stores `stfs`. Les segments
de contrôle de flux des cinq candidats montrent des bases globales sans
référence au singleton titre, au sélecteur ou au stride du slot. Les trois
constructions de pointeur `+0x6D0` appellent `0x821F1500`; celui-ci ne stocke
pas à travers `r5` et ne fait que lire `r5+12` et `r5+24`.

Preuves : `artifacts/title-level-writer-static-gate/decision.md`,
`generated-candidate-analysis.txt`, `register-and-callgraph-analysis.txt`,
`indirect-level-pointer-analysis.txt`, `field-registration-analysis.txt`,
`gate.status`, `validation.txt` et `marker-scan.txt`.

Prochaine frontière : constructeur et copies groupées du singleton titre.
## Gate `title-swg-vtable-slot-callsite-static-gate`

- `artifacts/title-swg-vtable-slot-callsite-static-gate/fieldread-0x0c.txt` : 289 dispatches de vtable à `+0x0C` sur l'ensemble du programme.
- `artifacts/title-swg-vtable-slot-callsite-static-gate/BLOCKER.md` : l'offset seul ne permet pas d'identifier l'instance visée.
## Gate `title-swg-context-producer-static-gate`

- `artifacts/title-swg-context-producer-static-gate/r26-producer-slice.txt` : `r4 → r26 → r4` autour de l'appel `0x820D2B18`.
- `artifacts/title-swg-context-producer-static-gate/BLOCKER.md` : le producteur de `param_2` se situe au-delà d'un dispatch virtuel sans appelant de code direct.
## Gate `guest-wait-frontier-static-gate`

- `artifacts/guest-wait-frontier-static-gate/source-ownership.txt` : le checkpoint 1753 relie six kicks XMA au contexte physique sans effet frontend ou scheduler.
- `artifacts/guest-wait-frontier-static-gate/decision.md` : XMA est écarté comme levier immédiat et `RB_COPY` devient la prochaine frontière.
## Gate `rb-copy-surface-static-gate`

- `artifacts/rb-copy-surface-static-gate/producer-and-receipts.txt` : draw RT0 à l'offset 239, `RB_COPY` à l'offset 387 et registres de copie associés.
- `artifacts/rb-copy-surface-static-gate/decision.md` : le resolve est relié au RGBA du draw normal sans source synthétique ; le contenu non noir reste le prochain contrat.
## Gate `rt0-writer-inputs-static-gate`

- `artifacts/rt0-writer-inputs-static-gate/selection-and-constants.txt` : trois références à `normal_draw_command_`, sans affectation ni `emplace`.
- `artifacts/rt0-writer-inputs-static-gate/decision.md` : la sélection du draw RT0 est le premier contrat natif manquant avant le writer EDRAM.


## Gate fermé — payload borné du bootstrap IB

Le test synthétique qualifie la journalisation uniquement pour un IB entièrement
contenu dans la fenêtre. Un probe natif d’un tick sous debugger capture sept IB
et, pour `0x16AE0980`, exactement 24 paires
`C0003600,00010081`. Le catalogue Xenos local et le décodeur natif attribuent
`0x36` à `PM4_DRAW_INDX_2`.

Preuves : `artifacts/ib-payload-trace-gate/decision.md`,
`target-payload-structure.txt`, `opcode-36-static-evidence.txt` et
`gdb-payload-observables.txt`.
# Producteur de l'événement `0xE0000040`

- Le champ `0x82934760` contient `0xE0000040` et correspond à `objet+0x58`.
- La chaîne productrice qualifiée est
  `0x822E5660 → 0x822E3EB8 → 0x821A6AB0` sur le thread 2.
- Les atlas bornés existants observent cette chaîne aux ticks 1 à 252.
- `0x822E3EC0` publie `0xE0000044` et ne constitue pas ce producteur.

## Gate `framebuffer-5800-classification-gate`

- `report-visual-slice.json` : 5 692 notifications, 24 draws, zéro soumission
  de ring et zéro commande `present` typée.
- `final-static-slice.txt` : le resolve et le writeback exigent une commande
  `present`; `VdSwap` se limite à écrire le paquet système guest.
- `decision.md` : absence de framebuffer rendu, et divergence entre l'adresse
  observée `0x1374A000` et la sonde historique `0x137A0000`.

## Gate `present-command-producer-static-gate`

- `producer-consumer-xrefs.txt` : `VdSwap` construit `XE_SWAP`, le processeur
  le convertit en `XenosPresentCommand`, et le renderer consomme cette commande.
- `submission-contract-slice.txt` : la soumission du ring dépend d'une
  publication absente du corridor courant.
- `prior-qualified-doorbell-evidence.txt` : une route antérieure a atteint un
  `present` typé et un writeback, écartant le parseur et Vulkan comme première
  rupture.

## Gate `vdswap-bounded-submit-implementation-gate`

- `build.log` et `core-tests.log` : compilation du runtime et succès du test
  synthétique de soumission du buffer système.
- `ctest.log` : suite globale à 25/26.
- `BLOCKER.md` : le probe de 300 ticks n'a pas produit de résumé promouvable ;
  la vérification dynamique reste indécidable.

### Résolution

- `probe-report.json` et `probe.stderr` : 88 swaps deviennent artificiellement
  88 présentations dans un même lot et déclenchent le rejet du renderer.
- `decision.md` : l'hypothèse de soumission directe est réfutée ; le changement
  est retiré.
- `revert-build.log` et `revert-core-tests.log` : retour au build et au test
  core qualifiés.

## Gate `wptr-821b9bc8-static-gate`

- `prior-doorbell-campaign.txt` : store WPTR du bootstrap dans
  `D3D::CDevice::AddCallsToPrimaryBuffer`.
- `counter-semantics-and-correction.txt` : `0x827AD2F0` et `device+21508`
  sont de l'instrumentation de performance, pas des gates de rendu.
- `decision.md` : la recherche est reroutée vers les callbacks D3D de
  `KickOff`.

## Gate `d3d-kickoff-callback-static-gate`

- `callback-bridge.json` : 8 fonctions et 5 producteurs structurés présents ;
  aucune divergence d'ancre ou de callsite rapportée.
- `image-selection.txt` : image du build courant et taille attendue.
- `BLOCKER.md` : sélection d'image à reprendre par provenance directe, sans
  garde d'empreinte.

## Gate `d3d-callback-image-provenance-gate`

- statut : **FERMÉ** ;
- la règle Ninja nomme directement le `Default.xex` de la démo PAL,
  `build_demo.py` et le manifest Ghidra canonique ;
- le manifest qualifie `ace-combat-6-demo/Default.xex`, base `0x82000000` ;
- `cmp` confirme l'identité octet par octet entre l'image courante et les
  images plates PAL historiques conservées de même format ;
- le scan courant retrouve 33 ancres, 8 fonctions et 5 producteurs ;
- conclusion : garde interne obsolète, routes statiques qualifiées.

Artefacts : `artifacts/d3d-callback-image-provenance-gate/decision.md` et
`historical-input-and-direct-comparison.txt`.

## Gate `post-kickoff-producer-static-gate`

- statut : **FERMÉ**, piste callback réfutée comme publisher ;
- la file de rendu a un producteur actif et aucun consommateur progressant ;
- le writer WPTR ne publie que les lots de bootstrap ;
- l'interruption utile dépend de l'avance du ring et se situe donc en aval ;
- `0x821B9710 → 0x821C5190` consomme l'interruption sans publier le ring ;
- `0x821C4A60 StartWorkerQueue` dépend lui-même d'un paquet du ring.

Artefacts : `artifacts/post-kickoff-producer-static-gate/decision.md`,
`qualified-prior-slices.txt` et `last-effect-evidence.txt`.

## Gate `render-queue-slot-writer-slice-gate`

- statut : **LIMITÉ** ;
- projet `ace-combat-6-demo`, programme `Default.xex`, lecture seule ;
- zéro instruction mémoire D-form avec déplacement immédiat `0x110` ;
- `0x820FF710` reste le témoin positif du C décompilé, donc l'adresse est
  recomposée par registres ;
- la passe par déplacements immédiats est insuffisante pour prouver l'unicité
  du writer.

Artefacts : `artifacts/render-queue-slot-writer-slice-gate/BLOCKER.md` et
`decision.md`.

## Gate `render-queue-slot-pcode-normalization-gate`

- statut : **LIMITÉ** ;
- `FindScaledStoreWriters.java` ajouté comme passe Ghidra lecture seule ;
- calibration obligatoire sur `0x820FF710` avant scan global ;
- premier lancement : entrée du script observée, aucun résultat fermé ni
  exception décisive avant terminaison autour de 30 secondes.

Artefacts : `artifacts/render-queue-slot-pcode-normalization-gate/BLOCKER.md`,
`headless.log` et `decision.md`.

### Résolution

- calibration : `0x820FF710`, un store type `0` ;
- scan complet : writers directs `0x820FF788`, `0x820FF7F8`, `0x820FFA88`,
  `0x820FFB50`, valeurs `1..4` ;
- table `0x82008EF0` : slots `+0x10..+0x1C` vers ces quatre writers ;
- appels calculés : `0x8211758C` vers type `1`, `0x821175BC` vers type `4` ;
- décompilation de `0x82117410` : deux appels sans garde locale.

Artefacts complets : `writers.txt`, `compact-writer-family.json`,
`artifacts/render-queue-writer-caller-condition-gate/decompile.log` et
`table-and-guards-full.txt`.

## Gate `render-queue-writer1-reachability-gate`

- statut : **LIMITÉ** ;
- xrefs Ghidra : un seul appel entrant vers `0x82117410` ;
- site : `0x8210ADB4`, fonction `0x8210A1C0`, appel direct ;
- après initialisation de champs `+0x18C`, `+0x190`, `+0x194`, le callsite
  invoque `0x82117410` puis retourne ;
- la garde dominante exacte n'est pas qualifiée par le C structuré seul.

Artefacts : `artifacts/render-queue-writer1-reachability-gate/xrefs.txt`,
`callers-decompile.log`, `call-guard-context.txt` et `BLOCKER.md`.

## Gate `render-queue-callsite-control-slice-gate`

- statut : **LIMITÉ** ;
- `ControlSliceAt.java` : passe lecture seule, une fonction/un callsite ;
- cible `0x8210ADB4` dans CFG de 502 blocs ;
- 18 branches nécessaires : dispatch 16 bits puis cinq validations répétées ;
- xref unique vers `0x8210A1C0` : `0x82165CC0:0x82165D8C` ;
- `0x823270DC` possède 33 appelants et n'est pas une frontière spécifique.

Artefacts : `artifacts/render-queue-callsite-control-slice-gate/control-slice.txt`,
`helper-xrefs.txt`, `helper-callers-decompile.log` et `BLOCKER.md`.

## Gate `ring-cursor-to-wptr-static-gate`

- statut : **FERMÉ**, prémisse réfutée ;
- `0x821C57D0` modifie un IB déjà référencé et non le curseur du ring primaire ;
- `0x820FF710` produit et `0x820FFCA0` consomme chaque tick ;
- `consumer_changes=0` était un artefact d'échantillonnage après reset ;
- tous les snapshots qualifiés des slots de 96 octets sont nuls ;
- conclusion : chercher le producteur de payload, non un writer WPTR.

Artefacts : `artifacts/ring-cursor-to-wptr-static-gate/decision.md`,
`exact-target-records.txt` et `render-queue-correction.txt`.

## Gate `render-queue-payload-source-static-gate`

- statut : **LIMITÉ** ;
- `0x820FF710` écrit un zéro littéral dans `slot[index]+64` ;
- la fonction ne reçoit aucune donnée de payload ;
- les exports courants exposent deux appels sortants, aucune arête entrante et
  aucune référence statique directe ;
- ils ne prouvent pas l'exhaustivité des writers dynamiques du champ.

Artefacts : `artifacts/render-queue-payload-source-static-gate/BLOCKER.md`,
`producer-body-and-callers.txt` et `producer-xrefs.txt`.

## Gate render-queue-producer-arguments — 2026-08-21

- **Observé:** mapping six arguments: `param1=uVar2`, `param2=slot vtable +4`, `param3=caller param5`, `param4=caller param6`, `param5=caller param7`, `param6=caller param8`.
- **Observé:** branche contenant `0x8210ADB4` commence par `param4 == 1` après troncature 16 bits.
- **Observé:** cinq appels successifs à `0x821080D0`; chaque résultat doit différer de `-1`, puis champ `+0x10C` de ressource doit être non nul.
- **Conclusion:** premier discriminant scalaire est type `1`; première garde dépendante données est résolution de première clé, puis son champ `+0x10C`.
- **Limite:** offsets exacts des cinq clés dans payload et calcul des booléens passés à `0x82117410` non encore qualifiés.
- Détails: `artifacts/render-queue-producer-arguments-gate/decision.md`, `argument-to-guard-map.json`, `batch5-static-full.txt`.

## Gate render-queue-type1-payload-layout — 2026-08-21

- **Observé:** clés 1 à 5 utilisent offsets dynamiques `O1=0`, `O(n+1)=On+2+Ln`; longueur `Ln` est lue à `payload+On`, clé à `payload+On+2`.
- **Observé:** fin des cinq clés `E=10+L1+L2+L3+L4+L5`.
- **Observé:** si taille égale `E`, arguments optionnels valent `true,false,0,false,0`. Sinon bytes `E..E+2` alimentent `bVar29,bVar27,uVar25`; bytes `E+3..E+4` alimentent `bVar30,uVar26`, avec tests d’égalité à `1`.
- **Observé:** `0x8231BCB0` écrit zéro ou valeur calculée au champ `+0x10C`; `0x820CBAB0` écrit zéro byte au même offset selon kind 6/7.
- **Inférence:** paramètre 6 est taille/offset limite 32 bits, pas pointeur de fin absolu.
- Détails: `artifacts/render-queue-type1-payload-layout-gate/decision.md`, `payload-layout.json`, `resource-10c-writer-context.txt`.

## Correction — resource-10c-producer-slice, 2026-08-21

- **Observé:** `0x8231BCB0` valide signatures `RIFF` et `WAVE`, parse données de chunk, puis écrit bytes source `0x38..0x3B` dans sa structure globale `+0x10C` si taille au moins `0x30`; sinon zéro.
- **Observé:** base écrite vient directement de `FUN_823270DC()`, pas du pointeur de record résolu via `DAT_826F6124` dans `0x8210A1C0`.
- **Conclusion:** attribution précédente de `0x8231BCB0` comme producteur render est retirée. Déplacement identique seul ne prouve pas alias.
- **Limite:** inventaire exact des stores D-form `0x010C` manque; launcher Ghidra local introuvable.
- Détails: `artifacts/resource-10c-producer-slice-gate/BLOCKER.md`.

## Gate record-10c-store-enumeration — 2026-08-21

- **Observé:** scan Ghidra canonique trouve 101 stores PPC D-form exacts avec déplacement `0x010C`.
- **Observé:** `0x8210D950` reçoit objet de `0x8210D8A0(param1,param4)`, écrit `param3` à `+0x108`, `param2` à `+0x10C`, marque `+0x31C=1` et copie nom à `+4`.
- **Observé:** `0x8210D9C0` utilise `+0x10C` comme handle, puis libère et met zéro en erreur; `0x8210DA80` fait teardown équivalent; `0x8210DB10` initialise champ zéro.
- **Inférence forte:** famille représente enregistrements nommés avec handle possédé.
- **Non prouvé:** retour de `0x8210D8A0` alias un record référencé par `DAT_826F6124`.
- Détails: `artifacts/record-10c-store-enumeration-gate/BLOCKER.md` et `displacement-010c-stores.txt`.

## Gate resource-record-alias — 2026-08-21

- **Observé:** lookup `0x821080D0` cherche noms dans manager `+0x928`, stride `0x40`, compte `+0x624`, et retourne index stocké à `manager+(slot+0x18A)*4`.
- **Observé:** `0x8210DD70` cherche nom, choisit index libre parmi maximum `0x800`, alloue `0x428` bytes, initialise via `0x8210DB10`, puis stocke pointeur à `manager+(index+2)*4`.
- **Observé:** ce record est passé à `0x8210D950`, qui écrit handle à `+0x10C`.
- **Observé:** `0x8210A1C0` déréférence précisément `DAT_826F6124+(index+2)*4`, puis lit `record+0x10C`.
- **Correction observée:** `0x82327100` est helper de sauvegarde des registres non volatils; l’apparente valeur de retour est un artefact Ghidra.
- **Observé:** `0x8210DD70` utilise son `param1` comme base manager. Le callsite `0x82108A98` lui passe la valeur chargée depuis `DAT_826F6124` à `0x82108A6C`.
- **Conclusion:** ce callsite producteur et `0x8210A1C0` partagent la même table de records.
- Détails: `artifacts/resource-record-alias-gate/BLOCKER.md`.

## Gate resource-manager-base-identity — 2026-08-21

- **Observé:** séquence `0x82327100..0x8232711C` sauvegarde `r26..r31`, sauvegarde LR, puis retourne; nombreux prologues l’appellent.
- **Observé:** `0x8210DD88` conserve le `r3` entrant dans `r31`; toutes les opérations manager de `0x8210DD70` utilisent ensuite `r31`.
- **Observé:** `0x82108A6C` charge le pointeur à l’adresse globale `DAT_826F6124`, puis `0x82108A8C` le passe en `r3` au call `0x82108A98`.
- **Conclusion:** chaîne qualifiée `DAT_826F6124 -> 0x8210DD70 -> 0x8210D950 -> record+0x10C` fermée pour ce producteur.
- Détails: `artifacts/resource-manager-base-identity-gate/decision.md`.

## Gate native-resource-registration-contract — 2026-08-21

- **Observé:** aucune occurrence textuelle des adresses ciblées dans sources maintenues ni build hôte courant.
- **Observé:** `AC6_DEMO_ENABLE_CODEGEN` est désactivé dans build courant; absence d’adresse n’est donc pas preuve d’absence guest.
- **Observé:** sorties XenonRecomp existantes se trouvent sous `recompilation/ace-combat-6-demo/build-codegen-on/codegen/`.
- **Indécidable:** appartenance de `0x82108A98`, `0x8210DD70`, `0x8210D950` et `0x8210A1C0` aux unités générées; inspection symboles interrompue avant synthèse.
- Détails: `artifacts/native-resource-registration-contract-gate/BLOCKER.md`.

## Reprise native-resource-registration-contract — 2026-08-21

- **Observé:** `demo.toml` qualifié contient explicitement les sept fonctions de famille, dont quatre racines ciblées.
- **Observé:** objet guest exporte 12 942 symboles `sub_<adresse>`; quatre racines sont couvertes.
- **Observé:** parcours des relocations a visité 223 fonctions distinctes depuis quatre racines sans erreur de désassemblage.
- **Limite:** parseur d’import construit zéro adresse car schéma réel des 238 entrées reste non qualifié; absence d’import trouvé n’est donc pas preuve d’absence.
- Détails: `artifacts/native-resource-registration-contract-resume-gate/BLOCKER.md`.

## Gate native-import-schema-join-resume — 2026-08-21

- **Observé:** schéma réel `caller -> [callee]` décodé dans le callgraph sauvegardé.
- **Observé:** jointure exacte avec les 228 adresses de thunks qualifiées.
- **Observé:** closures non triviales de 67, 61, 3 et 175 fonctions; zéro thunk atteint dans chaque closure.
- **Conclusion:** absence d'import natif direct prouvée dans cette chaîne statique; chercher le premier appel indirect/vtable.
- Détails: `artifacts/native-import-schema-join-resume-gate/decision.md`.

## Gate resource-indirect-dispatch — 2026-08-21

- **Observé:** 19 fonctions atteignables contiennent `AC6_PPC_CALL_INDIRECT`.
- **Observé:** `0x821075A0`, à profondeur 1 depuis `0x8210A1C0`, contient trois sites indirects.
- **Observé:** LR guest exacts `0x821075E4`, `0x821075FC`, `0x82107614`; cible passée depuis le champ contexte `+0x108`.
- **Observé:** aucun de ces LR n'est qualifié par la table de slots actuelle; le helper conserve toutefois une résolution générique.
- **Indécidable:** objet, slot et cibles possibles avant slice PPC des trois `bctrl`.
- Détails: `artifacts/resource-indirect-dispatch-gate/BLOCKER.md`.

## Gate resource-indirect-dispatch-slice — 2026-08-21

- **Observé:** `0x821075A0` appelle trois fois le même objet global `PTR_PTR_82386C10`.
- **Observé:** les deux premiers dispatchs utilisent `vtable + 0x04`, donc slot 1.
- **Observé:** le troisième utilise `vtable + 0x34`, donc slot 13.
- **Observé:** arguments respectifs `param_3`, champ `param_1+0x1A8`, puis constante `1`.
- **Indécidable:** valeurs de vtable et cibles des deux slots avant slice des producteurs du global.
- Détails: `artifacts/resource-indirect-dispatch-slice-gate/BLOCKER.md`.

## Gate resource-global-vtable — 2026-08-21

- **Observé:** 24 références directes à `0x82386C10`, toutes `READ`.
- **Observé:** aucun xref de store direct vers le global.
- **Observé:** les déplacements de slots corrects sont `0x04` et `0x34`.
- **Limite:** `TraceGlobalVirtualSlot` ajoute un niveau receiver qui n'existe pas dans `0x821075A0`; zéro résultat ne réfute donc pas les dispatchs.
- **Indécidable:** valeur initiale de l'objet, vtable et deux cibles avant lecture statique directe.
- Détails: `artifacts/resource-global-vtable-gate/BLOCKER.md`.

## Gate resource-global-static-chain — 2026-08-21

- **Observé:** `0x82386C10` contient `0x82386C0C` dans l'image démo qualifiée.
- **Observé:** le premier mot de cet objet pointe vers la vtable `0x82008E50`.
- **Observé:** slot 1 = `0x820FEED8`; slot 13 = `0x820FEF70`.
- **Observé:** chaque cible apparaît exactement une fois dans le manifeste codegen.
- **Conclusion:** les trois appels de `0x821075A0` ont des cibles guest compilées; famille réfutée comme service manquant.
- Détails: `artifacts/resource-global-static-chain-gate/decision.md`.

## Gate resource-next-indirect-family — 2026-08-21

- **Observé:** deux fonctions supplémentaires réutilisent les cibles qualifiées slots 1/13.
- **Observé:** `0x821154C0` appelle le slot 20 de son `param_2`.
- **Observé:** `0x8210A1C0` fournit le retour de `0x82220670` comme receiver.
- **Observé:** la branche sœur `0x82115530` appelle le slot 21.
- **Indécidable:** type/vtable et cibles slots 20/21; sortie de décompilation sauvegardée mais synthèse absente.
- Détails: `artifacts/resource-next-indirect-family-gate/BLOCKER.md`.
## Gate limité — producteur des receivers slots 20/21

La décompilation qualifiée de `0x82220670` montre un lookup borné dans un
tableau et un éventuel remplacement par le pointeur `entrée+0x18C`. Les deux
consommateurs sont `0x821154C0` (slot 20) et `0x82115530` (slot 21). La dernière
passe n'a ajouté aucune preuve binaire : le script d'export a rejeté la syntaxe
des arguments avant lecture. Voir
`artifacts/resource-slot20-producer-resume-gate/BLOCKER.md`.
## Gate limité — ABI PPC des receivers slots 20/21

L'export PPC canonique dans
`artifacts/resource-slot20-ppc-abi-gate/ppc-functions.txt` prouve que
`0x8210A1C0` place le conteneur global `+0x308` en `r3` et l'identifiant en
`r4` avant `bl 0x82220670`. Le retour alimente les dispatchs `+0x50/+0x54`.
La classification des vtables reste ouverte; voir `BLOCKER.md` dans le gate.
## Gate limité — routes de production des aliases slots 20/21

Le scan `+0x18C` qualifié isole notamment quatre stores dans `0x8210A1C0` et
un setter adjacent `0x82220550`. Les quatre premiers copient des objets
ressource `record+0x10C`; le setter copie le retour du sélecteur `0x821E1D80`.
Les callgraphs JSON existants ne couvrent pas les appelants du setter et ne
permettent pas encore de borner les vtables. Voir le `BLOCKER.md` du gate.
## Gate limité — appelant unique du setter d'alias

`ReferencesTo.java` et `FindDirectCallsTo.java` sur le projet canonique donnent
un seul appel : `0x82216614 -> 0x82220550`. Le PPC prouve `r3 = conteneur
global+0x308` et `r4 = indice actif normalisé`. La décompilation de
`0x82216498` relie cet indice à la structure `+0x2A4/+0x14`. Les vtables des
éléments sélectionnés restent ouvertes; voir le `BLOCKER.md` du gate.

### Table à deux niveaux alimentant `entry+0x18C`

- PPC de `0x82220550` : `r3 = *(PTR_DAT_823C27E0 + 0x29698)`, `r4/r5` viennent
  de `+0x57/+0x58`, puis le retour de `0x821E1D80` est écrit à `+0x18C`.
- PPC de `0x821E1D80` : sélection de `r3[(r4+1)]`, contrôle de borne via
  `first+0xDC`, puis retour de `(*(first+0xD8))[r5]`.
- La recherche scalaire seule est réfutée comme moyen d'identifier les
  producteurs, car elle mélange de nombreux usages indépendants.
- Artefacts : `artifacts/resource-slot20-alias-selector-args-gate/`.

### Recensement PPC de `0x29698`

- Observé : 120 instructions indexées après matérialisation exacte du décalage,
  réparties dans 114 fonctions.
- Contrôle positif : `0x82220640` dans `0x82220550` figure dans le résultat.
- Écritures candidates observées : `0x821E0B7C` et `0x82212E54`; leur base doit
  encore être reliée au pointeur global avant de leur attribuer un rôle.
- Conclusion : la matérialisation du décalage est nécessaire mais non
  suffisante; la provenance du registre base est le discriminant restant.
- Artefacts : `artifacts/resource-slot20-indexed-table-access-gate/`.

### Producteurs locaux du champ `+0x29698`

- `0x821E0B7C` : `[object+0x29698] = object+0x2969C` dans le constructeur
  `0x821E0A70`.
- `0x82212E54` : `[*(PTR_DAT_823C27E0)+0x29698] = *(param1+0x2E4)` lorsque le
  drapeau est non nul, sinon `*(param1+0x2E8)`.
- La sortie `global-base-classification.txt` est explicitement invalide : elle
  mélange ses entrées et ne respecte pas le CFG. Ne pas l'utiliser comme
  preuve positive ou négative.
- Détails : `artifacts/resource-slot20-global-base-join-gate/BLOCKER.md`.

### Classifieur SSA `ClassifyGlobalIndexedAccesses`

- Le script compile et démarre sur le projet canonique.
- Le lancement échoue avant analyse sur la première adresse sans préfixe; le
  résultat vide n'est pas une observation du binaire.
- Témoins prévus : inclure `0x82220640` et `0x82212E54`, exclure
  `0x821E0B7C`.
- Détails : `artifacts/resource-slot20-global-base-cfg-gate/BLOCKER.md`.

### Calibration SSA du champ `+0x29698`

- Positif : LOAD `0x82220640`.
- Positif : STORE `0x82212E54`, valeur fusionnée depuis `param1+0x2E4/+0x2E8`.
- Négatif : STORE constructeur `0x821E0B7C`.
- L'inventaire de 1119 opérations n'est pas une population valide; seules ces
  calibrations locales sont retenues comme preuves.
- Détails : `artifacts/resource-slot20-global-base-ssa-retry-gate/BLOCKER.md`.
## Allowlist SSA des accès indexés `+0x29698` — 2026-08-21

- Rapport complet:
  `artifacts/resource-slot20-global-base-ssa-allowlist-gate/global-indexed-accesses.txt`.
- Résumé: requested 120, qualified 117, rejected 2, missing 1, loads 116,
  stores 1, failures 0.
- Store qualifié unique: fonction `0x82212DF0`, PC `0x82212E54`.
- PC manquant: `0x82210188`, `lwzx`, sans fonction propriétaire Ghidra.
- Les longues expressions SSA restent dans l'artefact et ne constituent pas
  une preuve pour des PC hors allowlist.
## Qualification PPC de `0x82210188` — 2026-08-21

- Source: `artifacts/resource-slot20-orphan-82210188-gate/instructions.txt` et
  `dump-range.log`.
- `lis r11,0x2; ori r10,r11,0x9698` donne `r10=0x29698`.
- `lis r11,-0x7dc4; lwz r11,0x27e0(r11)` donne
  `r11=*(0x823C27E0)`.
- `lwzx r3,r11,r10` qualifie le load global manquant.
## Appelants de `0x82212DF0` — 2026-08-21

- Xrefs directs:
  `artifacts/resource-slot20-table-object-producer-gate/calls-and-entry.log`.
- Décompilations complètes des appelants:
  `artifacts/resource-slot20-table-object-producer-gate/all-writer-callers-decompile.log`.
- Total: 15 callsites, 9 fonctions propriétaires, zéro échec.
- PPC `0x822164A8`: `or r30,r3,r3`; `0x822164AC`: appel du setter avec
  `r4=0`, prouvant que l'objet est le `r3` entrant.
## Stores `object+0x2E4/+0x2E8` — 2026-08-21

- Inventaire PPC complet:
  `artifacts/resource-slot20-object-field-stores-gate/store-candidates.txt`.
- Décompilation des six familles plausibles:
  `artifacts/resource-slot20-object-field-stores-gate/plausible-stores-decompile.log`.
- Populator commun:
  `artifacts/resource-slot20-object-field-stores-gate/container-populator.log`.
- `0x820A4F58` écrit `iVar15+0xD8 = table` et `iVar15+0xDC = count`, puis
  configure les objets `piVar16` contenus dans cette table.
## Factory `piVar16` — 2026-08-21

- Slice des définitions:
  `artifacts/resource-slot20-pivar16-producer-gate/pivar16-slice.txt`.
- Constructeurs et vtables des conteneurs:
  `artifacts/resource-slot20-pivar16-producer-gate/container-owner-functions.log`.
- Appel producteur: `(**(code **)(*piVar14 + 0x14))(piVar14, kind, byte,
  flag)`; `piVar14` est le `r3` entrant.
- Vtable finale commune des conteneurs: `0x82000B94`.
## Slots receiver 20/21 — 2026-08-21

- Lecture directe de la vtable:
  `artifacts/resource-slot20-container-factory-vtable-gate/vtable-read.log`.
- Factory `0x82093658`:
  `artifacts/resource-slot20-container-factory-vtable-gate/factory-82093658.log`.
- Décompilation des cibles `0x8220E428` et `0x82211040`:
  `artifacts/resource-slot20-container-factory-vtable-gate/receiver-slots-20-21.log`.
- Recherche dans le natif (zéro correspondance):
  `artifacts/resource-slot20-container-factory-vtable-gate/native-address-crossmatch.txt`.
## Attribution des stores tick268 — 2026-08-21

- Contrat du hook: `src/guest_bridge/transition_memory_trace.hpp` journalise
  explicitement le LR avec chaque store.
- Contextes PPC et décompilations qualifiés:
  `artifacts/tick268-divergent-pc-static-slice-gate/`.
- `0x820CDC20`, `0x823255F0` et `0x82325644` sont des retours d'appel;
  les deux adresses différentielles appartiennent aux cadres de pile.
- Verdict consolidé:
  `artifacts/tick268-divergent-pc-static-slice-gate/RESULT.md`.
## Recherche du premier état START persistant — 2026-08-21

- Handoff, procédures et inventaire:
  `artifacts/start-first-persistent-store-static-gate/`.
- Analyses autoritatives tick268:
  `analysis/demo/ac6-demo-start-tick268-store-ab-v1.json`,
  `analysis/demo/ac6-demo-start-tick268-load-ab-v1.json` et
  `analysis/demo/ac6-demo-start-tick268-store-load-order-v1.json`.
- La fenêtre A/B est limitée à `0x2E3C0000..0x2E3F0000`; elle ne couvre pas
  les globals START et ne qualifie aucun store persistant.
- Limite et prochain test:
  `artifacts/start-first-persistent-store-static-gate/BLOCKER.md`.
## Lecteurs et premier effet durable de START — 2026-08-21

- Manifeste statique et rapports qualifiés:
  `artifacts/start-logical-bit-reader-xrefs-gate/`.
- Consommateurs: `0x82170FCC` dans `0x82170F58..0x82171250` et
  `0x82185210` dans `0x82185198..0x821852A8`.
- Premier effet demo: `0x82171128 → [0x827435F8]+0x18 = 1`.
- Les propriétaires des deux consommateurs sont absents de l'atlas exécuté
  des routes bornées; verdict:
  `artifacts/start-logical-bit-reader-xrefs-gate/RESULT.md`.
## Phase des propriétaires START — 2026-08-21

- Inventaire RTTI/dispatcher et rapports structurés:
  `artifacts/start-consumer-owner-factory-gate/`.
- Liste active: `CModeTaskStartUpDemoOffline`, `CTaskLoading`,
  `CTaskModeManager`; aucune mutation après le tick 221 dans la fenêtre
  qualifiée.
- Vtables hors phase: `0x8200C904` et `0x8200E5C4`.
- Frontière suivante: retour de `0x8218CE20` vers `0x82259D10` et chemins
  de retrait `0x82259E18/0x82259FF8`.
- Verdict:
  `artifacts/start-consumer-owner-factory-gate/RESULT.md`.
## Contrat de terminaison CTaskLoading — 2026-08-21

- Atlas et rapports ciblés:
  `artifacts/loading-task-retirement-contract-gate/static-contract-intake.txt`
  et `exact-function-contracts.txt`.
- Listing PPC canonique:
  `artifacts/loading-task-retirement-contract-gate/ghidra-function-listings.txt`.
- État 1: appel virtuel du sous-objet, aucune écriture d'état.
- État 2: compteur `this+0x44`, puis requête globale
  `[0x827435F8]+0x18 = 1`.
- Limite:
  `artifacts/loading-task-retirement-contract-gate/BLOCKER.md`.
## CFG dispatcher de CTaskLoading — 2026-08-21

- Listing isolé:
  `artifacts/loading-task-dispatcher-cfg-gate/four-function-listings.txt`.
- CFG dérivé:
  `artifacts/loading-task-dispatcher-cfg-gate/derived-cfg.md`.
- Le retour du slot 4 est ignoré; le slot `+0x28` est le seul prédicat
  précédant l'update.
- `0x82259E18` insère/active; `0x82259FF8` initialise.
- Verdict:
  `artifacts/loading-task-dispatcher-cfg-gate/RESULT.md`.
## Routage codegen — 2026-08-21

- Procédure locale et inventaire initial:
  `artifacts/resource-slot20-codegen-coverage-gate/procedures-and-catalog.txt`.
- Routage CMake/configuration et outils existants:
  `artifacts/resource-slot20-codegen-coverage-gate/focused-codegen-routing.txt`.
- Recherche dans les produits présents:
  `artifacts/resource-slot20-codegen-coverage-gate/generated-coverage-check.txt`.
- Limite et test discriminant:
  `artifacts/resource-slot20-codegen-coverage-gate/BLOCKER.md`.
## Couverture effective des slots — 2026-08-21

- Requête numérique sur `.pdata` + chunks qualifiés:
  `artifacts/resource-slot20-codegen-manifest-gate/effective-function-query.txt`.
- Résumés des deux builds générés:
  `artifacts/resource-slot20-codegen-manifest-gate/batch4-full.txt`.
- Mapping indirect et callsites générés:
  `artifacts/resource-slot20-codegen-manifest-gate/generated-symbol-crossmatch.txt`.
- Résultat:
  `artifacts/resource-slot20-codegen-manifest-gate/RESULT.md`.
## Frontière native courante — 2026-08-21

- Entrée AC6 du handoff et rapport cycle 1761:
  `artifacts/first-real-native-boundary-gate/ac6-current-and-source.txt`.
- Inventaire borné des frontières déjà capturées:
  `artifacts/first-real-native-boundary-gate/handoff-and-boundaries.txt`.
- Limite et prochain test:
  `artifacts/first-real-native-boundary-gate/BLOCKER.md`.
## Alignement cycle 1761 — 2026-08-21

- Résultat complet déterministe:
  `artifacts/post-resume-first-divergence-gate/trace-divergence-full.json`.
- Résumé compact:
  `artifacts/post-resume-first-divergence-gate/trace-divergence-summary.txt`.
- Verdict:
  `artifacts/post-resume-first-divergence-gate/RESULT.md`.
## Fenêtre START tick268 — 2026-08-21

- Chaîne input et dispatcher:
  `artifacts/start-minimal-observation-window-gate/static-start-evidence.txt`
  et `task-dispatch-continuation.txt`.
- Captures stores/loads tick268 et rapports consommateurs:
  `artifacts/start-minimal-observation-window-gate/dispatcher-and-tick268-evidence.txt`.
- Fenêtre et hypothèse statique:
  `artifacts/start-minimal-observation-window-gate/RESULT.md`.

## Loading vtable identity gate

- Projet qualifié : `ace-combat-6-demo`, programme `Default.xex`.
- Table brute `0x8200F388..0x8200F3B0`, constructeur
  `0x8218BF18..0x8218BF48`, thunk `0x8218CE20..0x8218CE2C` et xrefs de
  `0x8218CCD0` extraits par Ghidra en lecture seule.
- Résultat compact : `artifacts/loading-vtable-identity-gate/RESULT.md`.
- Listings complets :
  `artifacts/loading-vtable-identity-gate/ghidra-vtable-xrefs.full.txt` et
  `artifacts/loading-vtable-identity-gate/ghidra-owner-update.full.txt`.

## CTaskLoading field-transition contracts

- Projet qualifié : `ace-combat-6-demo`, programme `Default.xex`.
- Listings et décompilations de `0x8218CBD8`, `0x8218CCD0`, `0x8218CD78` ;
  xrefs directs des routines d'armement/finalisation et du provider ; fenêtre
  de l'appelant `0x82192D64`.
- Résultat : `artifacts/loading-field-transition-contracts-gate/RESULT.md`.
- Sources détaillées : `exact-contract-input.txt`,
  `decompile-and-callers.full.txt`, `producer-xrefs.full.txt`.

## CTaskLoading async-status provider

- Projet qualifié : `ace-combat-6-demo`, programme `Default.xex`.
- Décompilation/listing de `0x8219EE40` et `0x8219F5D0`, limites voisines et
  xrefs du poll : `artifacts/loading-async-status-provider-gate/provider-ghidra.full.txt`.
- Audit des sources et du câblage natifs :
  `artifacts/loading-async-status-provider-gate/native-coverage-intake.txt` et
  `generated-native-coverage.txt`.
- Conclusion : `artifacts/loading-async-status-provider-gate/RESULT.md`.

## Native loading contract integration gate

- Full static intake: `artifacts/native-loading-contract-integration-gate/startup-static-intake.txt`.
- Exact CLI/session route: `artifacts/native-loading-contract-integration-gate/exact-native-route.txt`.
- Frontend route qualification: `artifacts/native-loading-contract-integration-gate/frontend-route-qualification.txt`.
- Result: `artifacts/native-loading-contract-integration-gate/RESULT.md`.
- Distinction: observed source proves synchronous loading and automatic frontend state advancement; the blocker relocation to visible frontend orchestration is an inference from the absence of an intermediate-state render consumer.

## Visible native frontend contract gate

- Native contract inventory: `artifacts/visible-native-frontend-contract-gate/static-contract-intake.txt`.
- Resource/consumer slice: `artifacts/visible-native-frontend-contract-gate/frontend-resource-slice.txt`.
- Binary-producer/tooling intake: `artifacts/visible-native-frontend-contract-gate/binary-producer-intake.txt`.
- Prior qualified frontend evidence: `artifacts/visible-native-frontend-contract-gate/prior-frontend-evidence.txt`.
- Result: `artifacts/visible-native-frontend-contract-gate/RESULT.md`.
- Observed: only closure validation and state orchestration exist. Inference: Title is the minimal first visible state because it needs no preceding input transition.

## PAL NFH glyph producer contract gate

- Static inventory: `artifacts/pal-nfh-glyph-producer-contract-gate/static-intake.txt`.
- Official XDK and container contracts: `artifacts/pal-nfh-glyph-producer-contract-gate/official-and-container-contract.txt`.
- Content/FHM structural slice: `artifacts/pal-nfh-glyph-producer-contract-gate/pal-entry2-structural-slice.txt`.
- Extraction decision: `artifacts/pal-nfh-glyph-producer-contract-gate/extraction-route-decision.txt`.
- Negative cache-layout probe: `artifacts/pal-nfh-glyph-producer-contract-gate/cache-nfh-header-distribution.txt`; it found no candidate objects because its directory assumption was false and supplies no payload-content evidence.
- Result: `artifacts/pal-nfh-glyph-producer-contract-gate/RESULT.md`.

## Canonical PPC NFH consumer slice — partial

- Tooling and project intake: `artifacts/canonical-ppc-nfh-consumer-slice-gate/tooling-intake.txt`.
- Qualified program identity and unique NFH occurrence: `artifacts/canonical-ppc-nfh-consumer-slice-gate/nfh-discovery-headless.log` and `nfh-word-hits.txt`.
- Unique direct xref: `artifacts/canonical-ppc-nfh-consumer-slice-gate/nfh-magic-xrefs.txt`.
- Branch blocker: `artifacts/canonical-ppc-nfh-consumer-slice-gate/BLOCKER.md`.
- The absent decompile output is a tooling-invocation failure, not evidence about `FUN_822E2858` semantics.

## Unique NFH reader classification — partial

- Script contract: `artifacts/unique-nfh-reader-classification-gate/decompile-contract.txt`.
- Recognizer decompilation and caller xref: `reader-decompile-headless.log`, `reader-callers.txt`.
- Sole caller decompilation and data reference: `caller-822cc9f0-headless.log`, `caller-822cc9f0-xrefs.txt`.
- Branch blocker: `artifacts/unique-nfh-reader-classification-gate/BLOCKER.md`.

## NFH view table identity — closed

- Exact pointer/RTTI scan: `artifacts/nfh-view-vtable-identity-gate/pointer-rtti-scan-v2.txt`.
- Table bytes and boundary/xrefs: `ghidra-table-window.txt`, `table-boundary-xrefs.txt`, `table-materialization.log`.
- Constructor and consumer decompilations: `table-writers-decompile.log`, `view-consumer-pcode.txt`.
- Result: `artifacts/nfh-view-vtable-identity-gate/RESULT.md`.
- Qualified facts: table start `0x82027A64`, recognizer slot `+0xA8`, first reader slot `+0xB0`; reader stride is `0x20`, with data at object offset `0x10` and bound source at offset `0x08`.

## NFH view record layout — limited

- Numeric `+0xB0` dispatch inventory and bounded post-call windows:
  `artifacts/nfh-view-record-layout-gate/record-consumer-summary.txt` and
  `record-consumer-trace.log`.
- Owner decompilations and constructor xrefs:
  `artifacts/nfh-view-record-layout-gate/owner-decompile.log` and
  `constructor-xrefs.txt`.
- Result: `artifacts/nfh-view-record-layout-gate/BLOCKER.md`.
- Observed: 24 candidates share the slot offset, but none is statically tied
  to `0x82027A64` or to a direct read of the record pointer at `this+0x10`.

## NFH receiver provenance — limited

- Constructor callers and final vtable stores:
  `artifacts/nfh-receiver-provenance-gate/constructor-callers-decompile.log`;
  xrefs in `constructor-callers-xrefs.txt`.
- Qualified correction: `0x822CC118` is called after the derived store in
  four constructors, leaving `0x82027A64` as the final table for those
  receivers. The `0x82027BE8` and `0x82020F68` windows do not expose the NFH
  reader slot.
- The attempted broad displacement slice is retained in
  `subobject-dispatch-slice.log`; its `TraceVirtualRecordConsumers.java`
  invocation was invalid because `START END` were required. It produced no
  receiver intersection and is not semantic evidence.
- Branch result: `artifacts/nfh-receiver-provenance-gate/BLOCKER.md`.

## NFH direct `+0xB0` consumer — closed by refutation

- Corrected trace invocation and status:
  `artifacts/nfh-receiver-provenance-gate/trace-corrected-full.log` and
  `trace-corrected-status.txt`; 24 candidates, return code 0.
- Bounded return-use classification:
  `artifacts/nfh-receiver-provenance-gate/return-use-classification.txt`.
  Aucun candidat ne lit/écrit le retour `r3` comme pointeur de record dans la
  fenêtre post-`bctrl`; les lectures présentes sont des tests scalaires ou des
  copies.
- Conclusion : l'hypothèse du consommateur direct parmi les 24 dispatchs est
  réfutée. Résultat :
  `artifacts/nfh-receiver-provenance-gate/RESULT.md`.

## IB bootstrap producer static boundary

- `generated-guest.o` defines the generated functions for `0x821B55C0` and
  `0x821B9BC8`, but their stores use computed `PPCContext` addresses and do
  not contain a bootstrap-address literal.
- `graphics_ring.hpp` observes/captures the IB only after guest publication;
  it is not the producer.
- The direct address-based producer hypothesis is refuted; dynamic guest
  attribution remains open.
- Full reports: `artifacts/ib-producer-static-resume-gate/RESULT.md` and
  `BLOCKER.md`.

## Generated-store attribution route — source/build boundary closed

- The configurable range watcher was present for scalar stores but absent from
  `AC6_PPC_STORE_U128`.
- The U128 hook was added with width 16 and guest-order first-dword reporting;
  vector byte reversal and the actual store were left unchanged.
- Codegen-enabled runtime build, core-test link, and CTest passed (1/1).
- One bounded tick records 48 writes in `0x16AE0980..0x16AE0A3C`, all U32,
  from `__imp__sub_821B20A0`, LR `0x821B212C`, with generated lines 9966/9970.
  The resulting 48 dwords are 24 `C0003600,00010081` pairs.
- The writer gate is therefore closed. Full report:
  `artifacts/generated-store-hook-gate/RESULT.md` and
  `runtime-attribution-summary.txt`.

## Record writers and consumer dispatch

- `FindScaledStoreWriters.java` ran against the canonical demo project in
  read-only mode and scanned 11 253 functions. Full output:
  `artifacts/ib-writer-publication-static-gate/scaled-store-writers-run2.txt`.
- The qualified queue API is `index*0x60` from `+0x60d0`; the record type is at
  `+0x110`. `0x820FF710` writes type 0, while `0x820FF788`, `0x820FF7F8`,
  `0x820FFA88` and `0x820FFB50` write types 1, 2, 3 and 4 with their
  corresponding payload fields.
- `0x820FFCA0` advances the consumer index and calls `0x820FEFA8`, whose
  switch has explicit type 1–4 branches. The zero writer is therefore not a
  unique payload producer.
- Full decompilation evidence:
  `artifacts/ib-writer-publication-static-gate/slot-writer-decomp-run2.log`
  and `record-consumer-decomp-run2.log`.
- Remaining uncertainty is dynamic reachability of types 1–4 and their
  renderer/scheduler publication, not writer discovery.

## Record type reachability gate — static corridor qualified

- Le callsite unique `0x8210A1C0:0x8210ADB4` vers `0x82117410` est confirmé
  par le slice CFG/post-dominateur du projet canonique démo.
- Dix-huit gardes sont nécessaires ; elles couvrent l'état d'objet, le
  sélecteur `param_4` et les résultats des lookups `0x821080D0`/`0x826F6124`.
- Le résultat ne contient aucune observation runtime d'un type 1–4 ni de
  publication downstream.
- Artefacts : `artifacts/record-type-reachability-gate/RESULT.md`,
  `control-slice-run2.txt` et `BLOCKER.md`.

## Record type reachability — bounded runtime result (corrected selector)

- Le hook d'entrée strictement opt-in a été compilé et les tests core/trace
  passent.
- La fenêtre native START 2990–3021 ne franchit pas `0x8210A1C0` ni
  `0x82117410`; elle appelle `0x820FEFA8` 31 fois.
- Le sélecteur réellement lu par le dispatcher, `*(r3+0x40)`, vaut `0` sur
  les 31 entrées. La route observée est donc réfutée comme type 1–4 dans
  cette fenêtre; cela ne réfute pas le corridor statique jamais activé.
- 3021 ticks et 2913 PRESENT, frontend/mission/terminal faux.
- Références : `record-type-route-run4-summary.txt`, son log et son rapport
  sous `artifacts/record-type-reachability-gate/`.

## Record corridor — static producer of `param_4`

- Le seul appel direct `0x82165CC0:0x82165D8C` transmet `r8` via `r26` puis
  `r6` à `0x8210A1C0`.
- Le target réduit ce registre à `param_4 = r6 & 0xFFFF` (`0x8210A20C`).
- La source restante est une arête indirecte/vtable vers `0x82165CC0`; aucune
  constante ou shim natif n'est identifié.
- Référence compacte : `artifacts/record-type-reachability-gate/param4-producer-static-run5-summary.txt`.

## AVIObjectDemo vtable — static slot qualification

- `0x8200B6FC` est la vtable RTTI `AVIObjectDemo`; slot `+4` = `0x82165CC0`.
- `0x82166550` et `0x821674A8` écrivent cette vtable à `objet+0xC`.
- La dispatch `0x821710FC` lit la vtable primaire `0x8200B844`, pas le slot
  cible; aucun consommateur global décompilé ne montre `global+0xC` explicitement.
- La source table/slot est qualifiée; le récepteur ajusté et l'activation du
  corridor restent indécidables. Aucun shim env n'est justifié.
- Référence : `artifacts/record-type-reachability-gate/avi-vtable-static-run3-summary.txt`.

## AVIObjectDemo — bounded receiver witness

- Le hook d'entrée est présent dans l'objet généré; build et tests core/trace
  passent.
- La capture START bornée `2990..3021` (store frais, audio dummy) termine à
  3021 ticks avec 2913 PRESENT, sans frontend/mission/terminal.
- Aucun enregistrement pour `0x82165CC0`, `0x8210A1C0` ou `0x82117410` :
  l'activation est indécidable pour cette fenêtre, pas réfutée globalement.
- Le retour n'est pas observable depuis ce hook d'entrée; le call enfant aurait
  porté `LR=0x82165D90`.
- Référence compacte : `artifacts/record-type-reachability-gate/avi-receiver-run1-summary.txt`.

## AVI virtual edge — bounded static candidate set

- `TraceVirtualSlotDispatches` trouve 510 candidats slot `+4` globalement et 11
  dans `0x82160000..0x82172000`.
- `0x82165D6C` est l'appel virtuel interne de `0x82165CC0`; le call direct vers
  `0x8210A1C0` est à `0x82165D8C`.
- Les dix autres sites locaux n'ont pas encore de vtable résolue vers
  `0x8200B6FC`; aucun appelant direct entrant n'est identifié.
- Les scans de flux globaux `0x82731A30` et `0x823CD6E0` ne donnent aucun
  `DISPATCH` qualifié.
- Référence : `artifacts/avi-virtual-edge-static-gate/RESULT.md`.

## AVI virtual edge — constructor/receiver provenance pass

- `0x8200B6FC` n'est pas référencé comme pointeur de données; il est seulement
  matérialisé par `0x82166550` et `0x821674A8`, qui l'écrivent à `object+0xC`.
- `0x82373090` appelle le premier constructeur avec `0x82731A30`; le chemin
  du second alimente `0x823CD6E0`/`r31+0x84`.
- `0x821710FC` consomme la vtable primaire `0x8200B844`; aucun flux global
  connu ne résout `0x8200B6FC[+4]`.
- Les dix candidats locaux restent à qualifier (ou à réfuter) par propagation
  du receiver. Référence complète :
  `artifacts/avi-virtual-edge-static-gate/constructor-flow-final.txt`.

## AVI virtual edge — candidate caller census (limited)

- `0x821600C8` décompile comme receiver générique `param_1`; il n'est pas
  relié statiquement à `AVIObjectDemo`.
- `0x8216B3B4` n'a aucune fonction contenante dans le projet canonique.
- Les xrefs directs observés sont `82167378→82166AE0`, `8216EAC4/8216EEC4→
  8216E218`, `8216F920→8216F640`, `821A4454→82169B38` et deux annotations
  `bctrl` vers `821600C8`; aucune nouvelle arête directe vers `82165CC0`.
- La propagation receiver→vtable reste ouverte; le sous-gate est limité par
  quota. Référence : `artifacts/avi-virtual-edge-static-gate/candidate-callers-run1.log`.

## AVI virtual edge — candidate refutation by receiver value

- `0x821A4454` passe `0x82390574` à `0x82169B38`.
- `*(uint32_t *)0x82390574 = 0x8200BAEC`, donc son slot `+4` n'est pas celui
  de `AVIObjectDemo` (`0x8200B6FC[+4]`). `0x82169B7C` est réfuté comme arête
  AVI.
- Les `bctrl` à `0x821A3634` et `0x821A4400` chargent
  `PTR_PTR_82390034[+8]`; ils ne constituent pas des appels directs vers
  `0x821600C8`.
- Références : `artifacts/avi-virtual-edge-static-gate/caller-disassembly-run3.log`
  et `static-object-82390574-run4.log`.

## AVI virtual edge — manager/accessor provenance boundary

- `0x82165230` reads `FUN_82327108()+0xB5BC`; `0x82166AE0` reads
  `FUN_82327104()+0xD274`.
- `0x8216E218` and `0x8216F640` use `FUN_82327100()+0x7C`; `0x8216F7F8`
  uses `FUN_8232710C()` and a `+0x68` subobject.
- `0x82170CD0` uses `PTR_DAT_8238FEF4`; `0x82170F58` uses
  `FUN_82327108()` and the known primary global route.
- Ghidra emits empty `void` stubs for the accessor returns, so receiver
  provenance is indeterminate rather than null. No receiver is statically
  tied to vtable `0x8200B6FC`; no selector source for `r8` is added.
- Compact evidence: `artifacts/avi-virtual-edge-static-gate/manager-factory-summary-run5.txt`;
  full log: `decompile-manager-factories-run5.log`.

## AVI virtual edge — correction: ABI save helpers, not manager getters

- `0x82327100/04/08/0C` sont des entrées contiguës du helper de sauvegarde
  PPC (`std r26`, puis `r27`, `r28`, `r29` dans la chaîne); l'évidence ABI
  qualifiée indique que `r3` est préservé.
- Les labels générés `__savegprlr_28`/`__savegprlr_29` sur les appels vers
  `0x82327108/0C` corroborent uniquement l'ABI et le contrôle de flux; ils ne
  remplacent pas la preuve binaire.
- Les pseudo-retours `FUN_8232710X()` des candidats sont donc le `r3` entrant,
  et non une provenance de manager. Le receiver doit être joint au caller.
- `PTR_DAT_8238FEF4 = 0x82731150`; les mots importés à `0x82731150` sont nuls,
  donc le candidat `0x82170CD0` est indéterminé, non qualifié AVI.
- Référence compacte :
  `artifacts/avi-virtual-edge-static-gate/helper-boundary-correction-run8.txt`.

## AVI virtual edge — indirect table not qualified as vtable

- `DumpU32Range` établit une série de pointeurs exécutables de
  `0x8200C5C0` à `0x8200C664`, avec `0x8200C624 = 0x8216F7F8`; les mots à
  partir de `0x8200C668` sont des constantes non-code.
- `DescribeVtableRtti` échoue pour tous les alignements candidats
  `0x8200C5C0..0x8200C604`; aucun locator/type descriptor valide n'est obtenu.
- `FindRawPpcAddressMaterialization` retourne zéro hit pour les bases
  `0x8200C5C0`, `0x8200C600` et `0x8200C624`; `FindU32Set` ne trouve aucune
  occurrence exacte de ces adresses. La base de table n'est donc pas reliée
  statiquement à un objet dans le projet importé.
- `DumpRange` qualifie `0x823270F8` comme `std r24,-0x48(r1)`, entrée du
  helper ABI; le receiver de `0x82165490` reste le `r3` entrant.
- Conclusion : table de dispatch plausible mais vtable AVI non prouvée;
  aucune correspondance avec `0x8200B6FC` et `0x82165CC0`.
- Artefacts complets : `indirect-vtable-provenance-run10.log`,
  `indirect-vtable-owner-run10.log`, `indirect-vtable-rtti-run10.log`,
  `indirect-vtable-raw-materialization-run10.log`.

## AVI virtual edge — caller propagation checkpoint

- Le recensement direct qualifie cinq sites : `0x82165744 → 0x82165230`,
  `0x82167378 → 0x82166AE0`, `0x8216EAC4/0x8216EEC4 → 0x8216E218` et
  `0x8216F920 → 0x8216F640`; aucune arête directe vers `0x8216F7F8` ou
  `0x82170F58`.
- `Function_82167320(int param_1)` transmet `param_1` comme receiver à
  `0x82166AE0`. `Function_8216EA20`, `Function_8216ECE0` et
  `Function_8216F7F8` récupèrent le `r3` entrant via l'entrée ABI
  `0x8232710C`; `0x8216F7F8` transmet le même objet à `0x8216F640`.
- `0x82165490` est appelé directement par `0x821662CC` et utilise
  `FUN_823270F8()` avant d'appeler `0x82165230`; l'identité de cette entrée
  contiguë n'est pas encore qualifiée.
- `ReferencesTo(0x8216F7F8)` ne donne qu'une référence de données à
  `0x8200C624`; elle ne suffit pas à identifier la vtable. Aucun receiver
  n'est encore relié à `0x8200B6FC`.
- Preuve complète : `artifacts/avi-virtual-edge-static-gate/caller-propagation-run9.log`;
  résumé : `caller-propagation-summary-run9.txt` et
  `parent-caller-census-run9.log`.

## Primary counter static gate — closed

- `0x822E3F48` initialise l'état callback `0x82934708`, notamment
  `state+0x08`, `state+0x10` et `state+0x18` à zéro; `0x822E52D0` réécrit le
  seuil `state+0x10` par `last_argument-1` dans la route de démarrage, avec un
  argument qualifié égal à `1`.
- `0x822E3EC0` est le producteur live : il attend `0xE0000040`, incrémente
  `state+0x08`, puis appelle `0x822EEE10(state+0x40,count)`. Ce helper écrit
  `state+0x50` et signale `0xE000004C`.
- `0x822E40E8` est le consommateur; `0x822E4018` attend
  `state+0x50 >= state+0x10` et `0x822E4080` attend `state+0x50 > state+0x10`.
- `state+0x18` est le flag d'arrêt : il reste nul après l'initialisation et le
  seul chemin direct vers `1` est `0x822E3E48`, appelé lors de l'arrêt global.
- Cette chaîne causale est donc complète statiquement et ne justifie ni shim
  env ni contrat natif supplémentaire. Références complètes :
  `artifacts/guest-wait-producer-gate/primary-counter-fields-run2.txt`,
  `primary-counter-callsite-run11.txt`, `primary-counter-target-writer-run14.txt`,
  `artifacts/primary-counter-static-gate/static-summary.txt`.

## Frontend visual producer — static gate

- `FrontendController` ne fait que valider et faire avancer `FrontendState`;
  il n'a aucune sortie de rendu.
- `RetailFrontendResources::open` valide sept résumés de fonts et la fermeture
  FHM/NFH, mais ne conserve ni bytes NFH, ni métriques glyphes, ni atlas.
- `RetailSession::open` traverse la route frontend lorsqu'un cache complet est
  présent, sans appeler un renderer.
- `run_play_impl` appelle uniquement le renderer de scène mission ou le
  compositor CPU de mission; les renderers ne reçoivent ni `FrontendState` ni
  `RetailFrontendResources`.
- Le manque exact est donc le producteur visuel Title/compositor entre les
  ressources NFH et la cible native. Artefact complet :
  `artifacts/frontend-visual-producer-static-gate/RESULT.md` et les slices
  `key-sources.txt`, `route-sources.txt`, `frontend-renderer-gap.txt`,
`nfh-static-evidence.txt`.

## CSwgListener `+0x20` — static sweep closed

- Sur l'image plate du PAL demo, `tools/rtti_vtable_slot_sweep.py` résout 89
  slots dérivés de `.?AVCSwgListener@@`; les 89 sont résolus et 40 sont
  non-stub.
- Les deux objets actifs au `SendMsgI("M102")` post-START restent
  `CSelectMessageDlgManager` (`0x8200A584`) et `CModeTaskTitleDemoOffline`
  (`0x82011384`), tous deux avec `+0x20 = 0x820AC748`.
- Conclusion : des implémenteurs existent pour d'autres modes, mais aucun ne
  peut être attribué au plateau actif. Artefact :
  `artifacts/listener-slot20-sweep-gate/RESULT.md`.

## NFH PAL structural leaf — closed

- 28 leaves extraits commencent par `NFH\0` et vérifient tous
  `size = 0x450 + u32be(+8) * 0x20`.
- Le PPC qualifie `leaf+0x450` comme tableau et `0x822CC378` comme lecteur au
  stride `0x20`.
- `NFH+4` varie sans producteur/consommateur sémantique qualifié; le mapping
  caractère/record et l'atlas restent ouverts. Aucun parser/draw n'est ajouté.
- Référence : `artifacts/nfh-layout-static-gate/RESULT.md`.
## NFH consumer provenance — static gate closed

- `artifacts/nfh-record-consumer-resume-gate/RESULT.md` clôt la reprise du
  slice receiver.
- Les 24 propriétaires du `+0xB0` ne matérialisent ni les tables NFH
  qualifiées ni un accès au retour `r3` comme record ; l'hypothèse du
  consommateur direct est donc réfutée.
- La structure `NFH+0x450+count*0x20` est toujours une borne de format, pas
  encore une preuve de métriques glyphes/atlas ou de draw frontend.
## NFH canonical PPC accessor — static gate closed

- `0x822E2858` compares four bytes with `DAT_82028F5C` (`NFH\0`) and returns
  the leaf pointer only on recognition.
- Its sole code xref is `0x822CC9F0`, which binds `this+0x10` to `leaf+0x450`.
- `0x822CC378` bounds-checks the count at `leaf+8` and returns records at
  `base + index*0x20`; `0x822CC3B0`/`0x822CC3B8` expose `leaf+4`.
- This closes the accessor chain but does not prove glyph/atlas meaning or a
  visual consumer. Full evidence:
  `artifacts/canonical-ppc-nfh-consumer-slice-gate/RESULT.md`.
## Title resource-manager slice — static gate closed

- RTTI qualifie `0x82012E1C` comme `CResourceManager : CResource`.
- `0x8219EB20` forme une clé; `0x8219F7F8` fait lookup/alloc et appelle les
  slots de gestion, sans NFH/FHM/glyph/atlas/draw.
- Le slot `+0x40` (`0x8219DC50`) n'écrit que `this+0x10` et `this+0x11`.
- `0x822CC2A8` n'est qu'un store vers `this+0x18`; il ne rend rien.
- La provenance visuelle reste ouverte entre le child resource/view et les
  readers NFH `0x822CC9F0/0x822CC378`. Preuve compacte :
  `artifacts/frontend-compact-title-next/RESULT.md`.
## Child resource/view → NFH metrics — static gate closed

- Les wrappers `0x822CC7A8/0x822CC8E8/0x822CC988` franchissent le child/view
  puis appellent les accesseurs de métriques de `0x82027A64`.
- `0x822CCD98` appelle le slot `+0x5C`, qui lit un champ du record NFH; il ne
  produit pas de commande graphique.
- `0x822CCCF8` copie exactement 0x20 octets via `0x82327D90`; le disassembly
  montre une boucle de copie optimisée, sans écriture PM4/RT/présentation.
- `0x821A00E8` est un enqueue de ressource utilisé par les deux producteurs
  de titre, pas un consommateur NFH/draw.

La branche child/view est donc fermée par réfutation comme producteur visuel.
Artefact : `artifacts/child-resource-view-next/RESULT.md`.

## First non-bootstrap RT0 writer — static gate closed

- `0x822F84E0` lit `this+0x24` comme mot couleur et le transmet en `r8`.
- `0x821B6078` le place sur la pile `+0x5c`; `0x821B5B10` le reprend en
  `r19`; `0x821B58B0` le présente au builder `0x821B55C0` en `r10`.
- Le même builder écrit les vertices depuis le record flottant et émet la
  famille `DRAW_INDX_2`/`0xc0003601`; le resolve/copy RT0 est déjà qualifié.
- Conclusion : la chaîne writer est prouvée statiquement, mais son invocation
  native et un pixel non noir restent des questions runtime distinctes.

Artefact : `artifacts/rt0-writer-callers-next/RESULT.md` et
`artifacts/rt0-writer-callers-next/abi-map-compact.txt`.

## Runtime RT0 — le consommateur de render queue est le verrou précédent

Fait observé : la sonde ciblée n'a vu aucun `AC6_RECT_DRAW`, `AC6_RESOLVE` ou
`AC6_FRONTBUFFER_WRITE`; les seuls événements sont 24 `AC6_POINT_DRAW` au tick
0 avec surface et fetch nuls. Le dernier rapport complet atteint 3021 ticks,
2913 notifications de présentation, mais garde la render queue à
`producer=5647`, `consumer=0`, `packet_count=0`; le chemin de présentation ne
prouve donc aucun contenu Xenos.

Hypothèse réfutée : appeler directement le writer RT0 avant de fermer ce
consommateur ne serait pas un test causal du frontend. Question ouverte : quel
producteur/worker natif lit `0x82386CC0` et transforme le premier item en
soumission PM4 ?

Artefacts : `artifacts/global-progress/report-nested-compact.txt`,
`artifacts/native-writer-runtime/compact.txt`.

## Erratum — queue consumer actif, payload non produit

La qualification précédente du consumer comme « verrou manquant » est
obsolète. `0x820FFCA0` écrit l'index consommateur non nul puis remet les index
à zéro; cette remise à zéro explique le compteur de scheduler apparemment
immobile. Le worker lit `queue + index*0x60 + 0xD0`, copie les champs du record
et appelle `0x820FEFA8`.

Le probe RR corrigé observe un objet pile non nul à cette entrée, avec type et
champs bornés nuls; aucune branche de commande ne se déclenche. Les writers
`0x820FF788` et `0x820FF7F8` sont des producteurs statiques de types 1 et 2,
mais leur invocation n'est pas observée dans le chemin démo. Le prochain
slice doit donc joindre ces writers à leurs appelants/vtables et au type lu
par `0x820FEFA8`, sans ajout de shim.

Détails : `artifacts/render-queue-consumer-gate/RESULT.md`.

## Checkpoint — render-record corridor statique (2026-08-22)

La famille de writers de la vtable `0x82008EF0` est maintenant reliée au
builder `0x82117410`; son appelant direct est `0x8210A1C0`, avec un sélecteur
`param_4` et des gardes de lookup. La fenêtre native déjà capturée ne passe
par aucun de ces deux blocs et garde `record_type=0` aux 31 appels de
`0x820FEFA8`.

Cela ferme le contrôle de queue mais pas l'activation globale du payload. Le
prochain test statique doit trouver le producteur du sélecteur ou le récepteur
virtuel qui peut atteindre `0x8210A1C0`; aucun writer ne doit être forcé.
Artefacts : `artifacts/record-type-reachability-gate/RESULT.md`,
`artifacts/render-queue-slot-pcode-normalization-gate/compact-writer-family.json`.

## Wake-producer gate — bit `0x4` absent du chemin recompilé

- `0x822F8578` est l'entrée indirecte vers `0x822F85B8` via le slot 6 de la
  vtable dérivée `0x8202A488`.
- `0x822F85B8` appelle `0x821BB4C8` avec `r6=1`; l'initialiseur écrit donc
  `0x0C000001` à `state+0x56F8`.
- Le scan de tous les builds générés trouve un seul appel direct à cet
  initialiseur. Le writer ultérieur `0x821AEBE8` ne préserve/modifie que le
  bit 0; les autres sites lisent/testent le champ.
- `0x821C64E8` initialise des sous-systèmes et appelle des imports, mais ne
  produit pas d'écriture à `+0x56F8`.

Décision : le producteur du bit `0x4` est absent du chemin recompilé; la cause
reste ouverte seulement pour une voie indirecte/importée à qualifier dans le
projet Ghidra canonique. Artefact :
`artifacts/wake-producer-gate/RESULT.md`.

Le même graphe d'appels apparaît dans l'atlas Ghidra canonique :
`0x822DA7F0 -> 0x822E52D0 -> 0x822F23F0`, puis
`0x822F85B8 -> 0x821BB4C8 -> 0x821C64E8`. Les seuls computed calls liés à ce
slice sont `0x822E5330` et `0x822F8590`; ils ne constituent pas un second
constructeur direct qualifié.

## Wake-producer table classification

`0x8207DF10 = 0x821BB4C8` est dans `.pdata`, un bloc read-only de métadonnées
de frontières de fonctions. Les paires de huit octets et le descripteur
`0x40002D03` ne constituent pas un appel indirect; aucune référence code ou
branche vers les mots de table n'est qualifiée. Ce résultat ferme
`pdata_false_alternate_constructor` et retire cette fausse piste du slice.
Preuve : `artifacts/wake-producer-gate/constructor-table-classification.md`.

## CX360UnitManager — producteur du gate renderer

`0x820A45E0` est le slot `+0x14` de `CX360UnitManager` et son unique
descripteur utile (`0x820A4778`) encode `(17,6)`. Le callback `0x821ADAB8`
enregistre cette paire et arme `device+0x5460`; `0x821C57D0` lit ensuite ce
champ. Les scans de branches ne trouvent aucun appel direct au slot ou au
callback, et les sept sites de construction recensés restent non atteints
dans le parcours natif. Gate fermée : l'étape suivante est l'activation
mission de l'instance, pas une modification de renderer ou d'environnement.
Artefact : `artifacts/cx360-unit-manager-gate/RESULT.md`.

## Checkpoint — la construction CX360 est aval du chargement

Observation statique : `0x8217C4D8` est enregistré comme callback par
`CModeTaskGame*` (`0x8217C678`/`0x82173DF0`), puis son bras `-3` sélectionne les
constructeurs de gestionnaires de mission. Les sept constructeurs
`CX360UnitManager` n'ont donc aucun rôle d'activation avant l'entrée de la
tâche de jeu. Le chemin de chargement observé s'arrête plus tôt dans le
provider `0x8219F5D0`, dont la machine `0x8219DF00` attend
`0x8219AF20`/`0x82195B50`; le flag de readiness du poll reste non publié.

Interprétation : le premier contrat manquant est le producteur de readiness,
pas le callback `(17,6)`, le shim environnemental ou `-O3`. Preuves :
`artifacts/cx360-construction-activation-gate/RESULT.md` et
`artifacts/loading-resource-resolution-static-gate/BLOCKER.md`.

## Readiness-provider runtime gate — attente initiale réfutée

Une capture limitée à l'objet actif montre `0x8219AF20` aux ticks `4116..4123`,
avec transition `state=0 → state=1` et retour `0 → 1`. `0x82195B50` n'est pas
atteint. Le runtime reste à `5600` ticks sans frontend ni mission, mais le
provider observé n'est pas absent ni bloqué à zéro.

Décision : fermer l'hypothèse du provider comme premier verrou et reprendre par
la transition post-provider et le payload de rendu non-bootstrap. Captures
complètes : `artifacts/readiness-provider-gate/runtime.HcE2rT/`; synthèse :
`artifacts/readiness-provider-gate/RESULT.md`.

## Correctif — portée de la capture provider

Le retour `1` rapporté dans cette capture appartient au callback virtuel
`0x8219DF00`, appelé depuis `0x8219F5D0` (LR `0x8219F64C`). Les écritures de
l'objet `0x2E3B0040` confirment une progression `1→2→0` au tick 4123 et
`0x8219AF20` est bien exécuté, mais aucune observation directe ne ferme encore
le retour de `0x8219F5D0` ou les champs `+0x0A/+0x0C` de `CTaskLoading`.
L'hypothèse « callback de travail bloqué » est fermée; la frontière
agrégateur→tâche reste ouverte. Preuve :
`artifacts/post-provider-transition-gate/RESULT.md`.

## Clôture du gate agrégateur readiness (2026-08-22)

La définition de `sub_8219EE40` confirme la résolution par `object+0x1C`.
La capture directe observe `0x827745F0 + key 0x5908E2C8 → 0x18BB0100`.
Les champs `0x18BB0120`/`0x18BB0124` sont respectivement la tête et le statut
du manager; au tick 4123, les callbacks virtuels sont suivis jusqu'à la
vidange (`...→0`) puis au store de statut `1→0` au LR `0x8219F6B4`.
La cause « manager non terminé » est réfutée. Le plateau persiste à
`4140/4032 PRESENT`, `23 blocked/0 runnable`, sans frontend ni mission. Sources
compactes : `artifacts/post-provider-transition-gate/ee40-static.txt`,
`runtime-loading-resolver/stderr.log`, `runtime-provider-fields.IfH6nG/stderr.log`.

## Readiness consumer runtime gate — transition aval confirmée

À `tick=4123`, `sub_8218A4A0` appelle le slot 15 `0x8216CB40` sur
`0x2E3D0080` (vtable `0x8201130C`) au LR `0x8218A55C`, puis écrit
`0x2E3D008C = 1` au LR `0x8218A564`. Le chemin `sub_820CDF88` écrit également
`0x2E3D00E8` dans `0x826DF804`.

Décision : fermer l'hypothèse « le task ne franchit jamais l'état aval ».
Cette transition ne débloque pas la fenêtre : `4140` ticks, `4032 PRESENT`,
`23 blocked/0 runnable`, sans frontend ni mission. La prochaine frontière est
le callback/listener et le post-wake de `0xE000004C`.
Preuves : `artifacts/readiness-consumer-gate/runtime-consumer/stderr.log`,
`probe.report.json`, `static-next-focused.txt`.

## Gate fermé — séparation listener / post-wake

- Observé : les seules références directes qualifiées à `0x826DF804` sont les
  opérations du conteneur `sub_820CDF88`; les consommateurs parcourent le
  tableau depuis `0x826DF800`.
- Observé : `SendMsgI` appelle le slot `+0x20`, qualifié no-op; le pump
  `sub_820EA4A8` appelle le slot `+0x54` sous contrôle du script.
- Observé : les réveils `E000004C` reprennent correctement le waiter, puis
  atteignent l'accès guest `0x82327154`.
- Inférence : publication listener et reprise worker ne forment pas le verrou
  causal recherché; aucun correctif kernel/XAM n'est justifié ici.

Preuves : `analysis/demo/ac6-demo-static-references-v1.json`,
`reports/AC6_DEMO_SENDMSGI_IS_NOT_THE_MECHANISM_EITHER_CLASS_USES.md`,
`reports/AC6_DEMO_STATE_ADVANCE_IS_SCRIPT_TRIGGERED_NOT_FLAG_GATED.md`,
`reports/cycle-1728-demo-event-handoff-xma-frontier.md` et
`reports/cycle-1761-ac6-demo-post-resume-one-shot.md`.

## Writer provider — activation qualifiée (2026-08-22)

Projet `ghidra-projects/ace-combat-6-demo`, `Default.xex` qualifié. La chaîne
`0x82165490 → 0x82114F58 → 0x82114798` rend les sous-tables utilisables : le
count vient de la ressource d'index 1; chaque entrée est allouée, son
`entry+0x04` reçoit la valeur correspondante de l'index 0, puis
`0x82108918` et le couple `0x8210E548`/`0x8210E5E0` préparent et drainent les
ressources. Le contrat est observé sans appel forcé. La provenance de
l'instance AVI et du `r8` qui sélectionne le corridor graphique demeure
indéterminée. Artefact :
`artifacts/static-owner-shared-gate/provider-entry-contract-decomp.log`.

## Gate fermé comme blocker — provenance r8 et record utile

- Observé : `0x821710FC` charge une vtable manager distincte de
  `0x8200B6FC`/AVI.
- Observé : `0x82165CC0` propage `r8 → r26 → r6 → low16(r4)` vers
  `0x8210A1C0`; l’affectation caller de `r8` reste non qualifiée.
- Observé : `0x820FEFA8` consomme les records 1–4 sans arête prouvée vers la
  chaîne RT0/Xenos.
- Probe : 8 000 ticks, 7 892 PRESENT, frontend/mission faux, ring sans
  soumission ni paquet décodé.

Preuves : `artifacts/first-useful-record-r8-gate/RESULT.md`,
`artifacts/first-useful-record-r8-gate/BLOCKER.md` et les rapports agents
AVI/writers/RT0/title dans le même répertoire.

## Exécution du seam causal Edge (2026-08-22)

- Observé : build Edge `build-agent-clang` terminé (`status=0`) avec
  DTRACE/FTRACE activés; `xenia-base-tests` passe 79 cas et 3723 assertions.
- Observé : les trois runs bornés ont chargé le XEX PAL qualifié et créé les
  threads guest; les quatre PC ciblés n'apparaissent dans aucun FTRACE.
- Observé : aucun fichier causal n'est armé, aucun store `record+0x10c` n'est
  vu et aucun fichier PM4 n'est produit; les fenêtres se terminent par
  `timeout` 124.
- Décision : l'observateur Linux est vérifié, mais il n'existe encore aucune
  divergence Edge/native permettant un correctif. Aucun renderer, shim ou
  record synthétique n'est autorisé.

Preuve compacte : `artifacts/xenia-edge-causal-gate/RESULT.md` et ses
`edge-run-*`, `build-agent-*` et `agent-bridge-tests.*`.

## Validation codegen, writeback et installation (2026-08-22)

- codegen-OFF : build sans travail résiduel, CTest `27/27` réussi;
- codegen-ON : configuration avec XEX PAL qualifié, build et cible
  `ac6-demo-recomp` réussis, puis 7 tests Xenos/shader/tiling/Vulkan réussis;
- runners autonomes : différentielle copy, certificat runtime, oracle EDRAM et
  audit screencap réussis (PNG `2×2` et `1280×720` non noir de test);
- installation : `cmake --install ... --prefix "$PWD"` réussi, `bin/bin`
  absent, aucun fichier `.xex/.pac/.tbl/.cpp` ni base Ghidra dans le package.

Ces résultats ne constituent pas une progression guest : aucun draw réel ni
payload titre/menu n'a encore été observé. Artefacts complets :
`artifacts/build-validation/`.

## Slices statiques owner/records — blockers nommés (2026-08-22)

- `0x82114798` est le writer qualifié : count depuis la ressource d'index 1,
  slots `entry+0x04+4*i` et clés copiées depuis la liste source;
- `0x820FEFA8` branche les records 1–4 vers le service motion CPU et aucun
  appel PM4/Xenos n'est présent dans la fermeture directe; les frontières sont
  les callbacks `[entry+0x20]` et `[entry+0x1c]` aux PCs listés dans le rapport;
- les consommateurs directs inspectés ne fournissent pas la vtable AVI
  `0x8200B6FC` à `base+0xC`, donc la provenance de `r8` vers `0x82165CC0` reste
  inconnue.

Décision : pas de patch; poursuivre statiquement le producteur de la liste
`state+0xB5B4/B5B8`. Preuves :
`artifacts/goal-playable/{avi-owner,record-population,record-pm4}/RESULT.md`.

## Validation longue de reprise (2026-08-22)

Le CTest complet sous `SDL_AUDIODRIVER=dummy`/Xvfb et cgroup borné a exécuté
88 tests : 87 passent, 4 sont des skips attendus, et seul
`ac6-cpp-complexity` échoue sur des sources d'analyse existantes sous
`artifacts/` qui dépassent le budget de fonctions. Aucun test runtime ou
renderer n'a échoué; aucun fichier généré n'a été modifié pour masquer cet
audit. Preuves : `artifacts/goal-playable/ctest-refresh/`.

## Producteur d'état amont — `0x82165E68` (2026-08-22)

`0x82165E68` est le seul writer qualifié de `state+0xB5B4/B5B8` : les deux
champs sont obtenus par résolution de clés `param_2+0x12/+0x16`, puis la table
provider est activée. Cela ferme l'adresse et la forme du contrat, mais pas la
provenance des clés/valeurs ni l'instance secondaire qui fournit `r8` à
`0x82165CC0`. Aucun état synthétique n'est ajouté. Preuve :
`artifacts/goal-playable/state-list-producer/RESULT.md`.

## Suivis statiques owner AVI / callbacks (2026-08-22)

- `0x82166550` et `0x821674A8` sont les seuls constructeurs qui écrivent la
  vtable AVI `0x8200B6FC` à `object+0xC`; aucun des candidats entrants ne
  qualifie à la fois cet owner et le `r8` consommé par `0x82165CC0`;
- `0x82169B7C` et `0x821710FC` sont réfutés comme pistes AVI, tandis que les
  huit autres sites restent indécidables faute de provenance d'instance;
- les six callbacks de records (`entry+0x20`/`entry+0x1c`) chargent des cibles
  data-driven non qualifiées; la fermeture des wrappers reste CPU-only et ne
  contient aucun PM4/Xenos.

Décision : le contrat partagé manquant est l'initialisation de la table des
callbacks et la matérialisation de l'owner AVI/r8. Aucun correctif guest ou
renderer n'est justifié. Preuves :
`artifacts/goal-playable/avi-owner/RESULT-followup.md`,
`artifacts/goal-playable/record-pm4/RESULT-followup.md`.

Le groupe AVI prioritaire (`0x82160104`, `0x82165340`, `0x82167020`,
`0x8216E310`) est épuisé sans arête `owner+0xC → 0x8200B6FC`; ses receivers
sont arbitraires ou à des offsets incompatibles, et aucun `r8` ne peut être
attaché à `0x82165CC0`. Preuve :
`artifacts/goal-playable/avi-owner/RESULT-candidate-followup.md`.

Le census de configuration n'a trouvé aucun appel direct qualifié vers
`0x82165E68`/`0x82165CC0`, aucune écriture qualifiée de `param_2+0x12/+0x16`,
et aucune résolution vers la vtable primaire parmi les 11 dispatchs locaux.
Le contrat manquant est l'objet `param_2` et son producteur de clés, pas le
writer de records. Preuve :
`artifacts/goal-playable/record-population/RESULT-config-followup.md`.

## Fenêtre import/scheduler réfutée (2026-08-22)

La trace native bornée a compté 11,025 imports sans retour non géré. Le motif
`RtlTryEnterCriticalSection`/`RtlLeaveCriticalSection` des ticks 61–62 vise
`0x8219AF20`, un lookup guest borné déjà observé se terminer. Aucun appel
`XamContent*`, AVI, record ou PM4 n'apparaît. Cette preuve ferme la piste
lock/scheduler et ne justifie aucun patch partagé. Voir
`artifacts/goal-playable/runtime-frontier/import-window.md`.

## File et sélecteur — frontière guest restante (2026-08-22)

Le worker `0x820FFCA0` et le slot de contrôle `0x820FF710` sont des méthodes
indirectes de la vtable `0x82008EF0`; aucun appel direct ne les constitue en
source de payload. Les writers type 1/4 sont atteignables uniquement après le
dispatch AVI et un `r8` dont le low16 vaut 1, avec cinq validations de
ressource non nulles. Aucun import hôte manquant n'est démontré. Preuve :
`artifacts/goal-playable/record-population/RESULT-followup-next.md`.

## Census AVI final (2026-08-22)

Le headless qualifié (`exit=0`) confirme l'absence d'un caller direct de
`0x82165CC0` et d'une publication statique du receiver `base+0xC`; les seuls
stores de vtable sont les deux constructeurs. La dernière frontière est donc
l'owner indirect et son `r8`, pas le bridge graphique. Preuve :
`artifacts/goal-playable/avi-owner/RESULT-final.md`.

## MAIN_THREAD_PROMPT — seam Edge et probe natif (2026-08-22)

Le seam Linux borné demandé aux PCs `0x82386C58`, `0x82165CC0`,
`0x8210A1C0`, `0x82117410`, champs du record et `TraceWriter` PM4 est compilé
et couvert par le test ciblé. Les trois runs Edge qualifiés se terminent par
le timeout déclaré avant ces PCs; ils ne révèlent donc aucune divergence
import/receiver/ressource et ne justifient aucun changement renderer. Le probe
natif correspondant expire au tick 1220 après 2048 slots nuls : `0x820FF710`
et `0x820FFCA0` s'exécutent, mais aucun type 1–4, AVI ou PM4 n'apparaît.
Artefacts complets : `artifacts/xenia-edge-causal-gate/` et
`artifacts/goal-playable/runtime-frontier/RESULT-current-run.md`.

## Table AVI — propriétaire qualifié, producteur encore opaque (2026-08-22)

`0x8200C5C0..0x8200C664` appartient à la vtable `0x8200C614` des objets
`0x8216F4A8/0x8216F500`; l'entrée `0x8200C624` ne mène pas à AVI. Le dispatch
AVI réel est le `bctrl` de `0x82165D70` dans `0x82165CC0`; le receiver vient
du `r4` entrant et le sélecteur suit `r8 → r26 → r6`, `low16(r6)` étant lu
par `0x8210A1C0`. Aucun caller indirect producteur n'est encore résolu.
Preuve : `artifacts/goal-playable/avi-owner/RESULT-dispatch-table.md`.
# Dernière preuve — jonction logo→ACC toujours absente (22 août 2026)

Le slice final qualifié (`ghidra-projects/ace-combat-6-demo`) ferme
`0x8218BFB0` comme sélecteur de providers et `0x8219BFB0` comme simple load
dans `0x8219BF40`; ni index `170/171`, ni `brandLogo`, ni `texture_id 2` ne
traverse vers `source+0x20`/`0x8219E580`. Le PNG décodé prouve le payload disque
Namco, pas son affichage natif. Voir
`artifacts/goal-playable/acc-guest-join-next/RESULT-final.md`.

## Renderer titre persistant — preuves P1/P2/P0 (2026-08-22)

- Le run `persistent-title` rend P1 puis P2 sur une cible Vulkan persistante;
  leurs quatre vertices et constantes observés sont identiques.
- Le run `p0` qualifie ensuite le tuple exact
  `10348003/100A9002 + 103F1003/100087F2`, snapshotte 208 octets depuis
  `0x10348000`, et rend P0 puis les rotations suivantes jusqu'au tick 330.
- Les présents 100 à 132 sont non noirs en 1280×720. Le changement guest de
  couleur vertex produit au moins deux readbacks distincts; aucun pixel n'est
  synthétisé.
- Tous les différentiels copy/writeback observés indiquent zéro octet et zéro
  pixel divergent entre les voies CPU et Vulkan.
- `test-persistent-title.log` ferme 7/7 tests ciblés : core, reached-raster,
  Xenos command/shader/tiling, Vulkan resolve et source audit.
- Les trois IB P1/P2/P0 gardent le même fetch BC3 64×64 à `0x0DF22000`;
  aucun draw `brandLogo/003_NTXR` n'est encore prouvé.

Sources : `artifacts/goal-playable/title-array-writeback-runtime-20260822/`,
`artifacts/goal-playable/title-post-p2-static-20260822/RESULT.md` et
`artifacts/goal-playable/title-texture-mutation-frontier-static-20260822/RESULT.md`.

## Payload logo et capture native — 23 août 2026

- Le draw guest naturel 512×512 fournit un payload BC3 non nul ; le décodage
  CPU montre BANDAI NAMCO dans
  `artifacts/goal-playable/brandlogo-texture-dump-runtime-20260823/title-512x512.png`.
- Le même run produit
  `artifacts/goal-playable/brandlogo-clamp-runtime-20260823/title.png`, une
  capture 1280×720 bleu uniforme : le logo n'est pas encore visible dans le
  framebuffer natif.
- Le sampler clamp Xenos est qualifié mais son A/B reste à zéro fragment pour
  le draw 512 ; la voie `LOAD`/clear est également réfutée. Le résultat est un
  seam de shader/échantillonnage, pas une permission de fabriquer des pixels.

## Q writer et index fetch — 23 août 2026

Le SDK ReXGlue qualifie `xe_gpu_vertex_fetch_t` à deux dwords, donc
`kXenosTextureFetch00 + 94*2` est l'adresse correcte du slot 94. La trace
bornée `brandlogo-q-writer-trace-debug-20260823` observe les appels naturels
`0x82118D18` avec `record+0x14=1`, les écritures de 208 octets dans P et
`cQ=0`; aucun store dans Q0/Q1/Q2 n'apparaît avant le draw. À tick 255, le
vertex shader lit Q1 nul et le pipeline compte zéro fragment. La capture
native correspondante est donc noire, tandis que le PNG BC3 décodé du guest
reste seulement une preuve de ressource.

Cette preuve ferme l'hypothèse d'un mauvais index de registre et du défaut
Vulkan sampler/LOAD. Le contrat restant est guest : obtenir naturellement un
record type 4/5/6 relié à l'owner et publié vers Q, sans copier P vers Q dans
le renderer.

## Owner logo — réinitialisation du curseur (23 août 2026)

Une trace bornée des stores sur l'owner `0x2E3CED10` montre que
`sub_820E5124` appelle `sub_82322D28`, qui remet `owner+0xD8` à zéro et efface
`owner+0xD5`; `sub_82322D04` le réarme aussitôt. Le motif se répète à ticks
228, 231, 234 et suivants, tandis que `sub_82323BB8` republie seulement les
états intermédiaires. L'owner deux-frames ne progresse donc pas naturellement
vers le second record. Cette preuve est guest-side et ne motive aucun patch
Xenos/Vulkan.

Source : `artifacts/goal-playable/brandlogo-owner-store2-runtime-20260823/RESULT.md`.

## Resource-index du logo — contrat de retour fermé (23 août 2026)

La trace bornée `brandlogo-resource-index-runtime2-20260823` observe à ticks
228–237 le slot 6 retourner `8`, puis le slot 21 retourner un descripteur dont
`+24=15` et `+4=0x7F040438`. Le code qualifié choisit donc `result+4` et le
transmet à `sub_823230D8`; l'owner reçoit ensuite l'index `0` avant de remettre
son curseur `+0xD8` à zéro. Aucun slot 23 ni record #2 n'est publié. Le défaut
reste la production guest du record type 4/5/6, pas le renderer.

Preuve : `artifacts/goal-playable/brandlogo-resource-index-runtime2-20260823/RESULT.md`.

## Producteur de liste et handle 0x59 — fermeture statique (23 août 2026)

Le constructeur SWG `0x82323808`, sa factory `0x820D18C8` et l'insertion
`0x820D16A8` sont qualifiés, mais aucun xref statique ne relie le descripteur
`0x1AA48` à cette création pour l'owner `0x2E3CED10`. Son updater
`0x82323BB8` ne peut publier `+220=1` que si l'owner est réellement appelé,
activé (`+213!=0`), non inhibé (`+214==0`) et possède au moins deux entrées.

La chaîne asset est indépendante et ferme seulement la ressource :
`0x8219E580 → 0x821A00E8(type=4) → 0x8219F080 → resource+0x14=0x0E000059`
(`0x2DF67000`, `003_NTXR`, 1280×720). Le consumer exige en plus
`count>=3`, un lien record #1→#2, `record#2+0x0c==2` et
`self+0x18==0x0E000059` avant `0x820EA9A0`.

Le callback Q `0x82119048` est bien celui des records type 4/5/6, mais aucun
producteur naturel du titre ne les fournit : la route observée produit
types 1/3, tandis que la route type 4 de mouvement
`0x82117410 → 0x820FFB50 → 0x820FEFA8` ne rejoint ni ce dispatch ni le PM4.
Le blocage reste donc amont (énumération/liste/record), sans remapping 0x57→0x59,
appel forcé du callback ou payload Q synthétique.

Preuve : `artifacts/goal-playable/brandlogo-frame-owner-static-20260823/RESULT.md`;
complément : `artifacts/goal-playable/brandlogo-handle59-submit-static-20260823/RESULT.md`.

## Factory SWG — owners naturels, pas de record #2 (23 août 2026)

Le probe `swg-factory-return-runtime-20260823` observe les retours de la
factory qualifiée `0x820D18C8` (owners `0x2E3CDD10`, `0x2E3CE490`,
`0x2E3CED10`, puis `0x2E3CFCD0`) et leur stockage partagé. Il ferme 500 ticks
avec 189 presents, `frontend=false`, et aucun record type 2/handle
`0x0E000059`. Les 1280×720 screencaps d'audit ont tous `rgb_nonzero=0`.
Les tests ciblés Xenos, shaders, tiling, raster et Vulkan passent après retrait
des sondes ; aucune sémantique renderer n'est ajoutée.

Preuve : `artifacts/goal-playable/swg-factory-return-runtime-20260823/RESULT.md`.

## SWG interpreter / Q writer — join encore ouvert (23 août 2026)

Les passes statiques complémentaires ne trouvent aucun writer des champs
requis (`count>=3`, record #2 type 2, handle `0x0E000059`) :
`0x820E50D8/0x820E5140` ne matérialisent pas la liste. Le writer Q1
`0x82119048` est qualifié comme candidat, mais l’entrée naturelle du titre,
son type/count, la couverture des `0xd0` octets et l’ordre avant draw restent
non joints. Aucun code n’a été modifié.

Preuves : `artifacts/goal-playable/swg-interpreter-static-followup-20260823/RESULT.md`;
`artifacts/goal-playable/title-vertex-writer-followup-20260823/RESULT.md`.

## Route record précoce — worker vide (23 août 2026)

Une trace headless native bornée (ticks 190–300, 56 GiB maximum) a relevé 32
appels du worker `0x820FEFA8` aux ticks 206–297. Chaque appel porte
`r3=0x2EEEFE90`, `r4=1`, `selector=0`, `record_type=0` et
`record+0x10c=0`. Aucun appel à `0x8210A1C0`, `0x82117410` ou `0x820FFCA0`
n'est observé. Cette négative runtime est cohérente avec la classification
statique des 28 candidats : l'arête manquante précède le renderer et la
publication Q1.

Preuve : `artifacts/goal-playable/record-route-early-runtime-20260823/RESULT.md`;
log complet : `artifacts/goal-playable/record-route-early-runtime-20260823/service.log`.

## SWG materializer — curseur qualifié, record #2 non qualifié (23 août 2026)

`0x820E50D8/0x820E5140 → 0x82322D20` peut modifier naturellement l'état actif
et le curseur (`+0xD5/+0xD8`), mais `0x82323BB8` ne fait que publier/consommer
les frames déjà présentes. Le VM `0x823246C0` appelle des slots ASContext
virtuels sans chemin qualifié vers la liste owner, le record `+0x0C=2` ou le
handle `0x0E000059`. Le seam restant est donc le materializer SWG→ACC ou son
receiver virtuel, en amont du renderer.

Preuve : `artifacts/goal-playable/swg-materialization-followup-20260823/RESULT.md`.

## Récepteur slot 4 SWG — join producteur non fermé (23 août 2026)

La table SWG qualifiée `0x820064D8` contient `0x820D18C8` à `+0x10`.
Les callsites naturels `0x8232342C` et `0x82323594` chargent le récepteur
depuis `context+0x10` et lui passent le nouveau nœud ainsi que `param_3`.
Leurs corps ne matérialisent toutefois pas la liste de l'owner, le compteur,
le record #2 ou le handle `0x0E000059`. Le writer de `global_swg_context+0x10`
et l'identité de `param_3` restent non qualifiés ; aucun patch n'est permis.

Source : `artifacts/goal-playable/swg-slot4-receiver-20260823/RESULT.md`.

## Trace slot 4 SWG — factory vivante, payload titre non joint (23 août 2026)

Sous trace bornée et cgroup 56 GiB, `0x820D18C8` est observé trois fois dans
les ticks 190–430. Les appels naturels vers `0x820D0DB8` puis `0x82323808`
conservent des descripteurs `0x0B`/`0x01`, sans produire le record #2/handle
`0x0E000059` exigé par l'owner titre. Le probe atteint 300 ticks et 122
présentations, sans frontend ni readback non noir. Aucun changement renderer
ou guest n'est justifié.

Preuve : `artifacts/goal-playable/swg-slot4-trace-runtime-20260823/RESULT.md`.

## Writer contexte SWG — initialisation/teardown seulement (23 août 2026)

La recherche statique qualifiée trouve `context+0x10` écrit uniquement par
`0x820CF4E8` lors de l'initialisation et `0x820D0A00` lors du teardown. Le
join owner→contexte (`0x820E8B58 → 0x82321E18 → owner+0xE8`) est fermé, mais
aucun writer vers la liste titre, record #2, handle `0x0E000059` ou Q1 n'est
qualifié. Cette négative est cohérente avec le screencap noir ; aucune
sémantique renderer n'est ajoutée.

Preuve : `artifacts/goal-playable/swg-context-writer-static-20260823/RESULT.md`.

## Validation après sondes bornées (23 août 2026)

Le codegen-OFF reconstruit avec `build.status=0`, et les six tests ciblés
graphics/Xenos/raster/Vulkan passent sous Xvfb avec `ctest.status=0`. Ces
tests valident l'absence de régression de la pile existante ; ils ne changent
pas le verdict guest et ne rendent pas le screencap non noir.

Preuves : `artifacts/goal-playable/swg-slot4-build-off-20260823/` et
`artifacts/goal-playable/swg-slot4-tests-off-20260823/`.

## Producteur descripteur/blob SWG — non qualifié (23 août 2026)

La recherche statique ne trouve aucun writer valide de `0x1AA48`. Le seul
pseudo-hit (`0x821846A0/0x821846FC`) est invalidé par l'écrasement du registre.
`0x82326B80` ne fait que construire `owner+0x20`; aucun writer qualifié ne
remplit le blob/list avec `count>=3`, le lien vers record #2 ou
`record+0x0C==2`. Le blocage reste donc en amont du renderer.

Preuve : `artifacts/goal-playable/swg-descriptor-producer-static-20260823/RESULT.md`.

## Fetch titre naturel — renderer atteint, Q1 nul (23 août 2026)

La sonde codegen-ON Vulkan/Xvfb atteint les draws titre naturels et valide les
fetchs `0x0DF22054` (64x64) puis `0x0DF27054` (512x512). Les textures sont
non nulles, mais les quatre vertices du Q sélectionné restent zéro et aucune
sample ne passe ; les readbacks 1280x720 demeurent noirs. Le renderer existant
est donc suffisant pour les profils observés et reste fail-closed.

Preuve : `artifacts/goal-playable/title-fetch-trace-runtime-20260823/RESULT.md`;
image : `artifacts/goal-playable/title-screencap-vulkan-20260823/screens/ac6-demo-pal-present-t000000000429-p000000000165-s000000000398-0c660f2bd3eff315.png`.

## Trace provider population — aucun writer exécuté (23 août 2026)

La fenêtre native Vulkan 190..430 a sondé les writers provider qualifiés et
leurs sélecteurs : zéro événement `AC6_PROVIDER_POPULATION`. Le worker
`0x820FEFA8` ne reçoit que `record_type=0`/`record+0x10c=0`; le draw titre
produit toujours un readback 1280×720 entièrement noir (`rgb_nonzero=0`,
`passed_samples=0`, textures non nulles). Cette trace confirme que la
frontière causale précède la population provider et Q1 ; le renderer reste
inchangé et fail-closed.

Preuve : `artifacts/goal-playable/provider-population-runtime-20260823/RESULT.md`.

## Diagnostic RGBA/BGRA — non applicable à la démo native (23 août 2026)

Le projet démo utilise directement `VK_FORMAT_R8G8B8A8_UNORM` et encode son
audit PNG depuis le readback guest; il n'embarque pas la classe
`VulkanFramePresenter` de la reconstruction retail. La capture native source
est entièrement noire avant toute présentation (`rgb_nonzero=0`), donc aucune
permutation de canaux ne peut expliquer l'absence du logo.

Preuve : `artifacts/goal-playable/format-diagnostic-20260823/RESULT.md`.

## Producteur B5B4/B5B8 — seul writer direct, caller ouvert (23 août 2026)

`0x82165E68` remet puis remplit `state+0xB5B4/+0xB5B8` lorsque
`0x821EE130` valide les clés de `param_2+0x12/+0x16`; `0x82165490` ne fait que
les lire sous `state+0x119E4==0`. Aucun caller/constructeur virtuel qualifié
de `0x82165E68` n'est retrouvé, donc aucune valeur non nulle n'est justifiée
et le provider/Q1 reste non matérialisé.

Preuve : `artifacts/goal-playable/provider-state-fields-static-20260823/RESULT.md`.

## Caller virtuel du writer B5B4/B5B8 — contrat `param_2` incomplet (23 août 2026)

La passe statique canonique qualifie `0x82216498` comme caller du slot `+4`
de la vtable primaire `0x8200B63C`, donc vers `0x82165E68`. Son second
argument est toutefois le scalaire `bVar6`, alors que le callee sonde aussi
`param_2+0x12/+0x16`. Aucun producteur d'une base structurée ou de valeurs
non nulles n'est établi; aucune valeur synthétique n'est autorisée.

Preuve : `artifacts/goal-playable/caller-param2-static-20260823/RESULT.md`.

## Trace caller `param_2` — chemin inactif dans la fenêtre frontend (23 août 2026)

La sonde bornée `caller-param2-runtime-20260823` n'observe pas l'entrée
`0x82165E68` malgré les draws titre naturels. Aucun champ `+0x12/+0x16` ne peut
donc être promu en valeur de configuration. Le buffer CPU juste avant
présentation est disponible en PPM et reste entièrement noir ; la source du
défaut est donc toujours le payload Q1 guest absent, pas une permutation de
canaux Vulkan.

Preuve : `artifacts/goal-playable/caller-param2-runtime-20260823/RESULT.md`.
# Première frame enfant — cycle 1796 (24 août 2026)

- `PROUVÉ` : l'enfant pending reçoit son premier tick récursif dans
  `0x82323BB8`; son état constructeur impose la frame 0.
- `PROUVÉ` : un type 6 passe par `0x82322A80`, `MovieMemory`, puis
  `0x82325288` dans le même tick.
- `RÉFUTÉ` : identifier `D=0x2DD7936C` à partir d'un offset de descripteur
  BrandLogo. Le pointeur dépend du storage Title relocalisé.
- `INCONNU` : contenu des cinq mots de `D`, entrée frame 0, type/payload des
  éléments et premier callback persistant.

Preuve compacte :
`artifacts/goal-playable/title-child-initial-frame-static-20260824/RESULT.md`.
# Cycle 1799 — table relocalisée de la liste 13

- PROUVÉ : l'élément initial du sous-enfant `0x2E3F1350` est type 4,
  `list_index=13`.
- PROUVÉ : table runtime `0x2DD7963C`, descripteur 13 à
  `[0x2DD796A4,0x2DD796AC)` ; formule commune jusqu'au dispatch interne
  `0x8264D074`.
- PROUVÉ : `Title` et `TitleUS` coexistent dans le package PAL aux entrées
  `DATA.TBL[177]` et `[178]`; `TitleUS` est la variante anglaise.
- INCONNU : les deux mots de l'entrée 13, la liste et son record.
- RÉFUTÉ comme preuve : assimiler l'intégralité du leaf compact `TitleUS` à
  l'image heap matérialisée.
- Preuve :
  `artifacts/goal-playable/title-list13-static-boundary-20260824/RESULT.md`.
# Cycle 1815 — `0x82118D18` n'a pas d'appel direct matérialisé

- `PROUVÉ` : le HIR qualifié charge le dword couleur à `r4+0x18` et le copie
  aux quatre vertices P ; le record fournit aussi les flottants à
  `+0x48/+0x4C/+0x54`.
- `PROUVÉ` : seul le fichier HIR propre à `0x82118D18` contient cette adresse
  dans le corpus `hirdump_title_1313703910`.
- `PROUVÉ` : l'entrée `0x82118D18` de
  `analysis/demo/ac6-demo-static-decomp-atlas-v1.json` porte
  `direct_calls=[]`.
- `RÉFUTÉ` : remonter le producteur par un appel PPC direct déjà exposé dans
  le HIR/l'atlas.
- `INCONNU` : cellule et index de dispatch, producteur de `r4+0x18`, provenance
  amont de `BFFF0000`.

Preuves complètes :
`artifacts/goal-playable/title-p1-color-producer-static-20260824/`.
# Cycle 1816 — table de writers P/Q

- `PROUVÉ` : le build codegen-on lie son `xex-basefile.bin` au
  `demo-game-file/extracted/stfs-root/Default.xex` qualifié.
- `PROUVÉ` : `0x82118D18` apparaît exactement à `0x82009E98` et
  `0x82009EC0`, soit `+0x0C` des entrées `0x82009E8C` et `0x82009EB4` de
  taille `0x14`.
- `PROUVÉ` : les deux entrées diffèrent au dword `+0x04`
  (`09000001`/`09000003`) et partagent `+0x00=0D`, `+0x08=826F61C0`,
  `+0x10=821187A8`.
- `PROUVÉ` : zéro branche PPC directe vers `0x82118D18` et zéro construction
  directe des adresses de cellule ; l'indirection part d'une base de table.
- `INCONNU` : consommateur/index, sémantique des discriminants, producteur de
  `r4+0x18`.

Preuves complètes :
`artifacts/goal-playable/title-p1-dispatch-xref-static-20260824/`.
# Gate retail US — frontière SSSE3 (26 août 2026)

- `PROUVÉ` : XEX/ISO US, commit upstream et SDK sont qualifiés par le manifeste.
- `PROUVÉ` : la première erreur Clang est l'intrinsèque `_mm_shuffle_epi8`
  compilée sans la feature SSSE3 dans `rexcore/memory.cpp`.
- `PROUVÉ` : statut Ninja 1, aucun `generated/`, reçu ou binaire produit.
- `NON EXÉCUTÉ` : validations aval et capture d'audit gameplay.
- `BLOQUÉ` : PAL jusqu'à un succès gameplay US audité.

Preuve : `reports/retail-us-build-ssse3-boundary.md`.

## Retail US Vulkan — frontière de synchronisation de route (26 août 2026)

Le binaire installé
`ee41a8c33db3f9a1364ab4e7e59a31e54c7f7373d49afd83fc126eb671168af3`
passe les 16 tests ciblés avec Vulkan complet, SDL dummy, zéro symbole D3D12
et zéro `bin/bin`. Le run corrigé produit des `PRESENT` continus et affiche le
dialogue retail de création des données, mais expire à 4/96 étapes sans trap :
le premier prédicat `type28=30` et les trois autres familles de marqueurs sont
absents du host US. La capture système et le résultat complet sont conservés ;
ils ne constituent pas une preuve gameplay. Le prochain travail est le
cross-match statique qualifié des quatre fonctions candidates US ; PAL reste
bloqué.

Preuve : `reports/retail-us-vulkan-route-sync-boundary.md` ; artefacts :
`artifacts/retail-us-gate-dummy-audio-20260826/`.

## Retail US — observables qualifiés, première divergence `37 -> 36` (26 août 2026)

- `PROUVÉ` : projet `ghidra-projects/ac6-us`, programme `default.xex`, langage
  `PowerPC:BE:64:Xenon`, XEX `6eefba42…67cbbbc`.
- `PROUVÉ` : quatre entrées/étendues `.pdata`, octets et ABI scellés par
  `ac6.retail-hook-map.v1` (`2c67cc1f…5778b4`).
- `PROUVÉ` : wrappers post-appel read-only, aucun store invité ; relink sans
  codegen ; 16/16 tests, Vulkan, SDL dummy, zéro D3D12 et zéro `bin/bin`.
- `PROUVÉ` : la route publie `type28=30 -> 37 -> 36`, puis expire sur le
  prédicat `type28=35` à 10/96 étapes.
- `RÉFUTÉ` : succès gameplay US ; zéro des 27 captures et aucun arrêt propre.
- `BLOQUÉ` : PAL Europe Rev 1 et toute nouvelle exécution tant qu'une nouvelle
  décision explicite ne redéfinit pas ce gate.

Preuve : `reports/retail-us-route-type35-failed-closed.md` ; résultat
`artifacts/retail-us-gate-route-sync-20260826/RESULT.json`.

## Retail US — captures de la sélection et validation révisée (26 août 2026)

- `PROUVÉ VISUELLEMENT` : `30` est un écran à bouton unique `OK`; `37` est
  `Create new Game Data?` avec `NO` sélectionné; sa validation produit `36`.
- `PROUVÉ STATIQUEMENT` : dans `0x821C4FB0`, le résultat non nul écrit `36` et
  le chemin succès écrit `35` à `screen+0x1C`.
- `CORRIGÉ` : le même `Left` est déplacé après `37`; route toujours 96 étapes,
  SHA-256 `74e3f395…d81c8f`.
- `NON VALIDÉ` : le run complet révisé s'arrête avant tout `type28` après 841
  `PRESENT`, à 4/96 étapes, sans fatal ni erreur SDL/XAudio.
- `BLOQUÉ` : nouvelle exécution identique et PAL sans nouvelle autorisation.

Captures : `artifacts/retail-us-type37-36-visual-20260826/` ; reçu complet :
`artifacts/retail-us-gate-type37-selection-20260826/RESULT.json`.

## Retail US — couture `NO -> YES -> 35` et recapture propre (26 août 2026)

- `PROUVÉ AU RUNTIME` : dans le diagnostic 15/15, `type28=37` affiche `NO`,
  `Left` sélectionne `YES`, puis `space` atteint `type28=35`.
- `RÉFUTÉ` : le panneau diagnostics absorbe `Left`; la sélection change alors
  qu'il est visible.
- `NON AUDITABLE VISUELLEMENT` : ces trois PNG restent obstrués par le panneau,
  dont le clic de fermeture avait précédé la création ImGui.
- `CORRIGÉ DANS LE HARNESS` : fermeture différée au premier point de capture,
  réservée aux routes diagnostiques et sans rebuild.
- `NON ATTEINT` : la recapture corrective expire à 4/15 étapes après 1 725
  `PRESENT`, zéro `type28`, aucun fatal et aucun PNG.

Preuves : `artifacts/retail-us-type37-clean-20260826/RESULT.json` et
`artifacts/retail-us-type37-clean-v2-20260826/RESULT.json`.

## Retail US — monde soumis mais composition noire (26 août 2026)

- `PROUVÉ` : runtime statut 0, arrêt propre, zéro fatal/trap, 24 captures et
  panneau hôte absent.
- `PROUVÉ` : cinématique 3D visible, puis HUD/radar verts sur fond noir.
- `PROUVÉ` : au premier HUD noir, objet/caméra/radio restent actifs et le
  backend reçoit ~1 100–1 200 draws, 85–87 resolves et 28 pointlists.
- `RÉFUTÉ` : disparition de la soumission gameplay comme cause du premier
  écran noir ; la chute à 27/106 draws est postérieure et suit Escape/Start.
- `OUVERT` : premier resolve ou changement de RT qui perd la scène avant le
  frontbuffer. L'observable exact est patché, testé, non rebuildé.

Preuve : `reports/retail-us-black-world-resolve-boundary-20260826.md` et
`artifacts/retail-us-world-owner-runtime-20260826/RESULT.json`.

## Retail US — binaire resolve qualifié (26 août 2026)

- `PROUVÉ` : relink statut 0 avec réutilisation de
  `generated/sources.cmake`, sans codegen.
- `PROUVÉ` : binaire installé `98bf39a9…37e4e3`, 37 659 144 octets.
- `PROUVÉ` : Vulkan complet, SDL dummy, zéro D3D12, absence de `bin/bin` et 21
  tests légers réussis.
- `PROUVÉ` : marqueurs RT/resolve/frontbuffer présents et exigés par la
  validation.
- `NON EXÉCUTÉ` : corrélation runtime de la dernière frame 3D au premier HUD
  noir ; elle est réservée au job lourd unique de la prochaine session.

Preuve : `reports/retail-us-resolve-observable-rebuild-20260826.md`.
# Retail US 2026-08-26 — frontière intermédiaire Vulkan

- PROUVÉ VISUELLEMENT : étape 80 en 3D, étapes 82 et 88 avec HUD sur monde noir ; 24 captures, panneau absent.
- PROUVÉ : zéro fatal/trap et journal allant jusqu'à `Execution complete`.
- ÉCHEC FERMÉ : délai de nettoyage dépassé, `game_status=-9`, arrêt non propre.
- PROUVÉ : mêmes destinations/LR de resolve principal et final, même frontbuffer entre image 3D et HUD noir.
- RÉFUTÉ : changement des extrémités resolve/frontbuffer comme cause de la première perte du monde.
- PRÉPARÉ : sonde hôte opt-in des passes Vulkan et plages de resolve intermédiaires, bornée à 4 candidats puis 1/60.
- VALIDÉ LÉGÈREMENT : 22/22 tests et `diff --check`; aucun rebuild dans cette session lourde.

Preuve : `reports/retail-us-intermediate-resolve-boundary-20260826.md`.
# Retail US 2026-08-26 — binaire frontière Vulkan qualifié

- PROUVÉ : rebuild/relink unique sous cgroup, statut 0, pic mémoire ~2,3 Gio.
- PROUVÉ : réutilisation de `generated/sources.cmake`, aucun codegen.
- PROUVÉ : binaire build/install identique, `b854c984…b4de0`, 37 674 768 octets.
- PROUVÉ : validation statut 0, Vulkan complet, SDL dummy, zéro D3D12 et absence de `bin/bin`.
- PROUVÉ : marqueurs frame/pass/resolve présents ; sonde `false` par défaut.
- NON EXÉCUTÉ : runtime causal et audit visuel, réservés à la prochaine session lourde.

Preuve : `reports/retail-us-vulkan-frontier-observable-rebuild-20260826.md`.
# Retail US 2026-08-26 — frontière après fragment monde

- PROUVÉ : runtime effectif statut 0, arrêt propre, 24 captures, zéro fatal/trap, panneau absent.
- PROUVÉ VISUELLEMENT : cinématique 3D à l'étape 80 ; HUD noir aux étapes 82, 85 et 88.
- PROUVÉ : `sample 2400 -> 2460` conserve 60 passes, 65 resolves et le frontbuffer.
- PROUVÉ : 378 draws D5B4 monde subsistent vers EDRAM 4, depth 720/1, pitch 640, MSAA 1.
- RÉFUTÉ : absence de pass monde ou perte globale des resolves comme première cause.
- PRÉPARÉ : override blanc de la seule sortie finale RT0 D5B4, opt-in, sans panneau ni écriture invitée.
- VALIDÉ LÉGÈREMENT : 23/23 tests et `diff --check`; aucun rebuild après le runtime lourd.

Preuve : `reports/retail-us-vulkan-frontier-runtime-20260826.md`.
# Retail US 2026-08-26 — binaire final-white D5B4 qualifié

- PROUVÉ : rebuild/relink unique sous cgroup, statut 0, pic mémoire ~2,2 Gio.
- PROUVÉ : codegen existant réutilisé ; seul le traducteur SPIR-V modifié est recompilé.
- PROUVÉ : build/install identiques, `0f208aa3…5c4c`, 37 676 704 octets.
- PROUVÉ : validation statut 0, Vulkan complet, SDL dummy, zéro D3D12, aucun `bin/bin`.
- PROUVÉ : cvar et marqueur final-white liés ; override `false` par défaut.
- VALIDÉ LÉGÈREMENT : 23/23 tests et `diff --check`.
- NON EXÉCUTÉ : bras visuel final-white, réservé à la prochaine session lourde.

Preuve : `reports/retail-us-d5b4-final-output-rebuild-20260826.md`.
# Retail US 2026-08-26 — final-white D5B4 négatif

- PROUVÉ : runtime statut 0, arrêt propre, 88/88 étapes, 24 captures, zéro fatal/trap.
- PROUVÉ : marqueur final-white exactement une fois et dump D5B4 exact présent.
- PROUVÉ VISUELLEMENT : cinématique 3D puis trois HUD sur monde noir, panneau absent.
- PROUVÉ : `world_center_mean=0.0` malgré la sortie RT0 D5B4 forcée à blanc.
- RÉFUTÉ : valeur lumière/sortie finale D5B4 comme cause suffisante du monde noir.
- PRÉPARÉ : bypass depth/stencil limité aux draws D5B4, conservant final-white comme baseline.
- VALIDÉ LÉGÈREMENT : 24/24 tests et `diff --check`; aucun rebuild après le runtime lourd.

Preuve : `reports/retail-us-d5b4-final-white-runtime-20260826.md`.
# Retail US 2026-08-26 — binaire bypass depth/stencil qualifié

- PROUVÉ : rebuild/relink unique sous cgroup, statut 0, codegen réutilisé.
- PROUVÉ : build/install identiques, `d00b9022…eb1c`, 37 681 672 octets.
- PROUVÉ : validation statut 0, Vulkan complet, SDL dummy, zéro D3D12, aucun `bin/bin`.
- PROUVÉ : cvar et marqueur depth/stencil liés ; tous les diagnostics false par défaut.
- VALIDÉ LÉGÈREMENT : 24/24 tests et `diff --check`.
- NON EXÉCUTÉ : bras visuel depth/stencil, réservé à la prochaine session lourde.

Preuve : `reports/retail-us-d5b4-depth-stencil-rebuild-20260826.md`.
# Retail US 2026-08-26 — bypass depth/stencil négatif

- PROUVÉ : runtime statut 0, arrêt propre, 88/88 étapes, 24 captures, zéro fatal/trap.
- PROUVÉ : marqueurs final-white et depth/stencil exactement une fois chacun.
- PROUVÉ VISUELLEMENT : trois HUD sur monde noir, panneau absent.
- PROUVÉ : `world_center_mean=0.0` malgré sortie blanche et depth/stencil neutralisé.
- RÉFUTÉ : rejet depth/stencil D5B4 comme cause suffisante.
- PRÉPARÉ : bypass front/back culling limité aux draws D5B4, conservant les deux bras précédents.
- VALIDÉ LÉGÈREMENT : 25/25 tests et `diff --check`; aucun rebuild après le runtime lourd.

Preuve : `reports/retail-us-d5b4-depth-stencil-runtime-20260826.md`.
# Retail US 2026-08-26 — binaire bypass culling qualifié

- PROUVÉ : rebuild/relink unique sous cgroup, statut 0, pic mémoire ~1,2 Gio, codegen réutilisé.
- PROUVÉ : build/install identiques, `8600824e…97d1`, 37 682 448 octets.
- PROUVÉ : validation statut 0, Vulkan complet, SDL dummy, zéro D3D12, aucun `bin/bin`.
- PROUVÉ : cvar et marqueur culling liés ; tous les diagnostics false par défaut.
- VALIDÉ LÉGÈREMENT : 25/25 tests et `diff --check`.
- NON EXÉCUTÉ : bras visuel culling, réservé à la prochaine session lourde.

Preuve : `reports/retail-us-d5b4-cull-rebuild-20260826.md`.
# Retail US 2026-08-26 — bypass culling négatif

- PROUVÉ : runtime statut 0, arrêt propre, 88/88 étapes, 24 captures, zéro fatal/trap.
- PROUVÉ : trois marqueurs D5B4 exactement une fois chacun.
- PROUVÉ VISUELLEMENT : trois HUD sur monde noir, panneau absent.
- PROUVÉ : `world_center_mean=0.0` malgré sortie blanche, depth/stencil et culling neutralisés.
- RÉFUTÉ : front/back culling D5B4 comme cause suffisante.
- PRÉPARÉ : catalogue read-only viewport/scissor par passe, sans nouvelle mutation runtime.
- VALIDÉ LÉGÈREMENT : 25/25 tests et `diff --check`; aucun rebuild après le runtime lourd.

Preuve : `reports/retail-us-d5b4-cull-runtime-20260826.md`.
# Retail US 2026-08-26 — binaire fenêtre raster qualifié

- PROUVÉ : rebuild/relink unique sous cgroup, statut 0, pic mémoire ~2,5 Gio, codegen réutilisé.
- PROUVÉ : build/install identiques, `4b4a7140…41af`, 37 686 648 octets.
- PROUVÉ : validation statut 0, Vulkan complet, SDL dummy, zéro D3D12, aucun `bin/bin`.
- PROUVÉ : marqueur de passe lié avec viewport/scissor ; observable read-only.
- VALIDÉ LÉGÈREMENT : 25/25 tests et `diff --check`.
- NON EXÉCUTÉ : catalogue runtime D5B4, réservé à la prochaine session lourde.

Preuve : `reports/retail-us-d5b4-raster-window-rebuild-20260826.md`.
# Retail US 2026-08-26 — runtime fenêtre raster non concluant

- PROUVÉ : wrapper/jeu/Xvfb statut 0, arrêt propre, 88/88 opérations, 24 captures.
- PROUVÉ VISUELLEMENT : panneau vert absent.
- PROUVÉ : captures 70–88 identiques, SHA-256 `666ec8bf…f49259`, 0 pixel changé entre hangar et prétendue carte.
- RÉFUTÉ : le reçu générique `diagnostic-capture-ready` comme preuve de progression sémantique.
- PROUVÉ : quatre catalogues 628/41, aucune passe D5B4 ; le filtre lourd omet la frontière recherchée.
- PRÉPARÉ : inclusion read-only des frames D5B4 et rejet du handoff resté au hangar.
- VALIDÉ LÉGÈREMENT : 26/26 tests, compilation Python et `diff --check`.

Preuve : `reports/retail-us-d5b4-raster-window-runtime-20260826.md`.
# Retail US 2026-08-26 — catalogue D5B4 inclusif qualifié

- PROUVÉ : rebuild/relink unique sous cgroup, statut 0, codegen réutilisé.
- PROUVÉ : build/install identiques, `bec7412e…c75ac`, 37 686 656 octets.
- PROUVÉ : validation statut 0, Vulkan complet, SDL dummy, zéro D3D12 et aucun `bin/bin`.
- PROUVÉ : toute frame contenant D5B4 devient candidate du catalogue read-only.
- VALIDÉ LÉGÈREMENT : 26/26 tests, upstream/build identiques et `diff --check`.
- NON EXÉCUTÉ : runtime D5B4 inclusif, réservé à la prochaine session lourde.

Preuve : `reports/retail-us-d5b4-inclusive-catalog-rebuild-20260826.md`.
# Retail US 2026-08-26 — startup brand movie qualifié

- PROUVÉ : runtime limité à 910 s, échec opération 5, zéro fatal/trap et zéro capture de route.
- PROUVÉ VISUELLEMENT : scène 3D du générique Namco Bandai, panneau vert absent.
- PROUVÉ : 1 732 PRESENT puis arrêt ; aucun `type28=30`, aucune passe D5B4.
- RÉFUTÉ : 180 PRESENT comme prédicat suffisant de disponibilité du menu.
- PROUVÉ : reçus réussis antérieurs atteignent `type28=30` vers 45 s après lancement.
- PRÉPARÉ : settle de 45 s avant toute entrée, limité aux routes handoff/render-summary.
- VALIDÉ LÉGÈREMENT : compilation Python, 26/26 tests et `diff --check`.

Preuve : `reports/retail-us-brand-movie-startup-stall-20260826.md`.
# Retail US 2026-08-26 — fenêtre titre 35/42 s requalifiée

- PROUVÉ : runtime échec fermé, opération 6, 1 830 PRESENT, zéro fatal/trap.
- PROUVÉ VISUELLEMENT : transition entièrement noire, panneau vert absent.
- PROUVÉ : aucun `type28=30`, aucune capture de route, aucune passe D5B4.
- RÉFUTÉ : settle de 45 s après 180 PRESENT, qui place l'entrée vers 53 s.
- PROUVÉ HISTORIQUEMENT : Escape 35 s, A 42 s, dialogue Game Data visible à 50 s.
- PRÉPARÉ : séquence absolue 35/42 s puis attente passive, sans pulses aveugles.
- VALIDÉ LÉGÈREMENT : compilation Python, 26/26 tests et `diff --check`.

Preuve : `reports/retail-us-brand-movie-settle-runtime-20260826.md`.
# Retail US 2026-08-26 — fenêtre titre variable bornée

- PROUVÉ : runtime échec fermé après 70 s/5 opérations, 3 499 PRESENT.
- PROUVÉ : aucun `type28=30`, aucune capture de route, aucun fatal/trap ni D5B4.
- RÉFUTÉ : paire 35/42 s seule comme garantie sur chaque lancement actuel.
- PRÉPARÉ : retries Escape/A conditionnels, bornés localement à 60 s.
- PRÉPARÉ : capture automatique de l'écran avant teardown sur échec diagnostique.
- VALIDÉ LÉGÈREMENT : compilation Python, 27/27 tests et `diff --check`.

Preuve : `reports/retail-us-title-window-runtime-20260826.md`.
# Retail US 2026-08-26 — D5B4 valide, monde noir composé

- PROUVÉ : 88/88 opérations, 24 captures, arrêt propre, statut 0, zéro fatal/trap.
- PROUVÉ VISUELLEMENT : cinématique 3D puis HUD/radar sur monde noir, panneau absent.
- PROUVÉ : garde hangar franchie, 921 599 pixels changés vers la carte tactique.
- PROUVÉ : 44 échantillons D5B4, scissor 640×720 couvrant la cible pitch 640.
- RÉFUTÉ : viewport/scissor D5B4 comme cause suffisante du monde noir.
- PROUVÉ : resolve final `0x1AB60000` stable ; `0x1C191000` 7→1 et `0x1B9C0000` 7→4.
- PROCHAIN : qualifier statiquement le premier producteur/consommateur différent de cette chaîne.

Preuve : `reports/retail-us-d5b4-window-valid-compose-boundary-20260826.md`.
# Retail US 2026-08-26 — propriétaires Resolve et observable aligné

- PROUVÉ STATIQUEMENT : `0x82337C68` possède LR `0x82337CC4`, table `0x82864C88+index×52`.
- PROUVÉ STATIQUEMENT : `0x8234D550` possède LR `0x8234D5F4`, destination `*(object+0)+28`.
- PROUVÉ : `0x821E2BB8` est Resolve ; `0x821E4630` est la couture Clear voisine.
- MANQUANT : records intermédiaires persistés reliant LR/objet à `0x1C191000`/`0x1B9C0000`.
- PRÉPARÉ : catalogue read-only sample/ordinal/LR/arguments/RT/depth/viewport aligné sur Vulkan.
- VALIDÉ LÉGÈREMENT : 28/28 tests et `diff --check`.

Preuve : `reports/retail-us-compose-resolve-static-qualification-20260826.md`.
# Retail US 2026-08-26 — catalogue Resolve aligné rebuildé

- PROUVÉ : rebuild/relink unique sous cgroup, statut 0, codegen réutilisé.
- PROUVÉ : build/install identiques, `f77554a7…572ea`, 37 686 952 octets.
- PROUVÉ : validation statut `pass`, Vulkan complet, SDL dummy, zéro D3D12/`bin/bin`.
- PROUVÉ : marqueur sample/ordinal/LR/arguments/RT/depth/viewport lié.
- VALIDÉ LÉGÈREMENT : 28/28 tests, upstream/build identiques et `diff --check`.
- NON EXÉCUTÉ : corrélation runtime des deux catalogues, réservée à la prochaine session lourde.

Preuve : `reports/retail-us-compose-resolve-catalog-rebuild-20260826.md`.

# Retail US 2026-08-26 — divergence du resolve final corrélée

- PROUVÉ : 88/88 opérations et 24 captures ; échec fermé au teardown `game_status=-2`.
- PROUVÉ VISUELLEMENT : cinématique 3D puis HUD/radar sur monde noir.
- PROUVÉ VISUELLEMENT : fenêtre « AC6 Graphics Diagnostics » absente du run courant.
- PROUVÉ : `0x1C191000` 7→1, `0x1B9C0000` 7→4, `0x1AB60000` stable à 1.
- PROUVÉ : au sample 2040, 91 appels invités et 91 resolves physiques sont alignés.
- QUALIFIÉ : LR `0x8234D5F4` perd 6 objets 640×360 et 3 objets 1280×720 pendant la transition.
- QUALIFIÉ STATIQUEMENT : `0x8234D550` résout `*(r3+0)+28` sans choisir l'objet.
- PROCHAIN : qualifier les sélecteurs `0x8234EF60` et `0x8234F558`.

Preuve : `reports/retail-us-compose-resolve-runtime-20260826.md`.

# Retail US 2026-08-26 — observable du propriétaire parent prêt

- QUALIFIÉ STATIQUEMENT : LR parent `0x8234F0E8` identifie la liste `0x8234EF60`.
- QUALIFIÉ STATIQUEMENT : LR parent `0x8234F598` identifie l'entrée `0x8234F558`.
- PRÉPARÉ : `owner_lr` lit seulement `r1+152` sous LR exact `0x8234D5F4`.
- ÉCHEC FERMÉ : relink statut 1, symbole legacy `GetFrameCaptureSummary()` retiré par la synchronisation.
- RÉPARÉ : hunk legacy restauré dans la copie build ; aucun second job lourd.
- PRÉSERVÉ : binaire installé validé `f77554a7…572ea` intact.
- VALIDÉ LÉGÈREMENT : 28/28 tests et `diff --check`.

Preuve : `reports/retail-us-compose-owner-rebuild-failed-20260826.md`.

# Retail US 2026-08-26 — catalogue du propriétaire parent relié

- PROUVÉ : relink unique statut 0, aucun codegen.
- PROUVÉ : build/install identiques, `345dca09…e4f61`, 37 686 960 octets.
- PROUVÉ : validation statique `pass`, Vulkan complet, SDL dummy, zéro D3D12/`bin/bin`.
- PROUVÉ : marqueur `[ac6-compose-resolve] ... owner_lr=...` lié.
- VALIDÉ LÉGÈREMENT : 28/28 tests et `diff --check`.
- NON EXÉCUTÉ : runtime causal, réservé à la prochaine session lourde.

Preuve : `reports/retail-us-compose-owner-relink-20260826.md`.

# Retail US 2026-08-26 — runtime owner non consommable, startup corrigé

- ÉCHEC FERMÉ : 5/88 opérations, 117,9 s, `type28=30` absent, `game_status=-9`.
- PROUVÉ VISUELLEMENT : film Namco Bandai 3D encore affiché, panneau absent.
- PROUVÉ : aucun fatal/trap et aucun sample Mission 01 `owner_lr`.
- PROUVÉ PAR DEUX REÇUS : `type28=30` précédait leur appui A programmé.
- PRÉPARÉ : Escape seul, conditionnel après 20 s, borné à 90 s ; A après prédicat.
- VALIDÉ LÉGÈREMENT : 28/28 tests produit, 19/19 runner, `py_compile`, `diff --check`.

Preuve : `reports/retail-us-compose-owner-startup-failure-20260826.md`.

# Retail US 2026-08-26 — runtime owner terminé proprement

- PROUVÉ : route diagnostique `85/85`, 24 captures, `game_status=0`,
  `xvfb_status=0`, arrêt propre et zéro fatal/trap.
- PROUVÉ VISUELLEMENT : `step-77` conserve la 3D; `step-79/82/85` montrent le
  HUD invité sur monde noir; le panneau « AC6 Graphics Diagnostics » est absent.
- PROUVÉ : `0x8234F0E8` fournit les surfaces de composition et `0x8234F598` le
  resolve final vers `0x1AB60000`.
- PROUVÉ : D5B4 persiste après la transition noire; les essais white/depth/cull
  précédents n'ont pas restauré le monde.
- PROCHAIN : qualifier statiquement `ResolveInfo`, l'ownership map,
  `DumpRenderTargets` et `RequestSwapTexture`; aucun reçu gameplay US ni PAL
  n'est encore autorisé.

Preuve : `reports/retail-us-compose-owner-runtime-20260826.md`.

# Retail US 2026-08-26 — ResolveInfo statique borné

- PROUVÉ STATIQUEMENT : `GetResolveInfo` sépare source EDRAM, destination
  tuilée et plage de dump; aucun champ n'est synthétisé par AC6.
- PROUVÉ STATIQUEMENT : `DumpRenderTargets` consomme exclusivement la carte
  d'ownership puis écrit dans `edram_buffer_` avant la copie compute.
- PROUVÉ STATIQUEMENT : la destination est réservée avant dispatch et marquée
  GPU-écrite après dispatch, puis invalidée pour `RequestSwapTexture`.
- PRÉPARÉ : marqueur `[ac6-resolve-info]` borné et read-only (8 puis 1/256),
  activé par le harness render-summary.
- PROCHAIN : un seul rebuild/runtime dans une session fraîche pour comparer la
  première divergence aux frames US déjà capturées; pas de PAL avant gameplay.

Preuve : `reports/retail-us-resolve-info-static-20260826.md`.

# Retail US 2026-08-26 — runtime ResolveInfo/ownership

- PROUVÉ : route diagnostique `85/85`, 24 captures, `game_status=0`, arrêt
  propre et zéro fatal/trap avec le binaire `6b9c714a…e7b265`.
- PROUVÉ : `[ac6-resolve-info]` et `[ac6-resolve-dump]` ont été émis ; chaque
  échantillon de dump a `rectangles=1`, donc l'ownership vide est exclue.
- PROUVÉ : D5B4 (`A1863AF658456A14` / `D5B4F4A878949938`) persiste aux samples
  2460–2640 ; le monde reste noir (`world_center_mean=0`).
- PROCHAIN : observer `RequestSwapTexture` → `LoadTextureData` avec le cvar
  borné `ac6_log_swap_texture`.

Preuve : `reports/retail-us-resolve-info-runtime-20260826.md`.

# Retail US 2026-08-26 — chargement de la texture de swap qualifié

- PROUVÉ : rebuild/relink sans codegen, validation pass, binaire
  `bf80b852…cca56a0a`.
- PROUVÉ : 85/85 opérations, 24 captures, arrêt propre, statut 0, zéro
  fatal/trap et panneau hôte absent.
- PROUVÉ : fetch swap stable (`1280×720`, format 6, base `0x1AB60000`) et
  statut `ok`.
- PROUVÉ : chaque échantillon texture est `prepared → commit-ok`, plage
  `0x1AB60000+0x398000`, invalidation `outdated=0x1→0`.
- RÉFUTÉ : sélection de texture, watches/invalidation et chargement résident
  comme causes suffisantes du monde noir.
- PROUVÉ : la transition HSM arrive pendant des frames encore chargées ; la
  chute après `0x822E71A0` est postérieure au premier noir.
- PROCHAIN : qualification statique du contenu/producteur des surfaces
  intermédiaires `0x1C191000`/`0x1B9C0000` avant `0x1AB60000`, puis un seul
  correctif/runtime si une branche fautive est prouvée.

Preuve : `reports/retail-us-texture-load-runtime-20260826.md`.

# Retail US 2026-08-26 — sonde D5B4 reportée

- ÉCHEC FERMÉ : le runtime de sonde a utilisé `bf80…a0a` après un faux rejet
  du filtre D3D12 sur l'adresse `0x00d3d120`; aucune ligne D5B4 n'est probante.
- CORRIGÉ : le filtre borne maintenant le texte des symboles (`31` tests),
  sans changer la politique Vulkan-only.
- PROUVÉ : installation actuelle `dce792bd…7691`, marqueur
  `[ac6-d5b4-texture]` présent.
- PROCHAIN : une seule route render-summary fraîche pour consommer la sonde;
  aucun rebuild supplémentaire.

Preuve : `reports/retail-us-d5b4-texture-runtime-20260826.md`.

# Retail US 2026-08-27 — retry sonde D5B4 au point consommateur

- PROUVÉ : binaire `dce792bd…7691`, route `85/85`, 24 captures, statut 0,
  arrêt propre, zéro fatal/trap et panneau hôte absent.
- PROUVÉ : les passes `[ac6-frontier-pass]` sont émises, mais ni le hash
  pixel shader D5B4 (`D5B4F4A878949938`) ni la sonde `[ac6-d5b4-texture]`
  n'apparaissent dans le log.
- CORRIGÉ : la sonde read-only est déplacée juste après `Ac6FrontierPass`
  lorsque `pixel_shader == kAc6D5b4PixelShader`, dans upstream et build.
- PROCHAIN : un seul rebuild/relink puis une seule route fraîche ; aucun PAL
  avant le reçu gameplay US.

Preuve : `reports/retail-us-d5b4-texture-runtime-retry-20260827.md`.

# Retail US 2026-08-27 — rebuild sonde D5B4 relocalisée

- PROUVÉ : rebuild/relink ciblé sous cgroup, statut 0, sans nouvelle
  génération C++.
- PROUVÉ : validation statique pass, Vulkan/ReXGlue complet, SDL dummy, zéro
  D3D12 et aucun `bin/bin`.
- PROUVÉ : binaire installé `6db8d006…0768`, avec `[ac6-d5b4-texture]` et
  `D5B4F4A878949938`; 32 tests ciblés passent.
- PROCHAIN : une seule route `render-summary` fraîche dans une session
  lourde séparée ; le reçu US gameplay n'est pas encore atteint.

Preuve : `reports/retail-us-d5b4-texture-rebuild-20260827.md`.

# Retail US 2026-08-27 — sonde texture D5B4 consommée

- PROUVÉ : binaire `6db8d006…0768`, route `85/85`, 24 captures, arrêt propre,
  statut 0, zéro fatal/trap et panneau hôte absent.
- PROUVÉ : la sonde `[ac6-d5b4-texture]` produit 3 472 lignes ; les fetchs
  D5B4 sont des textures tuilées format 20 (`k_DXT4_5`) avec `used_mask=0x1`.
- NON RÉSOLU : les captures terminales restent HUD/radar sur monde noir
  (`world_center_mean=0`), donc le reçu gameplay US manque encore.
- PRÉPARÉ : ajout read-only de `RB_COLOR_MASK`, `normalized_color_mask`,
  `is_rasterization_done` et `edram_mode` au prochain échantillon D5B4.

Preuve : `reports/retail-us-d5b4-texture-runtime-20260827.md`.

# Retail US 2026-08-27 — rebuild observables masque/cible D5B4

- PROUVÉ : rebuild/relink ciblé sous cgroup, sans génération C++ ; validation
  statique pass, Vulkan/ReXGlue, SDL dummy, zéro D3D12 et `bin/bin` absent.
- PROUVÉ : binaire installé `24b8d603…9856`, 32 tests ciblés passés.
- PROUVÉ : le marqueur D5B4 consigne désormais `RB_COLOR_MASK`,
  `normalized_color_mask`, rasterisation et mode EDRAM.
- PROCHAIN : une seule route `render-summary` fraîche dans une session lourde
  séparée ; le reçu gameplay US manque encore.

Preuve : `reports/retail-us-d5b4-target-mask-rebuild-20260827.md`.

# Retail US 2026-08-27 — masque logique D5B4 fermé

- PROUVÉ : runtime `24b8d603…9856`, 85/85, 24 captures, arrêt propre,
  `game_status=0`, zéro fatal/trap.
- PROUVÉ : 3 406 échantillons D5B4 ; tous `RB_COLOR_MASK=0xF`,
  `normalized_color_mask=0xF`, rasterisation active, `edram=4`.
- RÉFUTÉ : masque logique couleur et rasterisation comme causes suffisantes du
  monde noir.
- PROCHAIN : qualifier chemin Vulkan/render pass/attachements avec une sonde
  read-only, puis corriger uniquement la première divergence prouvée.

Preuve : `reports/retail-us-d5b4-target-mask-runtime-20260827.md`.

# Retail US 2026-08-27 — rebuild observables d’attachement hôte D5B4

- PROUVÉ : rebuild/relink ciblé sous cgroup, statut 0, sans génération C++.
- PROUVÉ : validation statique pass ; Vulkan/ReXGlue, SDL dummy, zéro D3D12,
  `bin/bin` absent ; ctest 16/16 et suite Python 32/32.
- PROUVÉ : binaire installé
  `b6497489…45eaa` (37 713 112 octets), avec chemin Vulkan, clé render pass
  et `depth_and_color_used` ajoutés au marqueur D5B4.
- PROCHAIN : une seule route `render-summary` fraîche dans une session lourde
  séparée pour qualifier l’attachement au premier frame noir.

Preuve : `reports/retail-us-d5b4-host-target-rebuild-20260827.md`.

# Retail US 2026-08-27 — échec startup avant D5B4

- ÉCHEC FERMÉ : runtime `render-summary` avec
  `b6497489…45eaa`, opération 2 (`type28=30` non atteint), après 119,7 s ;
  `game_status=-9`, `clean_shutdown=false`, zéro fatal/trap.
- PROUVÉ : 165 `PRESENT` seulement, tous sur le logo Bandai Namco ; aucun
  `type28`, `selector44`, `state40`, frontier ou `[ac6-d5b4-texture]`.
- PROUVÉ : capture d’échec sans panneau de diagnostic vert.
- DÉCISION : la session ne qualifie pas le render pass/les attachements et ne
  justifie aucun rebuild ; un retry startup borné reste prévu.

Preuve : `reports/retail-us-d5b4-host-target-runtime-startup-failure-20260827.md`.

# Retail US 2026-08-27 — qualification statique composition/world-content

- QUALIFIÉ : les propriétaires composition `0x8234F0E8` et `0x8234F598` sont
  reliés aux appels Resolve canoniques `0x82337CC4` et `0x8234D5F4` ; les corps
  PPC générés ne forcent aucune destination.
- QUALIFIÉ : `VulkanRenderTargetCache::Resolve` conserve la chaîne
  `GetResolveInfo` → `DumpRenderTargets` → compute resolve et le marquage de la
  plage après dispatch.
- PRÉPARÉ : extension read-only du marqueur D5B4 aux bases/formats, pitch,
  MSAA, viewport et scissor ; aucune nouvelle génération ni runtime dans cette
  session.

Preuve : `reports/retail-us-compose-world-content-static-qualification-20260827.md`.

# Retail US 2026-08-27 — relink sonde composition/world-content

- PROUVÉ : build/relink sous cgroup terminé avec statut 0, sans nouvelle
  génération C++ ; `generated/sources.cmake` réutilisé.
- PROUVÉ : validation statique `16/16`, Vulkan/ReXGlue, SDL dummy, zéro D3D12
  et `bin/bin` absent.
- PROUVÉ : binaire installé
  `43a35a00c3196874e00b474277df32ddd310754b0d83a6af3f1d19d0aa9e313a` avec la
  sonde D5B4 étendue.
- PROCHAIN : runtime unique dans une session lourde séparée ; le gameplay US
  reste non validé et PAL fermé.

Preuve : `reports/retail-us-compose-world-content-probe-relink-20260827.md`.

# Retail US 2026-08-27 — runtime sonde composition/world-content

- ÉCHEC FERMÉ DU REÇU : `85/85`, 24 captures, mais teardown
  `clean_shutdown=false`, `game_status=-2`, zéro fatal/trap.
- PROUVÉ : D5B4 garde la même cible/attachements avant et après le noir
  (`color_base=0`, `depth_base=720`, pitch 640, MSAA 2×, render pass `0x8D`,
  attachements `0x3`) ; les draws/resolves restent nombreux.
- NON RÉSOLU : 3D visible à l'étape 77, monde noir avec HUD aux étapes 79/82/85.
- PRÉPARÉ : readback borné de contenu final via `ac6_log_resolve_content` et
  `--mission-resolve-content`.

Preuve : `reports/retail-us-compose-world-content-probe-runtime-20260827.md`.

# Retail US 2026-08-27 — build sonde resolve-content

- PROUVÉ : build/relink sous cgroup terminé avec statut 0 ;
  `generated/sources.cmake` réutilisé, aucune génération C++.
- PROUVÉ : validation statique `16/16`, Vulkan/ReXGlue, SDL dummy, zéro
  D3D12 et `bin/bin` absent.
- PROUVÉ : binaire installé
  `4f8142aa926ecb467d1743f6e2eb25ad05cc921ced7fc915a84dbcf1fa0784a` ; la
  sonde read-only `ac6_log_resolve_content` couvre la plage finale
  `0x1AB60000` par quatre fenêtres bornées.
- PROCHAIN : runtime unique séparé avec `--mission-resolve-content`, puis
  décision sur la première divergence de contenu. PAL reste fermé.

Preuve : `reports/retail-us-resolve-content-probe-build-20260827.md`.

# Retail US 2026-08-27 — runtime sonde resolve-content

- PROUVÉ : `RESULT.json` est propre (`clean_shutdown=true`, `game_status=0`,
  `85/85`, 24 captures) avec le binaire `4f8142aa…0784a`.
- PROUVÉ : comparaison visuelle : cinématique 3D à l'étape 77, puis monde
  noir avec HUD vert aux étapes 79/82/85 ; aucune interface de diagnostic hôte.
- PROUVÉ : readback borné `0x1AB60000` riche avant la transition puis réduit à
  `nonzero_bytes=3448`, `byte_sum=6108` pendant le HUD noir.
- QUALIFIÉ : les `ResolveInfo` 160×90 alternent entre `0x1AB60000` et
  `0x1B9C0000`; la couture restante est le choix/chargement de texture swap.
- PROCHAIN : une trace runtime ciblée `--mission-swap-source-probe`, sans
  rebuild ni génération C++, puis décision sur la première divergence.

Preuve : `reports/retail-us-resolve-content-probe-runtime-20260827.md`.

# Retail US 2026-08-27 — sonde resolve double-target préparée

- QUALIFIÉ : les logs `[ac6-swap-texture]` et `[ac6-texture-load]` confirment
  la clé `0x1AB60000` et son rechargement après invalidation ; le readback
  final devient quasi vide à la transition noire.
- PRÉPARÉ : extension read-only de `ac6_log_resolve_content` à
  `0x1B9C0000`, sans écriture invitée ni modification du resolve.
- PROCHAIN : build/relink unique sous cgroup puis runtime séparé pour mesurer
  les deux destinations. PAL reste fermé.

Preuve : `reports/retail-us-resolve-content-dual-target-static-20260827.md`.

# Retail US 2026-08-27 — build sonde resolve double-target

- PROUVÉ : build/relink cgroup terminé avec statut 0 ; codegen existant
  réutilisé, aucune nouvelle génération C++.
- PROUVÉ : validation statique `16/16`, Vulkan/ReXGlue, SDL dummy, zéro
  D3D12 et `bin/bin` absent.
- PROUVÉ : binaire installé
  `9cf04b15637ea6c7924ce01d388006bc85111b4675858cd37cb548d70b7e06e9` ; la
  sonde couvre désormais les destinations `0x1AB60000` et `0x1B9C0000`.
- PROCHAIN : runtime unique séparé avec `--mission-resolve-content`, puis
  décision sur la première divergence. PAL reste fermé.

Preuve : `reports/retail-us-resolve-content-dual-target-build-20260827.md`.

# Retail US 2026-08-27 — runtime sonde resolve double-target

- PROUVÉ : `RESULT.json` propre (`clean_shutdown=true`, `game_status=0`,
  `85/85`, 24 captures) avec le binaire `9cf04b15…e06e9`.
- PROUVÉ : la cinématique 77 montre la 3D ; les captures 79/82/85 montrent le
  monde noir et le HUD invité vert, sans diagnostic hôte.
- QUALIFIÉ : les deux destinations `0x1AB60000` et `0x1B9C0000` sont alimentées
  pendant la transition, mais `ac6-swap-texture`/`ac6-texture-load` présentent
  toujours la clé 1AB ; 1AB devient quasi vide plus tard.
- COUTURE À CORRIGER : le resolve compute utilise `Usage::kComputeWrite`, dont
  `GetUsageMasks` associe actuellement l'accès Vulkan read au lieu de write.
  Aucun override de destination ou de renderer n'est justifié.

Preuve : `reports/retail-us-resolve-content-dual-target-runtime-20260827.md`.

# Retail US 2026-08-27 — rebuild compute-write barrier

- PROUVÉ : rebuild/relink cgroup terminé avec statut 0, sans nouvelle
  génération C++ ; `generated/sources.cmake` réutilisé.
- PROUVÉ : validation statique `16/16`, Vulkan/ReXGlue, SDL dummy, zéro
  D3D12 et `bin/bin` absent.
- PROUVÉ : binaire installé
  `c159cbedb5b1f9f8cc661bf1f2ffbae5e7015035e329cd74a3911d293a1496e1`.
- CORRIGÉ : `Usage::kComputeWrite` utilise désormais
  `VK_ACCESS_SHADER_WRITE_BIT`, avec test source dédié 27/27.
- PROCHAIN : une session runtime séparée avec la route qualifiée ; gameplay US
  non validé et PAL fermé.

Preuve : `reports/retail-us-compute-write-barrier-build-20260827.md`.

# Retail US 2026-08-27 — runtime après correction compute-write

- PROUVÉ : runtime unique terminé après `85/85`, 24 captures et
  `xvfb_status=0`; `error` vide, zéro fatal/trap, journal jusqu'à
  `Execution complete`.
- QUALIFIÉ : `clean_shutdown=false`, `game_status=-9` correspond au teardown
  post-route ; il ne s'agit pas d'une trap invitée et les captures restent
  valides.
- RÉFUTÉ : la correction du masque Vulkan `kComputeWrite` n'a aucun effet
  visuel ; la cinématique `step-77` reste 3D et 79/82/85 restent HUD/radar sur
  monde noir, sans panneau hôte.
- QUALIFIÉ : les readbacks `0x1AB60000`/`0x1B9C0000` restent alimentés,
  `0x1B9C0000` est intermédiaire, et le dernier resolve/swap est `0x1AB60000`.
  Le propriétaire final est `0x8234F598` → `0x828C849C`; le gestionnaire
  monde observe toujours ses phases objet/caméra/radio.
- DÉCISION : fermer la piste swap-target et compute-barrier ; analyser
  statiquement la composition host/resolve-copy et l'état shader monde avant
  tout nouveau runtime. Gameplay US non validé, PAL fermé.

Preuve complète : reports/retail-us-compute-write-barrier-runtime-20260827.md.

# Retail US 2026-08-27 — runtime sonde ownership MSAA

- PROUVÉ : binaire `a0e9ad827f6196425786081cf532bbabfe1ba2d535153e8c23cffac00d5fbd64`
  relinké/validé avec la sonde read-only ; ctest 16/16 et suite Python 33/33.
- PROUVÉ : 56 lignes `[ac6-rt-transfer]` montrent le chemin ownership Vulkan
  actif (couleur base 0, `k1X`↔`k4X`, valeurs 0↔2), mais uniquement avant la
  campagne; aucun `k2X`→`k1X` du rendu monde.
- QUALIFIÉ : route interrompue à 55/85 après `campaign-intro`, sans
  `ac6-campaign-transition` ni `PRESENT`; fermeture contrôlée, `game_status=0`,
  `xvfb_status=0`, zéro fatal/trap. Pas de reçu gameplay.
- DÉCISION : résultat négatif/inconclusif pour le monde noir ; aucune
  modification de transfert. Le gate US et PAL restent fermés, prochaine
  étape statique sur composition et état shader monde.

Preuve complète : reports/retail-us-rt-transfer-probe-runtime-20260827.md.

# Retail US 2026-08-27 — retry host-target D5B4

- PROUVÉ : runtime `b6497489…45eaa`, route `85/85`, 24 captures, statut 0,
  arrêt propre, zéro fatal/trap et panneau hôte absent.
- PROUVÉ : 3424 échantillons D5B4 ; tous passent par le chemin Vulkan hôte
  (`render_path=0`) avec render pass `0x8D`, attachements `0x3`, masques
  couleur `0xF`, rasterisation et EDRAM actifs.
- NON RÉSOLU : `step-77-mission-cinematic.png` conserve la 3D, mais les
  captures 79/82/85 restent HUD/radar sur monde noir, `world_center_mean=0`.
- DÉCISION : qualifier statiquement composition/world-content avant toute
  nouvelle session lourde. PAL demeure fermé.

Preuve : `reports/retail-us-d5b4-host-target-runtime-retry-20260827.md`.

# Retail US 2026-08-26 — échec de handoff texture non causal

- ÉCHEC FERMÉ : `retail-us-swap-texture-runtime-20260826` est resté sur le
  prompt du premier hangar après l'appui A ; aucun crash ni fatal/trap.
- PROUVÉ : 41 échantillons swap `status=ok`, clé finale inchangée.
- CORRIGÉ PAR REPRISE : la route suivante franchit le hangar et atteint le
  vol ; l'échec est classé timing d'entrée, pas régression graphique.

Preuve : `reports/retail-us-swap-texture-runtime-20260826.md`.

# Retail US 2026-08-27 — save/load roundtrip séparé

- `run_save_experiment.py` crée un stockage isolé et conserve le conteneur
  `SAVE` (`save.dat`, 129112 octets) sans écriture dans la mémoire invitée.
- Le load-only r5 atteint les marqueurs `selector44=3`, `type28=6`,
  `type28=8` et `type28=10`, avec 18/18 opérations, quatre PNG, arrêt propre
  et zéro fatal/trap.
- `step-17-save-load-complete.png` affiche `Set Autosave to ON?`; le profil
  n'est toutefois pas encore un checkpoint Mission 01.

Preuve complète : reports/retail-us-save-roundtrip-20260827.md.

# Retail US 2026-08-27 — capture RenderDoc D5B4 fermée sans `.rdc`

- ÉCHEC FERMÉ : la tentative unique `ac6-retail-us-renderdoc-cinematic-d5b4-20260827`
  est restée avant `type28=30`; le worker vidéo a bouclé après les premières
  présentations et le superviseur a retourné `124`.
- PROUVÉ : aucun fatal/trap/SIGSEGV, aucune PNG et zéro `.rdc`; la session ne
  qualifie ni D5B4/F556 ni l'attachement/resolve.
- DÉCISION : ne pas répéter la capture identique. Fermer d'abord
  statiquement la frontière d'injection/attente RenderDoc; gameplay US et PAL
  restent fermés.

Preuve complète : reports/retail-us-renderdoc-cinematic-d5b4-20260827.md.

# Retail US 2026-08-27 — injection RenderDoc différée non supportée sous Linux

- ÉCHEC FERMÉ : la variante de lancement normal puis
  `renderdoccmd inject` a atteint `cinematic-view-1` (72/96) mais l'outil
  retourne `4`, avec le message d'injection dans un processus déjà lancé non
  supportée sous Linux.
- PROUVÉ : arrêt contrôlé, zéro fatal/trap et zéro `.rdc`; la ressource D5B4
  et le point-list F556 restent non qualifiés.
- DÉCISION : restaurer le wrapper de lancement RenderDoc et ne répéter aucun
  des deux mécanismes sans outil Linux différent ou hypothèse causale nouvelle.

Preuve complète : reports/retail-us-renderdoc-cinematic-d5b4-injected-20260827.md.

# Retail US 2026-08-27 — RenderDoc vsync désactivé sans `.rdc`

- `--opt-disallow-vsync` lève le blocage initial : route `96/96`, 27 PNG,
  arrêt propre, zéro fatal/trap.
- `renderdoc/` contient zéro `.rdc`; D5B4/F556 et le resolve ne sont pas
  observables dans cette session.

Preuve complète : reports/retail-us-renderdoc-cinematic-d5b4-vsync-20260827.md.

# Retail US 2026-08-27 — RenderDoc F12 sans `.rdc`

- La touche `F12` au point `cinematic-view-2` donne route `96/96`, 27 PNG,
  `clean_shutdown=true`, `game_status=0`, `xvfb_status=0` et zéro fatal/trap.
- Aucun `.rdc` n'est produit; la frontière RenderDoc Linux est fermée pour ce
  gate et aucune analyse D5B4/F556 ne peut être revendiquée.

Preuve complète : reports/retail-us-renderdoc-cinematic-d5b4-key-20260827.md.

# Retail US 2026-08-27 — build sonde post-process ordonnée

- Le fork qualifié contient déjà les corrections génériques candidates; la
  divergence est restreinte à la chaîne plein écran entre 1B9C et 1AB6.
- `ac6_log_postprocess_order` ne modifie aucun état et s'arrête après 30 frames
  `cinematic=0, world=1`; il corrèle draws, fetches, constantes, resolves et
  échantillons de contenu.
- Rebuild/validation reçus : binaire `9890d13f…f0f`, 16/16 tests natifs,
  Python 42/42, sorties lourdes avec marqueurs de statut 0.

Preuve complète : reports/retail-us-postprocess-order-probe-build-20260827.md.

# Retail US 2026-08-27 — frontière de route avant sonde

- `RESULT.json` : 2/85, erreur `log predicate not reached: type28=30`, sortie
  superviseur 2, processus invité terminé par le runner, aucun fatal/trap.
- La capture d'échec montre l'attract cinématique rendu; aucun marqueur
  `[ac6-postprocess-*]` n'a été produit, donc aucune preuve renderer nouvelle.
- Le routage statique est corrigé sans rebuild : la sonde autonome utilise les
  96 opérations/27 captures scellées; suite Python 42/42.

Preuve complète : reports/retail-us-postprocess-order-route-boundary-20260827.md.

# Retail US 2026-08-27 — chaîne post-process ordonnée sur route 96

- `artifacts/retail-us-postprocess-order-route96-runtime-20260827/RESULT.json` :
  route `96/96`, 27 PNG, binaire `9890d13f…84f0f`, erreur vide, zéro
  fatal/trap ; arrêt non propre isolé au teardown (`game_status=-9`).
- Le log borne la chaîne `17E5 -> F59F -> 9982/0869/08DE -> 0311 -> HUD/UI ->
  1AB6` sur 30 frames ; l'entrée `1B9C` et la sortie `1AB6` ne sont pas vides.
- Les captures 81/84/87/90/93 restent HUD/radar sur fond noir : aucun reçu
  gameplay.
- L'ancien fallback qualifié `3ac747eb…ab2a` rend avions/terrain/ciel/eau à
  l'étape 82 depuis la source `F59F tf0`; il borne la divergence en aval sans
  constituer un correctif.
- Le microcode désigne comme prochain observable minimal le facteur vertex
  `2EF:tf19 -> o0.z` et les coefficients pixel `F59F:c100.z/w`.

Preuve complète : reports/retail-us-postprocess-order-route96-runtime-20260827.md.

# Retail US 2026-08-27 — validation sonde exposition finale

- Les marqueurs build/validation/Python valent tous `0`; 16/16 tests natifs et
  42/42 tests Python passent.
- `static-validation.json` atteste ReXGlue/Vulkan, SDL dummy, zéro D3D12 et
  `bin_bin_absent=true`.
- `generated/sources.cmake` reste `c604796f…f72`, mtime 26 août : aucune
  génération C++ supplémentaire.
- Binaire qualifié : `687394ef…a63c` ; la sonde ne change aucun rendu.

Preuve complète : reports/retail-us-final-compose-exposure-probe-build-20260827.md.

# Retail US 2026-08-27 — exposition `2EF/F59F` qualifiée

- `RESULT.json` : diagnostic prêt, route `96/96`, 27 captures, arrêt propre,
  binaire `687394ef…a63c`, erreur vide et zéro fatal/trap.
- 31/31 draws : VS `c106=(0.25,0.5,0,0)`, PS `c100=(0,0,1,1)` ; aucune
  extinction par coefficient.
- 31/31 fetches vertex : `tf19=1C152000`, valide, 1×1, format 36
  (`k_32_FLOAT`), tiled, `k8in32`.
- La fonction d'exposition du microcode est positive pour toute entrée finie
  non négative ; le mot brut du resolve reste l'observable minimal.
- Inspection visuelle : capture 81, HUD vert sur monde noir, aucun gameplay.

Preuve complète : reports/retail-us-final-compose-exposure-probe-runtime-20260827.md.

# Retail US 2026-08-27 — validation readback exposition

- Build, validation et tests Python ont chacun un marqueur de statut `0`.
- 16/16 tests natifs et 42/42 tests Python passent.
- `static-validation.json` : binaire `43fc9068…a5fbf`, ReXGlue/Vulkan, SDL
  dummy, zéro D3D12, `bin_bin_absent=true`.
- `sources.cmake` reste `c604796f…f72` : aucun codegen.

Preuve complète : reports/retail-us-exposure-word-readback-build-20260827.md.

# Retail US 2026-08-27 — mot d'exposition qualifié

- L'essai initial ne franchit pas la fenêtre (`80/96`, 22 captures,
  superviseur 2) et ne produit aucun marqueur exploitable.
- La reprise strictement identique reçoit `96/96`, 27 captures,
  `clean_shutdown=true`, jeu/Xvfb/superviseur à 0 et zéro fatal/trap.
- Les 31 resolves `1C152000` décodent en floats finis positifs : min
  `0.03347363323`, max `1.148323894`, moyenne `0.5740333768`.
- Les 31 producteurs sont `EBCCC312988F6750/2662DA784858A2F4` ; avec les
  constantes qualifiées, `2EF/F59F` conserve un facteur strictement positif.
- Inspection visuelle : HUD/radar sur fond noir, aucun reçu gameplay.

Preuve complète : reports/retail-us-exposure-word-readback-runtime-20260827.md.

# Retail US 2026-08-27 — corrélation du slot readback préparée

- Le runner qualifié active `--readback_resolve=fast`; l'index lu peut différer
  de l'index écrit lors du resolve courant.
- Les métadonnées producteur/layout sont stockées par slot et consommées avec
  le même `read_index` que les octets Vulkan.
- 576 pixels tiled/endian sont échantillonnés par resolve `1B9C` exact ; quatre
  séries nonzero/somme/min/max permettront de distinguer couleur et noir.
- `git diff --check` passe ; 37/37 tests ciblés passent. Build/runtime non faits.

Preuve complète : reports/retail-us-1b9c-decoded-content-probe-static-20260827.md.

# Retail US 2026-08-27 — validation build sonde `1B9C`

- `build-exit-status.txt=0`, `167/167` ; `validate-exit-status.txt=0`.
- Binaire exact `f9cd73ffa92e25f011e4f5c1fa8828ac327b2aad66ed22389e7879dfeae25ce1`.
- `static-validation.json` : full ReXGlue Vulkan, SDL dummy, D3D12 `0`,
  `bin_bin_absent=true`; installation plate.
- `sources.cmake` SHA `c604796f…f72`, mtime 26 août ; codegen non exécuté.
- `python-tests-exit-status.txt=0`, 42/42 tests.
- Route 96 et sonde RGBA8 désormais autorisées pour le prochain runtime.

Preuve complète : reports/retail-us-1b9c-decoded-content-probe-build-20260827.md.

# Retail US 2026-08-27 — contenu `1B9C` non noir

- Le wrapper pré-créé échoue avant le jeu (`output exists`, superviseur 2) ;
  la reprise identique seule est comptée : route `96/96`, 27 captures,
  arrêt propre et zéro fatal/trap.
- 128 lignes décodées `1B9C` sur 30 frames ; les passes de composition gardent
  des RGB non nuls (exemple `2EF/F59F` : `576,576,576` non nuls).
- L'image finale visible reste noire malgré HUD/radar ; le contenu brut `1AB6`
  n'est pas encore décodé et ne peut pas être attribué causalement.

Preuve complète : reports/retail-us-1b9c-decoded-content-probe-runtime-20260827.md.

# Retail US 2026-08-27 — extension statique `1AB6`

- Métadonnées producteur/layout séparées pour les deux adresses et leurs slots
  `fast`, grille tiled/endian commune de 576 pixels.
- `git diff --check` et tests Python ciblés conservés ; build/runtime à venir.

Preuve complète : reports/retail-us-1ab6-decoded-content-probe-static-20260827.md.

# Retail US 2026-08-27 — validation build sonde décodée `1AB6`

- `build-exit-status.txt=0`, `167/167`; `validate-exit-status.txt=0`, 16/16
  tests natifs.
- Suite Python retail `python-tests-exit-status.txt=0`, `51/51` (`pytest -q
  tests`).
- `static-validation.json` : binaire `603f99da…7cbf1`, ReXGlue/Vulkan, SDL
  dummy, zéro D3D12, `bin_bin_absent=true`; installation plate.
- `generated/sources.cmake` reste `c604796f…f72`, sans génération C++ ; le
  binaire contient le marqueur `[ac6-postprocess-decoded]`.
- La route 96 `--mission-postprocess-order` est maintenant la seule mesure
  runtime autorisée pour décoder les deux cibles.

Preuve complète : reports/retail-us-1ab6-decoded-content-probe-build-20260827.md.

# Retail US 2026-08-27 — route `1AB6` non qualifiée (startup timing)

- `RESULT.json` : 22 captures, arrêt sur le prédicat
  `cinematic=0 world=1 hud=1 stable=30`, superviseur 2, Xvfb 0, teardown
  invité `-9`, zéro fatal/trap.
- Le binaire validé `603f99da…7cbf1` a lancé correctement; aucune ligne
  `[ac6-postprocess-decoded]` n'est présente, donc aucun octet `1AB6` n'est
  attribué causalement.
- `step-80-failure-observation.png` reste sur l'écran briefing; le retard
  d'un écran est une frontière de chargement guest, pas une preuve de
  divergence renderer.
- Une reprise identique est autorisée après ce run avorté avant l'observable.

Preuve complète : reports/retail-us-1ab6-decoded-content-probe-runtime-20260827.md.

# Retail US 2026-08-27 — gameplay Mission 01 validé, shader boundary explicitée

- PROUVÉ : le build/install Vulkan final (`d8b7b7b7…14fab8`) passe la
  validation statique, 16/16 tests natifs et 51/51 tests Python ; `bin/bin`
  est absent.
- PROUVÉ : la route qualifiée `mission01-qualified-96.steps` est exécutée
  96/96 (`771a77a8…b6043`), avec 27 captures, arrêt propre, zéro fatal/trap,
  monde centre non noir (`mean=0.332281`, `nonblack_fraction=1.0`) et cinq
  entrées de contrôle dépassant le seuil de différence.
- PROUVÉ : le reçu et l'audit `ac6.retail-gameplay-audit.v2` sont à `pass` ;
  le prédicat guest `cinematic=0 world=1 hud=1 stable=30` est atteint.
- CORRIGÉ : le handoff final qualifié présente la source de composition
  `0x1B9C0000` seulement lorsque le swap `0x1AB60000` est sparse, sous le couple
  `2EF9631F6325FA91/F59F21F4A1E7843E`.
- RÉSIDUEL EXPLICITE : ciel/eau écrêtés, avion/after-effects trop blancs et
  artefacts de cinématique pré-mission persistent. Les fetchs BC3/D5B4 et
  F556 ont image/vue/descripteur valides ; la texture absente est réfutée.
  Cette parité fragment/éclairage/blend/post-process reste ouverte et aucun
  clamp global n'est justifié.

Preuve complète : reports/retail-us-gameplay-final-compose-20260827.md.

# Retail US 2026-08-28 — run M01 autorisé, seed complet non concluant

- Le seed `/tmp/ac6-cache-seed-v2-complete` contient `cache/` et
  `cache-root/`, sans symlink ; preflight accepté.
- Résultat borné : route v2 `2/96`, `status=fail`, timeout exact
  `log predicate not reached: type28=30`, 0 capture, 0 fatal/trap,
  `clean_shutdown=false`, `game_status=-9`, `xvfb_status=0`.
- Le processus a bien sélectionné ReXGlue/Vulkan et présenté des frames ; la
  phase n'a jamais dépassé `cinematic=0 world=0 hud=1 stable=0`.
- Les deux XPSO comparés ont en-tête/version valides et zéro hash de
  description invalide ; différence observée : 193 descriptions contre 164.
- Aucun reçu gameplay v2, débrief, save/reload ou preuve missions 02–15 n'est
  promu. `release_ready=false` reste inchangé.

Preuve :
`artifacts/retail-us-mission01-gameplay-v2-authorized-20260828/output/RESULT.json` ;
analyse complète : `reports/retail-us-mission01-gameplay-candidate-20260828.md`.

## Gate M01 v2 — XPSO-164

Le seed `/tmp/ac6-cache-seed-v2-xpso164` ne remplace que
`4E4D07D1.fsi.vk.xpso` par la version 164 descriptions; XSH et GLCache restent
identiques au seed complet. Le reçu
`artifacts/retail-us-mission01-gameplay-v2-xpso164-20260828/output/RESULT.json`
atteste `96/96`, 27 captures, `cinematic=0 world=1 hud=1 stable=30`, centre
non noir, arrêt propre et zéro fatal/trap. Les cinq captures de contrôle ont
exactement 0 pixel différent. Conclusion: les 29 descriptions XPSO
supplémentaires ne causent pas le timeout startup et ne ferment pas le gate
contrôles.

## Reverse entrée — Linux

Le binaire installé est un ELF x86-64 PIE, non setuid; chaînes cvar et
marqueurs de phase présentes. Le code custom `ac6_kbm_input.cpp` utilise
`GetAsyncKeyState` sous `#if defined(_WIN32)` et est désactivé par défaut. Le
driver MnK stock ReXGlue expose W/S (pitch), A/D (roulis), Q/F (épaules), LMB
(accélération) et RMB (frein). La route v2 est alignée sur ces touches et
attend 15 s avant `Escape`, SHA courant
`44c7cac85dba7f7dad42453f7232a1e5c3f44798c9f74bc5e404626d27dcc8a9`.

Le contrôle borné avec `REX_AC6_KBM_ENABLED=true` journalise la désactivation
de `mnk_mode`, mais aucune injection Linux; il expire à l'étape 2. La voie
KBM custom est donc exclue du produit Linux. Le prochain signal discriminant
est le paquet MnK stock consommé pendant `FlightActive`, par statique ou une
trace unique bornée.

## Gate M01 v2 — settle stock post-cinématique (28 août 2026)

La route courante ajoute un settle de 15 s après l'appui `Space` avant
`Escape`, d'après la route historique dont les contrôles produisaient des
différences; hash `44c7cac85dba7f7dad42453f7232a1e5c3f44798c9f74bc5e404626d27dcc8a9`.
Les tests de forme passent. Le run dédié avec seed complet échoue au startup
(`executed_steps=2`, `type28=30` absent, `257,571 s`, zéro capture, zéro
fatal/trap):
`artifacts/retail-us-mission01-gameplay-v2-stock-settle15-20260828/output/RESULT.json`.

Le run distinct complet + route hash précédent a atteint la frontière
`96/96`, `cinematic=0 world=1 hud=1 stable=30`, monde non noir et 27 captures,
mais `control_changed_pixels` reste nul pour les cinq actions:
`artifacts/retail-us-mission01-gameplay-v2-stock-complete193-20260828/output/RESULT.json`.
La capture `artifacts/retail-us-mission01-gameplay-v2-stock-route-20260828/output/step-40-language.png`
montre le menu Campagne/New Game/Normal demandé par l'utilisateur. Le gate
M01 v2 n'est pas promu; débrief/save et missions 02–15 restent fermés.

# Retail US 2026-08-28 — limite trace GDB

- Une tentative unique a lancé le même binaire comme enfant GDB pour éviter
  `ptrace_scope`; le breakpoint `MnkInputDriver::GetState` était installé.
- ÉCHEC OUTIL : `SIGSEGV` guest à `rex_sub_821E4378` avant tout hit, route
  `1/96`, environ 10 s; reçu
  `artifacts/retail-us-mission01-mnk-gdb-start-20260828/output/RESULT.json`.
- DÉCISION : aucun état MnK n'est inféré; le mode GDB perturbateur est
  abandonné. La preuve active reste la chaîne statique US
  `GetState -> XAM -> 0x8234CEB8 -> 0x8234CE40/CC38`.

# Retail US 2026-08-28 — consommateur MnK stock qualifié statiquement

- IDENTITÉ : `ghidra-projects/ac6-us` / `default.xex`, XEX
  `6eefba42…67cbbbc`; aucune preuve PAL/démo.
- STATIQUE : `MnkInputDriver::GetState` remplit `buttons`, `LT/RT`,
  `LX/LY/RX/RY`; XAM appelle `InputSystem::GetState` et conserve ces huit
  champs. Le guest `0x8234CEB8` transmet `r31+68` à `0x82390CE0` (LR
  `0x8234CEE0`), puis `0x8234CE40` consomme les sticks et
  `0x8234CC38` les triggers.
- RUNTIME BORNÉ : le logger positif (`…stock-input-log-positive-route…`) fait
  96/96, arrêt propre, LR XAM et boutons; il n'enregistre pas les axes. Le
  record longstart n'a pas publié de fichier; l'attach GDB est refusé par
  `ptrace_scope`. Aucun de ces essais ne promeut le gameplay.
- DÉCISION : le premier consommateur n'est plus une hypothèse ouverte. La
  prochaine preuve doit seulement corréler paquet stock non nul, heartbeat de
  vol et image visuelle; pas d'injection Linux, guest write, A/B ou hack global.

Preuve : `reports/retail-us-mission01-gameplay-candidate-20260828.md`.

# Retail US 2026-08-28 — trace packet stock bornée non concluante

- Session unique read-only au wrapper XAM, binaire `c225f5e6…fc08923`, route
  historique `6ef77bef…14dac4b`, seed positif, `SDL_AUDIODRIVER=dummy`, cgroup
  `ac6-retail-us-m01-inputstate-log-20260828`.
- Reçu
  `artifacts/retail-us-mission01-stock-input-state-log-positive-route-20260828/output/RESULT.json`:
  `status=fail`, `2/96`, `249,431 s`, `type28=30` absent, capture cinématique
  unique, `clean_shutdown=false`, `game_status=-9`, zéro fatal/trap.
- Journal: deux sites XAM (`0x8234CFA4`, `0x8234CEE0`), zéro ligne
  `xinput user=... state`, zéro axe/trigger/heartbeat de vol; phases seulement
  `world=0` puis `hud=1 stable=0`.
- Conclusion : fenêtre fermée sans signal packet; pas de répétition identique,
  pas de patch renderer et aucune promotion gameplay.

# Retail US 2026-08-28 — instrumentation host réfutée, baseline propre échoue

- Le probe host-side a été exécuté une fois avec `68090b0494f42de96b4b8a5784216ce0c26df3bcd74741f5c74c15c5356919c7`:
  `2/96`, `253,028 s`, `PRESENT=1829`, aucune ligne packet host et capture
  noire 1-bit; `type28=30` absent, `clean_shutdown=false`, `game_status=-9`,
  zéro fatal/trap. Reçu :
  `artifacts/retail-us-mission01-stock-input-host-log-positive-route-20260828/output/RESULT.json`.
- La qualification statique explique cette absence : le stock
  `InputSystem::GetState` retourne `0x48F` sans device; le wrapper guest
  `0x82390CE0` écrit alors lui-même le paquet de 16 octets. Le probe host ne
  couvre donc pas le paquet synthétique.
- Le patch host a été retiré du working copy ignoré. Rebuild/validate propre :
  SHA installé `a279d55180226496b4d2b70194e13a71e122a678d87842944db7e7cc50c2c95c`,
  37 803 744 octets, `validate.py=0`, `bin/bin` absent.
- La baseline propre, même route/seed, reproduit la frontière :
  `2/96`, `250,685 s`, `PRESENT=1863`, phase `world=0/hud=1`, capture
  cinématique 375 704 octets, `type28=30` absent, arrêt forcé, zéro fatal/trap.
  Reçu :
  `artifacts/retail-us-mission01-baseline-clean-positive-route-20260828/output/RESULT.json`.
- La cause « instrumentation » est réfutée. Aucun nouveau runtime identique;
  poursuivre par statique sur `stock/cache/état -> type28`, puis seulement une
  frontière runtime réellement différente si une hypothèse causale subsiste.

# Retail US 2026-08-28 — cache chaud XPSO-193

- Test unique d'une hypothèse distincte : seed `stock-complete193` (193
  descriptions XPSO), baseline `a279d551…c95c`, route historique
  `6ef77bef…dac4b`, Vulkan, `SDL_AUDIODRIVER=dummy`, fenêtre 900 s,
  cgroup `ac6-retail-us-m01-warmcache193-a279-20260828`.
- Reçu :
  `artifacts/retail-us-mission01-warmcache193-a279-20260828/output/RESULT.json`.
  Progression `96/96`, `PRESENT=16046`, `type28=94`, phases
  `cinematic=0 world=1 hud=1 stable=30` puis `world=0/hud=1`; 20 captures.
  Le cache a été effectivement consommé (`Created 193 graphics pipelines`) et
  a ajouté un 194e record.
- Limites : monde 3D non noir mais très surexposé; captures de vol
  étapes 77/79/80/84/87/90/93/96 identiques; cinq contrôles à zéro pixel.
  Reçu `status=fail`, `clean_shutdown=false`, `game_status=-11`,
  `xvfb_status=0`, aucun fatal/trap. Le log contient la séquence de fermeture
  (`Window closing`, audio arrêté, `TerminateTitle`, `Execution complete`),
  donc le `-11` est post-teardown observable et non localisé au guest.
- Conclusion : cache chaud = précondition de progression startup, pas preuve
  de parité renderer/input ni d'arrêt propre. Pas de répétition identique;
  analyse statique requise sur focus/événements, durée de vie teardown et
  producteur postprocess.
# Retail US 2026-08-28 — parcours original + XAM stock

- Hypothèse bornée : le parcours `mission01-qualified-96.steps` pourrait
  laisser la simulation interactive; baseline `a279d551…c95c`, cache chaud
  `stock-complete193`, logger `--mission-stock-input-log`, cgroup
  `ac6-retail-us-m01-route-original-inputlog-20260828`, fenêtre 900 s.
- Reçu :
  `artifacts/retail-us-mission01-route-original-inputlog-20260828/output/RESULT.json`;
  `diagnostic-capture-ready`, `96/96`, arrêt propre, `11717 PRESENT`,
  `94 type28`, phase stable `cinematic=0 world=1 hud=1`, zéro fatal/trap.
- Au seam XAM `0x82390CE0`, LR `0x8234CFA4`/`0x8234CEE0`, les boutons
  `A=0x1000`, `START=0x0010`, `LB=0x0100` apparaissent. Le logger ne lit pas
  les axes/triggers; les images de vol varient mais ne prouvent pas une
  causalité d'entrée.
- Décision : la perte totale des événements GTK/X11 est réduite pour le chemin
  boutons/XAM, sans promotion du contrôle de vol ni du renderer. Ne pas
  répéter; statique restante = `FlightActive`, producteur postprocess et
  teardown `-11`.

# US full-native Linux — profil cible et reprise 2026-08-29

- Identités XDVDFS US qualifiées et bornées dans
  `analysis/oracle/ac6-recomp-ab90b-us/content-identity.json`; aucun octet
  retail n'est committé.
- `RetailTarget` sépare désormais PAL et NTSC-U/J dans l'importeur, les médias,
  frontend, campagne, monde, session et caches.
- Build Linux/Ninja, 4 tests CTest pertinents (2 fixtures ignorées), 28 tests
  Python (11 sous-tests) et boundary audit natif passent. L’audit de cache
  accepte désormais le profil `ntsc-uj` et conserve la compatibilité PAL.
- Le gate retail M01 reste `gameplay_pass=false` : la prochaine fenêtre runtime
  doit encore qualifier input `FlightActive`, monde/HUD et teardown en une seule
  exécution bornée.

# Retail US 2026-08-30 — scène/HUD et postprocess

- Rapport : `reports/retail-us-hud-layer-compose-20260830.md`.
- Run discriminant cache 193 :
  `artifacts/retail-us-hud-pre-gamma-warm193-runtime-20260830/`, route 96/96,
  monde `mean=0,996537`, HUD vert `0,0`, cinq deltas `180786`.
- Rollback validé :
  `artifacts/retail-us-hud-pre-gamma-rollback-build-20260830/`, build/validate
  statuts 0, 54 tests Python, installation plate.

# Retail US 2026-08-30 — frontière `1B9C → 0311`

- `1AB6` décodée contient le HUD clairsemé sur une scène déjà noire ; l’ordre
  guest est `0311`, HUD/UI, resolve final. Le HUD n’est pas écrasé.
- Run refresh propre, cache 193 et route originale 96/96 : `8418 prepared`,
  `2911 clean`, une clé 1280×720/pitch 1280/format 6. L’absence de reload est
  réfutée.
- Fetch qualifié : F59F/0311 partagent adresse, format, endian, swizzle, signe
  et mip ; seule différence utile, filtrage point contre linéaire.
- A/B unique `0311 → point` : 96/96, teardown propre, scène toujours noire.
  Override retiré ; rollback build/validate 0, 54 tests, binaire
  `15a7f5c894894d0214eaa4f462396613c744f781cf73d8548845a7c90ec2a291`.
- Prochaine preuve : texels de l’image host après upload `1B9C`, avant `0311`.
- Le fallback host combiné est absent de la source courante.
