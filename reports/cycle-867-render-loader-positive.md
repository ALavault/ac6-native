# Cycle 867 — validation positive du loader render

Le test runtime charge maintenant un manifeste externe complet (catalogue,
assets, lancement, 10 tables render et buffers), vérifie les contrats de
géométrie, décode un buffer qualifié et publie la base atomiquement. La
fixture est temporaire et synthétique : elle ne constitue pas une preuve de
parité retail et est supprimée en fin de test.

Validation : build CMake, CTest `1/1`, smoke SDL3/Vulkan sous Xvfb avec
`SDL_AUDIODRIVER=dummy` : tous réussis. Aucun fichier fixture ne reste après
le test.
