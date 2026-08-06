# Cycle 871 — différentiel de reprise complet

`--services-smoke` ne vérifie plus seulement le tick restauré : après lecture
du save, il rejoue les 15 frames restantes et compare tick et positions à la
trajectoire de référence. Le test unitaire couvre aussi deux
`MissionExecution` restaurées depuis le même snapshot et exige la même frame
suivante.

Validation : build CMake, CTest `1/1`, smoke SDL3/Vulkan sous Xvfb : OK.
