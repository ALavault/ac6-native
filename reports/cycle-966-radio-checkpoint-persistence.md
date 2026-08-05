# Cycle 966 — persistance du playback radio

`MissionExecution::Checkpoint` conserve maintenant le snapshot radio actif :
mission, message, assets audio/sous-titre, temps écoulé, durée et état. Le
format `AC6SESS` passe en version 4; les versions 1 à 3 restent lisibles, avec
un playback Idle implicite pour les anciens checkpoints.

La validation rejette états inconnus, identités absentes, durées non finies,
temps hors intervalle et mélange de mission. Le round-trip couvre le playback
Playing en plus des séquences published/pending. Build, CTest (`5/5`) sous Xvfb
avec `SDL_AUDIODRIVER=dummy`, smoke SDL3/Vulkan et audit campagne passent.
