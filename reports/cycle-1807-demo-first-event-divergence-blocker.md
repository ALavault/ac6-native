# Cycle 1807 — divergence événementielle avant le ring

Le contraste entre les binaires est réduit à une divergence guest antérieure
au ring. Le courant crée et publie seize événements supplémentaires depuis le
site invité dont le LR est `0x821A61F0`, puis bloque le thread principal sur
`0xE000004C`. Le témoin ancien ne publie pas ces handles et garde le thread
principal runnable jusqu'à la borne.

L'atlas statique place ce site dans `Function_821A6168`, mais ne conserve pas
le pseudocode. Après cinq batches, le gate est arrêté conformément à la
politique quota. L'unique artefact manquant est l'export ciblé de cette
fonction depuis le projet Ghidra démo canonique.

Voir
`artifacts/goal-playable/first-guest-divergence-20260824/BLOCKER.md`.
