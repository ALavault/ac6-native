# Prochain gate unique

## Question

Quel producteur guest doit effectuer le premier write `CP_RB_WPTR` après la
seconde `VdInitializeRingBuffer` (`base=0x126CA000`), et quelle frontière
native empêche actuellement ce write ?

## done_when

Un cold run process-fresh, sans comportement synthétique, observe après la
seconde initialisation un write MMIO guest authentifié vers `0x7FC80714`, puis
une soumission bornée du ring final (`submissions >= 1`, RPTR=WPTR consommé),
sans trap ni écriture à `RPTR-0x3c`.

## Test discriminant

Une trace ordonnée unique doit joindre : seconde
`VdInitializeRingBuffer(0x126CA000,16)` → producteur PPC nommé et LR exact →
write `0x7FC80714` non nul → décodage PM4 réussi. L’absence du write distingue
un blocage guest/producteur ; sa présence sans soumission distingue un défaut
du bridge CP.

## Actions

1. Relire `STATE.md`, `EVIDENCE.md`, le rapport durable et les corrections
   historiques déjà citées ; ne pas repartir des anciens verdicts rétractés.
2. Utiliser le source Edge épinglé pour le contrat ring/MMIO, sans lancer Edge.
3. Avec les sondes natives existantes, nommer le premier point de divergence
   après la seconde initialisation ; aucun A/B sauf ambiguïté causale nommée.
4. Si une sémantique runtime manque et est prouvée, ajouter le plus petit
   correctif fail-closed et un test ciblé ; sinon ne pas modifier le code.
5. Exécuter le test discriminant, puis build, CTest 26/26, installation et
   `test ! -e bin/bin`.

## Fichiers et commandes utiles

- `recompilation/ace-combat-6-demo/src/guest_bridge/graphics_ring.hpp`
- `recompilation/ace-combat-6-demo/src/guest_bridge/graphics_dispatch.hpp`
- `recompilation/ace-combat-6-demo/src/xenos_command_processor.cpp`
- `analysis/demo/ac6-demo-ring-readback-frontier-v1.json`
- `.tools/xenia-edge-source/src/xenia/gpu/command_processor.cc`
- `AC6_DEMO_WATCH_RING_KICK=1` sur un `probe` process-fresh borné.

## Abandon ou escalade

Arrêter sans implémenter si le producteur ne peut pas être nommé par une
frontière statique PAL qualifiée, si la seule avance exige une écriture guest
synthétique, ou si trois runs identiques reproduisent exactement la même
absence sans information nouvelle. Escalader alors avec l’adresse, l’input,
l’artefact attendu et une durée bornée.
