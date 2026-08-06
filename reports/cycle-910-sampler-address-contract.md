# Cycle 910 — modes d'adressage texture

Le sampler respecte maintenant `sampler_address` : `wrap` replie les UV par
partie fractionnaire, tandis que `clamp` borne à l'intervalle [0,1]. Une fixture
P6 2×1 couvre les deux comportements hors domaine.

Validations : CTest normal 3/3 et ASan/UBSan 3/3.
