# AC6 Demo — implementation status

Cycle 1784 qualifie le corridor du propriétaire titre `0x2E4035D0` : START
T3000 remplace ses handlers `0x82323968`/`0x82322A80`/`0x823239F0` par
`0x823239F0` seul à T3001. Deux probes sont byte-identiques et le handler
restant rejoint `0x820EB200` via `0x82325ED4`; le défaut est donc après ce
consommateur matriciel, sans frontend natif. Trace read-only opt-in
`AC6_DEMO_WATCH_MOVIE_FRAME_DISPATCH`; rapport
`reports/cycle-1784-demo-title-frame-dispatch.md`, capsule
`analysis/demo/ac6-demo-title-frame-dispatch-ab-v1.json`.

Cycle 1783 ferme la route visuelle oracle title→menu par deux cold boots Xenia
Edge natifs : un seul START sur `PRESS START` affiche `NORMAL/NOVICE/EXIT` en
moins de 750 ms. Une route séparée confirme chargement, cinématique et HUD
`MISSION START`. Le natif START→START et START→A reste sans frontend à 3020
ticks, et START→A à 4200 ticks ; aucun second bouton n'est donc requis par la
route réelle. Pixels Edge oracle-only, runtime `supported=false`. Rapport
`reports/cycle-1783-demo-xenia-edge-visual-route.md`, capsules
`analysis/oracle/ac6-demo-xenia-edge-visual-route-v1/manifest.json` et
`analysis/demo/ac6-demo-title-followup-inputs-v1.json`.

Cycle 1753 joint chaque kick XMA neutral à son `MmGetPhysicalAddress`
immédiat : les six contextes `0x2E800000..0x2E800140` et mots wire
`0x01000000..0x20000000` passent la garde opt-in, puis neutral atteint
`max_ticks=5400` sans frontend. Cette garde reste sans effet XMA/audio et
désactivée par défaut. Rapport `reports/cycle-1753-demo-xma-context-guard.md`,
capsule `analysis/demo/ac6-demo-xma-context-guard-1753-v1.json`.

Cycle 1752 qualifie en neutral l'état body-side de `0x822F8848`: 5153 appels
sur l'objet `0x82934280`/vtable `0x8202A488` slot 4, 51 530 stores bornés sur
10 adresses, valeurs limitées à zéro et aux pointeurs `0x829342A0`/
`0x82934500`. Aucun writer EDRAM, pixel, audio ou mission n'est déduit.
Rapport `reports/cycle-1752-demo-neutral-body-state.md`, capsule
`analysis/demo/ac6-demo-neutral-body-state-1752-v1.json`.

Cycle 1751 traverse les six tuples XMA neutral observés (bits `1/2/4` au tick
1048 puis `8/16/32` au tick 5052) sous garde expérimentale exacte. À
`max_ticks=5400`, 5263 PRESENT sont produits et les 23 threads sont bloqués
sur l'attente connue `0x822E559C -> 0x822F8848`; aucun frontend, pixel non
noir, audio ou mission n'est promu. Rapport `reports/cycle-1751-demo-neutral-xma-six-kicks.md`,
capsule `analysis/demo/ac6-demo-neutral-xma-six-kicks-1751-v1.json`.

Cycle 1750 ferme le quatrième kick XMA neutral observé: `0x7FEA1A80` reçoit
`0x08000000` (bit logique `0x8`) au tick 5052/thread 21 pour le contexte
`0x2E8000C0`, uniquement sous la garde expérimentale exacte. Le replay frais
accepte les quatre mots `1/2/4/8`, puis piège avant effet sur le cinquième
`0x10000000`, contexte `0x2E800100`, même PC/LR. Aucun frontend, pixel non
noir, audio ou mission n'est promu. Rapport `reports/cycle-1750-demo-neutral-xma-kick-bit3.md`,
capsule `analysis/demo/ac6-demo-neutral-xma-kick-1750-v1.json`.

Cycle 1749 ferme le thunk neutral `0x8236E550` sur huit bytes PAL exacts et
une tail branch vers `0x8236E698`. Codegen/atlas passent à 12 876 fonctions,
154 records et atlas A/B `b9728fde…40e3c`. Neutral atteint ensuite au tick
5052 le quatrième store XMA wire `0x08000000` vers `0x7FEA1A80`, contexte
`0x2E8000C0`; son effet reste fail-closed. CTest passe OFF 18/18 et ON 17/17.
Voir `reports/cycle-1749-demo-neutral-boundary-8236e550.md`.

Cycle 1748 qualifie et traverse les trois appels neutral atteints de
`XMAReleaseContext` ordinal 550 aux pointeurs `0x2E800000/+40/+80`. Chaque
contexte actif de 64 octets est remis à zéro puis invalidé; toutes les autres
formes restent fail-closed. La prochaine frontière est
`0x8236E588 -> 0x8236E550` à tick 5049/thread 21. CTest passe OFF 18/18 et ON
17/17. Voir `reports/cycle-1748-demo-xma-release-context.md`.

Cycle 1747 ferme le thunk d'ajustement neutral `0x82351198` sur huit bytes PAL
exacts et une tail branch vers `0x82351368`. Codegen/atlas passent à 12 875
fonctions, 153 records configurés et couverture `.text` complète; deux atlas
frais ont le SHA `0947f855…c491`. Neutral atteint ensuite
`xboxkrnl.exe:XMAReleaseContext` ordinal 550 à tick 5049/thread 21 depuis
`LR=0x82356820`. CTest passe OFF 18/18 et ON 17/17. Voir
`reports/cycle-1747-demo-neutral-boundary-82351198.md`.

Cycle 1746 ferme uniquement le tuple neutral atteint de
`xam.xex:XMsgStartIORequest` ordinal 503 : LR `0x821A55A0`, app `0xFB`,
message `0xB0006`, overlapped nul, buffer PAL exact de 24 octets. Le handler
est read-only et fail-closed. Deux probes neutral atteignent 5000 ticks avec
rapport et trace byte-identiques, 4863 présentations et aucun frontend.
CTest passe OFF 18/18 et ON 17/17. Voir
`reports/cycle-1746-demo-xgi-user-context.md`.

Cycle 1745 ferme le thunk neutral `0x820EB490`; neutral atteint
`max_ticks=4252`, puis piège sur `xam.xex:XMsgStartIORequest` ordinal 503 au
tick 4254. Codegen/atlas : 12 874 fonctions, couverture `.text` complète,
CTest OFF 18/18 et ON 17/17. Le buffer XGI, START et les pixels non noirs
restent non qualifiés. Voir
`reports/cycle-1745-demo-neutral-boundary-820eb490.md`.

