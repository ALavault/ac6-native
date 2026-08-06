# Cycle 882 — gate CTest headless

Le démarrage sans argument de `ac6-native` est maintenant un test CTest
distinct, en plus des tests runtime. Cette gate couvre le lancement Linux sans
SDL vidéo/Vulkan ; le smoke Vulkan double-frame reste une exécution Xvfb
séparée.

Validation : CTest `2/2`, smoke SDL3/Vulkan sous Xvfb code 0, audit `strings`
sans marqueurs Xbox/Xenia/PPC.
