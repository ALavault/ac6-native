# Cycle 874 — gate ASan/UBSan

Un build Debug séparé avec `-fsanitize=address,undefined` compile les trois
cibles. CTest et le smoke Vulkan passent avec `halt_on_error=1` et
`detect_leaks=0`, sans erreur ASan/UBSan.

Avec LeakSanitizer activé, le test SDL rapporte 2558 octets provenant de
libSDL3/libdbus/libdrm lors de l'initialisation des backends ; les frames
pointent hors du code AC6. Cette fuite d'environnement est conservée comme
risque résiduel, pas masquée par une modification du produit.