Cycle 1744 ferme le thunk neutral `0x820D32B0`; neutral atteint désormais
`max_ticks=3000` sans frontière non résolue, avec 2 863 présentations mais
toujours aucun frontend. Codegen/atlas : 12 873 fonctions, couverture `.text`
complète, CTest OFF 18/18 et ON 17/17. START et les pixels non noirs restent
non qualifiés. Voir `reports/cycle-1744-demo-neutral-boundary-820d32b0.md`.

Cycle 1743 ferme l'entrée neutral `0x823235E8`; neutral atteint ensuite
`max_ticks=2500` puis le prochain trap `0x820DC374 -> 0x820D32B0` à tick 2511.
Codegen/atlas : 12 872 fonctions, couverture `.text` complète, CTest OFF 18/18
et ON 17/17. START et les pixels non noirs restent non qualifiés. Voir
`reports/cycle-1743-demo-neutral-boundary-823235e8.md`.

Cycle 1742 ferme l'entrée neutral `0x822CD118` sur une fonction feuille PAL de
8 bytes. Neutral atteint `max_ticks=2127`, puis une extension inchangée avance
jusqu'au prochain trap `0x82323C4C -> 0x823235E8` à tick 2453. Le codegen et
l'atlas reproductible comptent désormais 12 871 fonctions, couverture `.text`
complète, CTest OFF 18/18 et ON 17/17. START et les pixels non noirs restent
non qualifiés. Voir `reports/cycle-1742-demo-neutral-boundary-822cd118.md`.

Cycle 1741 ferme trois nouvelles frontières indirectes neutral à tick 2126
(`0x82277768`, `0x822CCCB8`, `0x822CC3B8`) sur cibles dynamiques et bytes PAL
exacts. Le codegen strict compte désormais 12 870 fonctions sans diagnostic et
l'atlas exhaustif reproductible SHA-256 `37480d8c…604f` conserve une couverture
`.text` complète. Le prochain trap est `0x8223FF70 -> 0x822CD118`; START reste
gelé et aucun pixel non noir n'est qualifié. Voir
`reports/cycle-1741-demo-neutral-indirect-boundaries.md`.

Date: 2026-08-16

## Identity and scope

- Target: `Default.xex`, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.
- `.pdata`: 66,616 bytes, SHA-256
  `82c68b78f3256dd0c2bdd0df40e97daf6f3cf6dd1e162d5ddee4a47d1d14e50b`.
- Qualified XEX imports: 238.
- Ghidra maintenance is pinned exclusively to `12.1.2` / `PowerPC:BE:64:Xenon`
  in `ghidra-projects/ace-combat-6-demo`; historical projects are not inputs.
- Source tree contains no XEX, SDK, Ghidra database, ReXGlue checkout or
  generated C++.
- This demo is the exclusive active product target. PAL retail
  `acc302c1…bcde`, its modified worktree and its evidence remain frozen and
  separate; no retail trace or identity is accepted by the demo runtime.

## Implemented and verified

- Standalone C++20 runtime with a contiguous reserved 4 GiB guest address
  space (pages committed on demand), Xenon big-endian access,
  64-bit GPR/32-bit guest-pointer boundary, deterministic token scheduler,
  trace format `AC6RTPLY` v4 and strict replay-integrity checks.
- Guest reservation generations and barriers, timebase hook, VMX
  `vpkswss`/`vcmpbfp.` semantics, and explicit hooks for `vrefp` and
  `vrsqrtefp` without replacing them with host exact arithmetic.
- Atomic nine-file user-store mount, import registry traps, public XAM
  persistence boundary, strict D3D9LTCG/Vulkan and XMA/media boundaries, and
  the `import`, `play`, and `replay` command contract.
- Generic XenonRecomp context/ABI seams are retained in a build-only adapter.
  Game-specific generated C++ remains a build product and is never copied
  into the native runtime.
- The `probe` command emits `ac6-demo-frontier-report/v1`; its deterministic
  hook records every guest-indirect target and import with thread, LR, tick and
  register snapshot. `replay` now parses the recorded input seam, reruns the
  same guest ticks and requires a byte-identical `AC6RTPLY-v4` result. Static
  controller/input callers are qualified separately against the canonical Xenon
  Ghidra project. The controlled `START` replay passes this check for 763
  events. Historical reports used `frontend` as a presentation-count marker;
  the probe now keeps it false until a title/menu state and its decoded Xenos
  output are jointly qualified. Presentation notifications remain a separate
  instrumentation count and are not proof that a menu was rendered. Durable
  evidence is in `analysis/demo/ac6-demo-replay-evidence.json` and
  `analysis/demo/ac6-demo-controller-state-capsule.json`.

Validation completed:

- CMake/Ninja product sources build with the configured warning set; the
  build-only XenonRecomp/SIMDe adapter still emits 10 external-header warnings.
- CTest: codegen ON 17/17 and codegen OFF 18/18 passed; the Python corpus,
  complexity budget, source audit and derived-status check are included.
- Import, trace parsing, relative trace paths, tampered-trace rejection, and
  missing-store rejection passed. The generated-guest `play --ticks 1` and
  `play --ticks 3600` smokes return normally after reaching the qualified
  kernel wait boundary; no synthetic mission result is emitted.
- Pinned XenonRecomp checkout and both source patches pass `git apply --check`;
  patched XenonRecomp builds with Clang.

## Current dynamic/render checkpoint — cycle 1740

Cycle 1740 valorise en lecture seule l’archive Xenia Edge
`xenia-edge-ac6-gpu-run-20260816-113443.tar.zst` (SHA
`ef420b2e…6d7e8`, Edge commit `15200f64…7f92d`). Elle contient 23 couples
XTR/PNG. Les trois traces gameplay `5152`, `5505` et `5849` ont chacune 18/18
IB start/end, 36 185/36 644/36 248 paquets, un snapshot EDRAM brut de 10 MiB
non nul (respectivement 5 860 457, 5 126 138 et 5 286 025 octets non nuls) et
un événement swap. Les XTR et leur protocole sont donc présents, contrairement
au run Edge Release du cycle 1735. Les PNG sont des sorties de l’oracle Edge,
pas des screencaps guest-owned natives.

La jointure reste négative : aucune des trois traces ne lit une plage aux bases
PAL `0x1274A000`/`0x127CA0C0` et aucun IB fermé ne porte les hashes
`d121c8d8…358d6`/`ef7ab6e4…d2b0`. Les snapshots EDRAM et les paquets sont donc
`xenia-generic`/`demo-observed` seulement ; aucun writer EDRAM PAL, resolve,
pixel, audio ou mission n’est promu. Trois correspondances de bytes microcode
(`099625…`, `934488…`, `491360…`) sont conservées comme cross-match offline;
`586168…` reste absent. La capsule est
`analysis/oracle/ac6-demo-xenia-edge-gpu-run-v1.json` et le rapport
`reports/cycle-1740-demo-xenia-edge-gpu-run.md`.

