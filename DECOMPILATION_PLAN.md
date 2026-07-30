# AC6 complete native reconstruction plan

## Scope and completion definition

This plan covers `ac6-xbox360-pal`, module `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`,
image base `0x82000000`. It targets an independent native Linux AMD64 product;
Xbox 360 emulation, Xenia, firmware and retail containers remain external
evidence oracles and are not shipped dependencies.

"Complete" means that the product gates in
[`docs/native-port-acceptance.md`](../../docs/native-port-acceptance.md) have
executed, not merely that XenonRecomp emitted C++. In particular, the native
application must boot, select campaign entry 9 through the retail path, load a
mission, present a live frame, accept input, and continue without an emulator.
Generated functions remain `recompiler-generated` until their reachable
behavior is covered by deterministic or differential evidence.

## Stable sources of truth

1. The qualified PAL XEX and its SHA-256.
2. Canonical headless Ghidra project `ghidra-projects/ace-combat-6`.
3. `.tools/ac6-recomp-reference/ac6recomp_config.toml` plus a freshly
   regenerated XenonRecomp/ReXGlue tree; generated output is never edited.
4. Native tests and retail-data smokes under `reconstruction/ace-combat-6`.
5. Bounded Xenia traces only where native/static evidence cannot answer a
   named parity question.

The upstream `sal063/AC6_recomp` checkout is literal control-flow cross-match
evidence only. It cannot override the qualified XEX or headless boundaries.

## Current measured baseline

- The local ReXGlue runtime links on Linux and reaches recompiled PPC code.
- The generated corpus has 50 translation units and 23,321 implementations
  after the qualified boundary removal through cycle 301.
- The extended native AC6 corpus passes 48/48, including generated Xenon
  probes and the bounded Xenia shader-cache retail catalogue.
- The cycle-301 candidate `0x82345228` is confirmed internal and removed.
  The bounded retail smoke closes `0x8234522C -> 0x82345134` and advances to
  `0x8234530C -> 0x8234524C`; later configured entries remain preserved until
  a separate headless contract identifies the exact owner.
- The separate scene shell reaches `scene_complete` for its bounded CUT/Tcam
  slice but is not the full retail runtime.
- Renderer state capture, first material/texture joins and backend draw markers
  exist; retail observation of the host markers remains open.

## Workstreams and gates

| Phase | Workstream | Required output and exit gate |
| --- | --- | --- |
| 0 | Identity and reproducibility | XEX/config/tool revisions and all retail inputs are hash-qualified; codegen and build commands are reproducible; rollback is a TOML/source revert plus regeneration, never a generated-file edit. |
| 1 | Xenon PPC control flow | Audit false configured starts in bounded families, regenerate once per qualified batch, and advance the same retail smoke. Exit when every runtime-reachable direct branch has a generated target and startup has no unresolved-branch fatal. The residual global unresolved inventory is either eliminated or classified unreachable with explicit static evidence. |
| 2 | Xenon instruction and ABI fidelity | Exercise unaligned, byte-reversed, reservation/atomic, VMX128, floating-point, CR/XER, cache-publication and 32-bit guest-pointer behavior with generated-leaf probes. Exit when all instructions reached by the campaign path have an implementation and targeted tests. |
| 3 | xboxkrnl/XAM platform runtime | Inventory every imported ordinal reached by startup/campaign; implement memory, threads, synchronization, time, VFS, input, notifications, locale and overlapped I/O with ABI-qualified handlers. Exit when no reached service is a permissive stub and missing services fail with ordinal/context. |
| 4 | XMA and audio | Recover XMA voice/packet ownership and xboxkrnl/XAudio/XAM crossings; provide deterministic decode/stream scheduling and fixture tests. Exit with audible native mission/title output, stable packet timing, and no opaque emulator dependency. |
| 5 | Retail data and world construction | Complete DATA.TBL/PAC indexing, bounded decompression, resource graph, mission selector 1/entry 9, unit factories, scene/world population and preservation of unknown fields. Exit when the native runtime—not only the diagnostic shell—loads the first campaign world with non-empty proven state. |
| 6 | Xenos shaders and renderer | Inventory every shader reachable in the first mission; translate via XenosRecomp, preserve fetch/constant/endian semantics, and implement render targets, depth, vertex/index/stream state, materials, textures and command submission. Exit with zero missing reachable shader/state handlers and a deterministic frame manifest. |
| 7 | Gameplay runtime | Connect native input to the proven player owner, fixed-step simulation, camera, aircraft, weapons, AI, collision, mission scripts and pause/menu/save transitions. Exit when one first mission segment advances under native keyboard/controller input with stable state. |
| 8 | Differential parity | Define bounded checkpoints for startup, title, mission load and gameplay; compare registers/memory, resource identities, audio events and frame summaries against Xenia. Exit when every acceptance checkpoint is green or carries an explicit bounded deviation approved as a port change. |
| 9 | Product hardening | Linux normal and ASan/UBSan gates, deterministic missing/corrupt-data failures, root install, Windows AMD64 build and later executed startup. Exit only when all five product outcomes and executable gates are recorded. |

