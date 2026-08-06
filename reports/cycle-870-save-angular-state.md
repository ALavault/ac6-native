# Cycle 870 — snapshot complet de l’état de vol

`RuntimeSnapshot` inclut maintenant pitch, roll et yaw. `MissionRuntime` les
restaure avec validation finite, et `SaveStore` écrit le format AC6SAVE v2
(36 octets par entrée) tout en acceptant les fichiers v1 (angles initialisés à
zéro). Les tests vérifient la restauration et la persistance des trois états
angulaires.

Validation : build CMake, CTest `1/1` et smoke SDL3/Vulkan sous Xvfb : OK.