Le prochain test reste l’instrumentation native du premier writer EDRAM non nul
du draw rectangle avant `RB_COPY`, avec PC/LR guest, thread, tick et hash de
plage. L’archive Edge ne doit fournir ni EDRAM, ni shader, ni pixels à `play` ou
`replay`.

Cycle 1739 sonde en lecture seule les 24 `PointList` bootstrap du neutral

Cycle 1739 sonde en lecture seule les 24 `PointList` bootstrap du neutral
frais. Ils surviennent au tick 0/thread 1 avec VS
`099625f3…e4e3` et PS `4913603d…8e25`; `RB_SURFACE_INFO`, `RB_COLOR_MASK`,
`RB_DEPTH_INFO`, `RB_DEPTH_CONTROL` et les deux premiers fetch registers sont
zéro dans les 24 lignes. Aucun payload vertex n'est capturé. Ils ne sont donc
pas un writer EDRAM qualifiable, sans exclure un effet interne Xenos non
observé. La capsule est `analysis/demo/ac6-demo-point-draw-source-probe-v1.json`
et le rapport `reports/cycle-1739-demo-point-draw-source-probe.md`.

Cycle 1738 capture en lecture seule les registres bruts du draw normal et du
resolve sur des stores neutral et START frais, avec
`AC6_DEMO_WATCH_RESOLVE=1`. Les stderr sont byte-identiques
(`3df64ed2…6c884`) et le renderer stdout reste identique
(`933597e9…e2c0`). `RB_SURFACE_INFO` vaut `0x0A020280` au draw normal
(pitch 640, MSAA 4× selon les bitfields ReXGlue) puis `0x14000500` au
resolve (pitch 1280, MSAA 1×). La copie observée cible `0x1374A000`, avec
pitch/hauteur `1280×720`, format raw 6, endian 0 et swap 1. Aucun contenu
EDRAM non nul ni pixel frontend n'est qualifié; les readbacks restent noirs.
La capsule est `analysis/demo/ac6-demo-pal-rt-state-probe-v1.json` et le
rapport `reports/cycle-1738-demo-pal-rt-state-probe.md`.

L'audit Edge associé confirme que le wrapper conserve statiquement des
racines `storage/content/cache`, mais la racine par défaut est absente et
aucun second lancement réel ne prouve la réutilisation du profil. Le résultat
reste oracle `xenia-generic`, sans preuve PAL. Capsule
`analysis/oracle/ac6-xenia-edge-profile-reuse-audit-v1.json`, rapport
`reports/cycle-1738-xenia-edge-profile-reuse-audit.md`.

Cycle 1737 teste derrière garde `AC6_DEMO_EXPERIMENTAL_PAL_VIEWPORT=1` le
viewport observé `{x=0,y=360,width=640,height=-360}`. Le readback normal
devient `1dea584c…6154b5` (750 pixels noirs, 229600 pixels du clear magenta),
avant le trap fail-closed attendu. L’hypothèse « le viewport hôte maximal est
la seule cause du noir » est rejetée; les dimensions/états exacts de
`RB_SURFACE_INFO` et le contenu EDRAM restent inconnus. Le chemin par défaut
n’est pas modifié et aucune screencap n’est promue. La capsule est
`analysis/demo/ac6-demo-pal-viewport-probe-v1.json` et le rapport
`reports/cycle-1737-demo-pal-viewport-probe.md`.

Cycle 1736 ajoute une sonde read-only neutral, store neuf, Vulkan, 253 ticks,
avec `AC6_DEMO_WATCH_RESOLVE=1`. Elle joint les bytes des deux fetches de
rectangles PAL : `0x127CA03C` (21 dwords, SHA `cf61dc45…dac7db1`) et
`0x127CA090` (6 dwords, SHA `1187ed99…10fa12`). Le frontier reste 5 shader
loads/26 draws/1 PRESENT; les readbacks 640×360 et 1280×720 restent noirs.
Cette sonde ne qualifie aucun contenu EDRAM non nul, pixel ou screencap. Le
prochain test doit observer le résultat du draw normal dans le backend
Xenos/EDRAM avant `RB_COPY`; aucune source synthétique n'est admise. La
capsule est `analysis/demo/ac6-demo-renderer-input-trace-v1.json` et le
rapport `reports/cycle-1736-demo-renderer-input-trace.md`.

The authoritative handoff is cycle 1737. Fresh neutral/START stores reach the
same XMA slot load and import frontier at tick 1048: 
`xboxkrnl.exe:XMACreateContext`, ordinal 548, thread 21, LR `0x82357298`.
The guest slot is `0x17360050`; both routes store `0xFEFEFEFE` at tick 2 from
`0x823273E0`, then load zero at `0x82357240` immediately before the import.
Both routes reach 911 PRESENT and identical `VD_SWAP`; no frontend, mission or
terminal state is qualified. The receipts are in
`reports/cycle-1702-demo-xma-create-context-boundary.md`,
`reports/cycle-1703-demo-xma-table-static-join.md`,
`reports/cycle-1704-demo-xma-output-slot-rr-writer.md`,
`reports/cycle-1705-demo-xma-output-slot-fill-writer.md`,
`reports/cycle-1706-demo-xma-slot-load-consumer.md`,
`reports/cycle-1708-demo-xma-zero-fill-entry.md`,
`reports/cycle-1709-demo-xma-table-runtime-join.md`,
`reports/cycle-1710-demo-xma-table-entries.md`,
`reports/cycle-1711-demo-xma-entry-samples.md`,
`analysis/demo/ac6-demo-xma-create-context-v1.json` and
`analysis/demo/ac6-demo-xma-table-static-join-v1.json` and
`analysis/demo/ac6-demo-xma-output-slot-rr-writer-v1.json` and
`analysis/demo/ac6-demo-xma-output-slot-fill-writer-v1.json` and
`analysis/demo/ac6-demo-xma-slot-load-consumer-v1.json` and
`analysis/demo/ac6-demo-xma-zero-fill-entry-v1.json` and
`analysis/demo/ac6-demo-xma-table-runtime-join-v1.json` and
`analysis/demo/ac6-demo-xma-table-entries-v1.json` and
`analysis/demo/ac6-demo-xma-entry-samples-v1.json`.

