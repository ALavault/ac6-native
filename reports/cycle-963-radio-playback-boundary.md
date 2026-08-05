# Cycle 963 — frontière radio/playback

`RadioPlaybackService` résout les assets audio et sous-titre d’un message,
assure une lecture exclusive à durée explicite, suit l’état `Playing` puis
`Complete`, et permet l’interruption. `MissionExecution::play_radio` publie
l’historique radio seulement si le démarrage du playback et la transition HSM
réussissent. Le playback n’avance pas pendant `Paused`.

Le test couvre résolution audio/sous-titre, exclusivité, pause/reprise, fin et
message inexistant. Build, CTest (`5/5`) sous Xvfb avec
`SDL_AUDIODRIVER=dummy`, smoke SDL3/Vulkan et audit campagne passent.

Cette frontière ne prétend pas décoder XMA ni fournir une latence retail : les
services XMA et les durées qualifiées restent à brancher derrière l’interface.
