# Cycle 883 — paquet Linux minimal

CPack produit maintenant `ac6-native-0.1.0-Linux.tar.gz`. Le contenu est
limité à `bin/ac6-native` et aux headers `include/ac6/`; aucune archive retail
ou donnée de diagnostic n’est embarquée. L’audit des strings du binaire dans
le paquet ne trouve aucun marqueur Xbox/Xenia/PPC.

Validation : CTest `2/2`, smoke SDL3/Vulkan double-frame sous Xvfb code 0.
