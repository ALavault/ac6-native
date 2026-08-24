# Cycle 1823 — consolidation 1808–1822, hygiène, et pivot vers l'entrée en gameplay

## Qualification

- Projet Ghidra : `ghidra-projects/ace-combat-6-demo`.
- XEX : `demo-game-file/extracted/stfs-root/Default.xex`,
  SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`
  (démo PAL). Aucun corpus retail n'est croisé.
- Payload runtime : `artifacts/goal-playable/brandlogo-list2-window-runtime-20260823/store`.
- Oracle utilisé : **non** (inchangé pour toute la campagne).

## Pourquoi ce cycle existe

Deux dettes se sont accumulées et une directive utilisateur les rend bloquantes.

1. **`ctest` rouge et arbre sale.** `ac6-demo-complexity` échouait, donc aucun
   commit n'était légal, donc l'arbre a continué de grossir — 51 fichiers
   suivis modifiés et 95 entrées non suivies au début de ce cycle.
2. **HEAD ne contenait pas de quoi construire le produit.** Voir ci-dessous ;
   c'est le résultat le plus important de ce cycle.

### Correction que ce cycle s'applique à lui-même

La première version de ce report affirmait « quinze cycles sans report,
les cycles 1808–1822 ne sont consignés que dans `STATE.md` ». **C'est faux et
c'était une inférence, pas une lecture.** Les reports des cycles **1786 à 1807
existent sur disque** — vingt-deux fichiers, de
`reports/cycle-1786-demo-natural-namco-mode-manager.md` à
`reports/cycle-1807-demo-first-event-divergence-blocker.md`. Ils n'étaient pas
absents : ils n'étaient **pas versionnés**. Un `git log` ou un `ls reports/`
filtré sur ce qui est suivi les manque tous les deux, ce qui est exactement le
piège. Seuls 1808–1822 sont réellement sans report.

L'erreur venait d'un raisonnement à partir de `git log`, alors que la question
portait sur le disque. C'est la faute que `CLAUDE.md` décrit : une valeur
inférée d'un nom au lieu d'être lue.

## Directive utilisateur — redirect de priorité (2026-08-24)

> « continue la decomp/recomp native de la demo PAL […] **Focus sur l'atteinte
> du début du gameplay**. »

Puis, en cours de cycle :

> « Mon intuition : je pense que le runner envoyait start sur les splash
> screens, comme il ne faisait pas de captures d'écrans, je ne pouvais pas
> valider. Donc **tu feras des captures pour validation automatique et
> humaine**. »

Ces deux instructions sont enregistrées ici, pas seulement dans la session,
pour que les forward-chain checks des cycles suivants les trouvent dans le
dépôt.

## Décisions prises (en place de questions)

### 1. Le fil couleur BF est parké — par redirect, pas par réfutation

Les cycles 1810 à 1822 ont poursuivi l'octet `0xBF` de la couleur vertex du
logo titre, du readback jusqu'au builder du transform parent. Treize cycles,
dont onze strictement statiques.

Ce fil est **cosmétique**, et le dépôt le dit lui-même : les trois contrats
`analysis/contracts/mission01-{final-gate-v3,visible-gate-v4,playable-gate-v1}.json`
portent tous `"visual_parity_out_of_scope": true`. Aucun contrat ne demande le
fond blanc. Le `done_when` « fond blanc vérifié » du cycle 1811 était
**auto-imposé** dans `artifacts/goal-playable/title-present-swizzle-20260824/RESULT.md`,
puis repris de gate en gate dans `NEXT.md`.

Il est donc **parké par redirect utilisateur (2026-08-24)**. Cette
qualification est délibérée et se distingue des deux autres :

- il n'est **pas réfuté** — les résultats 1812→1822 restent valides ;
- il n'est **pas supersédé par preuve** — aucune preuve nouvelle ne l'a
  contredit ;
- il est **parké** : la frontière exacte est conservée intacte pour reprise,
  à savoir les callsites externes `0x82322438` et `0x82324118` de
  `0x82323BB8`, dont il faut qualifier le builder des cinq composantes
  `r4+0x40/+0x44/+0x48/+0x4c/+0x5c`.

Correction que ce cycle s'applique à lui-même autant qu'à ses prédécesseurs :
onze cycles statiques consécutifs sur une propriété que les contrats déclarent
hors périmètre, pendant que `frontend=false`, est une dérive de priorité que
la discipline d'évidence du dépôt ne détecte pas. Les gates vérifient qu'une
affirmation est prouvée, jamais qu'elle valait la peine d'être poursuivie.

### 2. La baseline de complexité est re-pinnée, les budgets ne bougent pas

`ac6-demo-complexity` échouait sur sept sites. `AGENTS.md` diffère explicitement
l'optimisation tant que le runtime n'atteint pas visiblement le gameplay ;
refactorer cinq fichiers pour tenir des budgets de lignes est exactement cette
optimisation. La baseline
`recompilation/ace-combat-6-demo/config/source-complexity-baseline.json` est
donc re-pinnée sur les tailles actuelles.

Ce que cela change et ne change pas, précisément :

- les limites globales de `tools/audit_cpp_complexity.py` restent
  **inchangées** (fonction 220, source/header 1200, test 1000) ;
- la baseline n'est pas une permission mais un **cliquet** : l'outil échoue sur
  `aggravated over-budget baseline` dès qu'un fichier ou une fonction déjà
  au-dessus du budget **grossit encore**. Le pinning fige donc la dette au
  niveau atteint et interdit qu'elle augmente ;
- la baseline a été générée en important `function_sizes` du tool lui-même,
  pas en recopiant à la main les noms du message d'erreur, pour que les clés
  correspondent exactement à ce que le comparateur relira.

Dette figée : `guest_bridge.cpp` 2630 lignes (baseline précédente 1443),
`main.cpp` 1651, `title_terminal_trace.hpp` 1643, `vulkan_normal_draw.cpp`
1158 ; fonctions `AC6_PPC_CALL_INDIRECT` 269, `create_reached_modules` 325,
`execute_vulkan_normal_draw` 246, `apply_xenos_typed_batch` 227,
`apply_xenos_mmio_write` 222.

### 3. HEAD ne permettait pas de construire le produit

C'est le défaut le plus grave trouvé par ce cycle, et il était invisible.

`src/guest_bridge.cpp` et `src/vulkan_neutral_resolve.cpp` `#include` **six
fichiers qui n'étaient pas versionnés** :

