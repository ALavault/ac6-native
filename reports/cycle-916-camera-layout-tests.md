# Cycle 916 — tests de non-régression du layout caméra

Le contrat `column_major` est désormais couvert par `product_runtime_tests` :
un manifeste retail-vectoriel est accepté, un manifeste historique reste
row-major, et tout suffixe de layout inconnu est refusé fail-closed.

Validation : `SDL_AUDIODRIVER=dummy xvfb-run -a ctest --test-dir
reconstruction/ace-combat-6/build --output-on-failure` — 3/3.