## Cross-workstream inventories

The following machine-readable inventories should be maintained rather than
reconstructed from prose:

- configured/generated/real PPC function starts and unresolved branch pairs;
- generated Xenon instruction opcodes with executed probe coverage;
- xboxkrnl/XAM/XMA imports with `implemented`, `fail-closed`, `unreached`, or
  `unknown` status;
- Xenos shader identities, translation results and runtime reachability;
- DATA.TBL/PAC resources with hashes, parent links and parser coverage;
- runtime checkpoints with native and oracle evidence identifiers.

No percentage may combine generated functions with semantically verified
functions. Report both denominators separately.

## Iteration policy

Each cycle must name one new input, one unresolved boundary and one measurable
exit. Boundary fixes are batched when they share a containing function or
failure family. After a qualified config change: regenerate, build with
`-j16`, run the bounded smoke, then run the relevant full CTest corpus. Do not
repeat the same fatal without changed inputs.

Human/VNC sessions remain deferred. A future request must name one target
action, expected artifact and time limit. Xenia may be run autonomously as a
bounded oracle, but is not required for static boundary repair.

## Active tranche

**État au cycle 324 : le front est l'interruption GPU de source 1.**

Le cycle 324 a trouvé pourquoi le runtime paraissait muet, et ce n'était pas les
correctifs manquants soupçonnés au cycle 323 : `ac6_performance_mode` vaut
**`true` par défaut** et force `log_level = "error"`. Une exécution nue émet
3 lignes et un `ac6recomp.log` de 0 octet ; avec
`--ac6_performance_mode=false`, la même exécution en journalise **716**. Toute
mesure « le runtime ne fait rien » depuis le cycle 318 a été prise à l'aveugle.

Mesure sur 90 s, télémétrie désormais dans l'arbre :

| observable | valeur |
| --- | ---: |
| interruptions vblank (source 0) | 300 → **5 400**, ~60 Hz, régulières |
| interruptions EOP (source 1) | **12, gelé** |
| `guest_swap_requests` | **4** |
| `host_swap_presents` | **3** |
| présentation refusée, motif journalisé | **1** |
| lectures `DATA00.PAC` | 129, **toutes en 1,1 s** |
| lignes de journal après t+1,5 s, hors audio et télémétrie | **0** sur 88 s |

Deux conséquences :

1. Le cycle 316 concluait « les deux nombres sont égaux, il n'y a aucun défaut
   côté hôte ». Vrai de son échantillon `2/2` ; ici c'est **4 demandées,
   3 présentées, 1 refusée** avec un motif nommé. À requalifier.
2. La branche **travail** du gestionnaire invité `sub_821E63B0` est réservée à la
   **source 1** (cycle 317 §1). Elle a tourné **12 fois puis jamais plus**, alors
   que la source 0 continue 5 400 fois. Les EOP naissent des soumissions GPU de
   l'invité : l'état est une boucle refermée sur elle-même.

