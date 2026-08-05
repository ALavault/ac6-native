# Cycle 965 — persistance du séquenceur

`AC6SESS` passe en version 3 pour ajouter l’état du
`MissionSequenceDirector` au checkpoint : événements triés, identité mission,
rang, type, durée et drapeau published/pending. Les versions 1 et 2 restent
lisibles; les anciennes v2 sont interprétées sans séquence.

La validation refuse les événements malformés, mélangés entre missions,
dupliqués ou hors bornes, avant toute publication. Le round-trip de session
couvre un événement publié et un événement encore pending, ainsi que la
compatibilité v1. Build, CTest (`5/5`) sous Xvfb avec
`SDL_AUDIODRIVER=dummy`, smoke SDL3/Vulkan et audit campagne passent.
