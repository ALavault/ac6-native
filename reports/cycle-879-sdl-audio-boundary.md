# Cycle 879 — frontière audio SDL3

Ajout de `SdlAudioDevice`, service séparé qui ouvre un flux playback SDL3,
reprend le périphérique, accepte des buffers PCM push et libère proprement le
sous-système. L’absence de périphérique reste fail-closed ; le test pousse un
buffer silencieux quand la configuration audio est disponible. La validation
headless utilise `SDL_AUDIODRIVER=dummy` conformément au contrat projet.

Validation : build CMake, CTest `1/1`, smoke SDL3/Vulkan sous Xvfb : OK.
