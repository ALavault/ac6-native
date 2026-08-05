# Cycle 964 — séquenceur HSM mission

`MissionSequenceDirector` ordonne par mission, tick et rang les événements
génériques d’activation/complétion/échec d’objectif et de playback radio.
`MissionExecution` les applique après chaque tick : les préconditions HSM,
campagne et radio restent donc centralisées. Les doublons de rang au même tick
sont rejetés et les événements ne sont marqués publiés qu’après succès.

Le test couvre l’ordre objectif → radio → objectif, le rejet d’un doublon et
les compteurs pending/dispatched. Build, CTest (`5/5`) sous Xvfb avec
`SDL_AUDIODRIVER=dummy`, smoke SDL3/Vulkan et audit campagne passent.

La position du séquenceur n’est pas encore dans `AC6SESS`; une reprise depuis
un checkpoint doit donc fournir le même directeur et ses événements non
publiés via la frontière runtime.