`XMACreateContext` remains fail-closed in the production path. A separate,
environment-gated experiment joins only the exact PAL callsite/registers to
the generic 64-byte/320-context output-pointer primitive, then lazily exposes
the PAL-observed XMA context-array read at `0x7FEA1800` with the exact
`lwbrx` wire-endian inversion. Neutral and START both allocate the
experimental table at `0x2E800000`, write the first context
`0x2E800000` to `0x17360050`, call `MmGetPhysicalAddress`, and advance the
first unknown store from `0x7FEA31E0` (`0x0C78`) to `0x7FEA1A80` at tick
1048/thread 21. The runtime diagnostic's `LR=0x823572AC` is the return PC
after `MmGetPhysicalAddress`; the qualified PAL store is `0x823572D8`
(`7D60552C`, `stwbrx r11,0,r10`), followed by `eieio` at `0x823572DC`. The
new address is observed only as a trap-before-effect and has no promoted
register or XMA semantics. The experiment is not used by `play` or `replay`,
and it does not decode audio or write pixels. The durable receipts are
`reports/cycle-1714-demo-xma-context-array-frontier.md`,
`analysis/demo/ac6-demo-xma-context-array-frontier-v1.json`,
`reports/cycle-1713-demo-xma-mmio-store-pc-correction.md`,
`analysis/demo/ac6-demo-xma-mmio-store-pc-correction-v1.json`,
`reports/cycle-1712-demo-xma-create-experimental-mmio-frontier.md` and
`analysis/demo/ac6-demo-xma-experimental-mmio-frontier-v1.json`.

The opt-in A/B was then recorded directly and under the local pinned `rr`
(`7352eb807ed75e3b51be85fa6a27f121235dbfb0`). For both neutral and START,
RTPLY, frontier report and stderr are byte-identical direct-versus-`rr`:
tick 1048, thread 21, 911 PRESENT, LR `0x823572AC`, trap address
`0x7FEA1A80`, trap before effect. This extends the rr fidelity claim only to
this opt-in experiment; it does not qualify the register, audio, renderer or
START under the older neutral-only gate. The durable receipt is
`reports/cycle-1715-demo-xma-rr-ab.md` and its capsule is
`analysis/demo/ac6-demo-xma-rr-ab-v1.json`.

The PAL arithmetic audit now joins the two observed XMA addresses without
assigning register semantics: `FUN_82356510` reads `0x7FEA1800` into global
`0x829DA52C`, while `Function_82357240` forms `I=((P-G)>>6)&0xFFFF` and then
`A=0x7FEA1A80+((I>>5)<<2)`. With the experimental context `P=0x2EEEC000`,
`G=0` gives the observed `0x7FEA31E0`; `G=P` would give `0x7FEA1A80`.
This is an address calculation proof only; the MMIO effect remains unknown.
The receipt is `reports/cycle-1716-demo-xma-address-join.md` and the capsule
is `analysis/demo/ac6-demo-xma-address-join-v1.json`.

The current atomic runtime builds pass CTest `18/18` (OFF) and `17/17` (ON), including the
complexity, source-audit and status checks. The complexity baseline is 1,443
lines after the disabled-by-default slot table/entry/load/store evidence hook.
Product support remains `false`.

The function-entry, table and scalar load/store hook is read-only and opt-in. It
confirms `0x821A4B70` with `r3=0x17360000`, `r5=0x6180`, tick 1048/thread 21,
then `0x82357240` with the same bounds and table `count=3`,
`flags=0x00030000`, `entries=0x17360010` before the slot load. Neutral/START
stderr is byte identical; RTPLY differs only in input-event counters. The
three entries are bounded and identical; 64-byte samples at all six pointers
are zero with SHA `f5a5fd42…fb4b`; their raw pointers remain unnamed.
The writer before FE preparation, XMA register `0x0C78`, XMA packets and audio
consumer remain unknown; ordinal 548 still traps by default. No audio decode,
readback or screencap is promoted.

Cycle 1717 adds a read-only opt-in address probe. Neutral and START both observe
`0x7FEA1800 -> 0x2E800000`, global `0x829DA52C=0x2E800000`,
`MmGetPhysicalAddress(P=0x2E800000)`, then `A=0x7FEA1A80` and wire value
`0x01000000` at tick 1048/thread 21. The trap remains before MMIO effect;
the default route still traps ordinal 548 with its baseline RTPLY. No register
name, packet, PCM, audio, readback or screencap is promoted. The durable
receipt is `reports/cycle-1717-demo-xma-address-trace.md` and its capsule is
`analysis/demo/ac6-demo-xma-address-trace-v1.json`.

The later-store static join is closed by cycle 1718: the PAL bytes confirm the
`0x82357310` path and its `0x7FEA1A40 + ((n >> 5) << 2)` address family, the
fixed `0x7FEA1804` stores, the `0x7FEA1818` read, and the
`0x7FEA1940 + ((n >> 5) << 2)` family. The older shorthand `0x7FEA1AC0` is
not a fixed base. The durable static receipt is
`reports/cycle-1718-demo-xma-static-aperture.md` and its capsule is
`analysis/demo/ac6-demo-xma-static-aperture-v1.json`.

Cycle 1719 strengthens only the Vulkan resolve test oracle. The test now emits
and checks both tiled and untiled SHA-256 values for black and asymmetric
nonzero patterns, verifies all canaries/gaps against the CPU tiling oracle, and
uses two fresh Vulkan harness instances. The CTest repeat wrapper still runs
two fresh processes and compares complete stdout. The exact outputs and
SPIR-V validation are recorded in
`reports/cycle-1719-demo-vulkan-resolve-fresh-digests.md` and
`analysis/demo/ac6-demo-vulkan-resolve-fresh-v1.json`; this does not promote
the result to guest-owned pixels or change `play`.

Cycle 1720 adds only an opt-in, read-only trace of those late XMA aperture
families. Fresh neutral and START probes with the final codegen-ON binary both
reach the same `store32 0x7FEA1A80 <- 0x01000000` at tick 1048/thread 21 and
trap before effect; no `0x7FEA1804`/`0x7FEA1818` access and no later-family
writer is observed. The direct RTPLY, report and stderr hashes are recorded in
`reports/cycle-1720-demo-xma-late-aperture.md` and
`analysis/demo/ac6-demo-xma-late-aperture-v1.json`. The hook is disabled by
default, adds no mapping, and leaves ordinal 548 fail-closed. The next useful
checkpoint requires an independent proof of the store effect before any packet
or audio implementation.

Cycle 1721 inventories the two static PAL voice packs without following their
runtime use. `voicepack_eng.bin` and `voicepack_jpn.bin` each contain 738
contiguous bounded RIFF/WAVE records; vgmstream and FFprobe open all 738
segments as XMA1/48 kHz/mono. No PCM/WAV is emitted, and no packet, timestamp,
volume, consumer or language route is promoted. The durable inventory is in
`reports/cycle-1721-demo-xma-static-voicepacks.md` and
`analysis/demo/ac6-demo-xma-static-voicepacks-v1.json`. The XMA effect proof
and guest-owned renderer remain the next runtime gates.

