# Cycle 930 — smoke frontend avec profil PAL

Le binaire reconstruit traverse effectivement `Title → New Game → Briefing →
Hangar → Loading → Mission` avec le profil explicite Normal/Normal/English.
Run : `SDL_AUDIODRIVER=dummy xvfb-run -a ac6-native --frontend-smoke ... 1`
retourne 0 sur le manifeste externe frontend.

Les assets restent externes et aucune preuve de vol retail n’est déduite de ce
smoke frontend.