**Question du cycle suivant : qu'est-ce qui a produit les 12 premières EOP, et
pourquoi la 13e n'arrive-t-elle pas ?** Ne pas reprendre « pourquoi
`sub_821EFBA0` ne dessine pas » : cette fonction est sur le chemin source 0, qui
n'est pas la branche travail.

Mesurer avec `tools/ac6-frame-loop-probe.sh <label> [secondes]`, qui passe
`--ac6_performance_mode=false` (**obligatoire**), `--log_flush_interval=1`, active
la télémétrie, borne réellement le processus invité et nettoie les fuites. Ne
jamais passer `--log_file` : `Ac6recompAppCreate` l'écrase.

Voir `reports/cycle-324-observability-restored-and-eop-frontier.md`.

---

**État au cycle 323 : le défaut de synchronisation est fermé, sans débloquer
l'affichage.**

Le cycle 323 ferme le défaut de synchronisation poursuivi des cycles 320 à 322.
La valeur partagée à `0x82870828` a été **mesurée** — `0` sur les deux moitiés
d'un mot de 64 bits, hors débogueur, mapping prouvé par ancre — ce qui renverse
la conclusion du cycle 322 (« l'anomalie n'existe pas ») et rétablit celle du
cycle 320. Le protocole est un ping-pong strict entre `sub_8233BA78` (attend
`0`, pose `1`) et `sub_8233AD70` (attend `1`, pose `0`), annoncé une seule fois
par changement d'état ; le voleur du signal est **le thread qui vient de le
poser**, qui reconsomme son propre `NtSetEvent`. Interblocage reproduit
**3/3 dès l'itération 1** par `scripts/ac6_condition_pingpong_regression.cpp`,
corrigé par
`patches/rexglue-auto-reset-event-nt-ordered-release-20260730.patch`.

**La correction ne débloque pas la présentation.** Le front reste celui du
cycle 317 §5 : la machine à états tourne dans un mode qui ne dessine pas.

Le cycle 323 a aussi trouvé que les chiffres qui structurent les cycles 316 et
317 venaient de correctifs d'instrumentation **archivés et non appliqués**, donc
qu'une exécution nue rapportait `0` partout sans le signaler. C'est corrigé
durablement : les compteurs vivent maintenant dans l'arbre, dans
`rex::graphics::frame_loop_telemetry`, et émettent une ligne unique toutes les
N interruptions vblank sous le cvar `frame_loop_telemetry_interval`
(`0` = silencieux, défaut). Ils survivent donc aux régénérations.

Mesurer avec `tools/ac6-frame-loop-probe.sh <label> [secondes]`, qui active la
télémétrie, échantillonne l'état invité, borne réellement le processus invité et
nettoie les fuites. Ne jamais passer `--log_file` ni `--log_level` au runtime :
`Ac6recompAppCreate` écrase `log_file` et ne monte le niveau à `debug` que si
`log_level` est resté par défaut, donc chacun de ces drapeaux **réduit** ce qui
est enregistré.

Voir `reports/cycle-323-self-consumed-wake-and-contamination-sweep.md`.

**État au cycle 313 : le runtime tourne en continu et présente des images.**

Le blocage de démarrage poursuivi des cycles 308 à 312 tenait à **une ligne de
configuration** : `[rexcrt] memcpy = 0x82382F18`, alors que cette adresse est
`__restgprlr_16`. Toute fonction dont l'épilogue restaure `r16`-`r31` appelait
`memcpy` à la place et ne restaurait jamais ses registres non volatils.

| | cycle 312 | cycle 313 |
| --- | --- | --- |
| sortie du smoke | 134 (abort) | **124 (survit)** |
| journal invité | 1,43 s | **111 s, sans fin** |
| images par seconde | 0,00 | **0,86** |
| dessins hôte | 0 | **2** |
| audio | inactif | **10 trames soumises** |