Cycle 1722 records a read-only cross-match against the pinned generic Xenia
XMA indexed-context helper and the ReXGlue register table. Xenia's
`StoreXmaContextIndexedRegister` documents generic bases `0x1A80`, `0x1940`
and `0x1A40`; the PAL bytes independently form `0x7FEA1A80` and the dynamic
probe traps before effect. The numeric suffix and formula are not a PAL
register identification, so no generic mapping is installed and ordinal 548
remains fail-closed. The durable cross-match is in
`reports/cycle-1722-demo-xma-xenia-generic-crossmatch.md` and
`analysis/demo/ac6-demo-xma-xenia-generic-crossmatch-v1.json`.

Cycle 1723 closes the reached PM4 packet structurally. Fresh codegen-ON
neutral and START probes consume the exact 11-dword and 3,029-dword IBs,
admit only the captured values at `0x0A02..0x0A05` as transactional opaque
storage, and reach five shader loads, 26 draws and one `XE_SWAP`. The same
frontbuffer metadata is observed in both routes. This does not name the four
registers or qualify EDRAM contents/pixels; unseen values still trap before
commit. The durable evidence is in
`reports/cycle-1723-demo-pm4-opaque-registers.md`,
`analysis/demo/ac6-demo-pm4-opaque-registers-v2.json`,
`analysis/demo/ac6-demo-neutral-pm4-inventory-v2-opaque-registers.json` and
`analysis/demo/ac6-demo-start-pm4-inventory-v2-opaque-registers.json`.

Cycle 1724 joins the reached Vulkan resolve to guest memory. Fresh neutral and
START probes under Xvfb both write and reread the exact `0x1374A000` tiled
1280×720/format-6 destination; the untiled guest digest equals the resolved
linear digest `0c660f2b…a4913a5f` and the normal 640×360 draw remains
`0b150fd3…ec58366`. This is a guest-owned black readback, not a frontend or
screencap. The durable receipt is
`reports/cycle-1724-demo-vulkan-guest-readback.md` with capsule
`analysis/demo/ac6-demo-vulkan-guest-readback-v1.json`; the non-black pixel
lane and XMA effect remain open.

Cycle 1727 extends the XMA probe only under explicit opt-in. Neutral and START
fresh stores traverse the three PAL output slots
`0x17360050/0x173600B0/0x17360110` and accept the ordered wire values
`0x01000000/0x02000000/0x04000000` at `0x7FEA1A80`, with the same
`PC=0x82357240`, `LR=0x823572AC`, tick 1048 and thread 21. Their stderr is
byte-identical; both stop at `max_ticks=1100` with 23 blocked threads, one
presentation, and no frontend/mission/terminal milestone. The final indirect
frontier is `LR=0x822E559C -> 0x822F8848`, wait key `0xE000004C`; its
semantics and the ordinal-548 consumer remain unknown. The three-slot hook is
not enabled by `play` or `replay`, and no audio, non-black pixel or screencap
is promoted. Durable evidence is in
`reports/cycle-1727-demo-xma-three-slot-optin.md` and
`analysis/demo/ac6-demo-xma-three-slot-optin-v1.json`.

Cycle 1731 joins the PAL `KeSetAffinityThread` body to the dynamic affinity
trace. The canonical `.pdata` function `0x821A5390..0x821A543F` contains the
`r4 < 6` / `1 << r4` path at `0x821A53DC`; 19 calls per route observe exactly
the masks `1,2,4,8,16,32`, with neutral/START stderr byte-identical. This is
`demo-qualified` for the PAL call path, not a six-vCPU scheduler proof. The
production scheduler remains sequential `ucontext` fibers on one host and
terminates at 23 blocked/0 runnable/0 finished at tick 1100. A bounded
stateful-affinity experiment crashed in host `record_import_edge/std::map`
before guest-visible output, was removed, and is not evidence. Binary SHA
`83bda520…f34e23`; CTest OFF 18/18 and ON 17/17 pass. Durable evidence is
`reports/cycle-1731-demo-affinity-static-dynamic-join.md` with capsule
`analysis/demo/ac6-demo-affinity-static-dynamic-join-v1.json` (capsule SHA
`868a080f…95ff`). The first nonzero EDRAM writer and all pixels remain unknown.

Cycle 1732 adds a bounded read-only point-draw trace on fresh codegen-ON
neutral/START probes (`max_ticks=253`). Each route emits 24 bootstrap
`PointList` rows at tick 0 on guest thread 1. The raw VS/PS hashes are
`099625f3…e4e3` and `4913603d…8e25`; all rows are identical with
`0x2104=0` (raw color-mask field), `0x2200=0`, `0x2201=0`, and zero fetch
length. The stderr SHA is identical between routes and the graphics/scheduler
report subtrees match; the only report difference is the expected START input
control-flow. This confirms that the bootstrap point-draw branch contributes
no observed color/depth write, but it does not identify a nonzero EDRAM writer
or a pixel. The hook is disabled by default and never a renderer fallback.
Receipt: `reports/cycle-1732-demo-point-draw-trace.md` and
`analysis/demo/ac6-demo-point-draw-trace-v1.json` (capsule SHA
`f4341a16…6c04`).

Cycle 1733 adds a read-only static PAL ABI join for the XMA frontier. Function
`0x82357240` walks a table with stride 96, calls `XMACreateContext` ordinal 548
with `r3=entry+64`, calls `MmGetPhysicalAddress`, writes `entry+80`, then
issues the unqualified `stwbrx` at `0x823572D8` followed by `eieio` at
`0x823572DC`. The qualified PAL import map contains only
`XMACreateContext`/548 and `XMAReleaseContext`/550 among XMA imports; no direct
`XMAInitializeContext` or `XMAEnableContext` call is present. This is structural
PAL evidence, not a register or XMA-effect proof. The production ordinal-548
trap is unchanged. Receipt: `reports/cycle-1733-demo-xma-pal-static-abi-join.md`
and `analysis/demo/ac6-demo-xma-pal-static-abi-join-v1.json` (report SHA
`f268c4dc…fdf9f`, capsule SHA `02660229…576c`).

Cycle 1734 records the native Linux Xenia Edge oracle release `60ff861`
(AppImage SHA `c2cac2a0…b828`) and adds
`scripts/run_xenia_edge_native.sh`. The launcher fixes persistent
`storage_root`, `content_root` and `cache_root` paths, so an Edge profile is
created once and reused on later launches; `XENIA_EDGE_PROFILE_XUID` is an
optional explicit slot-0 setting. Edge remains generic/oracle-only: no PAL
run, pixel, audio or mission claim is promoted. Receipt:
`reports/cycle-1734-xenia-edge-native-profile.md` and capsule
`analysis/oracle/ac6-xenia-edge-native-profile-v1.json` (report SHA
`36c7951c…d99d7`, capsule SHA `2293cb49…5a58`).

