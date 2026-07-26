# Cycle 2 — front door XEX AC6 PAL

`analysis-manifest.json` lie `default.xex` à son SHA-256, image base
`0x82000000`, entrée `0x821f5e90`, 12 sections XEXLoaderWV, 8 824 fonctions et
9 874 arêtes directes. Le XEX n'est pas interprété par `rabin2`; les segments
proviennent du journal XEXLoaderWV de l'import validé.

Le vertical slice statique actuel est : entrée `0x821f5e90` -> appel direct
`0x82382ef8`, confirmé à la fois par `VerifyXexEntry.java` sans analyse et par
l'export Ghidra. `0x82382ef8` reste sans rôle nommé et partagé; aucune conclusion
sur menu, renderer ou boucle de jeu n'est permise. Prochaine action : tracer
ce front door sous Xenia/XenonTests jusqu'à une scène attribuable.
