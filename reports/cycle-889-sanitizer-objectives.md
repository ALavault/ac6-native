# Cycle 889 — sanitizer après objectifs

Le build Debug ASan/UBSan a été reconfiguré après l’ajout du registre
d’objectifs et des tables optionnelles. Avec `halt_on_error=1` et
`detect_leaks=0`, CTest passe `2/2` et le smoke Vulkan double-frame passe.
Les fuites externes SDL/DBus/DRM documentées au cycle 874 restent hors du
code AC6.