Cycle 1735 audits the supplied Xenia Edge run archive
`xenia-edge-ac6-decomp-20260816-104009.tar.zst` (archive SHA
`09866105…864d2`). Its embedded `Default.xex` matches the PAL demo SHA
`de917873…5da8`, and all 13,558 internal checksums pass. The run ends at Edge
frame 4183 without Vulkan device loss or nonzero XMA status and contains
12,922 HIR files plus 630 shader files. Three reached microcodes match the
native hashes after a calculated 32-bit word swap (`099625…`, `93488…`,
`491360…`); `586168…` is absent. The Release build has no `.xtr` GPU trace and
no dynamic HIR coverage, so this remains Xenia-generic/oracle-only. No native
PM4, pixel, audio or mission claim is promoted. Receipt:
`reports/cycle-1735-demo-xenia-edge-run-archive.md` and capsule
`analysis/oracle/ac6-demo-xenia-edge-run-archive-v1.json` (report SHA
`c5bacad4…9aec`, capsule SHA `a2984445…7ba32`).

Cycle 1730 adds a read-only, opt-in body-state and affinity audit at the next
indirect frontier. The current scheduler uses deterministic `ucontext` guest
fibers on one host thread; the 23 blocked entries are guest threads, not 23
host `pthread` waits. `KeSetAffinityThread` ordinal 151 is observed with raw
`r4` values in `{1,2,4,8,16,32}`, but the current handler does not retain that
value in `GuestThread`. The body hook records 8,530 bounded stores per route
in `[0x82934000,0x82935000)`, only zeros and guest pointers
`0x829342A0/0x82934500`, with neutral/START stderr byte-identical. These are
object-state stores, not an EDRAM or pixel proof. The six-vCPU policy and the
effect of retained affinity remain unknown. Binary SHA
`ca318018…f8d4e`; Xenia, its archived patch and ptrace are not used. CTest
OFF 18/18 and ON 17/17 pass. Durable evidence is
`reports/cycle-1730-demo-thread-affinity-body-state.md` with capsule
`analysis/demo/ac6-demo-thread-affinity-body-state-v1.json`.

Cycle 1729 adds a read-only, opt-in object/vtable join at the next indirect
frontier. Each route records 853 calls from `LR=0x822E559C` to `0x822F8848`
with object `0x82934280`, vtable `0x8202A488` and slot 4 targeting the same
body; the register arguments are stable and neutral/START stderr is identical.
The RTTI/static PAL cross-check agrees, but the body role and the first nonzero
EDRAM writer remain unknown. The probe binary SHA is
`43677602…164d`; Xenia, its archived patch and ptrace are not used. The
durable receipt is `reports/cycle-1729-demo-dynamic-object-vtable-join.md`
with capsule `analysis/demo/ac6-demo-dynamic-object-vtable-join-v1.json`.

Cycle 1728 adds a read-only native event-handoff trace. In both routes, the
window ticks 1040..1099 contains 60 exact `E000004C` wakes from thread 12 to
thread 1, 60 waiter resumes and 60 re-entries into the same signal/wait pair;
neutral and START stderr are byte-identical. No lost wakeup is observed in
this window, but the reason for the immediate re-entry and the body/vtable
contract of `0x822F8848` remain unknown. This probe uses neither Xenia, the
archived Xenia patch nor ptrace, and does not alter production scheduling.
The durable receipt is `reports/cycle-1728-demo-event-handoff-xma-frontier.md`
with capsule `analysis/demo/ac6-demo-event-handoff-xma-frontier-v1.json`.

The project-wide build parallelism is permanently set to 12 for Ninja compile
pools, CTest targets, the pinned XenonRecomp build, and the generated-unit
syntax/object checks. `AC6_DEMO_BUILD_JOBS` is an explicit opt-down override
for constrained hosts; its default is 12. The current generated product build
completed in 46.7 seconds with up to 12 compiler processes and a serialized
link step.

## Strict codegen checkpoint

<!-- BEGIN MANIFEST-DERIVED FOUNDATION -->
- Canonical Ghidra: `ghidra-projects/ace-combat-6-demo`, `PowerPC:BE:64:Xenon`, 4,431 chunks and 6,789 data ranges.
- Strict codegen: 8,327 `.pdata` functions, 135 explicit boundary records, 12,857 total functions, 158 switches and 52 generated C++ units.
- Diagnostics: 0 boundary, 0 unsupported instruction; two clean codegens byte-identical: `true`.
- Product support remains `false`; CPU/runtime, frontend, graphics, audio, input/replay and mission gates are not all closed.
<!-- END MANIFEST-DERIVED FOUNDATION -->

`ac6-demo-static-decomp-atlas/v1` now deterministically reconciles all 12,857
strict records: 8,327 `.pdata` entries, 4,431 Ghidra chunks and 99 additional
non-overlapped confirmed entries. Two independent generations are byte-identical
at SHA-256 `7ee1e677dfac287fdcd8d80b1c5f34575cbabf1c41ab79e70bd1581f87114e2d`.
Every one of the 3,041,220 executable `.text` bytes is now classified. The
last range is the qualified 228-record XEX callable-import table; its 181 bogus
one-byte Ghidra functions do not decode as Xenon instructions and are retained
as explicit rejections rather than callable boundaries. The wide-decomp gate
contains 10,937 successful pseudocode hashes, 3 explicit failures, 5 timeouts,
1,912 unavailable exact-boundary matches and 28,535 direct edges. Two fresh
imports produce byte-identical manifests, semantic exports and enriched
atlases. The atlas also contains 29,982 global references, 1,990 hashed string
references, 1,102 qualified import edges, all 801 byte-derived MSVC RTTI
vtables with 11,019 slots and 2,385 base records, 6,631 function/vtable
memberships, and all 7,415 computed call/jump sites explicitly unresolved
until dynamic or static target proof exists. The 1,920 non-success
decompilations stay explicit `failed`, `timeout` or `unavailable` states; they
do not weaken or resize independently qualified boundaries. The reproducible
wide static-atlas gate is closed; semantic roles remain `unknown` unless
separate evidence supports a stronger classification.