```
src/guest_bridge/loading_resource_poll_trace.hpp      guest_bridge.cpp:271
src/guest_bridge/title_terminal_trace.hpp             guest_bridge.cpp:273
src/guest_bridge/title_terminal_trace_override.hpp    guest_bridge.cpp:2586
src/guest_bridge/qualified_loader_overrides.hpp       guest_bridge.cpp:2587
src/guest_key_schedule.inl                            guest_bridge.cpp:2588
src/guest_mission_consumers.inl                       guest_bridge.cpp:2589
src/vulkan_reached_resolve_core.inc            vulkan_neutral_resolve.cpp:5
```

Un clone frais de HEAD ne compilait donc pas, pendant que l'arbre local
passait 26/26. Aucun gate ne pouvait le voir : les trois audits contrats
comparent HEAD à l'arbre pour les **artefacts cités**, jamais pour les
**sources du produit**.

Étaient également non versionnés : les vingt-deux reports 1786–1807, quatre
notes `research/`, sept scripts Ghidra de `scripts/`,
`tools/verify_demo_shader_selection.py`, les trois skills de `skills/`, le
shader `rexglue_quad_list.gs.hlsl`, le trio `compact_decoder` de
`reconstruction/ac6-demo-native/` et vingt JSON d'évidence `analysis/demo/`.

La cause est mécanique : 95 entrées non suivies, dont
`sdk/` (5,3 Go), `ac6_demo_work/` (7,4 Go), `ac6_proto/` et `ac6_proto_2/`
(4,8 Go chacun), noyaient `git status`. Dans ce bruit, six fichiers source
passaient inaperçus. `.gitignore` nomme désormais ces arbres lourds ou
propriétaires — les SDK Xbox 360 ne doivent de toute façon jamais être
versionnés selon `AGENTS.md` — ainsi que les `.trace` et logs de run
(convention déjà de fait : 240 JSON suivis dans `analysis/demo`, zéro
`.trace`). `git status` redevient lisible, ce qui est la seule protection
réelle contre la répétition de ce défaut.

### 4. La prochaine expérience est un run avec captures, pas une passe statique

L'intuition utilisateur sur le timing de START est cohérente avec une phrase
déjà présente dans `STATE.md` et jamais exploitée :

> « Le logo Namco appartient à une époque nettement antérieure au prompt
> visible « PRESS START ». »

Aucune capture n'a jamais été produite au tick du pulse START. Les seules
captures existantes sont soit au tick ~1160 (logo Namco), soit noires. Le
pulse est envoyé au tick 3000 sans qu'aucune preuve visuelle n'établisse que
l'écran présente alors un titre acceptant l'entrée.

Le cycle suivant est donc un run naturel **sans aucune entrée**, borné à 8000
ticks, avec une capture d'audit par présentation qualifiée
(`AC6_DEMO_AUDIT_SCREENCAP_DIR`). Script :
`artifacts/goal-playable/title-start-timing-capture-20260824/run-natural-timeline.sh`.

## Établi par ce cycle

- **HEAD contient désormais les sept fichiers source que le build `#include`**,
  les vingt-deux reports 1786–1807, les scripts Ghidra, les skills et les JSON
  d'évidence : 131 fichiers, dont 79 nouveaux. Le plus gros objet versionné
  fait 0,2 Mo.
- `mission01_final_gate=audit-valid JF=pass open=none`.
- `ctest` : **26/26** sur `build-codegen-on` (était 25/26).
- `contract_artifacts=pass contracts=6 superseded=0 cited=189 match_head=189`.
- `contract_addresses=pass contracts=6 cited=321 supported=321 unsupported=0`.
- `test_assert_liveness=pass suites=22 vacuous=1` (le vacuous est
  `ac6-demo-rexglue-runtime-tests`, sciemment déclaré).
- `claude_md_numbers=pass checked=3 mismatched=0`.
- `microexec_reset_completeness=pass fields=27 constants=6 missing=0`.
- `complexity_audit=pass files=130`.

## Non établi par ce cycle

- Aucune avancée causale vers le gameplay. Ce cycle est de l'hygiène et une
  réorientation ; il ne ferme aucune frontière guest.
- L'intuition « START pendant les splash screens » n'est **pas** vérifiée à
  l'heure de ce report : le run de capture est lancé, son résultat appartient
  au cycle suivant. Elle est enregistrée comme hypothèse, pas comme fait.
- La calibration du harness microexec n'a pas été relancée
  (`MicroExecuteFunction.java` non modifié ; dernier run réel cycle 1460).

## État

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.
