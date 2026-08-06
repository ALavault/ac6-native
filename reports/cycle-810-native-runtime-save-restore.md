# Cycle 810 — reprise runtime et audit dépendances

`MissionRuntime` expose `snapshot()`/`restore()` ; `SaveStore` peut conserver
le checkpoint et une reprise restaure tick et transform avant de poursuivre.
Les snapshots invalides (tick nul) sont refusés.

Validation : CMake/CTest `1/1`, reprise testée après un tick divergent ; audit
`nm` sans symbole Xbox/XAM/XMA/Rex/PPC/Xenia et `ldd` limité aux bibliothèques
Linux standard.