After adding the three qualified data ranges, codegen OFF passes 14/14 and
codegen ON passes 13/13. Two clean codegens retain zero diagnostics and have
byte-identical manifest `b6fb4890…c201` and generated-tree digest
`4ce7acfa…f98b`. A tick-253 record/replay from two fresh stores records one
tick-252 `XamInputGetState` START call; strict movie replay yields identical
RTPLY-v4 `50776916…aa6c` with no HID path. The frontier remains scheduler-only,
not frontend-qualified.

The disabled-by-default generated-function hook now emits
`ac6-demo-reachability-atlas/v1`. Two fresh START movie replays at tick 253
produce byte-identical atlases (`525e9b31…842e0`): 2,285 exact entries,
12,591,995 calls, ticks 0..252. The neutral control reaches the same 2,285
entries (`5913b642…f5dee`) and differs only by 334 aggregate calls across the
two one-instruction critical-section wrappers `0x822E1DF0/0x822E1DF8`.
Neither virtual update owner `0x82170F58` (consumer instruction `0x82170FCC`)
nor `0x82185198` (consumer instruction `0x82185210`) is reached. Extending
strict XAM replay through tick 260 keeps the same 2,285-entry set and 115
present calls, with no qualified frontend transition. This is a bounded
scheduler frontier, not evidence of a menu state.

The task dispatcher is now joined dynamically. Exact literal ABI cross-match
shows that `0x82259D10` walks three task lists, calls slot 10, then calls slot 4
at `0x82259D74`. The tick-252 objects and RTTI-qualified vtables are
`0x2E7F0080/0x8201130C` (`CModeTaskStartUpDemoOffline`, slot 4
`0x8218A4A0`), `0x18BA2BF4/0x8200F388` (`CTaskLoading`, slot 4
`0x8218CE20`) and `0x18980000/0x82011694` (`CTaskModeManager`, slot 4
`0x821929A8`). Thus START is currently sampled while the offline startup task
is active; neither target consumer task has been constructed. After that
dispatch, the primary thread waits in `0x820FF8D8` for the guest render-command
queue at `0x82386CC0`, while its worker fiber entered `0x820FFCA0`. This exact
producer/consumer queue is the next scheduler corridor; no counter or task
state is synthesized by the runtime.

The 238 XEX imports are represented by 228 build-only
callable fail-closed stubs and 10 pinned `kVariable` data records; callable
stubs now raise the product `RuntimeTrap` with module, ordinal, guest LR and
tick when dispatch refuses them. Native kernel/XAM implementations remain
incomplete; the reached XAM input state/capability/vibration/keystroke ABI is
implemented fail-closed for the qualified gamepad flags and users.
Generated C++ and objects remain under the build directory and are not
versioned.

The previous 19,609/79 checkpoint included an invalid synthetic branch-delay
slot model and is not an acceptance result. The current exporter keeps callable
interior `bl` targets only when independently qualified and classifies the
`0x8233372C..0x82333738` gap as data in the demo manifest. This checkpoint
proves generator coverage and syntax only; execution of the generated guest,
reached import/kernel/graphics/media semantics and the six acceptance lanes
remain open. `supported=yes` is still forbidden.

## Reached boundary loop checkpoint

The tick-220 virtual call at `0x82323F04` now resolves through the live object
`0x2E3CEB10`, vtable `0x8200654C` and slot 11 to the independently bounded
getter `0x820D0FF8..0x820D1004`. Canonical Ghidra proves that this entry reads
one big-endian word from `object+0x1C[index]`; the bridge observed index zero.
The neutral trace remains byte-identical after closing the boundary.

The next reached virtual call at `0x820DF8E0` is also closed: the live object
`0x2E3CED14` resolves through vtable `0x82006D8C`, slot 6, to the bounded
constant-return entry `0x8222F2D0..0x8222F2D4`. This moves the deterministic
frontier from tick 220/111 presents to tick 244/114 presents.

The multi-target callsite `0x821042AC` is now closed without treating its
englobing Ghidra chunk as a single callable entry. On the live object
`0x826E47B0`, vtable `0x820092BC`, the bridge resolves four distinct thunks:
slots 8/9/10/11 to `0x821044C8/D0/D8/E0`. The corrected provenance was
reimported and regenerated twice; its guest object is unchanged by the
text-only correction. This moves the frontier to tick 252/115 presents.

The tick-252 slot-10 call is now closed as well. At `0x82259D54`, the live
object `0x2E7F0080` resolves through vtable `0x8201130C` to the independently
bounded reader `0x8216C940..0x8216C98C`; its adjacent slot-8/9 siblings at
`0x8216C900` and `0x8216C920` set and clear the two bytes that it reads. Two
fresh imports and codegens are byte-identical, and the neutral replay trace
remains byte-identical.

The user-slot enumeration at `0x8219BA40` is also closed for the explicit
single-profile offline bridge: slot 0 returns local state 1, slots 1..3
return not-signed-in state 0, and indices above 3 remain fail-closed. The
bridge observed all four title-permitted slots and advanced without creating
a second profile. Its behavioral capsule is
`analysis/demo/ac6-demo-xam-signin-state-capsule.json`; exact stock service
parity remains an explicit oracle debt.

The offline receive corridor is closed too: the socket created at tick 106
remains invalid, `NetDll_recvfrom` returns `-1/WSAENOTSOCK` at tick 252,
performs no guest-buffer write and never opens a host socket. A bounded replay
continues cleanly to tick 260; its only terminal diagnostic is the requested
max-tick scheduler bound, not an import, CPU or indirect-target fault. The
normalized capsule is
`analysis/demo/ac6-demo-offline-recvfrom-capsule.json`.

There is therefore no reached fail-closed CPU/import/dispatch frontier through
tick 260. The controlled XAM input seam is now closed at the first poll: at
tick 252 the neutral control leaves the live controller object's current,
pressed, released and previous fields at zero, while one `START=0x0010` frame
produces `[16,16,0,16]` after the title's PPC postprocessor. Its first title
consumer copies the live state through vtable slot 3 and normalizes raw
`START=0x0010` to player-0 bit `0x0400` at `0x827B37E0`. The trace replays
deterministically with no physical-controller fallback. This does not prove a
menu transition: no downstream menu-state branch or renderer output has yet
been qualified. Canonical Ghidra finds four direct xrefs to `0x827B37E0`: three
initialization/reset writes and one reader at `0x821995E8`. That reader selects
and resets input records; it is not a menu transition. The next boundary is
the first state branch after the logical-bitset handoff at `0x821DE990`,
followed by a guest-owned menu-state change. Static analysis has now mapped
normalized `START=0x0400` to pressed logical bit `0x10` at `0x82798488` and
identified the first typed consumers: `0x82170FCC` in
`CModeTaskDemoBase::update` and `0x82185210` in
`CModeTaskMissionTitle::update`. A neutral/START A/B probe now observes this
mapping dynamically: the logical current/previous/pressed tuple changes from
`[0,0,0]` to `[16,0,16]`, and both 763-event traces replay byte-identically.
The typed consumers' runtime reach at tick 252 remains
unknown, so neither type name is promoted to an observed menu. Normalized
input evidence is in `analysis/demo/ac6-demo-input-boundary-evidence.json` and
`analysis/demo/ac6-demo-controller-state-capsule.json`; function-boundary
evidence is in
`analysis/demo/ac6-demo-820d0ff8-boundary-evidence.json` and
`analysis/demo/ac6-demo-8222f2d0-boundary-evidence.json`, with the multi-slot
capsule in `analysis/demo/ac6-demo-821044d8-boundary-evidence.json` and the
two-byte state-reader capsule in
`analysis/demo/ac6-demo-8216c940-boundary-evidence.json`.

