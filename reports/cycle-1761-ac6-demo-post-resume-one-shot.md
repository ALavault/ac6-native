# AC6 demo post-resume one-shot — cycle 1761

La capsule maîtresse est
`analysis/demo/ac6-demo-post-resume-ab/sha256/940637146a447e48fc1619471b9910278c962ca0b261017a269c3cc4affca0c8/receipt.json`,
SHA-256 `940637146a447e48fc1619471b9910278c962ca0b261017a269c3cc4affca0c8`.
La cible est le projet Ghidra `ace-combat-6-demo`, démo PAL
`ac6-demo-xbox360-pal`, `Default.xex`, SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`,
architecture Xenon big-endian/Xenos.

Deux processus frais Vulkan, routes neutral et `buttons=16`, ont atteint
`max_ticks=5600`, `5463 PRESENT`, `23 blocked/0 runnable`, sans frontend,
mission ni terminal. L’entrée `buttons=16` est observée au tick 252. Les
sous-arbres JSON `outcome`, `milestones`, `graphics` et `scheduler` sont égaux;
les traces sont divergentes et restent conservées comme telles.

Chaque route produit exactement un handoff et un accès post-resume au tick 1,
thread 1 : `load64` à `0x7F0409D8`, valeur zéro, ligne générée 30, PC guest
unique `0x82327154`, octets `eb 61 ff d0`. Le LR `0x821A69CC` n’est pas le PC;
le mapper `ppc_func_mapping` ferme le mapping unique, mais le propriétaire
Ghidra reste indisponible. La frontière fermée est donc le premier accès
post-resume, pas la consommation de START ni une transition frontend.

Les reçus d’échec `f6084b87…` et `2c6c088b…` sont conservés comme provenance
seulement. La revue rapporte 13 cas de cycle de vie Xvfb plus un smoke réel et
9 tests du mapper. Aucun C++ généré/Ghidra n’a été modifié; Xenia et preuves
retail sont absents. `supported=false` reste inchangé.

Validation de ce checkpoint : JSON, hash canonique, test ciblé de reçu et diff
check; aucun build/runtime n’est relancé.
