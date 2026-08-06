# Cycle 812 — paquet Linux natif et audit de livraison

Les règles d’installation CMake produisent uniquement `bin/ac6-native` et
`include/ac6/product_runtime.h`. Un staging Release a été installé puis
inspecté ; `strings` ne trouve aucune trace Xbox, XAM, XMA, Xenia, RexGlue,
XenonRecomp ou PPC.

Validation : build Release, CTest `1/1`, installation staging et audit des
artefacts réussis.