The canonical SDK-import call graph is now exported for all 238 identified
XEX imports. It contains 743 owning nodes and 1,853 edges, including 1,435
direct calls, 118 bounded wrappers and 294 wrapper-frontier edges. All 320
owned `bctr`/`bctrl` sites remain explicitly unresolved because no target is
promoted without separate vtable/slot evidence. Two read-only/no-analysis
Ghidra exports are byte-identical. The graph and its identity, determinism and
negative-test evidence are
`analysis/demo/ac6-demo-sdk-callgraph.json` and
`analysis/demo/ac6-demo-sdk-callgraph-evidence.json`.

The reached graphics boundary is now explicit rather than inferred from a
presentation counter. By tick 252, the bridge has observed 115 `VdSwap` calls
with a 1280x720 frontbuffer at `0x1374A000`, format 6 and colour space 0. It
writes the bounded ReXGlue wire shape (six fetch dwords, `PM4_XE_SWAP`, then
type-2 NOP padding) into guest memory. The ring walker follows both reached
indirect buffers and structurally bounds all 877 packets / 3,065 dwords:
340 type-0, 252 type-2 and 285 type-3 packets, including two `DRAW_INDX_2` and
one `XE_SWAP`. Unknown packet types, opcodes or qualified payload-size
violations trap before RPTR/writeback completion. The exhaustive main-IB
inventory establishes a stricter semantic frontier: its type-0 packet at
dword offset 2 writes `0x0A02..0x0A05`, absent from the pinned register
authority. Current execution traps before any batch effect. Earlier
downstream effect counts and typed draw/present observations remain structural
discovery evidence, not an executable-semantics qualification. Shader
translation, fetch/tiling/resolve and pixel output remain unimplemented, so
`frontend=false`. The historical measurements are in
`analysis/demo/ac6-demo-renderer-frontier-evidence.json`; the current
fail-closed inventory is in
`analysis/demo/ac6-demo-pm4-inventory-v1.json`.

The two independent builds recorded in
`analysis/demo/ac6-demo-foundation-evidence.json` have identical generated
files, per-translation-unit objects, manifest and relocatable guest object
bytes. The relocatable link disables GNU ld's random build-id, so the
byte-identical result is a property of the actual build inputs rather than
just matching sizes. Import parsing is also qualified: 151 records come from
`xboxkrnl.exe` and 87 from `xam.xex`; the 228 callable records have a
build-only trap carrying module, ordinal, guest LR and tick, while the 10
`kVariable` records remain data imports. This trap is not the native
kernel/XAM implementation.

## DurableBin boundary

The supplied PAL evidence is recorded in `docs/durable-bin.md` under its own
XEX identity (`acc302c1…bcde`); it is not merged with the demo identity. The
native ABI now exposes only the proven opaque wrapper: `DurableBinAbi` is `0x10`
bytes with a non-owning 32-bit guest payload address at `+0x00`, and
`ObjBinAbi::durable` is at `+0x0C` in the `0x20`-byte object. The bounded
Ghidra evidence now proves the first demo consumer: runtime `+0x2A0` holds the
`ObjBin*`, then `ObjBin+0x0C -> DurableBin+0x00`, and `lbz` reads `payload+0`
at `0x82095DC8` and `0x8209611C`. Only that byte offset/width is qualified;
payload length, subsequent fields and runtime survivability semantics remain
open. The parallel table ABI is also recorded as `UnitTblBinAbi` (`+0x04`
units, stride `0x08`; `+0x08` objects, stride `0x20`) with the second
`UnitBinAbi` word intentionally opaque.

## Open acceptance lanes

The six lanes remain open: CPU/codegen boundary closure; kernel/XAM/VFS
coverage; D3D9LTCG/Vulkan coverage; input/time/replay equivalence; XMA/media
coverage; and frontend/mission/objective execution. In particular, no mission
logic or success/failure result is synthesized by the native hooks.

Exact Xenon behavior for the reached `vrefp`/`vrsqrtefp` paths still requires
bounded hardware evidence or an observable convergence proof. Any uncovered
MMIO, import, shader, media packet, graphics state or function boundary must
continue to trap rather than produce a synthetic result.

## Runtime scheduler checkpoint

The first reached worker pair calls `KeWaitForSingleObject` on a guest event
with a relative `LARGE_INTEGER` timeout of `-300000`. The bridge now runs the
generated functions on persistent Linux `ucontext` fibers with 1 MiB host
stacks, preserves their PPC contexts across waits, converts that timeout to a
deterministic guest-tick deadline, and wakes waiters by qualified event,
semaphore, kernel-event or thread key. `play --ticks 3600` completes in about
2.1 seconds without a host-side busy loop. This closes only the scheduler
boundary; frontend, mission, graphics, media and objective acceptance remain
open and no result is synthesized.

The reached kernel object split now distinguishes 22 events, two ownership-
tracking recursive mutants and one semaphore. `NtCreateMutant`, mutant waits,
`NtReleaseMutant`, and the signal half of
`NtSignalAndWaitForSingleObjectEx` preserve owner and recursion and use a
separate deterministic wait key. The SDK-qualified Xenon
`RTL_CRITICAL_SECTION` ABI is 28 bytes: synchronization storage at `+0x00`,
then lock count, recursion and owner at `+0x10/+0x14/+0x18`; contended enters
now block instead of allowing simultaneous owners. The historical 20-tick
probe reached only the controller constructor at `0x822F5C40`. The current
bounded probe reaches the poll function `0x822F61A0`, four capability calls
and one state call at tick 252. Read-only post-call observation proves the
`START` edge in the guest controller object. The first title consumer is also
closed: vtable slot 3 copies the state and the title normalizer publishes
`0x0400` at `0x827B37E0`. The following menu-state branch remains unknown.
Consequently no menu transition, visual state or mission progression is
synthesized. That branch and the renderer remain named fail-closed boundaries.