Le jeu charge ses données en continu (45 lectures `DATA00.PAC` sur 111 s) et
fait tourner son moteur audio. Il n'affiche toujours **aucun contenu de jeu** :
l'écran ne montre que le panneau de diagnostic. La tranche suivante porte sur le
faible nombre de dessins hôte.

**État au cycle 307 : le corpus ne contient plus aucun `REX_FATAL`.**

| Cycle | Pièges `Unresolved branch` | Pièges `Unresolved call` |
| --- | ---: | ---: |
| 304 (attribution) | 4 857 | 205 |
| 305 (retraits en masse) | 26 | 205 |
| 306 (correctifs générateur + 35 entrées) | **0** | 205 |
| 307 (167 retraits + 1 ajout) | **0** | **0** |

Le cycle 306 a corrigé deux défauts du générateur — GapFill recréant des
fonctions dans les trous inter-blocs, et `classifyTarget` identifiant l'appelant
par adresse plutôt que par la fonction en cours d'émission — et découvert
35 entrées `0xADDR = {}` en hexadécimal minuscule qu'un filtre trop étroit avait
masquées pendant tout le cycle 305. Le cycle 307 a appliqué la méthode de
retrait du cycle 305 à la famille `Unresolved call`, jamais analysée jusque-là
pour la même raison de filtre.

Le corpus compile (48/48), se lie (163,6 Mo) et le smoke survit. Les
1 239 `ppc_trap` restants sont des instructions PowerPC réelles (`twi`,
`twllei`, ...) et non des échecs de traduction.

**Prochaine tranche : dépasser le smoke.** Le smoke ne prouve que l'absence
d'arrêt ; il ne dit rien de l'affichage, de l'entrée ni de la progression vers
une mission. C'est l'objet du cycle 308.

Voir `reports/cycle-306-unresolved-branch-eliminated.md` et
`reports/cycle-307-zero-fatal-corpus.md`.

## Historique

Cycle 303 a rejoué ce retrait avec télémétrie mémoire, comme le cycle 302 le
demandait. **Le `std::bad_alloc` n'est pas reproductible.** Avec et sans
`0x82345250`, `rexglue codegen` sort en 0, à 269 812 et 270 076 ko de RSS
crête — 0,1 % d'écart, sur un hôte de 121 Go. Les deux corpus générés compilent
**52/52** en C++23. Le retrait supprime bien `sub_82345250` (2 unités -> 0) et
laisse `sub_823450D0` intact.

Le motif de blocage enregistré au cycle 302 n'existe donc pas : son échec est
attribuable au répertoire `generated/` partiellement vidé au moment de la
commande, et non à la configuration ni au SDK. Le retrait est accepté au niveau
codegen et compilation ; il reste à relier et exécuter le smoke runtime avant
promotion. Voir `reports/cycle-303-rexglue-bad-alloc-not-reproducible.md`.

Cycle 302 qualifie `0x82345250` comme coupure interne par **28/28** assertions.

Cycle 301 closed configured `0x82345228`: 27 headless assertions classify it
as the internal third-loop backedge setup in `sub_82345100`. Removing only
that split closes `0x8234522C -> 0x82345134` and advances the retail runtime to
`0x8234530C -> 0x8234524C`.

1. Qualify the exact owner of source `0x8234530C` and internal target
   `0x8234524C`; preserve every configured entry beginning at `0x82345250`
   until its own contract passes.
2. Preserve every ambiguous address; remove only entries proven to be internal
   fallthroughs by headless CFG/prologue/frame evidence.
3. Regenerate once, build with `-j16`, and test whether
   `0x82345214 -> 0x82345144` disappears; preserve every independent hook at
   an internal instruction unless its own evidence warrants a separate change.
4. Run the same bounded Xvfb/GDB retail smoke and record exactly one new
   frontier.
5. Keep the runtime `candidate` until the product gates above execute.
