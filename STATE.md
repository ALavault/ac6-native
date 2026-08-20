# État durable — AC6 démo PAL native

## Objectif

Faire fonctionner `recompilation/ace-combat-6-demo` sur l’unique
`Default.xex` démo PAL qualifié, du cold boot au menu visible, puis à une
mission jouable avec succès et échec endogènes, replays déterministes,
capsules et readbacks guest non noirs.

Identité canonique : Xbox 360 Xenon/Xenos, SHA-256 XEX
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`,
projet Ghidra `ace-combat-6-demo`.

## Faits établis

- Le cold path natif atteint 3 036 ticks avec START au tick 3 000 et relâche
  au tick 3 001, sans trap : 23 threads guest et 2 899 appels `VdSwap`.
- `VdSwap` publie un frontbuffer guest `0x1374A000`, format 6, `1280×720`.
- Le second ring primaire est `0x126CA000`, capacité 131 072 dwords. Dans le
  run final il reste à 0 soumission, RPTR 0, WPTR 0.
- Le source Xenia Edge `e4b13738c3c461b2c06241fa3f54b5a669b6a304`
  transmet à `EnableReadPointerWriteBack` uniquement l’adresse fournie. Le
  runtime natif fait désormais de même.
- Les packets PM4 atteints `0x61`, `0x62`, `0x63` et les valeurs dynamiques
  qualifiées de `EVENT_WRITE_SHD` sont acceptés avec tests bornés.
- Build codegen-on, CTest `26/26`, installation et absence de `bin/bin` sont
  validés. Détails et hashes :
  `analysis/demo/ac6-demo-ring-readback-frontier-v1.json`.

## Résultats négatifs importants

- Vulkan valide 2 modules, mais crée 0 pipeline raster, 0 normal draw,
  0 writeback guest et aucun screenshot d’audit.
- Aucun packet Xenos `PRESENT`, readback guest non noir, frontend qualifié,
  mission ou terminal n’est atteint.
- Les 2 899 notifications `VdSwap` ne constituent pas une preuve visuelle.
- Écrire le timebase hôte dans l’inconnu `KTHREAD+0x58` est inutile : retiré.
  Les digests headless/Vulkan finaux restent identiques sans cette écriture.
- Le faux writeback hôte à `adresse_RPTR-0x3c` touchait un champ guest distinct
  et n’est pas conforme au source Edge : retiré.

## Hypothèses ouvertes

- Le producteur guest qui doit publier le WPTR après la seconde
  `VdInitializeRingBuffer` est encore inconnu.
- L’ordre asynchrone du CP dans Edge peut expliquer une partie de la chaîne
  d’attente D3D, mais son impact dans ce runtime reste spéculatif.
- Les packets construits par `VdSwap` existent dans des buffers guest ; la
  condition qui doit les joindre au ring final reste inconnue.

## Décisions

- Utiliser le code source Xenia Edge comme référence statique ; ne pas lancer
  son runtime et ne pas utiliser Wine.
- Ne faire aucune optimisation avant affichage natif du début de mission.
- Ne pas lancer d’A/B par défaut ; seulement pour une ambiguïté causale nommée.
- Ne jamais promouvoir un visuel sans pipeline/readback guest et RGB non nul.
- Conserver les frontières inconnues fail-closed et ne jamais modifier le C++
  généré.
- Aucun CPJ, worker automatique ou sous-agent.

## État Git exact du checkpoint

- Branche : `main`, upstream `origin/main`.
- Base fonctionnelle avant les documents de checkpoint :
  `aa9b05347ad0c26dba16e1ca77bef902a664d47d`, alors synchronisée `+0/-0`.
- Les documents sont publiés par le commit contenant ce fichier ; le retrouver
  par `git log -1 --format=%H -- STATE.md`.
- Modifications préexistantes non incluses dans le checkpoint :
  `recompilation/ace-combat-6-demo/{CMakeLists.txt,src/guest_bridge.cpp,src/guest_bridge/dynamic_object_vtable_trace.hpp,src/guest_bridge/graphics_dispatch.hpp,src/guest_bridge/graphics_interrupt_trace.hpp,src/guest_bridge/import_journal.hpp,src/guest_bridge/lifecycle.hpp,src/guest_bridge/transition_memory_trace.hpp,src/guest_bridge_resources.cpp,tests/test_xam_return_chain.py,tools/map_xam_return_chain.py}` et
  `reports/AC6_DEMO_XAUDIO_CALLBACK_CPU_FRONTIER.md`.
- Artefacts préexistants non suivis :
  `analysis/demo/ac6-demo-{graphics-interrupt-gate-v1,ring-writeback-boundary-v1,title-doorbell-start-v1,title-imports-queue-ab-v1,title-matrix-consumer-ab-v1,title-queue-ab-v1,title-selector-ab-v1,title-task-list-ab-v1,title-xma-kick-v1,vdswap-corridor-v2,vulkan-xenos-frontier-v1,xaudio-cpu-ab-v1}.json`,
  `analysis/demo/ac6-demo-post-resume-ab/sha256/940637146a447e48fc1619471b9910278c962ca0b261017a269c3cc4affca0c8/`,
  `analysis/demo/ac6-demo-xam-return-chain-ab/sha256/{bcf3382c64ec0d415110ffdbc309161df26fe226ce84fc88e03ccc90ab19bee4,c939f016eec34118ffd46df7a4df81e1b916bd0353a26f788705e5ab1dce2e14}/`
  et `recompilation/ace-combat-6-demo/install/`.
