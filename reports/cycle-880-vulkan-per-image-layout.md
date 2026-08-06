# Cycle 880 — layouts swapchain par image

Le presenter Vulkan ne suppose plus qu'une seule image de swapchain est
initialisée après la première acquisition. Un tableau d'état par image choisit
`UNDEFINED` ou `PRESENT_SRC_KHR` correctement pour chaque acquisition, puis
marque l'image après soumission. Le smoke présente maintenant deux frames
consécutives pour exercer ce cas.

Validation : build CMake, CTest `1/1`, smoke SDL3/Vulkan double-frame sous
Xvfb : OK.
